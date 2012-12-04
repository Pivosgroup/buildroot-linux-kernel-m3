/*****************************************************************
**
**  Copyright (C) 2009 Amlogic,Inc.
**  All rights reserved
**        Filename : mxlfrontend.c
**
**  comment:
**        Driver for MXL101 demodulator
**  author :
**	    Shijie.Rong@amlogic
**  version :
**	    v1.0	 12/3/13
*****************************************************************/

/*
    Driver for MXL101 demodulator
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
#include "demod_MxL101SF.h"

#if 1
#define pr_dbg	printk
//#define pr_dbg(fmt, args...) printk( KERN_DEBUG"DVB: " fmt, ## args)
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

static struct aml_fe mxl101_fe[FE_DEV_COUNT];

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

static struct aml_fe mxl101_fe[FE_DEV_COUNT];

static int mxl101_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	//struct mxl101_state *state = fe->demodulator_priv;

	return 0;
}

static int mxl101_init(struct dvb_frontend *fe)
{
	struct mxl101_state *state = fe->demodulator_priv;
	pr_dbg("=========================demod init\r\n");
	gpio_direction_output(frontend_reset, 0);
	msleep(300);
	gpio_direction_output(frontend_reset, 1); //enable tuner power
	msleep(200);
	MxL101SF_Init();
	return 0;
}

static int mxl101_sleep(struct dvb_frontend *fe)
{
	struct mxl101_state *state = fe->demodulator_priv;
	return 0;
}

static int mxl101_read_status(struct dvb_frontend *fe, fe_status_t * status)
{
	struct mxl101_state *state = fe->demodulator_priv;
	unsigned char s=0;
	int ber,snr,strength;
	//msleep(1000);
	s=MxL101SF_GetLock();
	/*ber=MxL101SF_GetBER();
	snr=MxL101SF_GetSNR();
	strength=MxL101SF_GetSigStrength();
	printk("ber is %d,snr is%d,strength is%d\n",ber,snr,strength);
	s=1;*/
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

static int mxl101_read_ber(struct dvb_frontend *fe, u32 * ber)
{
	struct mxl101_state *state = fe->demodulator_priv;
	
	*ber=MxL101SF_GetBER();
	return 0;
}

static int mxl101_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct mxl101_state *state = fe->demodulator_priv;	
	*strength=MxL101SF_GetSigStrength();
	return 0;
}

static int mxl101_read_snr(struct dvb_frontend *fe, u16 * snr)
{
	struct mxl101_state *state = fe->demodulator_priv;
	*snr=MxL101SF_GetSNR() ;		
	return 0;
}

static int mxl101_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks)
{
	ucblocks=NULL;
	return 0;
}

static int mxl101_set_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	struct mxl101_state *state = fe->demodulator_priv;
	UINT8 bandwidth=8;
	bandwidth=p->u.ofdm.bandwidth;
	if(bandwidth==0)
		bandwidth=8;
	else if(bandwidth==1)
		bandwidth=7;
	else if(bandwidth==2)
		bandwidth=6;
	else
		bandwidth=8;	
	MxL101SF_Tune(p->frequency,bandwidth);
//	demod_connect(state, p->frequency,p->u.qam.modulation,p->u.qam.symbol_rate);
	state->freq=p->frequency;
	state->mode=p->u.qam.modulation ;
	state->symbol_rate=p->u.qam.symbol_rate; //these data will be writed to eeprom
//	Mxl101SF_Debug();
//	pr_dbg("mxl101=>frequency=%d,symbol_rate=%d\r\n",p->frequency,p->u.qam.symbol_rate);
	return  0;
}

static int mxl101_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{//these content will be writed into eeprom .

	struct mxl101_state *state = fe->demodulator_priv;
	
	p->frequency=state->freq;
	p->u.qam.modulation=state->mode;
	p->u.qam.symbol_rate=state->symbol_rate;
	
	return 0;
}

static void mxl101_release(struct dvb_frontend *fe)
{
	struct mxl101_state *state = fe->demodulator_priv;
	
//	demod_deinit(state);
	kfree(state);
}

static struct dvb_frontend_ops mxl101_ops;

struct dvb_frontend *mxl101_attach(const struct mxl101_fe_config *config)
{
	struct mxl101_state *state = NULL;

	/* allocate memory for the internal state */
	
	state = kmalloc(sizeof(struct mxl101_state), GFP_KERNEL);
	if (state == NULL)
		return NULL;

	/* setup the state */
	state->config = *config;
	
	/* create dvb_frontend */
	memcpy(&state->fe.ops, &mxl101_ops, sizeof(struct dvb_frontend_ops));
	state->fe.demodulator_priv = state;
	
	return &state->fe;
}

static struct dvb_frontend_ops mxl101_ops = {


		.info = {
		 .name = "AMLOGIC DVB-T",
		.type = FE_OFDM,
		.frequency_min = 51000000,
		.frequency_max = 858000000,
		.frequency_stepsize = 0,
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

	.release = mxl101_release,

	.init = mxl101_init,
	.sleep = mxl101_sleep,
	.i2c_gate_ctrl = mxl101_i2c_gate_ctrl,

	.set_frontend = mxl101_set_frontend,
	.get_frontend = mxl101_get_frontend,	
	.read_status = mxl101_read_status,
	.read_ber = mxl101_read_ber,
	.read_signal_strength =mxl101_read_signal_strength,
	.read_snr = mxl101_read_snr,
	.read_ucblocks = mxl101_read_ucblocks,

};

static void mxl101_fe_release(struct aml_dvb *advb, struct aml_fe *fe)
{
	if(fe && fe->fe) {
		pr_dbg("release MXL101 frontend %d\n", fe->id);
		dvb_unregister_frontend(fe->fe);
		dvb_frontend_detach(fe->fe);
		if(fe->cfg){
			kfree(fe->cfg);
			fe->cfg = NULL;
		}
		fe->id = -1;
	}
}

static int mxl101_fe_init(struct aml_dvb *advb, struct platform_device *pdev, struct aml_fe *fe, int id)
{
	struct dvb_frontend_ops *ops;
	int ret;
	struct resource *res;
	struct mxl101_fe_config *cfg;
	char buf[32];
	
	pr_dbg("init MXL101 frontend %d\n", id);


	cfg = kzalloc(sizeof(struct mxl101_fe_config), GFP_KERNEL);
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
	frontend_reset = cfg->reset_pin;
	frontend_i2c = cfg->i2c_id;
	frontend_tuner_addr = cfg->tuner_addr;
	frontend_demod_addr = cfg->demod_addr;	
	fe->fe = mxl101_attach(cfg);
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

int mxl101_get_fe_config(struct mxl101_fe_config *cfg)
{
	struct i2c_adapter *i2c_handle;
	cfg->i2c_id = frontend_i2c;
	cfg->demod_addr = frontend_demod_addr;
	cfg->tuner_addr = frontend_tuner_addr;
	cfg->reset_pin = frontend_reset;
	//printk("\n frontend_i2c is %d,,frontend_demod_addr is %x,frontend_tuner_addr is %x,frontend_reset is %d",frontend_i2c,frontend_demod_addr,frontend_tuner_addr,frontend_reset);
	i2c_handle = i2c_get_adapter(cfg->i2c_id);
	if (!i2c_handle) {
		printk("cannot get i2c adaptor\n");
		return 0;
	}
	cfg->i2c_adapter =i2c_handle;
	return 1;
	
//	printk("\n frontend0_i2c is %d, frontend_i2c is %d,",frontend0_i2c,frontend_i2c);
	
}

static int mxl101_fe_probe(struct platform_device *pdev)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	
	if(mxl101_fe_init(dvb, pdev, &mxl101_fe[0], 0)<0)
		return -ENXIO;

	platform_set_drvdata(pdev, &mxl101_fe[0]);
	
	return 0;
}

static int mxl101_fe_remove(struct platform_device *pdev)
{
	struct aml_fe *drv_data = platform_get_drvdata(pdev);
	struct aml_dvb *dvb = aml_get_dvb_device();

	platform_set_drvdata(pdev, NULL);
	
	mxl101_fe_release(dvb, drv_data);
	
	return 0;
}

static int mxl101_fe_resume(struct platform_device *pdev)
{
	printk("mxl101_fe_resume\n");
	gpio_direction_output(frontend_reset, 0);
	msleep(300);
	gpio_direction_output(frontend_reset, 1); //enable tuner power
	msleep(200);
	MxL101SF_Init();
	return 0;

}

static int mxl101_fe_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}



static struct platform_driver aml_fe_driver = {
	.probe		= mxl101_fe_probe,
	.remove		= mxl101_fe_remove,
	.resume		= mxl101_fe_resume,
	.suspend    = mxl101_fe_suspend,
	.driver		= {
		.name	= "mxl101",
		.owner	= THIS_MODULE,
	}
};

static int __init mxlfrontend_init(void)
{
	return platform_driver_register(&aml_fe_driver);
}


static void __exit mxlfrontend_exit(void)
{
	platform_driver_unregister(&aml_fe_driver);
}

module_init(mxlfrontend_init);
module_exit(mxlfrontend_exit);


MODULE_DESCRIPTION("mxl101 DVB-T Demodulator driver");
MODULE_AUTHOR("RSJ");
MODULE_LICENSE("GPL");


