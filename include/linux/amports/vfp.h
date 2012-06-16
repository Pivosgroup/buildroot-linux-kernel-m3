/*
#define DLOCK() unsigned long flags
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 */

#ifndef __VFP_H_
#define __VFP_H_

typedef struct {
    int rp;
    int wp;
    int size;
    int pre_rp;
    int pre_wp;
    struct vframe_s **pool;
} vfq_t;


#define DVLOCK() unsigned long _vlock_flags
#define VLOCK() {raw_local_save_flags(_vlock_flags);local_fiq_disable();}
#define VUNLOCK() {raw_local_irq_restore(_vlock_flags);}

static inline void vfq_lookup_start(vfq_t *q)
{
	DVLOCK();
	VLOCK();
	 q->pre_rp =  q->rp ;
	 q->pre_wp = q->wp;
	VUNLOCK();
}
static inline void vfq_lookup_end(vfq_t *q)
{
	DVLOCK();
        VLOCK();
	  q->rp = q->pre_rp ;
	  q->wp = q->pre_wp ;	
	VUNLOCK();
}

static inline void vfq_init(vfq_t *q, u32 size, struct vframe_s **pool)
{
	DVLOCK();
        VLOCK();
    q->rp = q->wp = 0;
    q->size = size;
    q->pool = pool;
	VUNLOCK();
}

static inline bool vfq_empty(vfq_t *q)
{
	bool ret;
	DVLOCK();
        VLOCK();
	ret=(q->rp == q->wp);
	VUNLOCK();
	return ret;
}

static inline void vfq_push(vfq_t *q, vframe_t *vf)
{
	DVLOCK();
        VLOCK();
    	q->pool[q->wp] = vf;
    	q->wp = (q->wp == (q->size-1)) ? 0 : (q->wp+1);
	VUNLOCK();
}

static inline vframe_t *vfq_pop(vfq_t *q)
{
    vframe_t *vf;
	DVLOCK();
    	if (vfq_empty(q))
        	return NULL;
        VLOCK();	    
	vf = q->pool[q->rp];

    	q->rp = (q->rp == (q->size-1)) ? 0 : (q->rp+1);
	VUNLOCK();
    return vf;
}

static inline vframe_t *vfq_peek(vfq_t *q)
{
	vframe_t * vf;
	DVLOCK();
        VLOCK();
	vf=(vfq_empty(q)) ? NULL : q->pool[q->rp];
	VUNLOCK();
	return vf;
}

static inline int vfq_level(vfq_t *q)
{
    int level;
	
	DVLOCK();
        VLOCK();
	level= q->wp - q->rp;
    
    	if (level < 0)
        	level += q->size;
	VUNLOCK();
   	 return level;
}

#endif /* __VFP_H_ */
