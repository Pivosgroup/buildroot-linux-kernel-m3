/*****************************************************************
**
**  Copyright (C) 2009 Amlogic,Inc.
**  All rights reserved
**        Filename : gxfrontend.c
**
**  comment:
**        Driver for GX1001 demodulator
**  author :
**	    jianfeng_wang@amlogic
**  version :
**	    v1.0	 09/03/04
*****************************************************************/

/*
    Driver for GX1001 demodulator
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
#include <mach/am_regs.h>
#endif
#include <linux/i2c.h>
#include <linux/gpio.h>
#include "gx1001.h"

#if 1
#define pr_dbg(fmt, args...) printk( KERN_DEBUG"DVB: " fmt, ## args)
#else
#define pr_dbg(fmt, args...)
#endif

#define pr_error(fmt, args...) printk( KERN_ERR"DVB: " fmt, ## args)

MODULE_PARM_DESC(frontend0_reset, "\n\t\t Reset GPIO of frontend0");
static int frontend0_reset = -1;
module_param(frontend0_reset, int, S_IRUGO);

MODULE_PARM_DESC(frontend0_i2c, "\n\t\t IIc adapter id of frontend0");
static int frontend0_i2c = -1;
module_param(frontend0_i2c, int, S_IRUGO);

MODULE_PARM_DESC(frontend0_tuner_addr, "\n\t\t Tuner IIC address of frontend0");
static int frontend0_tuner_addr = -1;
module_param(frontend0_tuner_addr, int, S_IRUGO);

MODULE_PARM_DESC(frontend0_demod_addr, "\n\t\t Demod IIC address of frontend0");
static int frontend0_demod_addr = -1;
module_param(frontend0_demod_addr, int, S_IRUGO);

static struct aml_fe gx1001_fe[FE_DEV_COUNT];

MODULE_PARM_DESC(frontend_reset, "\n\t\t Reset GPIO of frontend");
static int frontend_reset = -1;
module_param(frontend_reset, int, S_IRUGO);

MODULE_PARM_DESC(frontend_i2c, "\n\t\t IIc adapter id of frontend");
static int frontend_i2c = -1;
module_param(frontend_i2c, int, S_IRUGO);

MODULE_PARM_DESC(frontend_tuner_addr, "\n\t\t Tuner IIC address of frontend");
static int frontend_tuner_addr = -1;
module_param(frontend_tuner_addr, int, S_IRUGO);

MODULE_PARM_DESC(frontend_demod_addr, "\n\t\t Demod IIC address of frontend");
static int frontend_demod_addr = -1;
module_param(frontend_demod_addr, int, S_IRUGO);

static struct aml_fe gx1001_fe[FE_DEV_COUNT];

static int gx1001_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	//struct gx1001_state *state = fe->demodulator_priv;

	return 0;
}

static int gx1001_init(struct dvb_frontend *fe)
{
	struct gx1001_state *state = fe->demodulator_priv;
	
	//GX_Init_Chip();
	pr_dbg("=========================demod init\r\n");
	demod_init(state);
	msleep(200);
	return 0;
}

static int gx1001_sleep(struct dvb_frontend *fe)
{
	struct gx1001_state *state = fe->demodulator_priv;

	GX_Set_Sleep(state, 1);

	return 0;
}

static int gx1001_read_status(struct dvb_frontend *fe, fe_status_t * status)
{
	struct gx1001_state *state = fe->demodulator_priv;
	unsigned char s=0;
	
	demod_check_locked(state, &s);
	if(s==1)
	{
		*status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	}
	else
	{
		*status = FE_TIMEDOUT;
	}
	
	return  0;
}

static int gx1001_read_ber(struct dvb_frontend *fe, u32 * ber)
{
	struct gx1001_state *state = fe->demodulator_priv;
	
	demod_get_signal_errorate(state, ber);
	return 0;
}

static int gx1001_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct gx1001_state *state = fe->demodulator_priv;
	uint   rec_strength;
	
	demod_get_signal_strength(state, &rec_strength);
	*strength=rec_strength;
	return 0;
}

static int gx1001_read_snr(struct dvb_frontend *fe, u16 * snr)
{
	struct gx1001_state *state = fe->demodulator_priv;
	uint   rec_snr ;
	
	demod_get_signal_quality(state, &rec_snr) ;
	*snr = rec_snr;//>>16;		
	return 0;
}

static int gx1001_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks)
{
	ucblocks=NULL;
	return 0;
}

static int gx1001_set_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	struct gx1001_state *state = fe->demodulator_priv;
	
	demod_connect(state, p->frequency,p->u.qam.modulation,p->u.qam.symbol_rate);
	state->freq=p->frequency;
	state->mode=p->u.qam.modulation ;
	state->symbol_rate=p->u.qam.symbol_rate; //these data will be writed to eeprom
	
	pr_dbg("gx1001=>frequency=%d,symbol_rate=%d\r\n",p->frequency,p->u.qam.symbol_rate);
	return  0;
}

static int gx1001_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{//these content will be writed into eeprom .

	struct gx1001_state *state = fe->demodulator_priv;
	
	p->frequency=state->freq;
	p->u.qam.modulation=state->mode;
	p->u.qam.symbol_rate=state->symbol_rate;
	
	return 0;
}

static void gx1001_release(struct dvb_frontend *fe)
{
	struct gx1001_state *state = fe->demodulator_priv;
	
	demod_deinit(state);
	kfree(state);
}

static struct dvb_frontend_ops gx1001_ops;

struct dvb_frontend *gx1001_attach(const struct gx1001_fe_config *config)
{
	struct gx1001_state *state = NULL;

	/* allocate memory for the internal state */
	
	state = kmalloc(sizeof(struct gx1001_state), GFP_KERNEL);
	if (state == NULL)
		return NULL;

	/* setup the state */
	state->config = *config;
	
	/* create dvb_frontend */
	memcpy(&state->fe.ops, &gx1001_ops, sizeof(struct dvb_frontend_ops));
	state->fe.demodulator_priv = state;
	
	return &state->fe;
}

static struct dvb_frontend_ops gx1001_ops = {

	.info = {
		 .name = "AMLOGIC GX1001  DVB-C",
		 .type = FE_QAM,
		 .frequency_min = 1000,
		 .frequency_max = 1000000,
		 .frequency_stepsize = 62500,
		 .symbol_rate_min = 870000,
		 .symbol_rate_max = 11700000,
		 .caps = FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 |
		 FE_CAN_QAM_128 | FE_CAN_QAM_256 | FE_CAN_FEC_AUTO},

	.release = gx1001_release,

	.init = gx1001_init,
	.sleep = gx1001_sleep,
	.i2c_gate_ctrl = gx1001_i2c_gate_ctrl,

	.set_frontend = gx1001_set_frontend,
	.get_frontend = gx1001_get_frontend,

	.read_status = gx1001_read_status,
	.read_ber = gx1001_read_ber,
	.read_signal_strength =gx1001_read_signal_strength,
	.read_snr = gx1001_read_snr,
	.read_ucblocks = gx1001_read_ucblocks,
};

static void gx1001_fe_release(struct aml_dvb *advb, struct aml_fe *fe)
{
	if(fe && fe->fe) {
		pr_dbg("release GX1001 frontend %d\n", fe->id);
		dvb_unregister_frontend(fe->fe);
		dvb_frontend_detach(fe->fe);
		if(fe->cfg){
			kfree(fe->cfg);
			fe->cfg = NULL;
		}
		fe->id = -1;
	}
}

static int gx1001_fe_init(struct aml_dvb *advb, struct platform_device *pdev, struct aml_fe *fe, int id)
{
	struct dvb_frontend_ops *ops;
	int ret;
	struct resource *res;
	struct gx1001_fe_config *cfg;
	char buf[32];
	
	pr_dbg("init GX1001 frontend %d\n", id);


	cfg = kzalloc(sizeof(struct gx1001_fe_config), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;
	
	cfg->reset_pin = frontend_reset;
	if(cfg->reset_pin==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_reset", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			ret = -EINVAL;
			goto err_resource;
		}
		cfg->reset_pin = res->start;		
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
	}
	
	cfg->demod_addr = frontend_demod_addr;
	if(cfg->demod_addr==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_demod_addr", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			ret = -EINVAL;
			goto err_resource;
		}
		cfg->demod_addr = res->start>>1;
	}
	
	fe->fe = gx1001_attach(cfg);
	if (!fe->fe) {
		ret = -ENOMEM;
		goto err_resource;
	}

	if ((ret=dvb_register_frontend(&advb->dvb_adapter, fe->fe))) {
		pr_error("frontend registration failed!");
		ops = &fe->fe->ops;
		if (ops->release != NULL)
			ops->release(fe->fe);
		fe->fe = NULL;
		goto err_resource;
	}
	
	fe->id = id;
	fe->cfg = cfg;
	
	return 0;

err_resource:
	kfree(cfg);
	return ret;
}

static int gx1001_fe_probe(struct platform_device *pdev)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	
	if(gx1001_fe_init(dvb, pdev, &gx1001_fe[0], 0)<0)
		return -ENXIO;

	platform_set_drvdata(pdev, &gx1001_fe[0]);
	
	return 0;
}

static int gx1001_fe_remove(struct platform_device *pdev)
{
	struct aml_fe *drv_data = platform_get_drvdata(pdev);
	struct aml_dvb *dvb = aml_get_dvb_device();

	platform_set_drvdata(pdev, NULL);
	
	gx1001_fe_release(dvb, drv_data);
	
	return 0;
}

static struct platform_driver aml_fe_driver = {
	.probe		= gx1001_fe_probe,
	.remove		= gx1001_fe_remove,	
	.driver		= {
		.name	= "gx1001",
		.owner	= THIS_MODULE,
	}
};

static int __init gxfrontend_init(void)
{
	return platform_driver_register(&aml_fe_driver);
}


static void __exit gxfrontend_exit(void)
{
	platform_driver_unregister(&aml_fe_driver);
}

module_init(gxfrontend_init);
module_exit(gxfrontend_exit);


MODULE_DESCRIPTION("GX1001 DVB-C Demodulator driver");
MODULE_AUTHOR("Dennis Noermann and Andrew de Quincey");
MODULE_LICENSE("GPL");


