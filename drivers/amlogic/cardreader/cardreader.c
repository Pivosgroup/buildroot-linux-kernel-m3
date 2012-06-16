/*****************************************************************
**                                                              **
**  Copyright (C) 2004 Amlogic,Inc.                             **
**  All rights reserved                                         **
**        Filename : cardreader.c /Project:  driver         	**
**        Revision : 1.0                                        **
**                                                              **
*****************************************************************/  
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mii.h>
#include <linux/skbuff.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/device.h>
#include <linux/pagemap.h>
#include <linux/platform_device.h>
#include <linux/cardreader/sdio.h>
#include <linux/cardreader/cardreader.h>
#include <linux/cardreader/card_block.h>
#include <linux/kthread.h>
#include <asm/cacheflush.h>
#include <mach/am_regs.h>
#include <mach/irqs.h>
#include <mach/card_io.h>

#define card_list_to_card(l)	container_of(l, struct memory_card, node)
struct completion card_devadd_comp;

struct amlogic_card_host 
{
	struct card_host *host;
	struct aml_card_platform *board_data;
	int present;
 
	/*
	* Flag indicating when the command has been sent. This is used to
	* work out whether or not to send the stop
	*/ 
	unsigned int flags;
	/* flag for current bus settings */ 
	unsigned bus_mode;
	/* Latest in the scatterlist that has been enabled for transfer, but not freed */ 
	int in_use_index;
	/* Latest in the scatterlist that has been enabled for transfer */ 
	int transfer_index;
};

//wait_queue_head_t     sdio_wait_event;
extern void sdio_if_int_handler(struct card_host *host);
extern void sdio_cmd_int_handle(struct memory_card *card);
extern void sdio_timeout_int_handle(struct memory_card *card);

static int card_reader_monitor(void *arg);
void card_detect_change(struct card_host *host, unsigned long delay);
struct card_host *card_alloc_host(int extra, struct device *dev);
static void card_setup(struct card_host *host);
static void amlogic_card_request(struct card_host *host, struct card_blk_request *brq);
struct memory_card *card_find_card(struct card_host *host, u8 card_type);

static struct memory_card *card_alloc_card(struct card_host *host) 
{
	struct memory_card *card;

	card = kzalloc(sizeof(struct memory_card), GFP_KERNEL);
	if (!card)
		return ERR_PTR(-ENOMEM);

	card_init_card(card, host);
	list_add(&card->node, &host->cards);

	return card;
}

static void card_reader_initialize(struct card_host *host) 
{
	struct amlogic_card_host *aml_host = card_priv(host);
	struct aml_card_platform *card_platform = aml_host->board_data;
	struct aml_card_info *card_plat_info;
	struct memory_card *card;
	int i, err = 0;

	for (i=0; i<card_platform->card_num; i++) {
		card_plat_info = &card_platform->card_info[i];

		if (!strcmp("xd_card", card_plat_info->name)) {
#ifdef CONFIG_XD
			card = card_find_card(host, CARD_XD_PICTURE);
			if (card)
				continue;
			card = card_alloc_card(host);
			if (!card)
				continue;

			card->unit_state = CARD_UNIT_NOT_READY;
			strcpy(card->name, CARD_XD_NAME_STR);
			card->card_type = CARD_XD_PICTURE;
			card->card_plat_info = card_plat_info;
			err = xd_probe(card);
			if (err)
				continue;
#endif
		}
		else if (!strcmp("ms_card", card_plat_info->name)) {
#ifdef CONFIG_MS_MSPRO
			card = card_find_card(host, CARD_MEMORY_STICK);
			if (card)
				continue;
			card = card_alloc_card(host);
			if (!card)
				continue;

			card->unit_state = CARD_UNIT_NOT_READY;
			strcpy(card->name, CARD_MS_NAME_STR);
			card->card_type = CARD_MEMORY_STICK;
			card->card_plat_info = card_plat_info;
			err = ms_probe(card);
			if (err)
				continue;
#endif
		}
		else if (!strcmp("sd_card", card_plat_info->name)) {
#ifdef CONFIG_SD_MMC
			card = card_find_card(host, CARD_SECURE_DIGITAL);
			if (card)
				continue;
			card = card_alloc_card(host);
			if (!card)
				continue;

			card->unit_state = CARD_UNIT_NOT_READY;
			strcpy(card->name, CARD_SD_NAME_STR);
			card->card_type = CARD_SECURE_DIGITAL;
			card->card_plat_info = card_plat_info;
			err = sd_mmc_probe(card);
			if (err)
				continue;
#endif
		}
		else if (!strcmp("inand_card", card_plat_info->name)) {
#ifdef CONFIG_INAND
			card = card_find_card(host, CARD_INAND);
			if (card)
				continue;
			card = card_alloc_card(host);
			if (!card)
				continue;

			card->unit_state = CARD_UNIT_NOT_READY;
			strcpy(card->name, CARD_INAND_NAME_STR);
			card->card_type = CARD_INAND;
			card->card_plat_info = card_plat_info;
			err = inand_probe(card);
			if (err)
				continue;
#endif
		}
		else if (!strcmp("sdio_card", card_plat_info->name)) {
#ifdef CONFIG_SDIO
			card = card_find_card(host, CARD_SDIO);
			if (card)
				continue;
			card = card_alloc_card(host);
			if (!card)
				continue;

			card->unit_state = CARD_UNIT_NOT_READY;
			strcpy(card->name, CARD_SDIO_NAME_STR);
			card->card_type = CARD_SDIO;
			card->card_plat_info = card_plat_info;
			err = sdio_probe(card);
			if (err)
				continue;
#endif
		}
		else if (!strcmp("cf_card", card_plat_info->name)) {
#ifdef CONFIG_CF
			card = card_find_card(host, CARD_COMPACT_FLASH);
			if (card)
				continue;
			card = card_alloc_card(host);
			if (!card)
				continue;

			card->unit_state = CARD_UNIT_NOT_READY;
			strcpy(card->name, CARD_CF_NAME_STR);
			card->card_type = CARD_COMPACT_FLASH;
			card->card_plat_info = card_plat_info;
			err = cf_probe(card);
			if (err)
				continue;
#endif
		}
	}
}

static irqreturn_t sdio_interrupt_monitor(int irq, void *dev_id, struct pt_regs *regs) 
{
	struct card_host *host = (struct card_host *)dev_id;
	struct memory_card *card = host->card_busy;
	unsigned sdio_interrupt_resource;

	sdio_interrupt_resource = sdio_check_interrupt();
	switch (sdio_interrupt_resource) {
		case SDIO_IF_INT:
		    sdio_if_int_handler(host);
		    break;

		case SDIO_CMD_INT:
			sdio_cmd_int_handle(card);
			break;

		case SDIO_TIMEOUT_INT:
			sdio_timeout_int_handle(card);
			break;
	
		case SDIO_SOFT_INT:
		    //AVDetachIrq(sdio_int_handler);
		    //sdio_int_handler = -1;
		    break;
	
		case SDIO_NO_INT:	
			break;

		default:	
			break;	
	}

    return IRQ_HANDLED; 

} 

struct card_host *sdio_host;

static int card_reader_init(struct card_host *host) 
{	
       sdio_host = host;
	host->dma_buf = dma_alloc_coherent(NULL, host->max_req_size, (dma_addr_t *)&host->dma_phy_buf, GFP_KERNEL);
	if(host->dma_buf == NULL)
		return -ENOMEM;

	card_reader_initialize(host);	
	host->card_task = kthread_run(card_reader_monitor, host, "card");
	if (!host->card_task)	
		printk("card creat process failed\n");
	else	
		printk("card creat process sucessful\n");

	if (request_irq(INT_SDIO, (irq_handler_t) sdio_interrupt_monitor, 0, "sd_mmc", host)) {
		printk("request SDIO irq error!!!\n");
		return -1;
	}

#ifdef CONFIG_SDIO_HARD_IRQ
	host->caps |= CARD_CAP_SDIO_IRQ;
#endif
	return 0;
} 

static int card_reader_monitor(void *data)
{
    unsigned card_type, card_4in1_init_type;
    struct card_host *card_host = (struct card_host *)data;
    struct memory_card *card = NULL;
    card_4in1_init_type = 0;

	daemonize("card_read_monitor");
	
	while(1) {
		msleep(200);

		if(card_host->card_task_state)
		{
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			set_current_state(TASK_RUNNING);
		}
		for(card_type=CARD_XD_PICTURE; card_type<CARD_MAX_UNIT; card_type++) {

			card_reader_initialize(card_host);
			card = card_find_card(card_host, card_type);
			if (card == NULL)
				continue;

			__card_claim_host(card_host, card);
			card->card_io_init(card);
			card->card_detector(card);
			card_release_host(card_host);

	    	if((card->card_status == CARD_INSERTED) && (((card->unit_state != CARD_UNIT_READY) 
				&& ((card_type == CARD_SDIO) ||(card_type == CARD_INAND)
				|| (card_host->slot_detector == CARD_REMOVED)||(card->card_slot_mode == CARD_SLOT_DISJUNCT)))
				||(card->unit_state == CARD_UNIT_RESUMED))) {

				if(card->unit_state == CARD_UNIT_RESUMED){
					__card_claim_host(card_host, card);
					card->card_insert_process(card);
					card_release_host(card_host);
					card->unit_state = CARD_UNIT_READY;
					printk("monitor : resume\n");
					break;
				}
					
				__card_claim_host(card_host, card);
				card->card_insert_process(card);
				card_release_host(card_host);
					
				if(card->unit_state == CARD_UNIT_PROCESSED) {
					if(card->card_slot_mode == CARD_SLOT_4_1) {
						if (card_type != CARD_SDIO && card_type != CARD_INAND) {
	                		card_host->slot_detector = CARD_INSERTED;
	                		card_4in1_init_type = card_type;
	                	}
					}
					card->unit_state = CARD_UNIT_READY;
					card_host->card_type = card_type;
					if(card->state != CARD_STATE_PRESENT)
					card->state = CARD_STATE_INITED;
					if (card_type == CARD_SDIO)
						card_host->card = card;
					card_detect_change(card_host, 0);
					printk("monitor : INSERT\n");
	            }
	        }
	        else if((card->card_status == CARD_REMOVED) && ((card->unit_state != CARD_UNIT_NOT_READY)
				||(card->unit_state == CARD_UNIT_RESUMED))){

				if(card->card_slot_mode == CARD_SLOT_4_1) {                       
					if (card_type == card_4in1_init_type) 
						card_host->slot_detector = CARD_REMOVED;
				}

				card->card_remove_process(card);

				if(card->unit_state == CARD_UNIT_PROCESSED) {
					card->unit_state = CARD_UNIT_NOT_READY;

					printk("monitor : REMOVED\n");

					if(card) {
						list_del(&card->node);
						card_remove_card(card);
					}
				}
	        }
		}
	}

    return 0;
}

static void card_deselect_cards(struct card_host *host) 
{	
	if (host->card_selected)		
		host->card_selected = NULL;	
}

/*
 * Check whether cards we already know about are still present.
 * We do this by requesting status, and checking whether a card
 * responds.
 *
 * A request for status does not cause a state change in data
 * transfer mode.
 */ 
static void card_check_cards(struct card_host *host) 
{
	//struct list_head *l, *n;
	card_deselect_cards(host);

	/*list_for_each_safe(l, n, &host->cards)
	{
		struct memory_card *card = card_list_to_card(l);
	       
		if(card->card_type == host->card_type)
	       continue;
	       
		card->state = CARD_STATE_DEAD;
	} */ 
} 

int __card_claim_host(struct card_host *host, struct memory_card *card)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	int err = 0;

	add_wait_queue(&host->wq, &wait);
	spin_lock_irqsave(&host->lock, flags);
	while (1) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (host->card_busy == NULL)
			break;
		spin_unlock_irqrestore(&host->lock, flags);
		schedule();
		spin_lock_irqsave(&host->lock, flags);
	}
	set_current_state(TASK_RUNNING);
	host->card_busy = card;
	if (!host->claimed)
		host->claimed = 1;
	spin_unlock_irqrestore(&host->lock, flags);
	remove_wait_queue(&host->wq, &wait);

	if (card != (void *)-1) {
		host->card_selected = card;
	}

	return err;
}

EXPORT_SYMBOL(__card_claim_host);

void sdio_reinit(void)
{
    struct memory_card* sdio_card = card_find_card(sdio_host, CARD_SDIO);
    
    __card_claim_host(sdio_host, sdio_card);
    sdio_card->card_io_init(sdio_card);
    sdio_card->card_detector(sdio_card);
    sdio_card->card_insert_process(sdio_card);
    sdio_card->unit_state = CARD_UNIT_READY;
    card_release_host(sdio_host);
}
EXPORT_SYMBOL(sdio_reinit);

#ifdef CONFIG_SDIO
int sdio_read_common_cis(struct memory_card *card);
int sdio_read_func_cis(struct sdio_func *func);
static int card_sdio_init_func(struct memory_card *card, unsigned int fn)
{
	int ret = 0;
	struct sdio_func *func;

	BUG_ON(fn > SDIO_MAX_FUNCS);

	card_claim_host(card->host);

	func = sdio_alloc_func(card);
	if (IS_ERR(func))
		return PTR_ERR(func);

	func->num = fn;

	ret = sdio_read_func_cis(func);
	if (ret)
		goto fail;

	card_release_host(card->host);

	card->sdio_func[fn - 1] = func;

	return 0;

fail:
	card_release_host(card->host);
	sdio_remove_func(func);
	return ret;
}

#if defined(CONFIG_MACH_MESON_8726M_REFB09)
extern int amlogic_wifi_power(int on);
#endif
static int card_sdio_init_card(struct memory_card *card)
{
	int err, i;

       /*read common cis for samsung nrx600 wifi*/
        card_claim_host(card->host);
        sdio_read_common_cis(card);
        card_release_host(card->host);

	for (i = 0; i < card->sdio_funcs; i++) {
		err = card_sdio_init_func(card, i + 1);
		if (err) 
			return err;

		err = sdio_add_func(card->sdio_func[i]);
		if (err) 
			return err;
	}

#if defined(CONFIG_MACH_MESON_8726M_REFB09)
	amlogic_wifi_power(0);
#endif
	return 0;
}

/*
 * Host is being removed. Free up the current card.
 */
static void card_sdio_remove(struct card_host *host)
{
	int i;

	BUG_ON(!host);
	BUG_ON(!host->card);

	for (i = 0;i < host->card->sdio_funcs;i++) {
		if (host->card->sdio_func[i]) {
			sdio_remove_func(host->card->sdio_func[i]);
			host->card->sdio_func[i] = NULL;
		}
	}

	card_remove_card(host->card);
	host->card = NULL;
}

#else
static int card_sdio_init_func(struct memory_card *card, unsigned int fn)
{
	return 0;
}

static int card_sdio_init_card(struct memory_card *card)
{
	return 0;
}

/*
 * Host is being removed. Free up the current card.
 */
static void card_sdio_remove(struct card_host *host)
{
}


#endif
/*
 * Locate a Memory card on this Memory host given a raw CID.
 */ 
struct memory_card *card_find_card(struct card_host *host, u8 card_type) 
{
	struct memory_card *card;
	
	list_for_each_entry(card, &host->cards, node) {
		if (card->card_type == card_type)		
			return card;
	}

	return NULL;
}

/**
 *	card_add_host - initialise host hardware
 *	@host: card host
 */ 
int card_add_host(struct card_host *host) 
{
	int ret;
	ret = card_add_host_sysfs(host);
	/*if (ret == 0)
	{
		card_detect_change(host, 0);
	} */ 
    
	return ret;
}

/**
 *	card_remove_host - remove host hardware
 *	@host: card host
 *
 *	Unregister and remove all cards associated with this host,
 *	and power down the CARD bus.
 */ 
void card_remove_host(struct card_host *host) 
{
	struct list_head *l, *n;

	list_for_each_safe(l, n, &host->cards) {	
		struct memory_card *card = card_list_to_card(l);
		card_remove_card(card);
	} 

	if (host->dma_buf != NULL)
	{
	   dma_free_coherent(NULL, host->max_req_size, host->dma_buf, (dma_addr_t )host->dma_phy_buf);
	   host->dma_buf  = NULL;
	   host->dma_phy_buf = NULL;
	}

	free_irq(INT_SDIO, host);
	card_remove_host_sysfs(host);
} 

/**
 *	card_free_host - free the host structure
 *	@host: card host
 *
 *	Free the host once all references to it have been dropped.
 */ 
void card_free_host(struct card_host *host) 
{
	card_flush_scheduled_work();

	card_free_host_sysfs(host);
} 

static void card_discover_cards(struct card_host *host) 
{
	int err;
	struct memory_card *card;

	BUG_ON(host->card_busy == NULL);
	
	card = card_find_card(host, host->card_type);
	if (!card) {	
		card = card_alloc_card(host);
		if (IS_ERR(card)) {	
			err = PTR_ERR(card);		
		}	
		list_add(&card->node, &host->cards);
	}

	if (card->card_type == CARD_SDIO)
		host->card = card;
	
	card->state &= (~CARD_STATE_DEAD);
}

static void card_setup(struct card_host *host) 
{
	card_discover_cards(host);
} 

/**
 *	card_release_host - release a host
 *	@host: card host to release
 *
 *	Release a CARD host, allowing others to claim the host
 *	for their operations.
 */ 
void card_release_host(struct card_host *host) 
{
	unsigned long flags;

	BUG_ON(host->card_busy == NULL);
	spin_lock_irqsave(&host->lock, flags);
	host->card_busy = NULL;
	host->claimed = 0;
	spin_unlock_irqrestore(&host->lock, flags);
	wake_up(&host->wq);
} 

static void card_reader_rescan(struct work_struct *work) 
{	
	int err = 0;
	struct list_head *l, *n;
	struct card_host *host = container_of(work, struct card_host, detect);

	card_claim_host(host);

	card_setup(host);

	card_check_cards(host);

	card_release_host(host);

	list_for_each_safe(l, n, &host->cards) {
		struct memory_card *card = card_list_to_card(l);

		/*
		* If this is a new and good card, register it.
		*/ 
		if ((!(card->state & CARD_STATE_PRESENT)) && (!(card->state & CARD_STATE_DEAD) && (card->state & CARD_STATE_INITED))) {
			if (card_register_card(card))	
				card->state = CARD_STATE_DEAD;
			else	
				card->state = CARD_STATE_PRESENT;	

			if ((card->card_type == CARD_SDIO)) {
				err = card_sdio_init_card(card);
				if (err)
					card_sdio_remove(host);
			}
		}

		/*
		* If this card is dead, destroy it.
		*/ 
		if (card->state == CARD_STATE_DEAD) {	
			list_del(&card->node);
			card_remove_card(card);
		}
	}

}

struct card_host *card_alloc_host(int extra, struct device *dev) 
{	
	struct card_host *host;

	host = card_alloc_host_sysfs(extra, dev);	
	if (host) {	
		spin_lock_init(&host->lock);
		init_waitqueue_head(&host->wq);	
		INIT_LIST_HEAD(&host->cards);
		INIT_WORK(&host->detect, card_reader_rescan);
		
		    /*
		     * By default, hosts do not support SGIO or large requests.
		     * They have to set these according to their abilities.
		     */ 
		host->max_hw_segs = 1;
		host->max_phys_segs = 1;
		host->max_sectors = 1 << (PAGE_CACHE_SHIFT - 5);
		host->max_seg_size = PAGE_CACHE_SIZE;
		host->max_blk_size = 512;
		host->max_blk_count = 256;
		host->max_req_size = 512*256;	/*for CONFIG_CARD_BLOCK_BOUNCE fix me*/
		printk("card max_req_size is %dK \n", host->max_req_size/1024);
	}

	return host;

}

int card_wait_for_req(struct card_host *host, struct card_blk_request *brq) 
{
	WARN_ON(host->card_busy == NULL);
	host->ops->request(host, brq);	
	return 0;
}
EXPORT_SYMBOL(card_wait_for_req);

/**
 *	card_detect_change - process change of state on a memory card socket
 *	@host: host which changed state.
 *	@delay: optional delay to wait before detection (jiffies)
 *
 *	All we know is that card(s) have been inserted or removed
 *	from the socket(s).  We don't know which socket or cards.
 */ 
void card_detect_change(struct card_host *host, unsigned long delay) 
{
	/*if (delay)
		card_schedule_delayed_work(&host->detect, delay);
	else */ 
	init_completion(&card_devadd_comp);
	card_schedule_work(&host->detect);
	wait_for_completion(&card_devadd_comp);
} 
EXPORT_SYMBOL(card_detect_change);

static void amlogic_enable_sdio_irq(struct card_host *host, int enable)
{
	if (enable)
		sdio_open_host_interrupt(SDIO_IF_INT);
	else
		sdio_close_host_interrupt(SDIO_IF_INT);

	return;
}

static struct card_host_ops amlogic_card_ops = { 
	.request = amlogic_card_request, 
	.enable_sdio_irq = amlogic_enable_sdio_irq,
};

struct card_host * the_card_host;

static int amlogic_card_probe(struct platform_device *pdev) 
{
	struct card_host *host;
	struct amlogic_card_host *aml_host;
	int ret;	

	host = card_alloc_host(sizeof(struct amlogic_card_host), &pdev->dev);
	if (!host) {	
		printk("Failed to allocate card host\n");	
		return -ENOMEM;	
	}

	the_card_host = host;
	host->ops = &amlogic_card_ops;
	aml_host = card_priv(host);	
	aml_host->host = host;	
	aml_host->bus_mode = 0;
	aml_host->board_data = pdev->dev.platform_data;
  
	platform_set_drvdata(pdev, host);
	/*
	* Add host to CARD layer
	*/ 
	ret = card_add_host(host);
	/*
	* monitor card insertion/removal if we can
	*/ 
	ret = card_reader_init(host);
	if (ret) {
		card_free_host(host);
		return ret;	
	}

	return 0;
}

/*
 * Remove a device
 */ 
static int amlogic_card_remove(struct platform_device *pdev) 
{
	struct card_host *card = platform_get_drvdata(pdev);
	struct amlogic_card_host *host;
	
	if (!card)	
		return -1;

	host = card_priv(card);
	card_remove_host(card);	
	card_free_host(card);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void amlogic_card_request(struct card_host *host, struct card_blk_request *brq)
{
	struct memory_card *card = host->card_busy;

	BUG_ON(card == NULL);

	card->card_request_process(card, brq);

	return;
}

static struct platform_driver amlogic_card_driver = { 
	.probe = amlogic_card_probe, 
	.remove = amlogic_card_remove, 
	.driver =
	    {
			.name = "AMLOGIC_CARD", 
			.owner = THIS_MODULE, 
		}, 
};

static int __init amlogic_card_init(void) 
{	
	return platform_driver_register(&amlogic_card_driver);
}

static void __exit amlogic_card_exit(void) 
{	
	platform_driver_unregister(&amlogic_card_driver);
} 

module_init(amlogic_card_init);


module_exit(amlogic_card_exit);


MODULE_DESCRIPTION("Amlogic Memory Card Interface driver");

MODULE_LICENSE("GPL");

