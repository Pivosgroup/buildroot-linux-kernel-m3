/*****************************************************************
**
**  Copyright (C) 2010 Amlogic,Inc.
**  All rights reserved
**        Filename : amlfrontend.c
**
**  comment:
**        Driver for aml demodulator
** 
*****************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <mach/am_regs.h>

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>

#include "aml_demod.h"
#include "demod_func.h"
#include "../dvb/aml_dvb.h"
#include "amlfrontend.h"

#define AMDEMOD_FE_DEV_COUNT 1

#if 1
#define pr_dbg(fmt, args...) printk(KERN_DEBUG "DVB: " fmt, ## args)
#else
#define pr_dbg(fmt, args...)
#endif

#define pr_error(fmt, args...) printk(KERN_ERR "DVB: " fmt, ## args)


struct amlfe_state {
	struct amlfe_config config;
	struct aml_demod_sys sys;
	
	struct aml_demod_sta *sta;
	struct aml_demod_i2c *i2c;
	
	u32                       freq;
	fe_modulation_t     mode;
	u32                       symbol_rate;
	struct dvb_frontend fe;

	wait_queue_head_t  lock_wq;
};

MODULE_PARM_DESC(frontend_mode, "\n\t\t Frontend mode 0-DVBC, 1-DVBT");
static int frontend_mode = -1;
module_param(frontend_mode, int, S_IRUGO);

MODULE_PARM_DESC(frontend_i2c, "\n\t\t IIc adapter id of frontend");
static int frontend_i2c = -1;
module_param(frontend_i2c, int, S_IRUGO);

MODULE_PARM_DESC(frontend_tuner, "\n\t\t Frontend tuner type 0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316");
static int frontend_tuner = -1;
module_param(frontend_tuner, int, S_IRUGO);

MODULE_PARM_DESC(frontend_tuner_addr, "\n\t\t Tuner IIC address of frontend");
static int frontend_tuner_addr = -1;
module_param(frontend_tuner_addr, int, S_IRUGO);

static struct aml_fe amdemod_fe[1];/*[AMDEMOD_FE_DEV_COUNT] = 
	{[0 ... (AMDEMOD_FE_DEV_COUNT-1)] = {-1, NULL, NULL}};*/

static struct aml_demod_i2c demod_i2c;
static struct aml_demod_sta demod_sta;

static irqreturn_t amdemod_isr(int irq, void *data)
{
	struct amlfe_state *state = data;
	
	if(((frontend_mode==0)&&dvbc_isr_islock())
		||((frontend_mode==1)&&dvbt_isr_islock())) {
		if(waitqueue_active(&state->lock_wq))
			wake_up_interruptible(&state->lock_wq);
	}
	return IRQ_HANDLED;
}

static int install_isr(struct amlfe_state *state)
{
	int r = 0;

	/* hook demod isr */
	printk("amdemod irq register[IRQ(%d)].\n", INT_DEMOD);
	r = request_irq(INT_DEMOD, &amdemod_isr,
				IRQF_SHARED, "amldemod",
				(void *)state);
	if (r) {
		printk("amdemod irq register error.\n");
	}
	return r;
}

static void uninstall_isr(struct amlfe_state *state)
{
	printk("amdemod irq unregister[IRQ(%d)].\n", INT_DEMOD);

	free_irq(INT_DEMOD, (void*)state);
}


static int aml_fe_dvb_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	//struct amlfe_state *state = fe->demodulator_priv;

	return 0;
}

static int aml_fe_dvb_sleep(struct dvb_frontend *fe)
{
	//struct amlfe_state *state = fe->demodulator_priv;
	return 0;
}

static int _prepare_i2c(struct dvb_frontend *fe, struct aml_demod_i2c *i2c)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct i2c_adapter *adapter;
	
	adapter = i2c_get_adapter(state->config.i2c_id);
	i2c->i2c_id = state->config.i2c_id;
	i2c->i2c_priv = adapter;
	if(!adapter){
		printk("can not get i2c adapter[%d] \n", state->config.i2c_id);
		return -1;
	}
	return 0;
}

static int aml_fe_dvbc_init(struct dvb_frontend *fe)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sys sys;
	struct aml_demod_i2c i2c;
	
	pr_dbg("AML Demod DVB-C init\r\n");

	memset(&sys, 0, sizeof(sys));
	memset(&i2c, 0, sizeof(i2c));
	
    	sys.clk_en = 1;
	sys.clk_src = 0;
	sys.clk_div = 0;
	sys.pll_n = 1;
	sys.pll_m = 50;
	sys.pll_od = 0;
	sys.pll_sys_xd = 20;
	sys.pll_adc_xd = 21;
	sys.agc_sel = 2;
	sys.adc_en = 1;
	sys.i2c = (long)&i2c;
	sys.debug = 0;
	i2c.tuner = state->config.tuner_type;
	i2c.addr = state->config.tuner_addr;

#if 0
	i2c.scl_oe  = i2c.sda_oe  = CBUS_REG_ADDR(PREG_EGPIO_EN_N)/*0xc1108030*/;
	i2c.scl_out = i2c.sda_out = CBUS_REG_ADDR(PREG_EGPIO_O)/*0xc1108034*/;
	i2c.scl_in  = i2c.sda_in  = CBUS_REG_ADDR(PREG_EGPIO_I)/*0xc1108038*/;
	i2c.scl_bit = 15;
	i2c.sda_bit = 16;	
	i2c.udelay = 1;
	i2c.retries = 3;
	i2c.debug = 0;
#endif

	_prepare_i2c(fe, &i2c);
	
	demod_set_sys(state->sta, state->i2c, &sys);

	state->sys = sys;
	
	return 0;
}

static int aml_fe_dvbt_init(struct dvb_frontend *fe)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sys sys;
	struct aml_demod_i2c i2c;
	
	pr_dbg("AML Demod DVB-T init\r\n");

	memset(&sys, 0, sizeof(sys));
	memset(&i2c, 0, sizeof(i2c));
	
    	sys.clk_en = 1;
	sys.clk_src = 0;
	sys.clk_div = 0;
	sys.pll_n = 1;
	sys.pll_m = 50;
	sys.pll_od = 0;
	sys.pll_sys_xd = 20;
	sys.pll_adc_xd = 21;
	sys.agc_sel = 2;
	sys.adc_en = 1;
	sys.i2c = (long)&i2c;
	sys.debug = 0;
	i2c.tuner = state->config.tuner_type;
	i2c.addr = state->config.tuner_addr;
	
#if 0
	i2c.scl_oe  = i2c.sda_oe  = CBUS_REG_ADDR(PREG_EGPIO_EN_N)/*0xc1108030*/;
	i2c.scl_out = i2c.sda_out = CBUS_REG_ADDR(PREG_EGPIO_O)/*0xc1108034*/;
	i2c.scl_in  = i2c.sda_in  = CBUS_REG_ADDR(PREG_EGPIO_I)/*0xc1108038*/;
	i2c.scl_bit = 15;
	i2c.sda_bit = 16;	
	i2c.udelay = 1;
	i2c.retries = 3;
	i2c.debug = 0;
#endif

	_prepare_i2c(fe, &i2c);

	demod_set_sys(state->sta, state->i2c, &sys);

	state->sys = sys;
	
	return 0;
}

static int aml_fe_dvbc_read_status(struct dvb_frontend *fe, fe_status_t * status)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sts demod_sts;
	static int iii=-1;
	int ilock;
	
	dvbc_status(state->sta, state->i2c, &demod_sts);
	if(demod_sts.ch_sts&0x1)
	{
		ilock=1;
		*status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	}
	else
	{
		ilock=0;
		*status = FE_TIMEDOUT;
	}

	if(iii != ilock){
		printk("%s.\n", ilock? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		iii = ilock;
	}
	
	return  0;
}

static int aml_fe_dvbt_read_status(struct dvb_frontend *fe, fe_status_t * status)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sts demod_sts;
	static int iii=-1;
	int ilock;
	
	dvbt_status(state->sta, state->i2c, &demod_sts);
	if(demod_sts.ch_sts>>12&1)
	{
		ilock=1;
		*status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	}
	else
	{
		ilock=0;
		*status = FE_TIMEDOUT;
	}

	if(iii != ilock){
		printk("%s.\n", ilock? "!!  >> LOCK << !!" : "!! >> UNLOCK << !!");
		iii = ilock;
	}
	
	return  0;
}

static int aml_fe_dvbc_read_ber(struct dvb_frontend *fe, u32 * ber)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sts demod_sts;

	dvbc_status(state->sta, state->i2c, &demod_sts);
	*ber = demod_sts.ch_ber;
	
	return 0;
}
static int aml_fe_dvbt_read_ber(struct dvb_frontend *fe, u32 * ber)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sts demod_sts;

	dvbt_status(state->sta, state->i2c, &demod_sts);
	*ber = demod_sts.ch_ber&0xffff;
	
	return 0;
}

static int aml_fe_dvbc_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sts demod_sts;
	
	dvbc_status(state->sta, state->i2c, &demod_sts);
	*strength=demod_sts.ch_pow& 0xffff;

	return 0;
}

static int aml_fe_dvbt_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sts demod_sts;
	
	dvbt_status(state->sta, state->i2c, &demod_sts);
	*strength=demod_sts.ch_pow;

	return 0;
}

static int aml_fe_dvbc_read_snr(struct dvb_frontend *fe, u16 * snr)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sts demod_sts;
	
	dvbc_status(state->sta, state->i2c, &demod_sts);
	*snr = demod_sts.ch_snr/100;
	
	return 0;
}
static int aml_fe_dvbt_read_snr(struct dvb_frontend *fe, u16 * snr)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sts demod_sts;
	
	dvbt_status(state->sta, state->i2c, &demod_sts);
	*snr = (demod_sts.ch_snr>>20&0x3ff)>>3;
	
	return 0;
}

static int aml_fe_dvb_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_sts demod_sts;
	dvbc_status(state->sta, state->i2c, &demod_sts);
	*ucblocks = demod_sts.ch_per;
	return 0;
}

static int to_qam_mode(fe_modulation_t qam)
{
	switch(qam)
	{
		case QAM_16:  return 0;
		case QAM_32:  return 1;
		case QAM_64:  return 2;
		case QAM_128:return 3;
		case QAM_256:return 4;
		default:          return 2;
	}
	return 2;
}

static int amdemod_stat_islock(struct amlfe_state *state)
{
	struct aml_demod_sts demod_sts;
	if(frontend_mode==0){
		/*DVBC*/
		dvbc_status(state->sta, state->i2c, &demod_sts);
		return (demod_sts.ch_sts&0x1);
	} else if (frontend_mode==1){
		/*DVBT*/
		dvbt_status(state->sta, state->i2c, &demod_sts);
		return (demod_sts.ch_sts>>12&1);
	}
	return 0;
}

static int aml_fe_dvbc_set_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_dvbc param;//mode 0:16, 1:32, 2:64, 3:128, 4:256
	
	memset(&param, 0, sizeof(param));
	param.ch_freq = p->frequency/1000;
	param.mode = to_qam_mode(p->u.qam.modulation);
	param.symb_rate = p->u.qam.symbol_rate/1000;
	
	dvbc_set_ch(state->sta, state->i2c, &param);

	{
		int ret;
		ret = wait_event_interruptible_timeout(state->lock_wq, amdemod_stat_islock(state), 2*HZ);
		if(!ret)	printk("amlfe wait lock timeout.\n");
	}

	state->freq=p->frequency;
	state->mode=p->u.qam.modulation ;
	state->symbol_rate=p->u.qam.symbol_rate;
	
	pr_dbg("AML DEMOD => frequency=%d,symbol_rate=%d\r\n",p->frequency,p->u.qam.symbol_rate);
	return  0;
}

static int aml_fe_dvbt_set_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	struct amlfe_state *state = fe->demodulator_priv;
	struct aml_demod_dvbt param;
	
    //////////////////////////////////////
    // bw == 0 : 8M 
    //       1 : 7M
    //       2 : 6M
    //       3 : 5M
    // agc_mode == 0: single AGC
    //             1: dual AGC
    //////////////////////////////////////
    	memset(&param, 0, sizeof(param));
	param.ch_freq = p->frequency/1000;
	param.bw = p->u.ofdm.bandwidth;
	param.agc_mode = 1;
	
	dvbt_set_ch(state->sta, state->i2c, &param);

	{
		int ret;
		ret = wait_event_interruptible_timeout(state->lock_wq, amdemod_stat_islock(state), 2*HZ);
		if(!ret)	printk("amlfe wait lock timeout.\n");
	}
	
	state->freq=p->frequency;
	state->mode=p->u.ofdm.bandwidth;
	
	pr_dbg("AML DEMOD => frequency=%d,bw=%d\r\n",p->frequency,p->u.ofdm.bandwidth);
	return  0;
}

static int aml_fe_dvbc_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{//these content will be writed into eeprom .

	struct amlfe_state *state = fe->demodulator_priv;
	
	p->frequency=state->freq;
	p->u.qam.modulation=state->mode;
	p->u.qam.symbol_rate=state->symbol_rate;
	
	return 0;
}
static int aml_fe_dvbt_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{//these content will be writed into eeprom .

	struct amlfe_state *state = fe->demodulator_priv;
	
	p->frequency=state->freq;
	p->u.ofdm.bandwidth=state->mode;
	
	return 0;
}

static void aml_fe_dvb_release(struct dvb_frontend *fe)
{
	struct amlfe_state *state = fe->demodulator_priv;

	uninstall_isr(state);
	
	kfree(state);
}

static struct dvb_frontend_ops aml_fe_dvbc_ops;
static struct dvb_frontend_ops aml_fe_dvbt_ops;
static struct dvb_tuner_ops aml_fe_tuner_ops;

struct dvb_frontend *aml_fe_dvbc_attach(const struct amlfe_config *config)
{
	struct amlfe_state *state = NULL;
	struct dvb_tuner_info *tinfo;
	
	/* allocate memory for the internal state */
	
	state = kmalloc(sizeof(struct amlfe_state), GFP_KERNEL);
	if (state == NULL)
		return NULL;

	/* setup the state */
	state->config = *config;

	/* create dvb_frontend */
	memcpy(&state->fe.ops, &aml_fe_dvbc_ops, sizeof(struct dvb_frontend_ops));

	/*set tuner parameters*/
	tinfo = tuner_get_info(state->config.tuner_type, 0);
	memcpy(&state->fe.ops.tuner_ops, &aml_fe_tuner_ops,
	       sizeof(struct dvb_tuner_ops));
	memcpy(&state->fe.ops.tuner_ops.info, tinfo,
		sizeof(state->fe.ops.tuner_ops.info));
	
	state->sta = &demod_sta;
	state->i2c = &demod_i2c;

	init_waitqueue_head(&state->lock_wq);

	install_isr(state);
	
	state->fe.demodulator_priv = state;
	
	return &state->fe;
}

EXPORT_SYMBOL(aml_fe_dvbc_attach);

struct dvb_frontend *aml_fe_dvbt_attach(const struct amlfe_config *config)
{
	struct amlfe_state *state = NULL;
	struct dvb_tuner_info *tinfo;

	/* allocate memory for the internal state */
	
	state = kmalloc(sizeof(struct amlfe_state), GFP_KERNEL);
	if (state == NULL)
		return NULL;

	/* setup the state */
	state->config = *config;


	/* create dvb_frontend */
	memcpy(&state->fe.ops, &aml_fe_dvbt_ops, sizeof(struct dvb_frontend_ops));

	/*set tuner parameters*/
	tinfo = tuner_get_info(state->config.tuner_type, 1);
	memcpy(&state->fe.ops.tuner_ops, &aml_fe_tuner_ops,
	       sizeof(struct dvb_tuner_ops));
	memcpy(&state->fe.ops.tuner_ops.info, tinfo,
		sizeof(state->fe.ops.tuner_ops.info));

	state->sta = &demod_sta;
	state->i2c = &demod_i2c;

	init_waitqueue_head(&state->lock_wq);

	install_isr(state);
	
	state->fe.demodulator_priv = state;
	
	return &state->fe;
}

EXPORT_SYMBOL(aml_fe_dvbt_attach);


static struct dvb_frontend_ops aml_fe_dvbc_ops = {

	.info = {
		 .name = "AMLOGIC DVB-C",
		 .type = FE_QAM,
		 .frequency_min = 47400000,
		.frequency_max = 862000000,
		 .frequency_stepsize = 62500,
		 .symbol_rate_min = 3600000,
		 .symbol_rate_max = 71400000,
		 .caps = FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 |
		 FE_CAN_QAM_128 | FE_CAN_QAM_256 | FE_CAN_FEC_AUTO
	},

	.release = aml_fe_dvb_release,

	.init = aml_fe_dvbc_init,
	.sleep = aml_fe_dvb_sleep,
	.i2c_gate_ctrl = aml_fe_dvb_i2c_gate_ctrl,

	.set_frontend = aml_fe_dvbc_set_frontend,
	.get_frontend = aml_fe_dvbc_get_frontend,

	.read_status = aml_fe_dvbc_read_status,
	.read_ber = aml_fe_dvbc_read_ber,
	.read_signal_strength =aml_fe_dvbc_read_signal_strength,
	.read_snr = aml_fe_dvbc_read_snr,
	.read_ucblocks = aml_fe_dvb_read_ucblocks,
};

static struct dvb_frontend_ops aml_fe_dvbt_ops = {

	.info = {
		 .name = "AMLOGIC DVB-T",
		.type = FE_OFDM,
		.frequency_min = 51000000,
		.frequency_max = 858000000,
		.frequency_stepsize = 166667,
		.frequency_tolerance = 0,
		.caps =
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 |
			FE_CAN_QAM_64 | FE_CAN_QAM_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO |
			FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER |
			FE_CAN_MUTE_TS
	},

	.release = aml_fe_dvb_release,

	.init = aml_fe_dvbt_init,
	.sleep = aml_fe_dvb_sleep,
	.i2c_gate_ctrl = aml_fe_dvb_i2c_gate_ctrl,

	.set_frontend = aml_fe_dvbt_set_frontend,
	.get_frontend = aml_fe_dvbt_get_frontend,

	.read_status = aml_fe_dvbt_read_status,
	.read_ber = aml_fe_dvbt_read_ber,
	.read_signal_strength =aml_fe_dvbt_read_signal_strength,
	.read_snr = aml_fe_dvbt_read_snr,
	.read_ucblocks = aml_fe_dvb_read_ucblocks,
};


static int amdemod_fe_cfg_get(struct aml_dvb *advb, struct platform_device *pdev, int id, struct amlfe_config **pcfg)
{
	int ret=0;
	struct resource *res;
	char buf[32];
	struct amlfe_config *cfg;

	cfg = kzalloc(sizeof(struct amlfe_config), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	cfg->fe_mode = frontend_mode;
	if(cfg->fe_mode == -1) {
		snprintf(buf, sizeof(buf), "frontend%d_mode", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			ret = -EINVAL;
			goto err_resource;
		}
		cfg->fe_mode = res->start;
		frontend_mode = cfg->fe_mode;
	}
	
	cfg->i2c_id = frontend_i2c;
	if(cfg->i2c_id==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_i2c", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			ret = -EINVAL;
			goto err_resource;
		}
		cfg->i2c_id = res->start;
		frontend_i2c = cfg->i2c_id;
	}
	
	cfg->tuner_type = frontend_tuner;
	if(cfg->tuner_type == -1) {
		snprintf(buf, sizeof(buf), "frontend%d_tuner", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			ret = -EINVAL;
			goto err_resource;
		}
		cfg->tuner_type = res->start;
		frontend_tuner = cfg->tuner_type;
	}
	
	cfg->tuner_addr = frontend_tuner_addr;
	if(cfg->tuner_addr==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_tuner_addr", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			ret = -EINVAL;
			goto err_resource;
		}
		cfg->tuner_addr = res->start>>1;
		frontend_tuner_addr = cfg->tuner_addr;
	}

	*pcfg = cfg;
	
	return 0;

err_resource:
	kfree(cfg);
	return ret;

}

static void amdemod_fe_cfg_put(struct amlfe_config *cfg)
{
	if(cfg) 
		kfree(cfg);
}

static int amdemod_fe_register(struct aml_dvb *advb, struct aml_fe *fe, struct amlfe_config *cfg)
{
	int ret=0;
	struct dvb_frontend_ops *ops;

	if(cfg->fe_mode == 0)
		fe->fe = aml_fe_dvbc_attach(cfg);
	else if(cfg->fe_mode == 1)
		fe->fe = aml_fe_dvbt_attach(cfg);
	
	if (!fe->fe) {
		return -ENOMEM;
	}
	
	if ((ret=dvb_register_frontend(&advb->dvb_adapter, fe->fe))) {
		pr_error("frontend registration failed!");
		ops = &fe->fe->ops;
		if (ops->release != NULL)
			ops->release(fe->fe);
		fe->fe = NULL;
	}
	return ret;
}

static void amdemod_fe_release(struct aml_dvb *advb, struct aml_fe *fe)
{
	if(fe && fe->fe) {
		pr_dbg("release AML frontend %d\n", fe->id);
		dvb_unregister_frontend(fe->fe);
		dvb_frontend_detach(fe->fe);
		amdemod_fe_cfg_put(fe->cfg);
		fe->fe = NULL;
		fe->cfg = NULL;
		fe->id = -1;
	}
}


static int amdemod_fe_init(struct aml_dvb *advb, struct platform_device *pdev, struct aml_fe *fe, int id)
{
	int ret=0;
	struct amlfe_config *cfg = NULL;
	
	pr_dbg("init Amdemod frontend %d\n", id);

	ret = amdemod_fe_cfg_get(advb, pdev, id, &cfg);
	if(ret != 0){
		goto err_init;
	}

	ret = amdemod_fe_register(advb, fe, cfg);
	if(ret != 0){
		goto err_init;
	}

	fe->id = id;
	fe->cfg = cfg;
	
	return 0;

err_init:
	amdemod_fe_cfg_put(cfg);
	return ret;

}

static int amlfe_change_mode(struct aml_fe *fe, int mode)
{
	int ret;
	struct aml_dvb *dvb = aml_get_dvb_device();
	struct amlfe_config *cfg = (struct amlfe_config *)fe->cfg;

	if(fe->fe)
	{
		dvb_unregister_frontend(fe->fe);
		dvb_frontend_detach(fe->fe);
	}
	
	cfg->fe_mode = mode;
	
	ret = amdemod_fe_register(dvb, fe, cfg);
	if(ret < 0) {
	}
		
	return ret;
}

static ssize_t amlfe_show_mode(struct class *class, struct class_attribute *attr,char *buf)
{
	int ret;
	struct aml_fe *fe = container_of(class,struct aml_fe,class);
	struct amlfe_config *cfg = (struct amlfe_config *)fe->cfg;

	ret = sprintf(buf, ":%d\n0-DVBC, 1-DVBT\n", cfg->fe_mode);
	return ret;
}

static ssize_t amlfe_store_mode(struct class *class,struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
	int mode = simple_strtol(buf,0,16);

	if(mode<0 || mode>1){
		pr_error("fe mode must 0~2\n");
	}else{
		if(frontend_mode != mode){
			struct aml_fe *fe = container_of(class,struct aml_fe,class);
			if(amlfe_change_mode(fe, mode)!=0){
				pr_error("fe mode change failed.\n");
			}else{
				frontend_mode = mode;
			}
		}
	}
	
	return size;
}

static struct class_attribute amlfe_class_attrs[] = {
	__ATTR(mode,  S_IRUGO | S_IWUSR, amlfe_show_mode, amlfe_store_mode),	
	__ATTR_NULL
};

static int amlfe_register_class(struct aml_fe *fe)
{
#define CLASS_NAME_LEN 48
	int ret;
	struct class *clp;
	
	clp = &(fe->class);

	clp->name = kzalloc(CLASS_NAME_LEN,GFP_KERNEL);
	if (!clp->name)
		return -ENOMEM;
	
	snprintf((char *)clp->name, CLASS_NAME_LEN, "amlfe-%d", fe->id);
	clp->owner = THIS_MODULE;
	clp->class_attrs = amlfe_class_attrs;
	ret = class_register(clp);
	if (ret)
		kfree(clp->name);
	
	return 0;
}

static int amlfe_unregister_class(struct aml_fe *fe)
{
	class_unregister(&fe->class);
	kzfree(fe->class.name);
	return 0;
}

static int amdemod_fe_probe(struct platform_device *pdev)
{
	struct aml_dvb *dvb = aml_get_dvb_device();

	pr_dbg("amdemod_fe_probe \n");
	
	if(amdemod_fe_init(dvb, pdev, &amdemod_fe[0], 0)<0)
		return -ENXIO;

	platform_set_drvdata(pdev, &amdemod_fe[0]);

	amlfe_register_class(&amdemod_fe[0]);
	
	return 0;
}

static int amdemod_fe_remove(struct platform_device *pdev)
{
	struct aml_fe *fe = platform_get_drvdata(pdev);
	struct aml_dvb *dvb = aml_get_dvb_device();

	platform_set_drvdata(pdev, NULL);

	amlfe_unregister_class(fe);
	
	amdemod_fe_release(dvb, fe);
	
	return 0;
}

static struct platform_driver aml_fe_driver = {
	.probe		= amdemod_fe_probe,
	.remove		= amdemod_fe_remove,	
	.driver		= {
		.name	= "amlfe",
		.owner	= THIS_MODULE,
	}
};

static int __init amlfrontend_init(void)
{
	pr_dbg("Amlogic Demod DVB-T/C Init\n");
	return platform_driver_register(&aml_fe_driver);
}


static void __exit amlfrontend_exit(void)
{
	pr_dbg("Amlogic Demod DVB-T/C Exit\n");
	platform_driver_unregister(&aml_fe_driver);
}

 late_initcall(amlfrontend_init);
module_exit(amlfrontend_exit);


MODULE_DESCRIPTION("AML DEMOD DVB-C/T Demodulator driver");
MODULE_LICENSE("GPL");


