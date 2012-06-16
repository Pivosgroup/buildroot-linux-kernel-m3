#ifndef _AML8218_H_
#define _AML8218_H_

#include <linux/interrupt.h>
#include <linux/socket.h>
#include <linux/netdevice.h>
#include <linux/i2c.h>

#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/ca.h>
#include <linux/dvb/osd.h>
#include <linux/dvb/net.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include "../../media/dvb/dvb-core/dvbdev.h"
#include "../../media/dvb/dvb-core/demux.h"
#include "../../media/dvb/dvb-core/dvb_demux.h"
#include "../../media/dvb/dvb-core/dmxdev.h"
#include "../../media/dvb/dvb-core/dvb_filter.h"
#include "../../media/dvb/dvb-core/dvb_net.h"
#include "../../media/dvb/dvb-core/dvb_ringbuffer.h"
#include "../../media/dvb/dvb-core/dvb_frontend.h"

#define DMX_DEV_COUNT     3
#define FE_DEV_COUNT      1
#define CHANNEL_COUNT     32
#define FILTER_COUNT      32
#define FILTER_LEN        15
#define DSC_COUNT         8
#define SEC_BUF_GRP_COUNT 4
#define SEC_BUF_BUSY_SIZE 4
#define SEC_BUF_COUNT     (SEC_BUF_GRP_COUNT*8)
#define ASYNCFIFO_COUNT 2

typedef enum{
	AM_DMX_0=0,
	AM_DMX_1,
	AM_DMX_2,
	AM_DMX_MAX,
}aml_dmx_id_t;

typedef enum {
	AM_TS_SRC_TS0,
	AM_TS_SRC_TS1,
	AM_TS_SRC_TS2,	
	AM_TS_SRC_S2P0,
	AM_TS_SRC_S2P1,
	AM_TS_SRC_HIU
} aml_ts_source_t;

struct aml_sec_buf {
	unsigned long        addr;
	int                  len;
};

struct aml_channel {
	int                  type;
	enum dmx_ts_pes      pes_type;
	int                  pid;
	int                  used;
	int                  filter_count;
	struct dvb_demux_feed     *feed;
	struct dvb_demux_feed     *dvr_feed;
};

struct aml_filter {
	int                  chan_id;
	int                  used;
	struct dmx_section_filter *filter;
	u8                   value[FILTER_LEN];
	u8                   maskandmode[FILTER_LEN];
	u8                   maskandnotmode[FILTER_LEN];
	u8                   neq;
};

struct aml_dsc {
	int                  pid;
	u8                   even[8];
	u8                   odd[8];
	int                  used;
	int                  set;
	int                  id;
	struct aml_dvb      *dvb;
};

struct aml_dmx {
	struct dvb_demux     demux;
	struct dmxdev        dmxdev;
	int                  id;
	int                  feed_count;
	int                  chan_count;
	aml_ts_source_t      source;
	int                  init;
	int                  record;
	struct dmx_frontend  hw_fe[DMX_DEV_COUNT];
	struct dmx_frontend  mem_fe;
	struct dvb_net       dvb_net;
	spinlock_t           slock;
	int                  dmx_irq;
	int                  dvr_irq;
	struct tasklet_struct     dmx_tasklet;
	struct tasklet_struct     dvr_tasklet;
	unsigned long        sec_pages;
	int                  sec_total_len;
	struct aml_sec_buf   sec_buf[SEC_BUF_COUNT];
	unsigned long        pes_pages;
	int                  pes_buf_len;
	unsigned long        sub_pages;
	int                  sub_buf_len;
	struct aml_channel   channel[CHANNEL_COUNT];
	struct aml_filter    filter[FILTER_COUNT];
	irq_handler_t        irq_handler;
	void                *irq_data;
	int                  aud_chan;
	int                  vid_chan;
	int                  sub_chan;
	u32                  section_busy[SEC_BUF_BUSY_SIZE];
};


struct aml_fe {
	int                  id;
	struct dvb_frontend *fe;
	void               *cfg;
	struct platform_device *pdev;
	struct class class;
};

struct aml_asyncfifo {
	int	id;
	int	init;
	int	asyncfifo_irq;
	aml_dmx_id_t	source;
	unsigned long	pages;
	int	buf_len;
	int	buf_toggle;
	struct tasklet_struct     asyncfifo_tasklet;
	spinlock_t           slock;
	struct aml_dvb *dvb;
};

struct aml_dvb {
	struct dvb_device    dvb_dev;
	struct aml_dmx       dmx[DMX_DEV_COUNT];
	struct aml_dsc       dsc[DSC_COUNT];
	struct aml_asyncfifo asyncfifo[ASYNCFIFO_COUNT];
	struct dvb_device   *dsc_dev;
	struct dvb_adapter   dvb_adapter;
	struct device       *dev;
	struct platform_device *pdev;
	aml_ts_source_t      stb_source;
	aml_ts_source_t      dsc_source;
	int                  dmx_init;
	int                  reset_flag;
	spinlock_t           slock;
	struct timer_list    watchdog_timer;
};


/*AMLogic demux interface*/
extern int aml_dmx_hw_init(struct aml_dmx *dmx);
extern int aml_dmx_hw_deinit(struct aml_dmx *dmx);
extern int aml_dmx_hw_start_feed(struct dvb_demux_feed *dvbdmxfeed);
extern int aml_dmx_hw_stop_feed(struct dvb_demux_feed *dvbdmxfeed);
extern int aml_dmx_hw_set_source(struct dmx_demux* demux, dmx_source_t src);
extern int aml_stb_hw_set_source(struct aml_dvb *dvb, dmx_source_t src);
extern int aml_dsc_hw_set_source(struct aml_dvb *dvb, dmx_source_t src);

extern int  dmx_alloc_chan(struct aml_dmx *dmx, int type, int pes_type, int pid);
extern void dmx_free_chan(struct aml_dmx *dmx, int cid);

/*AMLogic dsc interface*/
extern int dsc_set_pid(struct aml_dsc *dsc, int pid);
extern int dsc_set_key(struct aml_dsc *dsc, int type, u8 *key);
extern int dsc_release(struct aml_dsc *dsc);

/*AMLogic ASYNC FIFO interface*/
extern int aml_asyncfifo_hw_init(struct aml_asyncfifo *afifo);
extern int aml_asyncfifo_hw_deinit(struct aml_asyncfifo *afifo);
extern int aml_asyncfifo_hw_set_source(struct aml_asyncfifo *afifo, aml_dmx_id_t src);

/*Get the DVB device*/
extern struct aml_dvb* aml_get_dvb_device(void);

#endif

