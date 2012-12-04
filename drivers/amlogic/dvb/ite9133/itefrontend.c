/*****************************************************************
**
**  Copyright (C) 2009 Amlogic,Inc.
**  All rights reserved
**        Filename : itefrontend.c
**
**  comment:
**        Driver for ite9133 demodulator
**  author :
**	      Shijie.Rong@amlogic
**  version :
**	      v1.0	 12/5/21
*****************************************************************/

/*
    Driver for ite9133 demodulator
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
#include <mach/am_regs.h>
#endif
#include <linux/i2c.h>
#include <linux/gpio.h>
#include "IT9133.h"
#include "itefrontend.h"

#if 0
#define pr_dbg(fmt, args...) printk( "DVB: " fmt, ## args)
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

static struct aml_fe ite9133_fe[FE_DEV_COUNT];

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

MODULE_PARM_DESC(frontend_power, "\n\t\t ANT_PWR_CTRL of frontend");
static int frontend_power = -1;
module_param(frontend_power, int, S_IRUGO);

static struct mutex ite_lock;
static struct aml_fe ite9133_fe[FE_DEV_COUNT];


StreamType streamType = StreamType_DVBT_PARALLEL;//StreamType_DVBT_SERIAL;//StreamType_DVBT_PARALLEL;//StreamType_DVBT_SERIAL;//Modified by Roan 2012-03-14

DefaultDemodulator demod = {
	NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    12000,
    2025000,
//20480,
//2048000,
    StreamType_DVBT_PARALLEL,//StreamType_DVBT_PARALLEL,//StreamType_DVBT_SERIAL,//Modified by Roan 2012-03-14
    8000,
    642000,
    0x00000000,
	{False, False, 0, 0},
    0,
    False,
    False,
	0,
	User_I2C_ADDRESS,
	False
};


static Demodulator *pdemod = &demod;

static int ite9133_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	//struct ite9133_state *state = fe->demodulator_priv;

	return 0;
}

static int ite9133_init(struct dvb_frontend *fe)
{
	struct ite9133_state *state = fe->demodulator_priv;

	gpio_direction_output(frontend_reset, 0);
	msleep(300);
	gpio_direction_output(frontend_reset, 1);  //reset
	msleep(500);
//	gpio_direction_output(frontend_power, 1);  //enable tuner power

	if(Error_NO_ERROR != Demodulator_initialize (pdemod, streamType))
		return -1;

	printk("=========================demod init\r\n");

	return 0;
}

static int ite9133_sleep(struct dvb_frontend *fe)
{
	struct ite9133_state *state = fe->demodulator_priv;

	return 0;
}

static int ite9133_read_status(struct dvb_frontend *fe, fe_status_t * status)
{
	struct ite9133_state *state = fe->demodulator_priv;

	Dword ret;
	Bool locked = 0;

	pr_dbg("ite9133_read_status\n");

	mutex_lock(&ite_lock);
	ret = Demodulator_isLocked(pdemod,&locked);
	mutex_unlock(&ite_lock);

	if(locked==1) {
		*status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	} else {
		*status = FE_TIMEDOUT;
	}

	pr_dbg("ite9133_read_status--\n");
	return  0;
}

static int ite9133_read_ber(struct dvb_frontend *fe, u32 * ber)
{
	struct ite9133_state *state = fe->demodulator_priv;

	pr_dbg("ite9133_read_ber\n");

	return 0;
}

static int ite9133_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct ite9133_state *state = fe->demodulator_priv;

	pr_dbg("ite9133_read_signal_strength\n");

//	if(Error_NO_ERROR != Demodulator_getSignalStrength(pdemod,(Byte*)strength))
//		return -1;

	return 0;
}

static int ite9133_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct ite9133_state *state = fe->demodulator_priv;

	pr_dbg("ite9133_read_snr\n");

	mutex_lock(&ite_lock);
	if(Error_NO_ERROR != Demodulator_getSNR(pdemod,(Byte*)snr))
;
	mutex_unlock(&ite_lock);
		//return -1;

	pr_dbg("ite9133_read_snr--\n");
	return 0;
}

static int ite9133_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks)
{

	pr_dbg("ite9133_read_ucblocks\n");

	ucblocks=NULL;

	return 0;
}

static int ite9133_set_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{

	pr_dbg("ite9133_set_frontend\n");

	struct ite9133_state *state = fe->demodulator_priv;

	Dword ret;
	Word bandwidth=8;

	bandwidth=p->u.ofdm.bandwidth;
	if(bandwidth==0)
		bandwidth=8;
	else if(bandwidth==1)
		bandwidth=7;
	else if(bandwidth==2)
		bandwidth=6;
	else
		bandwidth=8;	

	state->freq=(p->frequency/1000);

	pr_dbg("state->freq ==== %d \n", state->freq);
	if(state->freq>0&&state->freq!=-1) {
		mutex_lock(&ite_lock);
		ret = Demodulator_acquireChannel(pdemod, bandwidth*1000,state->freq);
		mutex_unlock(&ite_lock);
	}else
		printk("\n--[xsw]: Invalidate Fre!!!!!!!!!!!!!--\n");

	pr_dbg("ite9133_set_frontend--\n");
	return  0;
}

static int ite9133_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{//these content will be writed into eeprom .

	pr_dbg("ite9133_get_frontend\n");

	struct ite9133_state *state = fe->demodulator_priv;

	p->frequency=1000*state->freq;

	return 0;
}

static void ite9133_release(struct dvb_frontend *fe)
{
	struct ite9133_state *state = fe->demodulator_priv;

	pr_dbg("ite9133_release\n");

//	demod_deinit(state);
	kfree(state);
}

static struct dvb_frontend_ops ite9133_ops;

struct dvb_frontend *ite9133_attach(const struct ite9133_fe_config *config)
{
	struct ite9133_state *state = NULL;

	/* allocate memory for the internal state */

	state = kmalloc(sizeof(struct ite9133_state), GFP_KERNEL);
	if (state == NULL)
		return NULL;

	/* setup the state */
	state->config = *config;

	/* create dvb_frontend */
	memcpy(&state->fe.ops, &ite9133_ops, sizeof(struct dvb_frontend_ops));
	state->fe.demodulator_priv = state;

	return &state->fe;
}


static struct dvb_frontend_ops ite9133_ops = {

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

	.release = ite9133_release,

	.init = ite9133_init,
	.sleep = ite9133_sleep,
	.i2c_gate_ctrl = ite9133_i2c_gate_ctrl,

	.set_frontend = ite9133_set_frontend,
	.get_frontend = ite9133_get_frontend,

	.read_status = ite9133_read_status,
	.read_ber = ite9133_read_ber,
	.read_signal_strength =ite9133_read_signal_strength,
	.read_snr = ite9133_read_snr,
	.read_ucblocks = ite9133_read_ucblocks,
};


static void ite9133_fe_release(struct aml_dvb *advb, struct aml_fe *fe)
{
	if(fe && fe->fe) {
		pr_dbg("release ite9133 frontend %d\n", fe->id);
		mutex_destroy(&ite_lock);
		dvb_unregister_frontend(fe->fe);
		dvb_frontend_detach(fe->fe);
		if(fe->cfg){
			kfree(fe->cfg);
			fe->cfg = NULL;
		}
		fe->id = -1;
	}
}

static int ite9133_fe_init(struct aml_dvb *advb, struct platform_device *pdev, struct aml_fe *fe, int id)
{
	struct dvb_frontend_ops *ops;
	int ret;
	struct resource *res;
	struct ite9133_fe_config *cfg;
	char buf[32];

	pr_dbg("init ite9133 frontend %d\n", id);

	cfg = kzalloc(sizeof(struct ite9133_fe_config), GFP_KERNEL);
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

	if(frontend_power==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_power", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			ret = -EINVAL;
			goto err_resource;
		}
		frontend_power = res->start;
	}

	frontend_reset = cfg->reset_pin;
	frontend_i2c = cfg->i2c_id;
	frontend_tuner_addr = cfg->tuner_addr;
	frontend_demod_addr = cfg->demod_addr;

	fe->fe = ite9133_attach(cfg); 
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

	mutex_init(&ite_lock);
pr_dbg("ite9133_ite9133_fe_init--%d\n", ite_lock);
	return 0;

err_resource:
	kfree(cfg);
	return ret;
}

int ite9133_get_fe_config(struct  ite9133_fe_config *cfg)
{
	struct i2c_adapter *i2c_handle;
	cfg->i2c_id = frontend_i2c;
	cfg->demod_addr = frontend_demod_addr;
	cfg->tuner_addr = frontend_tuner_addr;
	cfg->reset_pin = frontend_reset;

	i2c_handle = i2c_get_adapter(cfg->i2c_id);
	if (!i2c_handle) {
		printk("cannot get i2c adaptor\n");
		return 0;
	}
	cfg->i2c_adapter =i2c_handle;
	return 1;
}

static int ite9133_fe_probe(struct platform_device *pdev)
{
	struct aml_dvb *dvb = aml_get_dvb_device();

	if(ite9133_fe_init(dvb, pdev, &ite9133_fe[0], 0)<0)
		return -ENXIO;

	platform_set_drvdata(pdev, &ite9133_fe[0]);

	return 0;
}

static int ite9133_fe_remove(struct platform_device *pdev)
{
	struct aml_fe *drv_data = platform_get_drvdata(pdev);
	struct aml_dvb *dvb = aml_get_dvb_device();

	platform_set_drvdata(pdev, NULL);

	ite9133_fe_release(dvb, drv_data);

	return 0;
}

static int ite9133_fe_resume(struct platform_device *pdev)
{
	pr_dbg("ite9133_fe_resume \n");
	gpio_direction_output(frontend_reset, 0);
	msleep(300);
	gpio_direction_output(frontend_reset, 1);  //reset
	msleep(500);
//	gpio_direction_output(frontend_power, 1);  //enable tuner power

	if(Error_NO_ERROR != Demodulator_initialize (pdemod, streamType))
		return -1;

	printk("ite9133_fe_resume\n");

	return 0;


}

static int ite9133_fe_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}



static struct platform_driver aml_fe_driver = {
	.probe		= ite9133_fe_probe,
	.remove		= ite9133_fe_remove,
	.resume		= ite9133_fe_resume,
	.suspend	= ite9133_fe_suspend,
	.driver		= {
		.name	= "ite9133",
		.owner	= THIS_MODULE,
	}
};

static int __init itefrontend_init(void)
{
	return platform_driver_register(&aml_fe_driver);
}


static void __exit itefrontend_exit(void)
{
	platform_driver_unregister(&aml_fe_driver);
}

module_init(itefrontend_init);
module_exit(itefrontend_exit);


MODULE_DESCRIPTION("ite9133 DVB-T demodulator driver");
MODULE_AUTHOR("RSJ");
MODULE_LICENSE("GPL");


