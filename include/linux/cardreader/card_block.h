#ifndef __CARD_BLOCK_H
#define __CARD_BLOCK_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/genhd.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/wakelock.h>
#include "cardreader.h"

#define card_get_drvdata(c)		dev_get_drvdata(&(c)->dev)
#define card_set_drvdata(c,d)	dev_set_drvdata(&(c)->dev, d)

#define CARD_NAME_LEN            6
#define SDIO_MAX_FUNCS			 7

typedef unsigned int card_pm_flag_t;

struct memory_card;
struct card_blk_request;

struct sdio_cccr {
	unsigned int		sdio_vsn;
	unsigned int		sd_vsn;
	unsigned int		multi_block:1,
				low_speed:1,
				wide_bus:1,
				high_power:1,
				high_speed:1,
				disable_cd:1;
};

struct sdio_cis {
	unsigned short		vendor;
	unsigned short		device;
	unsigned short		blksize;
	unsigned int		max_dtr;
};

struct card_driver {
	struct device_driver drv;
	int (*probe)(struct memory_card *);
	void (*remove)(struct memory_card *);
	int (*suspend)(struct memory_card *, pm_message_t);
	int (*resume)(struct memory_card *);
};

struct memory_card {
	struct list_head	node;		/* node in hosts devices list */
	struct card_host	*host;		/* the host this device belongs to */
	struct device		dev;		/* the device */
	unsigned int		state;		/* (our) card state */
	unsigned int		capacity;	/* card capacity*/
	u32					raw_cid[4]; /*card raw cid */
	CARD_TYPE_t			card_type;	/* card type*/
	char				name[CARD_NAME_LEN];
	unsigned int		quirks; 	/* card quirks */
#define MMC_QUIRK_LENIENT_FN0	(1<<0)		/* allow SDIO FN0 writes outside of the VS CCCR range */
#define MMC_QUIRK_BLKSZ_FOR_BYTE_MODE (1<<1)	/* use func->cur_blksize */

	unsigned int		sdio_funcs;	/* number of SDIO functions */
	struct sdio_func	*sdio_func[SDIO_MAX_FUNCS]; /* SDIO functions (devices) */
	struct sdio_cccr	cccr;		/* common card info */
	struct sdio_cis		cis;		/* common tuple info */
	unsigned		num_info;	/* number of info strings */
	const char		**info;		/* info strings */
	struct sdio_func_tuple	*tuples;	/* unknown common tuples */

	struct aml_card_info *card_plat_info;
	void				 *card_info;

#ifdef CONFIG_PM
	struct wake_lock card_wakelock;
#endif 

	u8					card_status;
	u8					unit_state;
	u8					card_slot_mode;
	void (*card_io_init)(struct memory_card *card); 
	void (*card_detector) (struct memory_card *card);
	void (*card_insert_process) (struct memory_card *card);
	void (*card_remove_process) (struct memory_card *card);
	void (*card_suspend) (struct memory_card *card);
	void (*card_resume) (struct memory_card *card);
	int (*card_request_process) (struct memory_card *card, struct card_blk_request *brq);
};

struct card_queue {
	struct memory_card		*card;
	struct task_struct	*thread;
	//struct completion	thread_complete;
	//wait_queue_head_t	thread_wq;
	struct semaphore	thread_sem;
	unsigned int		flags;
	struct request		*req;
	int			(*prep_fn)(struct card_queue *, struct request *);
	int			(*issue_fn)(struct card_queue *, struct request *);
	void			*data;
	struct request_queue	*queue;
	struct scatterlist	*sg;
	
	char			*bounce_buf;
	struct scatterlist	*bounce_sg;
	unsigned int		bounce_sg_len;
};

struct card_request {
	unsigned int cmd;
	unsigned char *buf;
	unsigned char *back_buf;
};

struct card_data {
	unsigned int		timeout_ns;	/* data timeout (in ns, max 80ms) */
	unsigned int		timeout_clks;	/* data timeout (in clocks) */
	unsigned int 		lba;			/*logic block address*/
	unsigned int		blk_size;		/* data block size */
	unsigned int		blk_nums;		/* number of blocks */
	unsigned int		error;		/* data error */
	unsigned int		flags;

	unsigned int		bytes_xfered;

	struct card_request	*crq;		/* associated request */

	unsigned int		sg_len;		/* size of scatter list */
	struct scatterlist	*sg;		/* I/O scatter list */
};

struct card_host_ops {
	void	(*request)(struct card_host *host, struct card_blk_request *req);
	void	(*enable_sdio_irq)(struct card_host *card, int enable);
};

struct card_host {
	struct device		*parent;
	struct device	class_dev;
	struct task_struct *card_task;
	struct task_struct *queue_task;
	unsigned char  card_task_state; //1:stop
	unsigned char  queue_task_state;
	
	int			index;
	const struct card_host_ops *ops;
	u32			ocr_avail;
	unsigned long		caps;

#define CARD_CAP_4_BIT_DATA	(1 << 0)	/* Can the host do 4 bit transfers */
#define CARD_CAP_MMC_HIGHSPEED	(1 << 1)	/* Can do MMC high-speed timing */
#define CARD_CAP_SD_HIGHSPEED	(1 << 2)	/* Can do SD high-speed timing */
#define CARD_CAP_SDIO_IRQ	(1 << 3)	/* Can signal pending SDIO IRQs */
#define CARD_CAP_SPI		(1 << 4)	/* Talks only SPI protocols */
#define CARD_CAP_NEEDS_POLL	(1 << 5)	/* Needs polling for card-detection */
#define CARD_CAP_8_BIT_DATA	(1 << 6)	/* Can the host do 8 bit transfers */
#define CARD_CAP_DISABLE		(1 << 7)	/* Can the host be disabled */
#define CARD_CAP_NONREMOVABLE	(1 << 8)	/* Nonremovable e.g. eMMC */
#define CARD_CAP_WAIT_WHILE_BUSY	(1 << 9)	/* Waits while card is busy */

	card_pm_flag_t		pm_caps;		/* Host capabilities */

	/* host specific block data */
	unsigned int		max_req_size;	/* maximum number of bytes in one req */
	unsigned int		max_seg_size;	/* see blk_queue_max_segment_size */
	unsigned short		max_hw_segs;	/* see blk_queue_max_hw_segments */
	unsigned short		max_phys_segs;	/* see blk_queue_max_phys_segments */
	unsigned short		max_sectors;	/* see blk_queue_max_sectors */
	unsigned short		unused;
	unsigned int		max_blk_size;	/* maximum size of one mmc block */
	unsigned int		max_blk_count;	/* maximum number of blocks in one req */

	/* group bitfields together to minimize padding */
	unsigned int		use_spi_crc:1;
	unsigned int		claimed:1;	/* host exclusively claimed */
	unsigned int		bus_dead:1;	/* bus has been released */
	unsigned int		removed:1;	/* host is being removed */

	unsigned int		mode;		/* current card mode of host */
	struct memory_card	*card;		/* device attached to this host */
	struct list_head	cards;		/* devices attached to this host */

	wait_queue_head_t	wq;
	spinlock_t		lock;		/* card_busy lock */
	struct memory_card		*card_busy;	/* the MEMORY card claiming host */
	struct memory_card		*card_selected;	/* the selected MEMORY card */

	CARD_TYPE_t			card_type;	/* card type*/
	unsigned char 		slot_detector;	
	struct work_struct	detect;

	unsigned int		sdio_irqs;
	struct task_struct	*sdio_irq_thread;
	unsigned char 	sdio_task_state;
	atomic_t		sdio_irq_thread_abort;
	card_pm_flag_t		pm_flags;	/* requested pm features */

	unsigned char *dma_phy_buf;
	unsigned char *dma_buf;

	unsigned long		private[0] ____cacheline_aligned;
};

struct card_blk_request {
	struct card_request	crq;
	struct card_data	card_data;
};


#define CARD_STATE_PRESENT	(1<<0)		/* present in sysfs */
#define CARD_STATE_DEAD		(1<<1)		/* device no longer in stack */
#define CARD_STATE_BAD		(1<<2)		/* unrecognised device */
#define CARD_STATE_READONLY	(1<<3)		/* card is read-only */
#define CARD_STATE_INITED	(1<<31)		/* card inited */

#define READ_AFTER_WRITE	(1<<31)
#define SDIO_OPS_REG		(1<<30)
#define SDIO_FIFO_ADDR		(1<<29)

#define card_card_id(c)		(dev_name(&(c)->dev))
#define card_hostname(x)	(dev_name(&(x)->class_dev))

static inline void *card_priv(struct card_host *host)
{
	return (void *)host->private;
}

extern unsigned char sdio_irq_handled;

extern int __card_claim_host(struct card_host *host, struct memory_card *card);

static inline void card_claim_host(struct card_host *host)
{
	__card_claim_host(host, (struct memory_card *)-1);
}
extern int sd_mmc_probe(struct memory_card *card);
extern int sdio_probe(struct memory_card *card);
extern int inand_probe(struct memory_card *card);
extern int ms_probe(struct memory_card *card);
extern int add_card_partition(struct gendisk * disk, 
		struct mtd_partition * part, unsigned int nr_part);

extern unsigned int card_align_data_size(struct memory_card *card, unsigned int sz);
extern void card_init_card(struct memory_card *card, struct card_host *host);
extern int card_register_card(struct memory_card *card);
extern void card_remove_card(struct memory_card *card);
extern int card_add_host_sysfs(struct card_host *host);
extern void card_flush_scheduled_work(void);
extern void card_remove_host_sysfs(struct card_host *host);
extern int card_wait_for_req(struct card_host *host, struct card_blk_request *brq);
extern void card_release_card(struct device *dev);
extern int card_register_driver(struct card_driver *drv);
extern void card_unregister_driver(struct card_driver *drv);
extern void card_free_host_sysfs(struct card_host *host);
extern struct card_host *card_alloc_host_sysfs(int extra, struct device *dev);
extern int card_schedule_work(struct work_struct *work);
extern int card_schedule_delayed_work(struct delayed_work *work, unsigned long delay);
extern void card_release_host(struct card_host *host);

extern void sd_io_init(struct memory_card *card);
#endif
