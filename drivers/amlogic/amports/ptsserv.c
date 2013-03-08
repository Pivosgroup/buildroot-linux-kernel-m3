#include <linux/module.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/amports/ptsserv.h>
#include <linux/amports/timestamp.h>
#include <linux/amports/tsync.h>

#include <mach/am_regs.h>

//#define DEBUG_VIDEO
//#define DEBUG_AUDIO
//#define DEBUG_CHECKIN
//#define DEBUG_CHECKOUT

#define VIDEO_REC_SIZE  8192
#define AUDIO_REC_SIZE  8192
#define VIDEO_LOOKUP_RESOLUTION 2500
#define AUDIO_LOOKUP_RESOLUTION 1024

#define OFFSET_DIFF(x, y)  ((int)(x - y))
#define OFFSET_LATER(x, y) (OFFSET_DIFF(x, y) > 0)
#define OFFSET_EQLATER(x, y) (OFFSET_DIFF(x, y) >= 0)

enum {
    PTS_IDLE       = 0,
    PTS_INIT       = 1,
    PTS_LOADING    = 2,
    PTS_RUNNING    = 3,
    PTS_DEINIT     = 4
};

typedef struct pts_rec_s {
    struct list_head list;
    u32 offset;
    u32 val;
} pts_rec_t;

typedef struct pts_table_s {
    u32 status;
    int rec_num;
    int lookup_threshold;
    u32 lookup_cache_offset;
    bool lookup_cache_valid;
    u32 lookup_cache_pts;
    u32 buf_start;
    u32 buf_size;
    int first_checkin_pts;
    int first_lookup_ok;
    int first_lookup_is_fail;    /* 1: first lookup fail;  0: first lookup success */
    pts_rec_t *pts_recs;
	unsigned long *pages_list;
    struct list_head *pts_search;
    struct list_head valid_list;
    struct list_head free_list;
#ifdef CALC_CACHED_TIME
    u32 last_checkin_offset;
    u32 last_checkin_pts;
    u32 last_checkout_pts;
#endif
} pts_table_t;

static spinlock_t lock = SPIN_LOCK_UNLOCKED;

static pts_table_t pts_table[PTS_TYPE_MAX] = {
    {
        .status = PTS_IDLE,
        .rec_num = VIDEO_REC_SIZE,
        .lookup_threshold = VIDEO_LOOKUP_RESOLUTION,
    },
    {
        .status = PTS_IDLE,
        .rec_num = AUDIO_REC_SIZE,
        .lookup_threshold = AUDIO_LOOKUP_RESOLUTION,
    },
};

static inline void get_wrpage_offset(u8 type, u32 *page, u32 *page_offset)
{
    ulong flags;
    u32 page1, page2, offset;

    if (type == PTS_TYPE_VIDEO) {
        do {
            local_irq_save(flags);

            page1 = READ_MPEG_REG(PARSER_AV_WRAP_COUNT) & 0xffff;
            offset = READ_MPEG_REG(PARSER_VIDEO_WP);
            page2 = READ_MPEG_REG(PARSER_AV_WRAP_COUNT) & 0xffff;

            local_irq_restore(flags);
        } while (page1 != page2);

        *page = page1;
        *page_offset = offset - pts_table[PTS_TYPE_VIDEO].buf_start;
    } else if (type == PTS_TYPE_AUDIO) {
        do {
            local_irq_save(flags);

            page1 = READ_MPEG_REG(PARSER_AV_WRAP_COUNT) >> 16;
            offset = READ_MPEG_REG(PARSER_AUDIO_WP);
            page2 = READ_MPEG_REG(PARSER_AV_WRAP_COUNT) >> 16;

            local_irq_restore(flags);
        } while (page1 != page2);

        *page = page1;
        *page_offset = offset - pts_table[PTS_TYPE_AUDIO].buf_start;
    }
}

static inline void get_rdpage_offset(u8 type, u32 *page, u32 *page_offset)
{
    ulong flags;
    u32 page1, page2, offset;

    if (type == PTS_TYPE_VIDEO) {
        do {
            local_irq_save(flags);

            page1 = READ_MPEG_REG(VLD_MEM_VIFIFO_WRAP_COUNT) & 0xffff;
            offset = READ_MPEG_REG(VLD_MEM_VIFIFO_RP);
            page2 = READ_MPEG_REG(VLD_MEM_VIFIFO_WRAP_COUNT) & 0xffff;

            local_irq_restore(flags);
        } while (page1 != page2);

        *page = page1;
        *page_offset = offset - pts_table[PTS_TYPE_VIDEO].buf_start;
    } else if (type == PTS_TYPE_AUDIO) {
        do {
            local_irq_save(flags);

            page1 = READ_MPEG_REG(AIU_MEM_AIFIFO_BUF_WRAP_COUNT) & 0xffff;
            offset = READ_MPEG_REG(AIU_MEM_AIFIFO_MAN_RP);
            page2 = READ_MPEG_REG(AIU_MEM_AIFIFO_BUF_WRAP_COUNT) & 0xffff;


            local_irq_restore(flags);
        } while (page1 != page2);


        *page = page1;
        *page_offset = offset - pts_table[PTS_TYPE_AUDIO].buf_start;
    }
}

#ifdef CALC_CACHED_TIME
int pts_cached_time(u8 type)
{
    pts_table_t *pTable;
    u32 pts;

    if (type >= PTS_TYPE_MAX) {
        return 0;
    }

    pTable = &pts_table[type];

    if(type==PTS_TYPE_VIDEO)
        pts = timestamp_apts_get();
    else
        pts = timestamp_vpts_get();

    if(pts==-1)
        pts = pTable->last_checkout_pts;

    if((pTable->last_checkin_pts==-1) || (pts==-1))
        return 0;

    return pTable->last_checkin_pts-pts;
}

EXPORT_SYMBOL(pts_cached_time);
#endif

int pts_checkin_offset(u8 type, u32 offset, u32 val)
{
    ulong flags;
    const u32 pts_reg[PTS_TYPE_MAX] = {VIDEO_PTS, AUDIO_PTS};
    pts_table_t *pTable;

    if (type >= PTS_TYPE_MAX) {
        return -EINVAL;
    }

    pTable = &pts_table[type];

    spin_lock_irqsave(&lock, flags);

    if (likely((pTable->status == PTS_RUNNING) ||
               (pTable->status == PTS_LOADING))) {
        pts_rec_t *rec;

        if (type == PTS_TYPE_VIDEO && pTable->first_checkin_pts == -1) {
            pTable->first_checkin_pts = val;
#ifdef DEBUG_CHECKIN
#ifdef DEBUG_VIDEO
            printk("first check in vpts <0x%x:0x%x> ok!\n", offset, val);
#endif
#endif
        }
        if (type == PTS_TYPE_AUDIO && pTable->first_checkin_pts == -1) {
            pTable->first_checkin_pts = val;
#ifdef DEBUG_CHECKIN
#ifdef DEBUG_AUDIO
            printk("first check in apts <0x%x:0x%x> ok!\n", offset, val);
#endif
#endif
        }

#ifdef DEBUG_CHECKIN
#ifdef DEBUG_VIDEO
        if (type == PTS_TYPE_VIDEO) {
            printk("check in vpts <0x%x:0x%x>\n", offset, val);
        }
#endif
#ifdef DEBUG_AUDIO
        if (type == PTS_TYPE_AUDIO) {
            printk("check in apts <0x%x:0x%x>\n", offset, val);
        }
#endif
#endif
        if (list_empty(&pTable->free_list)) {
            rec = list_entry(pTable->valid_list.next, pts_rec_t, list);
        } else {
            rec = list_entry(pTable->free_list.next, pts_rec_t, list);
        }

        rec->offset = offset;
        rec->val = val;

#ifdef CALC_CACHED_TIME
	{
		s32 diff = offset-pTable->last_checkin_offset;
		if(diff>0){
			pTable->last_checkin_offset = offset;
			pTable->last_checkin_pts    = val;
		}
	}
#endif

        list_move_tail(&rec->list, &pTable->valid_list);

        spin_unlock_irqrestore(&lock, flags);

        if (pTable->status == PTS_LOADING) {
#ifdef DEBUG_VIDEO
            if (type == PTS_TYPE_VIDEO) {
                printk("init vpts[%d] at 0x%x\n", type, val);
            }
#endif
#ifdef DEBUG_VIDEO
            if (type == PTS_TYPE_AUDIO) {
                printk("init apts[%d] at 0x%x\n", type, val);
            }
#endif
            WRITE_MPEG_REG(pts_reg[type], val);
            pTable->status = PTS_RUNNING;
        }

        return 0;

    } else {
        spin_unlock_irqrestore(&lock, flags);

        return -EINVAL;
    }
}

EXPORT_SYMBOL(pts_checkin_offset);

/* This type of PTS could happen in the past,
 * e.g. from TS demux when the real time (wr_page, wr_ptr)
 * could be bigger than pts parameter here.
 */
int pts_checkin_wrptr(u8 type, u32 ptr, u32 val)
{
    u32 offset, cur_offset, page, page_no;

    if (type >= PTS_TYPE_MAX) {
        return -EINVAL;
    }

    offset = ptr - pts_table[type].buf_start;
    get_wrpage_offset(type, &page, &cur_offset);

    page_no = (offset > cur_offset) ? (page - 1) : page;

    return pts_checkin_offset(type,
                              pts_table[type].buf_size * page_no + offset, val);
}

EXPORT_SYMBOL(pts_checkin_wrptr);

int pts_checkin(u8 type, u32 val)
{
    u32 page, offset;

    get_wrpage_offset(type, &page, &offset);

    if (type == PTS_TYPE_VIDEO) {
        offset = page * pts_table[PTS_TYPE_VIDEO].buf_size + offset;
        pts_checkin_offset(PTS_TYPE_VIDEO, offset, val);
        return 0;
    } else if (type == PTS_TYPE_AUDIO) {
        offset = page * pts_table[PTS_TYPE_AUDIO].buf_size + offset;
        pts_checkin_offset(PTS_TYPE_AUDIO, offset, val);
        return 0;
    } else {
        return -EINVAL;
    }
}

EXPORT_SYMBOL(pts_checkin);

int pts_lookup(u8 type, u32 *val, u32 pts_margin)
{
    u32 page, offset;

    get_rdpage_offset(type, &page, &offset);

    if (type == PTS_TYPE_VIDEO) {
        offset = page * pts_table[PTS_TYPE_VIDEO].buf_size + offset;
        pts_lookup_offset(PTS_TYPE_VIDEO, offset, val, pts_margin);
        return 0;
    } else if (type == PTS_TYPE_AUDIO) {
        offset = page * pts_table[PTS_TYPE_AUDIO].buf_size + offset;
        pts_lookup_offset(PTS_TYPE_AUDIO, offset, val, pts_margin);
        return 0;
    } else {
        return -EINVAL;
    }
}

EXPORT_SYMBOL(pts_lookup);

int pts_lookup_offset(u8 type, u32 offset, u32 *val, u32 pts_margin)
{
    ulong flags;
    pts_table_t *pTable;
    int lookup_threshold;
#if defined(DEBUG_VIDEO) || defined(DEBUG_AUDIO)
    int look_cnt = 0;
#endif

    if (type >= PTS_TYPE_MAX) {
        return -EINVAL;
    }

    pTable = &pts_table[type];

    if (pts_margin == 0) {
        lookup_threshold = pTable->lookup_threshold;
    } else {
        lookup_threshold = pts_margin;
    }

    if (!pTable->first_lookup_ok) {
        lookup_threshold <<= 1;
    }

    spin_lock_irqsave(&lock, flags);

    if (likely(pTable->status == PTS_RUNNING)) {
        pts_rec_t *p, *p2 = NULL;

        if ((pTable->lookup_cache_valid) &&
            (offset == pTable->lookup_cache_offset)) {
            spin_unlock_irqrestore(&lock, flags);

            *val = pTable->lookup_cache_pts;
            return 0;
        }

        if (list_empty(&pTable->valid_list)) {
            spin_unlock_irqrestore(&lock, flags);

            return -1;
        }

        if (pTable->pts_search == &pTable->valid_list) {
            p = list_entry(pTable->valid_list.next, pts_rec_t, list);
        } else {
            p = list_entry(pTable->pts_search, pts_rec_t, list);
        }

        if (OFFSET_LATER(offset, p->offset)) {
            p2 = p; /* lookup candidate */

            list_for_each_entry_continue(p, &pTable->valid_list, list) {
#if 0
                if (type == PTS_TYPE_VIDEO) {
                    printk("   >> rec: 0x%x\n", p->offset);
                }
#endif
#if defined(DEBUG_VIDEO) || defined(DEBUG_AUDIO)
                look_cnt++;
#endif

                if (OFFSET_LATER(p->offset, offset)) {
                    break;
                }

                if (type == PTS_TYPE_AUDIO) {
                    list_move_tail(&p2->list, &pTable->free_list);
                }

                p2 = p;
            }
        } else if (OFFSET_LATER(p->offset, offset)) {
            list_for_each_entry_continue_reverse(p, &pTable->valid_list, list) {
#if 0
                if (type == PTS_TYPE_VIDEO) {
                    printk("   >> rec: 0x%x\n", p->offset);
                }
#endif
#ifdef DEBUG
                look_cnt++;
#endif
                if (OFFSET_EQLATER(offset, p->offset)) {
                    p2 = p;
                    break;
                }
            }
        } else {
            p2 = p;
        }

        if ((p2) &&
            (OFFSET_DIFF(offset, p2->offset) < lookup_threshold)) {
#ifdef DEBUG_CHECKOUT
#ifdef DEBUG_VIDEO
            if (type == PTS_TYPE_VIDEO)
                printk("vpts look up offset<0x%x> --> <0x%x:0x%x>, look_cnt = %d\n",
                       offset, p2->offset, p2->val, look_cnt);
#endif
#ifdef DEBUG_AUDIO
            if (type == PTS_TYPE_AUDIO)
                printk("apts look up offset<0x%x> --> <0x%x:0x%x>, look_cnt = %d\n",
                       offset, p2->offset, p2->val, look_cnt);
#endif
#endif
            *val = p2->val;

#ifdef CALC_CACHED_TIME
	    pTable->last_checkout_pts = p2->val;
#endif

            pTable->lookup_cache_pts = *val;
            pTable->lookup_cache_offset = offset;
            pTable->lookup_cache_valid = true;

            /* update next look up search start point */
            pTable->pts_search = p2->list.prev;

            list_move_tail(&p2->list, &pTable->free_list);

            spin_unlock_irqrestore(&lock, flags);

            if (!pTable->first_lookup_ok) {
                pTable->first_lookup_ok = 1;
#ifdef DEBUG_CHECKOUT
#ifdef DEBUG_VIDEO
                if (type == PTS_TYPE_VIDEO) {
                    printk("=====first vpts look up offset<0x%x> --> <0x%x:0x%x> ok!\n", offset, p2->offset, p2->val);
                }
#endif
#ifdef DEBUG_AUDIO
                if (type == PTS_TYPE_AUDIO) {
                    printk("====first apts look up offset<0x%x> --> <0x%x:0x%x> ok!\n", offset, p2->offset, p2->val);
                }
#endif
#endif
            }
            return 0;

        } else {
            /* when first pts lookup failed, use first checkin pts instead */
            if (!pTable->first_lookup_ok) {
                *val = pTable->first_checkin_pts;
                pTable->first_lookup_ok = 1;
		  pTable->first_lookup_is_fail = 1;

#ifdef DEBUG_CHECKOUT
#ifdef DEBUG_VIDEO
                if (type == PTS_TYPE_VIDEO)
                    printk("first vpts look up offset<0x%x> failed, return first_checkin_pts<0x%x>\n",
                           offset, *val);
#endif

#ifdef DEBUG_AUDIO
                if (type == PTS_TYPE_AUDIO)
                    printk("first apts look up offset<0x%x> failed, return first_checkin_pts<0x%x>\n",
                           offset, *val);
#endif
#endif
                spin_unlock_irqrestore(&lock, flags);

                return 0;
            }

#ifdef DEBUG_CHECKOUT
#ifdef DEBUG_VIDEO
            if (type == PTS_TYPE_VIDEO) {
                printk("vpts look up offset<0x%x> failed, look_cnt = %d\n", offset, look_cnt);
            }
#endif
#ifdef DEBUG_AUDIO
            if (type == PTS_TYPE_AUDIO) {
                printk("apts look up offset<0x%x> failed, look_cnt = %d\n", offset, look_cnt);
            }
#endif
#endif
            spin_unlock_irqrestore(&lock, flags);

            return -1;
        }
    }

    spin_unlock_irqrestore(&lock, flags);

    return -1;
}

EXPORT_SYMBOL(pts_lookup_offset);

int pts_set_resolution(u8 type, u32 level)
{
    if (type >= PTS_TYPE_MAX) {
        return -EINVAL;
    }

    pts_table[type].lookup_threshold = level;
    return 0;
}

EXPORT_SYMBOL(pts_set_resolution);

int pts_set_rec_size(u8 type, u32 val)
{
    ulong flags;

    if (type >= PTS_TYPE_MAX) {
        return -EINVAL;
    }

    spin_lock_irqsave(&lock, flags);

    if (pts_table[type].status == PTS_IDLE) {
        pts_table[type].rec_num = val;

        spin_unlock_irqrestore(&lock, flags);

        return 0;

    } else {
        spin_unlock_irqrestore(&lock, flags);

        return -EBUSY;
    }
}

EXPORT_SYMBOL(pts_set_rec_size);
//#define SIMPLE_ALLOC_LIST
static void free_pts_list(pts_table_t *pTable)
{
#ifdef SIMPLE_ALLOC_LIST
	if(0){/*don't free,used a static memory*/
		kfree(pTable->pts_recs);
		pTable->pts_recs = NULL;
	}
#else
	unsigned long *p=pTable->pages_list;
	void *onepage=(void  *)p[0];
	while(onepage!=NULL){
		free_page((unsigned long)onepage);
		p++;
		onepage=(void  *)p[0];
	}
	if(pTable->pages_list)
		kfree(pTable->pages_list);
	pTable->pages_list=NULL;
#endif
	INIT_LIST_HEAD(&pTable->valid_list);
	INIT_LIST_HEAD(&pTable->free_list);
}

static int alloc_pts_list(pts_table_t *pTable)
{
	int i;
	int page_nums;
	INIT_LIST_HEAD(&pTable->valid_list);
	INIT_LIST_HEAD(&pTable->free_list);
#ifdef SIMPLE_ALLOC_LIST
	if (!pTable->pts_recs){
		pTable->pts_recs = kcalloc(pTable->rec_num,
							sizeof(pts_rec_t), GFP_KERNEL);
	}
	if (!pTable->pts_recs) {
            pTable->status = 0;
            return -ENOMEM;
        }
	for (i = 0; i < pTable->rec_num; i++)
		list_add_tail(&pTable->pts_recs[i].list, &pTable->free_list);
	return 0;
#else
	page_nums=pTable->rec_num*sizeof(pts_rec_t)/PAGE_SIZE;
	if(PAGE_SIZE/sizeof(pts_rec_t)!=0){
		page_nums=(pTable->rec_num+page_nums+1)*sizeof(pts_rec_t)/PAGE_SIZE;
	}
	pTable->pages_list=kzalloc(page_nums*4+4,GFP_KERNEL);
	if(pTable->pages_list==NULL)
		return -ENOMEM;
	for(i=0;i<page_nums;i++)
	{
		int j;
		void  *one_page=(void *)__get_free_page(GFP_KERNEL);
		pts_rec_t *recs=one_page;
		if(one_page==NULL)
			goto error_alloc_pages;
		for(j=0;j<PAGE_SIZE/sizeof(pts_rec_t);j++)
			list_add_tail(&recs[j].list, &pTable->free_list);
		pTable->pages_list[i]=(unsigned long)one_page;
	}
	return 0;
error_alloc_pages:
	free_pts_list(pTable);
#endif
	return -ENOMEM;
}

int pts_start(u8 type)
{
    ulong flags;
    pts_table_t *pTable;

    if (type >= PTS_TYPE_MAX) {
        return -EINVAL;
    }

    pTable = &pts_table[type];

    spin_lock_irqsave(&lock, flags);

    if (likely(pTable->status == PTS_IDLE)) {
        pTable->status = PTS_INIT;

        spin_unlock_irqrestore(&lock, flags);

        if(alloc_pts_list(pTable)!=0){
			return -ENOMEM;
        }

        if (type == PTS_TYPE_VIDEO) {
            pTable->buf_start = READ_MPEG_REG(VLD_MEM_VIFIFO_START_PTR);
            pTable->buf_size = READ_MPEG_REG(VLD_MEM_VIFIFO_END_PTR)
                               - pTable->buf_start + 8;

            /* since the HW buffer wrap counter only have 16 bits,
             * a too small buf_size will make pts lookup fail with streaming
             * offset wrapped before 32 bits boundary.
             * This is unlikely to set such a small streaming buffer though.
             */
            //BUG_ON(pTable->buf_size <= 0x10000);

            WRITE_MPEG_REG(VIDEO_PTS, 0);
            timestamp_pcrscr_set(0);//video always need the pcrscr,Clear it to use later
            pTable->first_checkin_pts = -1;
            pTable->first_lookup_ok = 0;
	     pTable->first_lookup_is_fail = 0;
        } else if (type == PTS_TYPE_AUDIO) {
            pTable->buf_start = READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR);
            pTable->buf_size = READ_MPEG_REG(AIU_MEM_AIFIFO_END_PTR)
                               - pTable->buf_start + 8;

            //BUG_ON(pTable->buf_size <= 0x10000);

            WRITE_MPEG_REG(AUDIO_PTS, 0);
            pTable->first_checkin_pts = -1;
            pTable->first_lookup_ok = 0;
	     	pTable->first_lookup_is_fail = 0;
        }

#ifdef CALC_CACHED_TIME
	pTable->last_checkin_offset = 0;
	pTable->last_checkin_pts    = -1;
	pTable->last_checkout_pts   = -1;
#endif

        pTable->pts_search = &pTable->valid_list;
        pTable->status = PTS_LOADING;
        pTable->lookup_cache_valid = false;

        return 0;

    } else {
        spin_unlock_irqrestore(&lock, flags);

        return -EBUSY;
    }
}

EXPORT_SYMBOL(pts_start);

int pts_stop(u8 type)
{
    ulong flags;
    pts_table_t *pTable;

    if (type >= PTS_TYPE_MAX) {
        return -EINVAL;
    }

    pTable = &pts_table[type];

    spin_lock_irqsave(&lock, flags);

    if (likely((pTable->status == PTS_RUNNING) ||
               (pTable->status == PTS_LOADING))) {
        pTable->status = PTS_DEINIT;

        spin_unlock_irqrestore(&lock, flags);

        free_pts_list(pTable);
		
        pTable->status = PTS_IDLE;

        if (type == PTS_TYPE_AUDIO) {
            timestamp_apts_set(-1);
        }
 		tsync_mode_reinit();
        return 0;

    } else {
        spin_unlock_irqrestore(&lock, flags);

        return -EBUSY;
    }
}

EXPORT_SYMBOL(pts_stop);

int first_lookup_pts_failed(u8 type)
{
    pts_table_t *pTable;

    if (type >= PTS_TYPE_MAX) {
        return -EINVAL;
    }

    pTable = &pts_table[type];

    return pTable->first_lookup_is_fail;
}
EXPORT_SYMBOL(first_lookup_pts_failed);

int first_pts_checkin_complete(u8 type)
{
    pts_table_t *pTable;

    if (type >= PTS_TYPE_MAX) {
        return -EINVAL;
    }

    pTable = &pts_table[type];

    if (pTable->first_checkin_pts == -1)
        return 0;
    else
        return 1;
}
EXPORT_SYMBOL(first_pts_checkin_complete);
