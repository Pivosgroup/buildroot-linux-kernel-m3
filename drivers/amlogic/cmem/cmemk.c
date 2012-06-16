/*
 *  Copyright 2010 by AMLOGIC INC.
 *  Common physical memory allocator driver.
 */

/*
 *  Copyright 2009 by Texas Instruments Incorporated.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

/*
 * cmemk.c
 */
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
#include <linux/dma-mapping.h>
#else
#include <asm/cacheflush.h>
#endif
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/io.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
/*
 * L_PTE_MT_BUFFERABLE and L_PTE_MT_ WRITETHROUGH were introduced in 2.6.28
 * and are preferred over L_PTE_BUFFERABLE and L_PTE_CACHEABLE (which are
 * themselves going away).
 */
#define USE_PTE_MT
#else
#undef USE_PTE_MT
#endif

/*
 * The following macros control version-dependent code:
 * USE_CACHE_DMAAPI - #define if more common DMA functions are used
 * USE_CACHE_VOID_ARG - #define if dmac functions take "void *" parameters,
 *    otherwise unsigned long is used
 * USE_CLASS_SIMPLE - #define if Linux version contains "class_simple*",
 *    otherwise "class*" or "device*" is used (see USE_CLASS_DEVICE usage).
 * USE_CLASS_DEVICE - #define if Linux version contains "class_device*",
 *    otherwise "device*" or "class_simple*" is used (see USE_CLASS_SIMPLE
 *    usage).
 * If neither USE_CLASS_SIMPLE nor USE_CLASS_DEVICE is set, there is further
 *    kernel version checking embedded in the module init & exit functions.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
	
#define USE_CACHE_DMAAPI
#undef USE_CLASS_DEVICE
#undef USE_CLASS_SIMPLE
	
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)

#define USE_CACHE_VOID_ARG
#undef USE_CLASS_DEVICE
#undef USE_CLASS_SIMPLE

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)

#define USE_CACHE_VOID_ARG
#define USE_CLASS_DEVICE
#undef USE_CLASS_SIMPLE

#else  /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18) */

#define USE_CLASS_SIMPLE
#undef USE_CLASS_DEVICE
#undef USE_CACHE_VOID_ARG

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18) */

#include <linux/cmem.h>

/*
 * Poor man's config params
 */

/*
 * NOCACHE means physical memory block is ioremap()'ed as noncached
 */
#define NOCACHE

/*
 * USE_MMAPSEM means acquire/release current->mm->mmap_sem around calls
 * to dma_[flush/clean/inv]_range.
 */
//#define USE_MMAPSEM

/*
 * CHECK_FOR_ALLOCATED_BUFFER means ensure that the passed addr/size block
 * is actually an allocated, CMEM-defined buffer.
 */
//#define CHECK_FOR_ALLOCATED_BUFFER

/* HEAP_ALIGN is used in place of sizeof(HeapMem_Header) */
#define HEAP_ALIGN PAGE_SIZE


#ifdef __DEBUG
#define __D(fmt, args...) printk(KERN_DEBUG "CMEMK Debug: " fmt, ## args)
#else
#define __D(fmt, args...)
#endif

#define __E(fmt, args...) printk(KERN_ERR "CMEMK Error: " fmt, ## args)

#define MAXTYPE(T) ((T) (((T)1 << ((sizeof(T) * 8) - 1) ^ ((T) -1))))

/*
 * Change here for supporting more than 2 blocks.  Also change all
 * NBLOCKS-based arrays to have NBLOCKS-worth of initialization values.
 */
#define NBLOCKS 2

#define BLOCK_IOREMAP    (1 << 0)
#define BLOCK_MEMREGION  (1 << 1)
#define BLOCK_REGION     (1 << 2)
static unsigned int block_flags[NBLOCKS] = {0, 0};

static unsigned long block_virtp[NBLOCKS] = {0, 0};
static unsigned long block_virtoff[NBLOCKS] = {0, 0};
static unsigned long block_virtend[NBLOCKS] = {0, 0};
static unsigned long block_start[NBLOCKS] = {0, 0};
static unsigned long block_end[NBLOCKS] = {0, 0};
static unsigned int block_avail_size[NBLOCKS] = {0, 0};
static unsigned int total_num_buffers[NBLOCKS] = {0, 0};

static int cmem_major;
static struct proc_dir_entry *cmem_proc_entry;
static atomic_t reference_count = ATOMIC_INIT(0);
static unsigned int version = CMEM_VERSION;

#define USE_UDEV 1
#define MAX_POOLS 2


#if (USE_UDEV==1)
#ifdef USE_CLASS_SIMPLE
static struct class_simple *cmem_class;
#else
static struct class *cmem_class;
#endif
#endif // USE_UDEV

/* Register the module parameters. */
MODULE_PARM_DESC(phys_start, "\n\t\t Start Address for CMEM Pool Memory");
static char *phys_start = NULL;
MODULE_PARM_DESC(phys_end, "\n\t\t End Address for CMEM Pool Memory");
static char *phys_end = NULL;
module_param(phys_start, charp, S_IRUGO);
module_param(phys_end, charp, S_IRUGO);

static int npools[NBLOCKS] = {0, 0};

static char *pools[MAX_POOLS] = {
    NULL
};
MODULE_PARM_DESC(pools,
    "\n\t\t List of Pool Sizes and Number of Entries, comma separated,"
    "\n\t\t decimal sizes");
module_param_array(pools, charp, &npools[0], S_IRUGO);

/* cut-and-paste below as part of adding support for more than 2 blocks */
MODULE_PARM_DESC(phys_start_1, "\n\t\t Start Address for Extended CMEM Pool Memory");
static char *phys_start_1 = NULL;
MODULE_PARM_DESC(phys_end_1, "\n\t\t End Address for Extended CMEM Pool Memory");
static char *phys_end_1 = NULL;
module_param(phys_start_1, charp, S_IRUGO);
module_param(phys_end_1, charp, S_IRUGO);

static char *pools_1[MAX_POOLS] = {
    NULL
};
MODULE_PARM_DESC(pools_1,
    "\n\t\t List of Pool Sizes and Number of Entries, comma separated,"
    "\n\t\t decimal sizes, for Extended CMEM Pool");
module_param_array(pools_1, charp, &npools[1], S_IRUGO);
/* cut-and-paste above as part of adding support for more than 2 blocks */

static int allowOverlap = 0;
MODULE_PARM_DESC(allowOverlap,
    "\n\t\t Set to 1 if cmem range is allowed to overlap memory range"
    "\n\t\t allocated to kernel physical mem (via mem=xxx)");
module_param(allowOverlap, int, S_IRUGO);

static DECLARE_MUTEX(cmem_mutex);

/* Describes a pool buffer */
typedef struct pool_buffer {
    struct list_head element;
    struct list_head users;
    int id;
    unsigned long physp;
    int flags;                  /* CMEM_CACHED or CMEM_NONCACHED */
    unsigned long kvirtp;       /* used only for heap-based allocs */
    size_t size;                /* used only for heap-based allocs */
} pool_buffer;

/* Describes a pool */
typedef struct pool_object {
    struct list_head freelist;
    struct list_head busylist;
    unsigned int numbufs;
    unsigned int size;
    unsigned int reqsize;
} pool_object;

typedef struct registered_user {
    struct list_head element;
//    struct task_struct *user;
    struct file *filp;
} registered_user;

pool_object p_objs[NBLOCKS][MAX_POOLS];

/* Forward declaration of system calls */
static int ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long args);
static int mmap(struct file *filp, struct vm_area_struct *vma);
static int open(struct inode *inode, struct file *filp);
static int release(struct inode *inode, struct file *filp);

static struct file_operations cmem_fxns = {
    ioctl:   ioctl,
    mmap:    mmap,
    open:    open,
    release: release
};


/*
 *  NOTE: The following implementation of a heap is taken from the
 *  DSP/BIOS 6.0 source tree (avala-f15/src/ti/sysbios/heaps).  Changes
 *  are necessary due to the fact that CMEM is not built in an XDC
 *  build environment, and therefore XDC types and helper APIs (e.g.,
 *  Assert) are not available.  However, these changes were kept to a
 *  minimum.
 *
 *  The changes include:
 *      - renaming XDC types to standard C types
 *      - replacing sizeof(HeapMem_Header) w/ HEAP_ALIGN throughout
 *
 *  As merged with CMEM, the heap becomes a distinguished "pool" and
 *  is sometimes treated specially, and at other times can be treated
 *  as a normal pool instance.
 */

/*
 * HeapMem compatibility stuff
 */
typedef struct HeapMem_Header {
    struct HeapMem_Header *next;
    unsigned long size;
} HeapMem_Header;

void *HeapMem_alloc(int bi, size_t size, size_t align);
void HeapMem_free(int bi, void *block, size_t size);

/*
 * Heap configuration stuff
 */
static unsigned long heap_size[NBLOCKS] = {0, 0};
static unsigned long heap_virtp[NBLOCKS] = {0, 0};
static int heap_pool[NBLOCKS] = {-1, -1};
static HeapMem_Header heap_head[NBLOCKS] = {
    {
        NULL,   /* next */
        0       /* size */
    },
/* cut-and-paste below as part of adding support for more than 2 blocks */
    {
        NULL,   /* next */
        0       /* size */
    }
/* cut-and-paste above as part of adding support for more than 2 blocks */
};

/*
 *  ======== HeapMem_alloc ========
 *  HeapMem is implemented such that all of the memory and blocks it works
 *  with have an alignment that is a multiple of HEAP_ALIGN and have a size
 *  which is a multiple of HEAP_ALIGN. Maintaining this requirement
 *  throughout the implementation ensures that there are never any odd
 *  alignments or odd block sizes to deal with.
 *
 *  Specifically:
 *  The buffer managed by HeapMem:
 *    1. Is aligned on a multiple of HEAP_ALIGN
 *    2. Has an adjusted size that is a multiple of HEAP_ALIGN
 *  All blocks on the freelist:
 *    1. Are aligned on a multiple of HEAP_ALIGN
 *    2. Have a size that is a multiple of HEAP_ALIGN
 *  All allocated blocks:
 *    1. Are aligned on a multiple of HEAP_ALIGN
 *    2. Have a size that is a multiple of HEAP_ALIGN
 *
 */
void *HeapMem_alloc(int bi, size_t reqSize, size_t reqAlign)
{
    HeapMem_Header *prevHeader, *newHeader, *curHeader;
    char *allocAddr;
    size_t curSize, adjSize;
    size_t remainSize; /* free memory after allocated memory      */
    size_t adjAlign, offset;
//    long key;

#if 0
    /* Assert that requested align is a power of 2 */
    Assert_isTrue(((reqAlign & (reqAlign - 1)) == 0), HeapMem_A_align);
    
    /* Assert that requested block size is non-zero */
    Assert_isTrue((reqSize != 0), HeapMem_A_zeroBlock);
#endif
    
    adjSize = reqSize;
    
    /* Make size requested a multiple of HEAP_ALIGN */
    if ((offset = (adjSize & (HEAP_ALIGN - 1))) != 0) {
        adjSize = adjSize + (HEAP_ALIGN - offset);
    }

    /*
     *  Make sure the alignment is at least as large as HEAP_ALIGN.
     *  Note: adjAlign must be a power of 2 (by function constraint) and
     *  HEAP_ALIGN is also a power of 2,
     */
    adjAlign = reqAlign;
    if (adjAlign & (HEAP_ALIGN - 1)) {
        /* adjAlign is less than HEAP_ALIGN */
        adjAlign = HEAP_ALIGN;
    }

/*
 * We don't need to enter the "gate" since this function is called
 * with it held already.
 */
//    key = Gate_enterModule();

    /* 
     *  The block will be allocated from curHeader. Maintain a pointer to
     *  prevHeader so prevHeader->next can be updated after the alloc.
     */
    prevHeader  = &heap_head[bi];
    curHeader = prevHeader->next;

    /* Loop over the free list. */
    while (curHeader != NULL) {

        curSize = curHeader->size;

        /*
         *  Determine the offset from the beginning to make sure
         *  the alignment request is honored.
         */
        offset = (unsigned long)curHeader & (adjAlign - 1);
        if (offset) {
            offset = adjAlign - offset;
        }
        
#if 0
        /* Internal Assert that offset is a multiple of HEAP_ALIGN */
        Assert_isTrue(((offset & (HEAP_ALIGN - 1)) == 0), NULL);
#endif

        /* big enough? */
        if (curSize >= (adjSize + offset)) {

            /* Set the pointer that will be returned. Alloc from front */
            allocAddr = (char *)((unsigned long)curHeader + offset);

            /*
             *  Determine the remaining memory after the allocated block.
             *  Note: this cannot be negative because of above comparison.
             */
            remainSize = curSize - adjSize - offset;
            
#if 0
            /* Internal Assert that remainSize is a multiple of HEAP_ALIGN */
            Assert_isTrue(((remainSize & (HEAP_ALIGN - 1)) == 0), NULL);
#endif
            
            /*
             *  If there is memory at the beginning (due to alignment
             *  requirements), maintain it in the list.
             *
             *  offset and remainSize must be multiples of
             *  HEAP_ALIGN. Therefore the address of the newHeader
             *  below must be a multiple of the HEAP_ALIGN, thus
             *  maintaining the requirement.
             */
            if (offset) {

                /* Adjust the curHeader size accordingly */
                curHeader->size = offset;

                /*
                 *  If there is remaining memory, add into the free list.
                 *  Note: no need to coalesce and we have HeapMem locked so
                 *        it is safe.
                 */
                if (remainSize) {
                    newHeader = (HeapMem_Header *)
                        ((unsigned long)allocAddr + adjSize);
                    newHeader->next = curHeader->next;
                    newHeader->size = remainSize;
                    curHeader->next = newHeader;
                }
            }
            else {
                /*
                 *  If there is any remaining, link it in,
                 *  else point to the next free block.
                 *  Note: no need to coalesce and we have HeapMem locked so
                 *        it is safe.
                 */
                if (remainSize) {
                    newHeader = (HeapMem_Header *)
                        ((unsigned long)allocAddr + adjSize);
                    newHeader->next  = curHeader->next;
                    newHeader->size  = remainSize;
                    prevHeader->next = newHeader;
                }
                else {
                    prevHeader->next = curHeader->next;
                }
            }

/*
 * See above comment on Gate_enterModule for an explanation of why we
 * don't use the "gate".
 */
//            Gate_leaveModule(key);
                                    
            /* Success, return the allocated memory */
            return ((void *)allocAddr);
        }
        else {
            prevHeader = curHeader;
            curHeader = curHeader->next;
        }
    }

/*
 * See above comment on Gate_enterModule for an explanation of why we
 * don't use the "gate".
 */
//    Gate_leaveModule(key);

    return (NULL);
}

/*
 *  ======== HeapMem_free ========
 */
void HeapMem_free(int bi, void *addr, size_t size)
{
//    long key;
    HeapMem_Header *curHeader, *newHeader, *nextHeader;
    size_t offset;

    /* obj->head never changes, doesn't need Gate protection. */
    curHeader = &heap_head[bi];

    /* Restore size to actual allocated size */
    if ((offset = size & (HEAP_ALIGN - 1)) != 0) {
        size += HEAP_ALIGN - offset;
    }
    
/*
 * We don't need to enter the "gate" since this function is called
 * with it held already.
 */
//    key = Gate_enterModule();

    newHeader = (HeapMem_Header *)addr;
    nextHeader = curHeader->next;

    /* Go down freelist and find right place for buf */
    while (nextHeader != NULL && nextHeader < newHeader) {
        curHeader = nextHeader;
        nextHeader = nextHeader->next;
    }

    newHeader->next = nextHeader;
    newHeader->size = size;
    curHeader->next = newHeader;

    /* Join contiguous free blocks */
    /* Join with upper block */
    if ((nextHeader != NULL) && 
        (((unsigned long)newHeader + size) == (unsigned long)nextHeader)) {
        newHeader->next = nextHeader->next;
        newHeader->size += nextHeader->size;
    }

    /*
     *  Join with lower block. Make sure to check to see if not the
     *  first block.
     */
    if ((curHeader != &heap_head[bi]) &&
        (((unsigned long)curHeader + curHeader->size) == (unsigned long)newHeader)) {
        curHeader->next = newHeader->next;
        curHeader->size += newHeader->size;
    }
    
/*
 * See above comment on Gate_enterModule for an explanation of why we
 * don't use the "gate".
 */
//    Gate_leaveModule(key);

}

/* Traverses the page tables and translates a virtual adress to a physical. */
static unsigned long get_phys(unsigned long virtp)
{
    unsigned long physp = ~(0L);
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vma;
    int bi;

    /* For CMEM block kernel addresses */
    for (bi = 0; bi < NBLOCKS; bi++) {
        if (virtp >= block_virtp[bi] && virtp < block_virtend[bi]) {
            physp = virtp - block_virtoff[bi];
            __D("get_phys: block_virtoff[%d] translated kernel %#lx to %#lx\n",
                bi, virtp, physp);

            return physp;
        }
    }

    /* For kernel direct-mapped memory, take the easy way */
    if (virtp >= PAGE_OFFSET) {
        physp = virt_to_phys((void *)virtp);
        __D("get_phys: virt_to_phys translated direct-mapped %#lx to %#lx\n",
            virtp, physp);
    }

    /* this will catch, kernel-allocated, mmaped-to-usermode addresses */
    else if ((vma = find_vma(mm, virtp)) &&
             (vma->vm_flags & VM_IO) &&
             (vma->vm_pgoff)) {
        physp =  (vma->vm_pgoff << PAGE_SHIFT) + (virtp - vma->vm_start);
        __D("get_phys: find_vma translated user %#lx to %#lx\n", virtp, physp);
    }

    /* otherwise, use get_user_pages() for general userland pages */
    else {
        int res, nr_pages = 1;
        struct page *pages;

        down_read(&current->mm->mmap_sem);

        res = get_user_pages(current, current->mm,
                             virtp, nr_pages,
                             1, 0,
                             &pages, NULL);
        up_read(&current->mm->mmap_sem);

        if (res == nr_pages) {
            physp = __pa(page_address(&pages[0]) + (virtp & ~PAGE_MASK));
            __D("get_phys: get_user_pages translated user %#lx to %#lx\n",
                virtp, physp);
        } else {
            __E("%s: Unable to find phys addr for 0x%08lx\n",
                __FUNCTION__, virtp);
            __E("%s: get_user_pages() failed: %d\n", __FUNCTION__, res);
        }
    }

    return physp;
}

/* Allocates space from the top "highmem" contiguous buffer for pool buffer. */
static unsigned long alloc_pool_buffer(int bi, unsigned int size)
{
    unsigned long virtp;

    __D("alloc_pool_buffer: Called for size %u\n", size);

    if (size <= block_avail_size[bi]) {
        __D("alloc_pool_buffer: Fits req %u < avail: %u\n",
            size, block_avail_size[bi]);
        block_avail_size[bi] -= size;
        virtp = block_virtp[bi] + block_avail_size[bi];

        __D("alloc_pool_buffer: new available block size is %d\n",
            block_avail_size[bi]);

        __D("alloc_pool_buffer: returning allocated buffer at %#lx\n", virtp);

        return virtp;
    }

    __E("Failed to find a big enough free block\n");

    return 0;
}


#ifdef __DEBUG
/* Only for debug */
static void dump_lists(int bi, int idx)
{
    struct list_head *freelistp = &p_objs[bi][idx].freelist;
    struct list_head *busylistp = &p_objs[bi][idx].busylist;
    struct list_head *e;
    struct pool_buffer *entry;

    if (down_interruptible(&cmem_mutex)) {
        return;
    }

    __D("Busylist for pool %d:\n", idx);
    for (e = busylistp->next; e != busylistp; e = e->next) {

        entry = list_entry(e, struct pool_buffer, element);

        __D("Busy: Buffer with id %d and physical address %#lx\n",
            entry->id, entry->physp);
    }

    __D("Freelist for pool %d:\n", idx);
    for (e = freelistp->next; e != freelistp; e = e->next) {

        entry = list_entry(e, struct pool_buffer, element);

        __D("Free: Buffer with id %d and physical address %#lx\n",
            entry->id, entry->physp);
    }

    up(&cmem_mutex);
}
#endif

/*
 *  ======== find_busy_entry ========
 *  find_busy_entry looks for an allocated pool buffer with
 *  phyisical addr physp.
 *
 *  Should be called with the cmem_mutex held.
 */
static struct pool_buffer *find_busy_entry(unsigned long physp, int *poolp, struct list_head **ep, int *bip)
{
    struct list_head *busylistp;
    struct list_head *e;
    struct pool_buffer *entry;
    int num_pools;
    int i;
    int bi;

    for (bi = 0; bi < NBLOCKS; bi++) {
        num_pools = npools[bi];
        if (heap_pool[bi] != -1) {
            num_pools++;
        }

        for (i = 0; i < num_pools; i++) {
            busylistp = &p_objs[bi][i].busylist;

            for (e = busylistp->next; e != busylistp; e = e->next) {
                entry = list_entry(e, struct pool_buffer, element);
                if (entry->physp == physp) {
                    if (poolp) {
                        *poolp = i;
                    }
                    if (ep) {
                        *ep = e;
                    }
                    if (bip) {
                        *bip = bi;
                    }

                    return entry;
                }
            }
        }
    }

    return NULL;
}

static void cmem_seq_stop(struct seq_file *s, void *v);
static void *cmem_seq_start(struct seq_file *s, loff_t *pos);
static void *cmem_seq_next(struct seq_file *s, void *v, loff_t *pos);
static int cmem_seq_show(struct seq_file *s, void *v);

static struct seq_operations cmem_seq_ops = {
    .start = cmem_seq_start,
    .next = cmem_seq_next,
    .stop = cmem_seq_stop,
    .show = cmem_seq_show,
};

#define SHOW_BUSY_BANNER (1 << 0)
#define SHOW_PREV_FREE_BANNER (1 << 1)
#define SHOW_LAST_FREE_BANNER (1 << 2)
#define BUSY_ENTRY (1 << 3)
#define FREE_ENTRY (1 << 4)

void *find_buffer_n(struct seq_file *s, int n)
{
    struct list_head *listp = NULL;
    int busy_empty;
    int free_empty = 0;
    int found = 0;
    int count;
    int i;
    int bi;

    __D("find_buffer_n: n=%d\n", n);

    s->private = (void *)0;
    count = 0;

    for (bi = 0; bi < NBLOCKS; bi++) {
        for (i = 0; i < npools[bi]; i++) {
            listp = &p_objs[bi][i].busylist;
            listp = listp->next;
            busy_empty = 1;
            while (listp != &p_objs[bi][i].busylist) {
                busy_empty = 0;
                if (count == n) {
                    found = 1;
                    s->private = (void *)((int)s->private | BUSY_ENTRY);

                    break;
                }
                count++;
                listp = listp->next;
            }
            if (found) {
                break;
            }

            listp = &p_objs[bi][i].freelist;
            listp = listp->next;
            free_empty = 1;
            while (listp != &p_objs[bi][i].freelist) {
                if (i == 0 ||
                    (p_objs[bi][i - 1].freelist.next !=
                    &p_objs[bi][i - 1].freelist)) {

                    free_empty = 0;
                }
                if (count == n) {
                    found = 1;
                    s->private = (void *)((int)s->private | FREE_ENTRY);

                    break;
                }
                count++;
                listp = listp->next;
            }
            if (found) {
                break;
            }
        }
        if (found) {
            break;
        }
    }

    if (!found) {
        listp = NULL;
    }
    else {
        if (busy_empty) {
            s->private = (void *)((int)s->private | SHOW_BUSY_BANNER);
        }
        if (free_empty) {
            s->private = (void *)((int)s->private | SHOW_PREV_FREE_BANNER);
        }
        if (count == (total_num_buffers[bi] - 1)) {
            s->private = (void *)((int)s->private | SHOW_LAST_FREE_BANNER);
        }
    }

    return listp;
}

static void cmem_seq_stop(struct seq_file *s, void *v)
{
    __D("cmem_seq_stop: v=%p\n", v);

    up(&cmem_mutex);
}

static void *cmem_seq_start(struct seq_file *s, loff_t *pos)
{
    struct list_head *listp;
    int total_num;

    if (down_interruptible(&cmem_mutex)) {
        return ERR_PTR(-ERESTARTSYS);
    }

    __D("cmem_seq_start: *pos=%d\n", (int)*pos);

    total_num = total_num_buffers[0] + total_num_buffers[1];
    if (*pos >= total_num) {
        __D("  %d >= %d\n", (int)*pos, total_num);

        return NULL;
    }

    listp = find_buffer_n(s, *pos);

    __D("  returning %p\n", listp);

    return listp;
}

static void *cmem_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    struct list_head *listp;
    int total_num;

    __D("cmem_seq_next: *pos=%d\n", (int)*pos);

    __D("  incrementing *pos\n");
    ++(*pos);

    total_num = total_num_buffers[0] + total_num_buffers[1];
    if (*pos >= total_num) {
        __D("  %d >= %d\n", (int)*pos, total_num);

        return NULL;
    }

    listp = find_buffer_n(s, *pos);

    __D("  returning %p\n", listp);

    return listp;
}

int show_busy_banner(int bi, struct seq_file *s, int n)
{
    return seq_printf(s, "\nBlock %d: Pool %d: %d bufs size %d (%d requested)\n"
                      "\nPool %d busy bufs:\n",
                      bi, n, p_objs[bi][n].numbufs, p_objs[bi][n].size,
                      p_objs[bi][n].reqsize, n);
}

int show_free_banner(struct seq_file *s, int n)
{
    return seq_printf(s, "\nPool %d free bufs:\n", n);
}

/*
 * Show one pool entry, w/ banners for first entries in a pool's busy or
 * free list.
 */
static int cmem_seq_show(struct seq_file *s, void *v)
{
    struct list_head *listp = v;
    struct list_head *e = v;
    struct pool_buffer *entry;
    char *attr;
    int i;
    int bi;
    int rv;

    __D("cmem_seq_show:\n");

    for (bi = 0; bi < NBLOCKS; bi++) {
        /* look for banners to show */
        for (i = 0; i < npools[bi]; i++) {
            if (listp == p_objs[bi][i].busylist.next) {
                /* first buffer in busylist */
                if ((int)s->private & SHOW_PREV_FREE_BANNER) {
                    /*
                     * Previous pool's freelist empty, need to show banner.
                     */
                    show_free_banner(s, i - 1);
                }
                show_busy_banner(bi, s, i);

                break;
            }
            if (listp == p_objs[bi][i].freelist.next) {
                /* first buffer in freelist */
                if ((int)s->private & SHOW_PREV_FREE_BANNER) {
                    /*
                     * Previous pool's freelist & this pool's busylist empty,
                     * need to show banner.
                     */
                    show_free_banner(s, i - 1);
                }
                if ((int)s->private & SHOW_BUSY_BANNER) {
                    /*
                     * This pool's busylist empty, need to show banner.
                     */
                    show_busy_banner(bi, s, i);
                }
                show_free_banner(s, i);

                break;
            }
        }
    }

    entry = list_entry(e, struct pool_buffer, element);

    /*
     * Check the final seq_printf return value.  No need to check previous
     * ones, since if they fail then the seq_file object is not changed and
     * any subsequent seq_printf will also fail w/o changing the object.
     */
    if ((int)s->private & BUSY_ENTRY) {
        attr = entry->flags & CMEM_CACHED ? "(cached)" : "(noncached)";
        rv = seq_printf(s, "id %d: phys addr %#lx %s\n", entry->id,
                   entry->physp, attr);
    }
    else {
        rv = seq_printf(s, "id %d: phys addr %#lx\n", entry->id, entry->physp);
    }
    if (rv == -1) {
        __D("seq_printf returned -1\n");

        return -1;
    }

    if ((int)s->private & BUSY_ENTRY &&
        (int)s->private & SHOW_LAST_FREE_BANNER) {

        rv = show_free_banner(s, npools[bi] - 1);
        if (rv == -1) {
            __D("seq_printf returned -1\n");

            return -1;
        }
    }

    return 0;
}

static int cmem_proc_open(struct inode *inode, struct file *file);

static struct file_operations cmem_proc_ops = {
    .owner = THIS_MODULE,
    .open = cmem_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};

static int cmem_proc_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &cmem_seq_ops);
}

/* Allocate a contiguous memory pool. */
static int alloc_pool(int bi, int idx, int num, int reqsize, unsigned long *virtpRet)
{
    struct pool_buffer *entry;
    struct list_head *freelistp = &p_objs[bi][idx].freelist;
    struct list_head *busylistp = &p_objs[bi][idx].busylist;
    int size = PAGE_ALIGN(reqsize);
    unsigned long virtp;
    int i;

    __D("Allocating %d buffers of size %d (requested %d)\n",
                num, size, reqsize);

    p_objs[bi][idx].reqsize = reqsize;
    p_objs[bi][idx].numbufs = num;
    p_objs[bi][idx].size = size;

    INIT_LIST_HEAD(freelistp);
    INIT_LIST_HEAD(busylistp);

    for (i=0; i < num; i++) {
        entry = kmalloc(sizeof(struct pool_buffer), GFP_KERNEL);

        if (!entry) {
            __E("alloc_pool failed to malloc pool_buffer struct");
            return -ENOMEM;
        }

        virtp = alloc_pool_buffer(bi, size);

        if (virtp == 0) {
            __E("alloc_pool failed to get contiguous area of size %d\n",
                size);

            /*
             * Need to free this entry now since it didn't get added to
             * a list that will be freed during module removal (cmem_exit())
             * Fixes SDSCM00027040.
             */
            kfree(entry);

            return -ENOMEM;
        }

        entry->id = i;
        entry->physp = get_phys(virtp);
        entry->size = size;
        INIT_LIST_HEAD(&entry->users);

        if (virtpRet) {
            *virtpRet++ = virtp;
        }

        __D("Allocated buffer %d, virtual %#lx and physical %#lx and size %d\n",
            entry->id, virtp, entry->physp, size);

        list_add_tail(&entry->element, freelistp);
    }

#ifdef __DEBUG
    dump_lists(bi, idx);
#endif

    return 0;
}

static int set_noncached(struct vm_area_struct *vma)
{
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    vma->vm_flags |= VM_RESERVED | VM_IO;

    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
                        vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
        __E("set_noncached: failed remap_pfn_range\n");
        return -EAGAIN;
    }

    return 0;
}

static int set_cached(struct vm_area_struct *vma)
{
#ifdef USE_PTE_MT
    vma->vm_page_prot = __pgprot(pgprot_val(vma->vm_page_prot) |
                                 (L_PTE_MT_WRITETHROUGH | L_PTE_MT_BUFFERABLE)
                                );
#else
    vma->vm_page_prot = __pgprot(pgprot_val(vma->vm_page_prot) |
                                 (L_PTE_CACHEABLE | L_PTE_BUFFERABLE)
                                );
#endif

    vma->vm_flags |= VM_RESERVED | VM_IO;

    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
                        vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
        __E("set_cached: failed remap_pfn_range\n");
        return -EAGAIN;
    }

    return 0;
}

struct block_struct {
    unsigned long addr;
    size_t size;
};

static int ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long args)
{
    unsigned int __user *argp = (unsigned int __user *) args;
    struct list_head *freelistp = NULL;
    struct list_head *busylistp = NULL;
    struct list_head *registeredlistp;
    struct list_head *e = NULL;
    struct list_head *u;
    struct list_head *unext;
    struct pool_buffer *entry;
    struct registered_user *user;
    unsigned long physp;
    unsigned long virtp, virtp_end;
    size_t reqsize, size, align;
    int delta = MAXTYPE(int);
    int pool = -1;
    int i;
    int bi;
    int id;
    struct block_struct block;
    union CMEM_AllocUnion allocDesc;

    if (_IOC_TYPE(cmd) != _IOC_TYPE(CMEM_IOCMAGIC)) {
        __E("ioctl(): bad command type 0x%x (should be 0x%x)\n",
            _IOC_TYPE(cmd), _IOC_TYPE(CMEM_IOCMAGIC));
    }

    switch (cmd & CMEM_IOCCMDMASK) {
        case CMEM_IOCALLOCHEAP:
            if (copy_from_user(&allocDesc, argp, sizeof(allocDesc))) {
                return -EFAULT;
            }

            size = allocDesc.alloc_heap_inparams.size;
            align = allocDesc.alloc_heap_inparams.align;
            bi = allocDesc.alloc_heap_inparams.blockid;

            __D("ALLOCHEAP%s ioctl received on heap pool for block %d\n",
                cmd & CMEM_CACHED ? "CACHED" : "", bi);

            if (bi >= NBLOCKS) {
                __E("ioctl: invalid block id %d, must be < %d\n",
                    bi, NBLOCKS);
                return -EINVAL;
            }
            if (heap_pool[bi] == -1) {
                __E("ioctl: no heap available in block %d\n", bi);
                return -EINVAL;
            }


            if (down_interruptible(&cmem_mutex)) {
                return -ERESTARTSYS;
            }

            virtp = (unsigned long)HeapMem_alloc(bi, size, align);
            if (virtp == (unsigned long)NULL) {
                __E("ioctl: failed to allocate heap buffer of size %#x\n",
                    size);
                up(&cmem_mutex);
                return -ENOMEM;
            }

            entry = kmalloc(sizeof(struct pool_buffer), GFP_KERNEL);

            if (!entry) {
                __E("ioctl: failed to kmalloc pool_buffer struct for heap");
                HeapMem_free(bi, (void *)virtp, size);
                up(&cmem_mutex);
                return -ENOMEM;
            }

            physp = get_phys(virtp);

            entry->id = heap_pool[bi];
            entry->physp = physp;
            entry->kvirtp = virtp;
            entry->size = size;
            entry->flags = cmd & ~CMEM_IOCCMDMASK;
            INIT_LIST_HEAD(&entry->users);

            busylistp = &p_objs[bi][heap_pool[bi]].busylist;
            list_add_tail(&entry->element, busylistp);

            user = kmalloc(sizeof(struct registered_user), GFP_KERNEL);
//          user->user = current;
            user->filp = filp;
            list_add(&user->element, &entry->users);

            up(&cmem_mutex);

            if (put_user(physp, argp)) {
                return -EFAULT;
            }

            __D("ALLOCHEAP%s: allocated %#x size buffer at %#lx (phys address)\n",
                cmd & CMEM_CACHED ? "CACHED" : "", entry->size, entry->physp);

            break;

        /*
         * argp contains a pointer to an alloc descriptor coming in, and the
         * physical address and size of the allocated buffer when returning.
         */
        case CMEM_IOCALLOC:
            if (copy_from_user(&allocDesc, argp, sizeof(allocDesc))) {
                return -EFAULT;
            }

            pool = allocDesc.alloc_pool_inparams.poolid;
            bi = allocDesc.alloc_pool_inparams.blockid;

            __D("ALLOC%s ioctl received on pool %d for memory block %d\n",
                cmd & CMEM_CACHED ? "CACHED" : "", pool, bi);

            if (bi >= NBLOCKS) {
                __E("ioctl: invalid block id %d, must be < %d\n",
                    bi, NBLOCKS);
                return -EINVAL;
            }

            if (pool >= npools[bi] || pool < 0) {
                __E("ALLOC%s: invalid pool (%d) passed.\n",
                    cmd & CMEM_CACHED ? "CACHED" : "", pool);
                return -EINVAL;
            }

            freelistp = &p_objs[bi][pool].freelist;
            busylistp = &p_objs[bi][pool].busylist;

            if (down_interruptible(&cmem_mutex)) {
                return -ERESTARTSYS;
            }

            e = freelistp->next;
            if (e == freelistp) {
                __E("ALLOC%s: No free buffers available for pool %d\n",
                    cmd & CMEM_CACHED ? "CACHED" : "", pool);
                up(&cmem_mutex);
                return -ENOMEM;
            }
            entry = list_entry(e, struct pool_buffer, element);

            allocDesc.alloc_pool_outparams.physp = entry->physp;
            allocDesc.alloc_pool_outparams.size = p_objs[bi][pool].size;

            if (copy_to_user(argp, &allocDesc, sizeof(allocDesc))) {
                up(&cmem_mutex);
                return -EFAULT;
            }

            entry->flags = cmd & ~CMEM_IOCCMDMASK;

            list_del_init(e);
            list_add(e, busylistp);

            user = kmalloc(sizeof(struct registered_user), GFP_KERNEL);
//          user->user = current;
            user->filp = filp;
            list_add(&user->element, &entry->users);

            up(&cmem_mutex);

            __D("ALLOC%s: allocated a buffer at %#lx (phys address)\n",
                cmd & CMEM_CACHED ? "CACHED" : "", entry->physp);

#ifdef __DEBUG
            dump_lists(bi, pool);
#endif
            break;

        /*
         * argp contains either the user virtual address or the physical
         * address of the buffer to free coming in, and contains the pool
         * where it was freed from and the size of the block on return.
         */
        case CMEM_IOCFREE:
            __D("FREE%s%s ioctl received.\n",
                cmd & CMEM_HEAP ? "HEAP" : "",
                cmd & CMEM_PHYS ? "PHYS" : "");

            if (!(cmd & CMEM_PHYS)) {
                if (get_user(virtp, argp)) {
                    return -EFAULT;
                }

                physp = get_phys(virtp);

                if (physp == ~(0L)) {
                    __E("FREE%s: Failed to convert virtual %#lx to physical\n",
                        cmd & CMEM_HEAP ? "HEAP" : "", virtp);
                    return -EFAULT;
                }

                __D("FREE%s: translated %#lx user virtual to %#lx physical\n",
                    cmd & CMEM_HEAP ? "HEAP" : "", virtp, physp);
            }
            else {
                if (get_user(physp, argp)) {
                    return -EFAULT;
                }
            }

            if (down_interruptible(&cmem_mutex)) {
                return -ERESTARTSYS;
            }

            size = 0;

            entry = find_busy_entry(physp, &pool, &e, &bi);
            if (entry) {
                /* record values in case entry gets kfree()'d for CMEM_HEAP */
                id = entry->id;
                size = entry->size;

                registeredlistp = &entry->users;
                u = registeredlistp->next;
                while (u != registeredlistp) {
                    unext = u->next;

                    user = list_entry(u, struct registered_user, element);
                    if (user->filp == filp) {
                        __D("FREE%s%s: Removing file %p from user list of buffer %#lx...\n",
                            cmd & CMEM_HEAP ? "HEAP" : "",
                            cmd & CMEM_PHYS ? "PHYS" : "", filp, physp);

                        list_del(u);
                        kfree(user);

                        break;
                    }

                    u = unext;
                }

                if (u == registeredlistp) {
                    __E("FREE%s%s: Not a registered user of physical buffer %#lx\n",
                        cmd & CMEM_HEAP ? "HEAP" : "",
                        cmd & CMEM_PHYS ? "PHYS" : "", physp);
                    up(&cmem_mutex);

                    return -EFAULT;
                }

                if (registeredlistp->next == registeredlistp) {
                    /* no more registered users, free buffer */
                    if (cmd & CMEM_HEAP) {
                        HeapMem_free(bi, (void *)entry->kvirtp, entry->size);
                        list_del(e);
                        kfree(entry);
                    }
                    else {
                        list_del_init(e);
                        list_add(e, &p_objs[bi][pool].freelist);
                    }

                    __D("FREE%s%s: Successfully freed buffer %d from pool %d\n",
                        cmd & CMEM_HEAP ? "HEAP" : "",
                        cmd & CMEM_PHYS ? "PHYS" : "", id, pool);
                }
            }

            up(&cmem_mutex);

            if (!entry) {
                __E("Failed to free memory at %#lx\n", physp);
                return -EFAULT;
            }

#ifdef __DEBUG
            dump_lists(bi, pool);
#endif
            if (cmd & CMEM_HEAP) {
                allocDesc.free_outparams.size = size;
            }
            else {
                allocDesc.free_outparams.size = p_objs[bi][pool].size;
            }
            allocDesc.free_outparams.poolid = pool;
            if (copy_to_user(argp, &allocDesc, sizeof(allocDesc))) {
                return -EFAULT;
            }

            __D("FREE%s%s: returning size %d, poolid %d\n",
                cmd & CMEM_HEAP ? "HEAP" : "",
                cmd & CMEM_PHYS ? "PHYS" : "",
                allocDesc.free_outparams.size,
                allocDesc.free_outparams.poolid);

            break;

        /*
         * argp contains the user virtual address of the buffer to translate
         * coming in, and the translated physical address on return.
         */
        case CMEM_IOCGETPHYS:
            __D("GETPHYS ioctl received.\n");
            if (get_user(virtp, argp)) {
                return -EFAULT;
            }

            physp = get_phys(virtp);

            if (physp == ~(0L)) {
                __E("GETPHYS: Failed to convert virtual %#lx to physical.\n",
                    virtp);
                return -EFAULT;
            }

            if (put_user(physp, argp)) {
                return -EFAULT;
            }

            __D("GETPHYS: returning %#lx\n", physp);
            break;

        /*
         * argp contains the pool to query for size coming in, and the size
         * of the pool on return.
         */
        case CMEM_IOCGETSIZE:
            __D("GETSIZE ioctl received\n");
            if (copy_from_user(&allocDesc, argp, sizeof(allocDesc))) {
                return -EFAULT;
            }

            pool = allocDesc.get_size_inparams.poolid;
            bi = allocDesc.get_size_inparams.blockid;

            if (bi >= NBLOCKS) {
                __E("ioctl: invalid block id %d, must be < %d\n",
                    bi, NBLOCKS);
                return -EINVAL;
            }

            if (pool >= npools[bi] || pool < 0) {
                __E("GETSIZE: invalid pool (%d) passed.\n", pool);
                return -EINVAL;
            }

            if (put_user(p_objs[bi][pool].size, argp)) {
                return -EFAULT;
            }
            __D("GETSIZE returning %d\n", p_objs[bi][pool].size);
            break;

        /*
         * argp contains the requested pool buffers size coming in, and the
         * pool id (index) on return.
         */
        case CMEM_IOCGETPOOL:
            __D("GETPOOL ioctl received.\n");
            if (copy_from_user(&allocDesc, argp, sizeof(allocDesc))) {
                return -EFAULT;
            }

            reqsize = allocDesc.get_pool_inparams.size;
            bi = allocDesc.get_pool_inparams.blockid;

            if (bi >= NBLOCKS) {
                __E("ioctl: invalid block id %d, must be < %d\n",
                    bi, NBLOCKS);
                return -EINVAL;
            }

            if (down_interruptible(&cmem_mutex)) {
                return -ERESTARTSYS;
            }

            __D("GETPOOL: Trying to find a pool to fit size %d\n", reqsize);
            for (i=0; i<npools[bi]; i++) {
                size = p_objs[bi][i].size;
                freelistp = &p_objs[bi][i].freelist;

                __D("GETPOOL: size (%d) > reqsize (%d)?\n", size, reqsize);
                if (size >= reqsize) {
                    __D("GETPOOL: delta (%d) < olddelta (%d)?\n",
                        size - reqsize, delta);
                    if ((size - reqsize) < delta) {
                        if (list_empty(freelistp) == 0) {
                            delta = size - reqsize;
                            __D("GETPOOL: Found a best fit delta %d\n", delta);
                            pool = i;
                        }
                    }
                }
            }

            up(&cmem_mutex);

            if (pool == -1) {
                __E("Failed to find a pool which fits %d\n", reqsize);
                return -ENOMEM;
            }

            if (put_user(pool, argp)) {
                return -EFAULT;
            }
            __D("GETPOOL: returning %d\n", pool);
            break;

        case CMEM_IOCCACHE:
            __D("CACHE%s%s ioctl received.\n",
                cmd & CMEM_WB ? "WB" : "", cmd & CMEM_INV ? "INV" : "");

            if (copy_from_user(&block, argp, sizeof(block))) {
                return -EFAULT;
            }
            virtp = block.addr;
            virtp_end = virtp + block.size;

#ifdef CHECK_FOR_ALLOCATED_BUFFER
            physp = get_phys(virtp);
            if (physp == ~(0L)) {
                __E("CACHE%s%s: Failed to convert virtual %#lx to physical\n",
                    cmd & CMEM_WB ? "WB" : "", cmd & CMEM_INV ? "INV" : "",
                    virtp);
                return -EFAULT;
            }

            __D("CACHE%s%s: translated %#lx user virtual to %#lx physical\n",
                cmd & CMEM_WB ? "WB" : "", cmd & CMEM_INV ? "INV" : "",
                virtp, physp);

            if (down_interruptible(&cmem_mutex)) {
                return -ERESTARTSYS;
            }
            entry = find_busy_entry(physp, &pool, &e, &bi);
            up(&cmem_mutex);
            if (!entry) {
                __E("CACHE%s%s: Failed to find allocated buffer at virtual %#lx\n",
                    cmd & CMEM_WB ? "WB" : "", cmd & CMEM_INV ? "INV" : "",
                    virtp);
                return -ENXIO;
            }
            if (!(entry->flags & CMEM_CACHED)) {
                __E("CACHE%s%s: virtual buffer %#lx not cached\n",
                    cmd & CMEM_WB ? "WB" : "", cmd & CMEM_INV ? "INV" : "",
                    virtp);
                return -EINVAL;
            }
#endif

#ifdef USE_MMAPSEM
            __D("CACHE%s%s: acquiring mmap_sem ...\n",
                cmd & CMEM_WB ? "WB" : "", cmd & CMEM_INV ? "INV" : "");
            down_write(&current->mm->mmap_sem);
#endif

            switch (cmd) {
              case CMEM_IOCCACHEWB:
#ifdef USE_CACHE_DMAAPI
                __dma_single_cpu_to_dev((const void *)virtp, block.size, DMA_TO_DEVICE);
#elif defined USE_CACHE_VOID_ARG
                dmac_clean_range((void *)virtp, (void *)virtp_end);
#else
                dmac_clean_range(virtp, virtp_end);
#endif
                __D("CACHEWB: cleaned user virtual %#lx->%#lx\n",
                       virtp, virtp_end);

                break;

              case CMEM_IOCCACHEINV:
#ifdef USE_CACHE_DMAAPI
                __dma_single_dev_to_cpu((const void *)virtp, block.size, DMA_FROM_DEVICE);
#elif defined USE_CACHE_VOID_ARG
                dmac_inv_range((void *)virtp, (void *)virtp_end);
#else
                dmac_inv_range(virtp, virtp_end);
#endif
                __D("CACHEINV: invalidated user virtual %#lx->%#lx\n",
                       virtp, virtp_end);

                break;

              case CMEM_IOCCACHEWBINV:
#ifdef USE_CACHE_DMAAPI
                __dma_single_cpu_to_dev((const void *)virtp, block.size, DMA_FROM_DEVICE);
#elif defined USE_CACHE_VOID_ARG
                dmac_flush_range((void *)virtp, (void *)virtp_end);
#else
                dmac_flush_range(virtp, virtp_end);
#endif
                __D("CACHEWBINV: flushed user virtual %#lx->%#lx\n",
                       virtp, virtp_end);

                break;
            }

#ifdef USE_MMAPSEM
            __D("CACHE%s%s: releasing mmap_sem ...\n",
                cmd & CMEM_WB ? "WB" : "", cmd & CMEM_INV ? "INV" : "");
            up_write(&current->mm->mmap_sem);
#endif

            break;

        case CMEM_IOCGETVERSION:
            __D("GETVERSION ioctl received, returning %#x.\n", version);

            if (put_user(version, argp)) {
                return -EFAULT;
            }

            break;

        case CMEM_IOCGETBLOCK:
            __D("GETBLOCK ioctl received.\n");

            if (copy_from_user(&allocDesc, argp, sizeof(allocDesc))) {
                return -EFAULT;
            }

            bi = allocDesc.blockid;

            allocDesc.get_block_outparams.physp = block_start[bi];
            allocDesc.get_block_outparams.size = block_end[bi] -
                                                 block_start[bi];

            __D("GETBLOCK ioctl received, returning phys base "
                "0x%lx, size 0x%x.\n", allocDesc.get_block_outparams.physp,
                allocDesc.get_block_outparams.size);

            if (copy_to_user(argp, &allocDesc, sizeof(allocDesc))) {
                return -EFAULT;
            }

            break;

        case CMEM_IOCREGUSER:
            __D("REGUSER ioctl received.\n");

            if (get_user(physp, argp)) {
                return -EFAULT;
            }

            if (down_interruptible(&cmem_mutex)) {
                return -ERESTARTSYS;
            }

            entry = find_busy_entry(physp, &pool, &e, &bi);
            if (entry) {
                /*
                 * Should we check if the "current" process is already on
                 * the list and return error if so?  Or should we just
                 * silently not put it on the list twice and return success?
                 * Or should we put it on the list a second time, which seems
                 * to be OK to do and will require being removed from the
                 * list twice?  So many questions...
                 *
                 * The code below, lacking the test, will put a process on
                 * the list multiple times (every time IOCREGUSER is called).
                 */
                user = kmalloc(sizeof(struct registered_user), GFP_KERNEL);
//              user->user = current;
                user->filp = filp;
                list_add(&user->element, &entry->users);
            }

            up(&cmem_mutex);

            if (!entry) {
                return -EFAULT;
            }

            if (put_user(entry->size, argp)) {
                return -EFAULT;
            }

            break;

        default:
            __E("Unknown ioctl received.\n");
            return -EINVAL;
    }

    return 0;
}

static int mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long physp;
    struct pool_buffer *entry;

    __D("mmap: vma->vm_start     = %#lx\n", vma->vm_start);
    __D("mmap: vma->vm_pgoff     = %#lx\n", vma->vm_pgoff);
    __D("mmap: vma->vm_end       = %#lx\n", vma->vm_end);
    __D("mmap: size              = %#lx\n", vma->vm_end - vma->vm_start);

    physp = vma->vm_pgoff << PAGE_SHIFT;

    if (down_interruptible(&cmem_mutex)) {
        return -ERESTARTSYS;
    }

    entry = find_busy_entry(physp, NULL, NULL, NULL);
    up(&cmem_mutex);

    if (entry != NULL) {
        if (entry->flags & CMEM_CACHED) {
            __D("mmap: calling set_cached(%p) ...\n", vma);

            return set_cached(vma);
        }
        else {
            __D("mmap: calling set_noncached(%p) ...\n", vma);

            return set_noncached(vma);
        }
    }
    else {
        __E("mmap: can't find allocated buffer with physp %lx\n", physp);

        return -EINVAL;
    }
}

static int open(struct inode *inode, struct file *filp)
{
    __D("open: called.\n");

    atomic_inc(&reference_count);

    return 0;
}

static int release(struct inode *inode, struct file *filp)
{
    struct list_head *registeredlistp;
    struct list_head *freelistp;
    struct list_head *busylistp;
    struct list_head *e;
    struct list_head *u;
    struct list_head *next;
    struct list_head *unext;
    struct pool_buffer *entry;
    struct registered_user *user;
    int last_close = 0;
    int num_pools;
    int bi;
    int i;

    __D("close: called.\n");

    /* Force free all buffers owned by the 'current' process */

    if (atomic_dec_and_test(&reference_count)) {
        __D("close: all references closed, force freeing all busy buffers.\n");

        last_close = 1;
    }

    for (bi = 0; bi < NBLOCKS; bi++) {
        num_pools = npools[bi];
        if (heap_pool[bi] != -1) {
            num_pools++;
        }
        
        /* Clean up any buffers on the busy list when cmem is closed */
        for (i=0; i<num_pools; i++) {
            __D("Forcing free on pool %d\n", i);

            /* acquire the mutex in case this isn't the last close */
            if (down_interruptible(&cmem_mutex)) {
                return -ERESTARTSYS;
            }

            freelistp = &p_objs[bi][i].freelist;
            busylistp = &p_objs[bi][i].busylist;

            e = busylistp->next;
            while (e != busylistp) {
                __D("busy entry(s) found\n");

                next = e->next;

                entry = list_entry(e, struct pool_buffer, element);
                registeredlistp = &entry->users;
                u = registeredlistp->next;
                while (u != registeredlistp) {
                    unext = u->next;

                    user = list_entry(u, struct registered_user, element);

                    if (last_close || user->filp == filp) {
                        __D("Removing file %p from user list of buffer %#lx...\n",
                            user->filp, entry->physp);

                        list_del(u);
                        kfree(user);

#if 0
                        /*
                         * If a process is limited to appearing on an entry's
                         * registered user list only one time, then the
                         * test below could be done as an optimization, since
                         * if we're here and it's not last close, we just
                         * removed the "current" process from the list (see
                         * IOCREGUSER ioctl() command comment).
                         */
                        if (!last_close) {
                            break;
                        }
#endif
                    }

                    u = unext;
                }

                if (registeredlistp->next == registeredlistp) {
                    /* no more registered users, free buffer */

                    if ((heap_pool[bi] != -1) && (i == (num_pools - 1))) {
                        /* HEAP */
                        __D("Warning: Freeing 'busy' buffer from heap at "
                            "%#lx\n", entry->physp);

                        HeapMem_free(bi, (void *)entry->kvirtp, entry->size);
                        list_del(e);
                        kfree(entry);
                    }
                    else {
                        /* POOL */
                        __D("Warning: Putting 'busy' buffer from pool %d at "
                            "%#lx on freelist\n", i, entry->physp);

                        list_del_init(e);
                        list_add(e, freelistp);
                    }
                }

                e = next;
            }

            up(&cmem_mutex);
        }
    }

    __D("close: returning\n");

    return 0;
}

static void banner(void)
{
    printk(KERN_INFO "CMEMK module: built on " __DATE__ " at " __TIME__ "\n");
    printk(KERN_INFO "  Reference Linux version %d.%d.%d\n",
           (LINUX_VERSION_CODE & 0x00ff0000) >> 16,
           (LINUX_VERSION_CODE & 0x0000ff00) >> 8,
           (LINUX_VERSION_CODE & 0x000000ff) >> 0
          );
    printk(KERN_INFO "  File " __FILE__ "\n");
}

int __init cmem_init(void)
{
    int bi;
    int i;
    int err;
    char *t;
    int pool_size;
    int pool_num_buffers;
    unsigned long length;
    unsigned long phys_end_kernel;
    HeapMem_Header *header;
    char *pstart[NBLOCKS];
    char *pend[NBLOCKS];
    char **pool_table[MAX_POOLS];
    char tmp_str[4];

    banner();

    if (npools[0] > MAX_POOLS) {
        __E("Too many pools specified (%d) for Block 0, only %d supported.\n",
            npools[0], MAX_POOLS);
        return -EINVAL;
    }

/* cut-and-paste below as part of adding support for more than 2 blocks */
    if (npools[1] > MAX_POOLS) {
        __E("Too many pools specified (%d) for Block 0, only %d supported.\n",
            npools[1], MAX_POOLS);
        return -EINVAL;
    }
/* cut-and-paste above as part of adding support for more than 2 blocks */

    cmem_major = register_chrdev(0, "cmem", &cmem_fxns);

    if (cmem_major < 0) {
        __E("Failed to allocate major number.\n");
        return -ENODEV;
    }

    __D("Allocated major number: %d\n", cmem_major);

#if (USE_UDEV==1)
#ifdef USE_CLASS_SIMPLE
    cmem_class = class_simple_create(THIS_MODULE, "cmem");
#else
    cmem_class = class_create(THIS_MODULE, "cmem");
#endif
    if (IS_ERR(cmem_class)) {
        __E("Error creating cmem device class.\n");
        err = -EIO;
        goto fail_after_reg;
    }

#ifdef USE_CLASS_SIMPLE
    class_simple_device_add(cmem_class, MKDEV(cmem_major, 0), NULL, "cmem");
#else
#ifdef USE_CLASS_DEVICE
    class_device_create(cmem_class, NULL, MKDEV(cmem_major, 0), NULL, "cmem");
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
    device_create(cmem_class, NULL, MKDEV(cmem_major, 0), NULL, "cmem");
#else
    device_create(cmem_class, NULL, MKDEV(cmem_major, 0), "cmem");
#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#endif // USE_CLASS_DEVICE
#endif // USE_CLASS_SIMPLE
#endif // USE_UDEV

    pstart[0] = phys_start;
    pend[0] = phys_end;
    pool_table[0] = pools;

/* cut-and-paste below as part of adding support for more than 2 blocks */
    pstart[1] = phys_start_1;
    pend[1] = phys_end_1;
    pool_table[1] = pools_1;
/* cut-and-paste above as part of adding support for more than 2 blocks */

    for (bi = 0; bi < NBLOCKS; bi++) {

        if (bi == 0 && (!phys_start || !phys_end)) {
            if (pools[0]) {
                __E("pools specified: must specify both phys_start and phys_end, exiting...\n");
                err = -EINVAL;
                goto fail_after_create;
            }
            else {
                printk(KERN_INFO "no physical memory specified, continuing "
                       "with no memory allocation capability...\n");

                break;
            }
        }
/* cut-and-paste below as part of adding support for more than 2 blocks */
        if (bi == 1 && (!phys_start_1 || !phys_end_1)) {
            break;
        }
/* cut-and-paste above as part of adding support for more than 2 blocks */

        /* Get the start and end of CMEM memory */
        block_start[bi] = PAGE_ALIGN(simple_strtol(pstart[bi], NULL, 16));
        block_end[bi] = PAGE_ALIGN(simple_strtol(pend[bi], NULL, 16));
        length = block_end[bi] - block_start[bi];

        if (block_start[bi] == 0) {
            sprintf(tmp_str, "_%d", bi);
            __E("Physical address of 0 not allowed (phys_start%s)\n",
                bi == 0 ? "" : tmp_str);
            __E("  (minimum phsyical address is %#lx)\n", PAGE_SIZE);
            err = -EINVAL;
            goto fail_after_create;
        }

        if (length < 0) {
            __E("Negative length of physical memory (%ld)\n", length);
            err = -EINVAL;
            goto fail_after_create;
        }

        block_avail_size[bi] = length;

        /* attempt to determine the end of Linux kernel memory */
        phys_end_kernel = virt_to_phys((void *)PAGE_OFFSET) +
                   (num_physpages << PAGE_SHIFT);

        if (phys_end_kernel > block_start[bi]) {
            if (allowOverlap == 0) {
                __E("CMEM phys_start (%#lx) overlaps kernel (%#lx -> %#lx)\n",
                    block_start[bi], virt_to_phys((void *)PAGE_OFFSET), phys_end_kernel);
                err = -EINVAL;
                goto fail_after_create;
            }
            else {
                printk("CMEM Range Overlaps Kernel Physical - allowing overlap\n");
                printk("CMEM phys_start (%#lx) overlaps kernel (%#lx -> %#lx)\n",
                   block_start[bi], virt_to_phys((void *)PAGE_OFFSET), phys_end_kernel);
            }
        }

        /* Initialize the top memory chunk in which to put the pools */

        __D("calling request_mem_region(%#lx, %ld, \"CMEM\")\n", block_start[bi], length);

        if (!request_mem_region(block_start[bi], length, "CMEM")) {
            __E("Failed to request_mem_region(%#lx, %ld)\n", block_start[bi], length);
            err = -EFAULT;
            goto fail_after_create;
        }
        else {
            block_flags[bi] |= BLOCK_MEMREGION;
        }

    #ifdef NOCACHE
        block_virtp[bi] = (unsigned long) ioremap_nocache(block_start[bi], length);
    #else
        block_virtp[bi] = (unsigned long) ioremap_cached(block_start[bi], length);
    #endif

        if (block_virtp[bi] == 0) {
    #ifdef NOCACHE
            __E("Failed to ioremap_nocache(%#lx, %ld)\n", block_start[bi], length);
    #else
            __E("Failed to ioremap_cached(%#lx, %ld)\n", block_start[bi], length);
    #endif
            err = -EFAULT;
            goto fail_after_create;
        }
        else {
    #ifdef NOCACHE
            __D("ioremap_nocache(%#lx, %ld)=%#lx\n", block_start[bi], length, block_virtp[bi]);
    #else
            __D("ioremap_cached(%#lx, %ld)=%#lx\n", block_start[bi], length, block_virtp[bi]);
    #endif

            block_flags[bi] |= BLOCK_IOREMAP;
        }

        block_virtoff[bi] = block_virtp[bi] - block_start[bi];
        block_virtend[bi] = block_virtp[bi] + length;

        for (i = 0; i < length - 1; i += PAGE_SIZE) {
            *(int *)(block_virtp[bi] + i) = 0;
        }

        /* Parse and allocate the pools */
        for (i=0; i<npools[bi]; i++) {
            t = strsep(&pool_table[bi][i], "x");
            if (!t) {
                err = -EINVAL;
                goto fail_after_create;
            }
            pool_num_buffers = simple_strtol(t, NULL, 10);

            t = strsep(&pool_table[bi][i], "\0");
            if (!t) {
                err = -EINVAL;
                goto fail_after_create;
            }
            pool_size = simple_strtol(t, NULL, 10);

            if (alloc_pool(bi, i, pool_num_buffers, pool_size, NULL) < 0) {
                __E("Failed to alloc pool of size %d and number of buffers %d\n",
                    pool_size, pool_num_buffers);
                err = -ENOMEM;
                goto fail_after_create;
            }

            total_num_buffers[bi] += pool_num_buffers;
        }

        /* use whatever is left for the heap */
        heap_size[bi] = block_avail_size[bi] & PAGE_MASK;
        if (heap_size[bi] > 0) {
            err = alloc_pool(bi, npools[bi], 1, heap_size[bi], &heap_virtp[bi]);
            if (err < 0) {
                __E("Failed to alloc heap of size %#lx\n", heap_size[bi]);
                goto fail_after_create;
            }
            printk(KERN_INFO "allocated heap buffer %#lx of size %#lx\n",
                   heap_virtp[bi], heap_size[bi]);
            heap_pool[bi] = npools[bi];
            header = (HeapMem_Header *)heap_virtp[bi];
            heap_head[bi].next = header;
            heap_head[bi].size = heap_size[bi];
            header->next = NULL;
            header->size = heap_size[bi];
        }
        else {
            __D(KERN_INFO "no remaining memory for heap, no heap created "
                   "for memory block %d\n", bi);
            heap_head[bi].next = NULL;
            heap_head[bi].next = 0;
        }

        __D(KERN_INFO "cmem initialized %d pools between %#lx and %#lx\n",
               npools[bi], block_start[bi], block_end[bi]);
    }

    /* Create the /proc entry */
    cmem_proc_entry = create_proc_entry("cmem", 0, NULL);
    if (cmem_proc_entry) {
        cmem_proc_entry->proc_fops = &cmem_proc_ops;
    }

    printk(KERN_INFO "cmemk initialized\n");

    return 0;

fail_after_create:

    for (bi = 0; bi < NBLOCKS; bi++) {
        if (block_flags[bi] & BLOCK_IOREMAP) {
            __D("calling iounmap(%#lx)...\n", block_virtp[bi]);

            iounmap((void *) block_virtp[bi]);
            block_flags[bi] &= ~BLOCK_IOREMAP;
        }
    }

    for (bi = 0; bi < NBLOCKS; bi++) {
        if (block_flags[bi] & BLOCK_MEMREGION) {
            length = block_end[bi] - block_start[bi];

            __D("calling release_mem_region(%#lx, %ld)...\n", block_start[bi], length);

            release_mem_region(block_start[bi], length);
            block_flags[bi] &= ~BLOCK_MEMREGION;
        }
    }

#if (USE_UDEV==1)

#ifdef USE_CLASS_SIMPLE
    class_simple_device_remove(MKDEV(cmem_major, 0));
    class_simple_destroy(cmem_class);
#else
#ifdef USE_CLASS_DEVICE
    class_device_destroy(cmem_class, MKDEV(cmem_major, 0));
#else
    device_destroy(cmem_class, MKDEV(cmem_major, 0));
#endif // USE_CLASS_DEVICE
    class_destroy(cmem_class);
#endif // USE_CLASS_SIMPLE

#endif // USE_UDEV

fail_after_reg:
    __D("Unregistering character device cmem\n");
    unregister_chrdev(cmem_major, "cmem");

    return err;
}

void __exit cmem_exit(void)
{
    struct list_head *registeredlistp;
    struct list_head *freelistp;
    struct list_head *busylistp;
    struct list_head *e;
    struct list_head *u;
    struct list_head *unext;
    struct pool_buffer *entry;
    struct registered_user *user;
    unsigned long length;
    int num_pools;
    int bi;
    int i;

    __D("In cmem_exit()\n");

    /* Remove the /proc entry */
    remove_proc_entry("cmem", NULL);

    for (bi = 0; bi < NBLOCKS; bi++) {
        num_pools = npools[bi];

        if (heap_pool[bi] != -1) {
            num_pools++;
        }

        /* Free the pool structures and empty the lists. */
        for (i=0; i<num_pools; i++) {
            __D("Freeing memory associated with pool %d\n", i);

            freelistp = &p_objs[bi][i].freelist;
            busylistp = &p_objs[bi][i].busylist;

            e = busylistp->next;
            while (e != busylistp) {
                entry = list_entry(e, struct pool_buffer, element);

                __D("Warning: Freeing busy entry %d at %#lx\n",
                    entry->id, entry->physp);

                registeredlistp = &entry->users;
                u = registeredlistp->next;
                while (u != registeredlistp) {
                    unext = u->next;

                    user = list_entry(u, struct registered_user, element);

                    __D("Removing file %p from user list of buffer %#lx...\n",
                        user->filp, entry->physp);

                    list_del(u);
                    kfree(user);

                    u = unext;
                }

                e = e->next;
                kfree(entry);
            }

            e = freelistp->next;
            while (e != freelistp) {
                entry = list_entry(e, struct pool_buffer, element);

                __D("Freeing free entry %d at %#lx\n", entry->id, entry->physp);

                registeredlistp = &entry->users;
                u = registeredlistp->next;
                while (u != registeredlistp) {
                    /* should never happen, but check to avoid mem leak */
                    unext = u->next;

                    user = list_entry(u, struct registered_user, element);

                    __D("Removing file %p from user list of buffer %#lx...\n",
                        user->filp, entry->physp);

                    list_del(u);
                    kfree(user);

                    u = unext;
                }

                e = e->next;
                kfree(entry);
            }
        }

        if (block_flags[bi] & BLOCK_MEMREGION) {
            length = block_end[bi] - block_start[bi];

            __D("calling release_mem_region(%#lx, %ld)...\n", block_start[bi], length);

            release_mem_region(block_start[bi], length);
            block_flags[bi] &= ~BLOCK_MEMREGION;
        }

        if (block_flags[bi] & BLOCK_IOREMAP) {
            __D("calling iounmap(%#lx)...\n", block_virtp[bi]);

            iounmap((void *) block_virtp[bi]);
            block_flags[bi] &= ~BLOCK_IOREMAP;
        }
    }

#if (USE_UDEV==1)

#ifdef USE_CLASS_SIMPLE
    class_simple_device_remove(MKDEV(cmem_major, 0));
    class_simple_destroy(cmem_class);
#else
#ifdef USE_CLASS_DEVICE
    class_device_destroy(cmem_class, MKDEV(cmem_major, 0));
#else
    device_destroy(cmem_class, MKDEV(cmem_major, 0));
#endif // USE_CLASS_DEVICE
    class_destroy(cmem_class);
#endif // USE_CLASS_SIMPLE

#endif // USE_UDEV

    __D("Unregistering character device cmem\n");
    unregister_chrdev(cmem_major, "cmem");

    printk(KERN_INFO "cmemk unregistered\n");
}

MODULE_LICENSE("GPL");
module_init(cmem_init);
module_exit(cmem_exit);

