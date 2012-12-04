/*
 * Linux-DVB Driver for DiBcom's DiB7090P.
 *
 * Copyright (C) Amlogic (http://www.amlogic.com/)
 *
 * This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 */
 
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <mach/gpio.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>

#include "../aml_dvb.h"
#include "dib7000p.h"
#include "dib0090.h"

#define DIB7090P 1

#define from_kernel_org

static int nim7090_debug=1;
module_param(nim7090_debug, int, 0644);
MODULE_PARM_DESC(nim7090p_debug, "turn on debugging (default: 0)");

#define pr_dbg(fmt, args...) do{if(nim7090_debug) printk("NIM7090: " fmt "\n", ## args);}while(0)
#define pr_error(fmt, args...) printk(KERN_ERR "NIM7090: " fmt "\n", ## args)
#define pr_warn(fmt, args...) printk(KERN_WARNING "NIM7090: " fmt "\n", ## args)

MODULE_PARM_DESC(fe_i2c, "\n\t\t IIc adapter id of frontend");
static int fe_i2c = -1;
module_param(fe_i2c, int, S_IRUGO);

MODULE_PARM_DESC(fe_demod_addr, "\n\t\t Demod IIC address of frontend");
static int fe_demod_addr = -1;
module_param(fe_demod_addr, int, S_IRUGO);

MODULE_PARM_DESC(fe_demod_rst_pin, "\n\t\t Demod reset pin of frontend");
static int fe_demod_rst_pin = -1;
module_param(fe_demod_rst_pin, int, S_IRUGO);

MODULE_PARM_DESC(fe_demod_pwr_pin, "\n\t\t Demod power pin of frontend");
static int fe_demod_pwr_pin = -1;
module_param(fe_demod_pwr_pin, int, S_IRUGO);

MODULE_PARM_DESC(fe_demod_rst_val_en, "\n\t\t Demod reset enable value of frontend");
static int fe_demod_rst_val_en = -1;
module_param(fe_demod_rst_val_en, int, S_IRUGO);

MODULE_PARM_DESC(fe_demod_pwr_val_en, "\n\t\t Demod power enable value of frontend");
static int fe_demod_pwr_val_en = -1;
module_param(fe_demod_pwr_val_en, int, S_IRUGO);

struct nim7090_adapter_state {
	int i2c_id;
	int demod_addr;
	int reset_pin;
	int reset_val_en;
	int power_pin;	
	int power_val_en;
	int (*set_param_save) (struct dvb_frontend *, struct dvb_frontend_parameters *);
};

static struct aml_fe mfe[1];

static int dib7090_fe_cfg_get(struct aml_dvb *advb, struct platform_device *pdev, int id, struct nim7090_adapter_state **pcfg)
{
	struct resource *res;
	char buf[32];
	int ret=0;

	struct nim7090_adapter_state *cfg;
	
	cfg = kzalloc(sizeof(struct nim7090_adapter_state), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	cfg->i2c_id = fe_i2c;
	if(cfg->i2c_id == -1) {
		snprintf(buf, sizeof(buf), "frontend%d_i2c", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			ret = -EINVAL;
			goto err_resource;
		}
		cfg->i2c_id = res->start;
		fe_i2c = cfg->i2c_id;
	}

	cfg->demod_addr = fe_demod_addr;
	if(cfg->demod_addr==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_demod_addr", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_error("cannot get resource \"%s\"\n", buf);
			ret = -EINVAL;
			goto err_resource;
		}
		cfg->demod_addr = res->start;
		fe_demod_addr = cfg->demod_addr;
	}
	
	cfg->reset_pin  = fe_demod_rst_pin;
	if(cfg->reset_pin==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_reset_pin", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_warn("cannot get resource \"%s\"\n", buf);
			cfg->reset_pin = 0;
		} else {
			cfg->reset_pin = res->start;
		}
		fe_demod_rst_pin = cfg->demod_addr;
	}

	cfg->reset_val_en= fe_demod_rst_val_en;
	if(cfg->reset_val_en==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_reset_value_enable", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_warn("cannot get resource \"%s\"\n", buf);
		} else {
			cfg->reset_val_en = res->start;
		}
		fe_demod_rst_val_en = cfg->reset_val_en;
	}

	cfg->power_pin  = fe_demod_pwr_pin;
	if(cfg->power_pin==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_power_pin", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_warn("cannot get resource \"%s\"\n", buf);
			cfg->power_pin = 0;
		} else {
			cfg->power_pin = res->start;
		}
		fe_demod_pwr_pin = cfg->power_pin;
	}

	cfg->power_val_en= fe_demod_pwr_val_en;
	if(cfg->power_val_en==-1) {
		snprintf(buf, sizeof(buf), "frontend%d_power_value_enable", id);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, buf);
		if (!res) {
			pr_warn("cannot get resource \"%s\"\n", buf);
		} else {
			cfg->power_val_en = res->start;
		}
		fe_demod_pwr_val_en = cfg->power_val_en;
	}

	*pcfg = cfg;
	
	return 0;

err_resource:
	kfree(cfg);
	return ret;
}

static void dib7090_fe_cfg_put(struct nim7090_adapter_state *cfg)
{
	if(cfg) 
		kfree(cfg);
}

static struct dibx000_agc_config dib7090_agc_config[2] = {
	{
		.band_caps      = BAND_UHF,
		/* P_agc_use_sd_mod1=0, P_agc_use_sd_mod2=0, P_agc_freq_pwm_div=1, P_agc_inv_pwm1=0, P_agc_inv_pwm2=0,
		* P_agc_inh_dc_rv_est=0, P_agc_time_est=3, P_agc_freeze=0, P_agc_nb_est=5, P_agc_write=0 */
		.setup          = (0 << 15) | (0 << 14) | (5 << 11) | (0 << 10) | (0 << 9) | (0 << 8) | (3 << 5) | (0 << 4) | (5 << 1) | (0 << 0),

		.inv_gain       = 687,
		.time_stabiliz  = 10,

		.alpha_level    = 0,
		.thlock         = 118,

		.wbd_inv        = 0,
		.wbd_ref        = 1200,
		.wbd_sel        = 3,
		.wbd_alpha      = 5,

		.agc1_max       = 65535,
		.agc1_min       = 0,

		.agc2_max       = 65535,
		.agc2_min       = 0,

		.agc1_pt1       = 0,
		.agc1_pt2       = 32,
		.agc1_pt3       = 114,
		.agc1_slope1    = 143,
		.agc1_slope2    = 144,
		.agc2_pt1       = 114,
		.agc2_pt2       = 227,
		.agc2_slope1    = 116,
		.agc2_slope2    = 117,

		.alpha_mant     = 18,
		.alpha_exp      = 0,
		.beta_mant      = 20,
		.beta_exp       = 59,

		.perform_agc_softsplit = 0,
	} , {
		.band_caps      = BAND_FM | BAND_VHF | BAND_CBAND,
		/* P_agc_use_sd_mod1=0, P_agc_use_sd_mod2=0, P_agc_freq_pwm_div=1, P_agc_inv_pwm1=0, P_agc_inv_pwm2=0,
		* P_agc_inh_dc_rv_est=0, P_agc_time_est=3, P_agc_freeze=0, P_agc_nb_est=5, P_agc_write=0 */
		.setup          = (0 << 15) | (0 << 14) | (5 << 11) | (0 << 10) | (0 << 9) | (0 << 8) | (3 << 5) | (0 << 4) | (5 << 1) | (0 << 0),

		.inv_gain       = 732,
		.time_stabiliz  = 10,

		.alpha_level    = 0,
		.thlock         = 118,

		.wbd_inv        = 0,
		.wbd_ref        = 1200,
		.wbd_sel        = 3,
		.wbd_alpha      = 5,

		.agc1_max       = 65535,
		.agc1_min       = 0,

		.agc2_max       = 65535,
		.agc2_min       = 0,

		.agc1_pt1       = 0,
		.agc1_pt2       = 0,
		.agc1_pt3       = 98,
		.agc1_slope1    = 0,
		.agc1_slope2    = 167,
		.agc2_pt1       = 98,
		.agc2_pt2       = 255,
		.agc2_slope1    = 104,
		.agc2_slope2    = 0,

		.alpha_mant     = 18,
		.alpha_exp      = 0,
		.beta_mant      = 20,
		.beta_exp       = 59,

		.perform_agc_softsplit = 0,
	}
};
static struct dibx000_bandwidth_config dib7090_clock_config_12_mhz = {
	60000, 15000,
	1, 5, 0, 0, 0,
	0, 0, 1, 1, 2,
	(3 << 14) | (1 << 12) | (524 << 0),
	(0 << 25) | 0,
	20452225,
	15000000,
};

static int dib7090_agc_restart(struct dvb_frontend *fe, u8 restart)
{
	pr_dbg("AGC restart callback: %d", restart);
	if (restart == 0) /* before AGC startup */
		dib0090_set_dc_servo(fe, 1);
	return 0;
}


static int dib7090e_update_lna(struct dvb_frontend *fe, u16 agc_global)
{
	u16 agc1 = 0, agc2, wbd = 0, wbd_target, wbd_offset, threshold_agc1;
	s16 wbd_delta;

	if ((fe->dtv_property_cache.frequency) < 400000000)
		threshold_agc1 = 25000;
	else
		threshold_agc1 = 30000;

	wbd_target = (dib0090_get_wbd_target(fe)*8+1)/2;
	wbd_offset = dib0090_get_wbd_offset(fe);
	dib7000p_get_agc_values(fe, NULL, &agc1, &agc2, &wbd);
	wbd_delta = (s16)wbd - (((s16)wbd_offset+10)*4) ;

	pr_dbg("update lna, agc_global=%d agc1=%d agc2=%d",
			agc_global, agc1, agc2);
	pr_dbg("update lna, wbd=%d wbd target=%d wbd offset=%d wbd delta=%d",
			wbd, wbd_target, wbd_offset, wbd_delta);

	if ((agc1 < threshold_agc1) && (wbd_delta > 0)) {
		dib0090_set_switch(fe, 1, 1, 1);
		dib0090_set_vga(fe, 0);
		dib0090_update_rframp_7090(fe, 0);
		dib0090_update_tuning_table_7090(fe, 0);
	} else {
		dib0090_set_vga(fe, 1);
		dib0090_update_rframp_7090(fe, 1);
		dib0090_update_tuning_table_7090(fe, 1);
		dib0090_set_switch(fe, 0, 0, 0);
	}

	return 0;
}


static struct dib7000p_config dib7090p_dib7000p_cfg = {
	.output_mpeg2_in_188_bytes  = 1,
	.hostbus_diversity			= 1,
	.tuner_is_baseband			= 1,
	.update_lna					= NULL,

	.agc_config_count			= 2,
	.agc						= dib7090_agc_config,

	.bw							= &dib7090_clock_config_12_mhz,

	.gpio_dir					= DIB7000P_GPIO_DEFAULT_DIRECTIONS,
	.gpio_val					= DIB7000P_GPIO_DEFAULT_VALUES,
	.gpio_pwm_pos				= DIB7000P_GPIO_DEFAULT_PWM_POS,

	.pwm_freq_div				= 0,

	.agc_control				= dib7090_agc_restart,

	.spur_protect				= 0,
	.disable_sample_and_hold	= 0,
	.enable_current_mirror		= 0,
	.diversity_delay			= 0,
#ifdef CONFIG_DIB7090_OUTPUT_SERIAL
	.output_mode				= OUTMODE_MPEG2_SERIAL,
#else
	.output_mode				= OUTMODE_MPEG2_PAR_GATED_CLK,
#endif
	.enMpegOutput				= 1,
};

static struct dib7000p_config dib7090e_dib7000p_config = {
	.output_mpeg2_in_188_bytes  = 1,
	.hostbus_diversity			= 1,
	.tuner_is_baseband			= 1,
	.update_lna					= dib7090e_update_lna,

	.agc_config_count			= 2,
	.agc						= dib7090_agc_config,

	.bw							= &dib7090_clock_config_12_mhz,

	.gpio_dir					= DIB7000P_GPIO_DEFAULT_DIRECTIONS,
	.gpio_val					= DIB7000P_GPIO_DEFAULT_VALUES,
	.gpio_pwm_pos				= DIB7000P_GPIO_DEFAULT_PWM_POS,

	.pwm_freq_div				= 0,

	.agc_control				= dib7090_agc_restart,

	.spur_protect				= 0,
	.disable_sample_and_hold	= 0,
	.enable_current_mirror		= 0,
	.diversity_delay			= 0,

#ifdef CONFIG_DIB7090_OUTPUT_SERIAL
	.output_mode				= OUTMODE_MPEG2_SERIAL,
#else
	.output_mode				= OUTMODE_MPEG2_PAR_GATED_CLK,
#endif

	.enMpegOutput				= 1,
};

static int dib7090_frontend_attach(struct aml_fe *fe, struct nim7090_adapter_state *cfg)
{

	struct i2c_adapter *adapter;

	adapter = i2c_get_adapter(cfg->i2c_id);
/*
	if (dib7000p_i2c_enumeration(adapter, 1, 0x10,
				     &dib7090p_dib7000p_cfg) != 0) {
		pr_err("%s: dib7000p_i2c_enumeration failed.  Cannot continue\n",
		    __func__);
		return -ENODEV;
	}
*/
#if DIB7090P
	fe->fe = dvb_attach(dib7000p_attach, adapter, cfg->demod_addr, &dib7090p_dib7000p_cfg);
#else
	fe->fe = dvb_attach(dib7000p_attach, adapter, cfg->demod_addr, &dib7090e_dib7000p_config);
#endif
	if (!fe->fe) {
		return -EREMOTEIO;
	}

	return fe->fe == NULL ?  -ENODEV : 0;
}

/* NIM7090 */
struct dib7090p_best_adc {
	u32 timf;
	u32 pll_loopdiv;
	u32 pll_prediv;
};

static int dib7090p_get_best_sampling(struct dvb_frontend *fe , struct dib7090p_best_adc *adc)
{
	u8 spur = 0, prediv = 0, loopdiv = 0, min_prediv = 1, max_prediv = 1;

	u16 xtal = 12000;
	u32 fcp_min = 1900;  /* PLL Minimum Frequency comparator KHz */
	u32 fcp_max = 20000; /* PLL Maximum Frequency comparator KHz */
	u32 fdem_max = 76000;
	u32 fdem_min = 69500;
	u32 fcp = 0, fs = 0, fdem = 0;
	u32 harmonic_id = 0;

	adc->pll_loopdiv = loopdiv;
	adc->pll_prediv = prediv;
	adc->timf = 0;

	pr_dbg("bandwidth = %d fdem_min =%d", fe->dtv_property_cache.bandwidth_hz, fdem_min);

	/* Find Min and Max prediv */
	while ((xtal/max_prediv) >= fcp_min)
		max_prediv++;

	max_prediv--;
	min_prediv = max_prediv;
	while ((xtal/min_prediv) <= fcp_max) {
		min_prediv--;
		if (min_prediv == 1)
			break;
	}
	pr_dbg("MIN prediv = %d : MAX prediv = %d", min_prediv, max_prediv);

	min_prediv = 2;

	for (prediv = min_prediv ; prediv < max_prediv; prediv++) {
		fcp = xtal / prediv;
		if (fcp > fcp_min && fcp < fcp_max) {
			for (loopdiv = 1 ; loopdiv < 64 ; loopdiv++) {
				fdem = ((xtal/prediv) * loopdiv);
				fs   = fdem / 4;
				/* test min/max system restrictions */

				if ((fdem >= fdem_min) && (fdem <= fdem_max) && (fs >= fe->dtv_property_cache.bandwidth_hz/1000)) {
					spur = 0;
					/* test fs harmonics positions */
					for (harmonic_id = (fe->dtv_property_cache.frequency / (1000*fs)) ;  harmonic_id <= ((fe->dtv_property_cache.frequency / (1000*fs))+1) ; harmonic_id++) {
						if (((fs*harmonic_id) >= ((fe->dtv_property_cache.frequency/1000) - (fe->dtv_property_cache.bandwidth_hz/2000))) &&  ((fs*harmonic_id) <= ((fe->dtv_property_cache.frequency/1000) + (fe->dtv_property_cache.bandwidth_hz/2000)))) {
							spur = 1;
							break;
						}
					}

					if (!spur) {
						adc->pll_loopdiv = loopdiv;
						adc->pll_prediv = prediv;
						adc->timf = 2396745143UL/fdem*(1 << 9);
						adc->timf += ((2396745143UL%fdem) << 9)/fdem;
						pr_dbg("loopdiv=%i prediv=%i timf=%i", loopdiv, prediv, adc->timf);
						break;
					}
				}
			}
		}
		if (!spur)
			break;
	}


	if (adc->pll_loopdiv == 0 && adc->pll_prediv == 0)
		return -EINVAL;
	else
		return 0;
}


static int dib7090_agc_startup(struct dvb_frontend *fe, struct dvb_frontend_parameters *fep)
{
	struct aml_dvb *dvb = fe->dvb->priv;
	struct nim7090_adapter_state *state;
	struct dibx000_bandwidth_config pll;
	u16 target;
	struct dib7090p_best_adc adc;
	int ret;
	
	int i;
	for(i=0; i<FE_DEV_COUNT; i++)
		if(fe == mfe[i].fe) break;
	if(i>=FE_DEV_COUNT) {
		printk(" *** Fatal:wrong dvb_frontend in %s ***.\n", __func__);
		return 0;
	}

	printk("dib7090_agc_startup\n");
	
	state = mfe[i].priv;

	ret = state->set_param_save(fe, fep);
	if (ret < 0)
		return ret;

	memset(&pll, 0, sizeof(struct dibx000_bandwidth_config));
	dib0090_pwm_gain_reset(fe);
	target = (dib0090_get_wbd_offset(fe) * 8 + 1) / 2;
	dib7000p_set_wbd_ref(fe, target);

	if (dib7090p_get_best_sampling(fe, &adc) == 0) {
		pll.pll_ratio  = adc.pll_loopdiv;
		pll.pll_prediv = adc.pll_prediv;

		dib7000p_update_pll(fe, &pll);
		dib7000p_ctrl_timf(fe, DEMOD_TIMF_SET, adc.timf);
	}
	return 0;
}

static int dib7090_tuner_attach(struct aml_fe *fe, struct nim7090_adapter_state *cfg)
{
	static struct dib0090_wbd_slope dib7090_wbd_table[] = {
		{ 380,   81, 850, 64, 540,  4},
		{ 860,   51, 866, 21,  375, 4},
		{1700,    0, 250, 0,   100, 6},
		{2600,    0, 250, 0,   100, 6},
		{ 0xFFFF, 0,   0, 0,   0,   0},
	};

	static struct dib0090_wbd_slope dib7090e_wbd_table[] = {
		{ 380,   81, 850, 64, 540,	4},
		{ 700,   51, 866, 21,  320,	4},
		{ 860,   48, 666, 18,  330,	6},
		{1700,    0, 250, 0,   100, 6},
		{2600,    0, 250, 0,   100, 6},
		{ 0xFFFF, 0,   0, 0,   0,	0},
	};


	static const struct dib0090_config nim7090_dib0090_config = {
		.io.clock_khz = 12000,
		.io.pll_bypass = 0,
		.io.pll_range = 0,
		.io.pll_prediv = 3,
		.io.pll_loopdiv = 6,
		.io.adc_clock_ratio = 0,
		.io.pll_int_loop_filt = 0,
		.reset = dib7090_tuner_sleep,
		.sleep = dib7090_tuner_sleep,

		.freq_offset_khz_uhf = 0,
		.freq_offset_khz_vhf = 0,

		.get_adc_power = dib7090_get_adc_power,

		.clkouttobamse = 1,
		.analog_output = 0,

		.wbd_vhf_offset = 0,
		.wbd_cband_offset = 0,
		.use_pwm_agc = 1,
		.clkoutdrive = 0,

		.fref_clock_ratio = 0,

		.wbd = dib7090_wbd_table,

		.ls_cfg_pad_drv = 0,
		.data_tx_drv = 0,
		.low_if = NULL,
		.in_soc = 1,
	};
	
	static const struct dib0090_config tfe7090e_dib0090_config = {
		.io.clock_khz = 12000,
		.io.pll_bypass = 0,
		.io.pll_range = 0,
		.io.pll_prediv = 3,
		.io.pll_loopdiv = 6,
		.io.adc_clock_ratio = 0,
		.io.pll_int_loop_filt = 0,
		.reset = dib7090_tuner_sleep,
		.sleep = dib7090_tuner_sleep,

		.freq_offset_khz_uhf = 0,
		.freq_offset_khz_vhf = 0,

		.get_adc_power = dib7090_get_adc_power,

		.clkouttobamse = 1,
		.analog_output = 0,

		.wbd_vhf_offset = 0,
		.wbd_cband_offset = 0,
		.use_pwm_agc = 1,
		.clkoutdrive = 0,

		.fref_clock_ratio = 0,

		.wbd = dib7090e_wbd_table,

		.ls_cfg_pad_drv = 0,
		.data_tx_drv = 0,
		.low_if = NULL,
		.in_soc = 1,
		.force_cband_input = 1,
		.is_dib7090e = 1,
	};

	struct i2c_adapter *tun_i2c = dib7090_get_i2c_tuner(fe->fe);
	static struct nim7090_adapter_state stat;
#if DIB7090P
	if (dvb_attach(dib0090_register, fe->fe, tun_i2c, &nim7090_dib0090_config) == NULL)
		return -ENODEV;
#else
	if (dvb_attach(dib0090_register, fe->fe, tun_i2c, &tfe7090e_dib0090_config) == NULL)
		return -ENODEV;
#endif

	dib7000p_set_gpio(fe->fe, 8, 0, 1);

	stat.set_param_save = fe->fe->ops.tuner_ops.set_params;
	fe->priv = &stat;
	fe->fe->ops.tuner_ops.set_params = dib7090_agc_startup;
	return 0;

}

static int dib7090_fe_register(struct aml_dvb *advb, struct aml_fe *fe, struct nim7090_adapter_state *cfg)
{
	int ret=0;

	ret = dib7090_frontend_attach(fe, cfg);
	if (ret == 0 && fe->fe != NULL) {	
		if ((ret=dvb_register_frontend(&advb->dvb_adapter, fe->fe))) {
			pr_error("frontend registration failed!");
			dvb_frontend_detach(fe->fe);
			fe->fe = NULL;
			return -ENODEV;
		}

		/* only attach the tuner if the demod is there */
		dib7090_tuner_attach(fe, cfg);
		
	} else {
		pr_err("no frontend was attached");
	}
	return ret;
}

static int  dib7090_power(struct nim7090_adapter_state *cfg, int enable)
{
	if(cfg && cfg->power_pin && (cfg->power_val_en!=-1)) {
		gpio_request(cfg->power_pin , "dib7090:POWER");
		pr_dbg("the power pin is %x[%x], enable[%x]",cfg->power_pin, cfg->power_val_en, enable);
		gpio_direction_output(cfg->power_pin, enable? cfg->power_val_en : !cfg->power_val_en);
	}
	return 0;
}
static int  dib7090_reset(struct nim7090_adapter_state *cfg)
{
	if(cfg && cfg->reset_pin && (cfg->reset_val_en!=-1)) {
		gpio_request(cfg->reset_pin , "dib7090:RESET");
		pr_dbg("the reset pin is %x[%x]",cfg->reset_pin, cfg->reset_val_en);
		gpio_direction_output(cfg->reset_pin, !cfg->reset_val_en);
		msleep(10);
		gpio_direction_output(cfg->reset_pin, cfg->reset_val_en);
		msleep(100);
		gpio_direction_output(cfg->reset_pin, !cfg->reset_val_en);
		msleep(10);
	}
	return 0;
}

static int dib7090_fe_init(struct aml_dvb *advb, struct platform_device *pdev, struct aml_fe *fe, int id)
{
	int ret=0;
	struct nim7090_adapter_state *cfg = NULL;
	
	pr_dbg("init dib7090 frontend %d\n", id);
	
	ret = dib7090_fe_cfg_get(advb, pdev, id, &cfg);
	if(ret != 0){
		goto err_init;
	}

	dib7090_power(cfg, 1);
	
	dib7090_reset(cfg);
	
	ret = dib7090_fe_register(advb, fe, cfg);
	if(ret != 0){
		goto err_init;
	}

	fe->id = id;
	fe->cfg = cfg;
	
	return 0;

err_init:
	dib7090_fe_cfg_put(cfg);
	return ret;

}

static void dib7090_fe_release(struct aml_dvb *advb, struct aml_fe *fe)
{
	if(fe && fe->fe) {
		pr_dbg("release fe Dib7090 %d\n", fe->id);
		dvb_unregister_frontend(fe->fe);
		dvb_frontend_detach(fe->fe);
		dib7090_fe_cfg_put(fe->cfg);
		fe->fe = NULL;
		fe->cfg = NULL;
		fe->id = -1;
	}
}

static int Dib7090_probe(struct platform_device *pdev)
{
	struct aml_dvb *dvb = aml_get_dvb_device();
	int ret = 0;

	pr_dbg("dib7090_probe \n");

	if(dib7090_fe_init(dvb, pdev, &mfe[0], 0)<0) {
		ret = -ENXIO;
		goto fail;
	}
	
	platform_set_drvdata(pdev, &mfe[0]);

	return 0;
	
fail:

	return ret;
}

static int Dib7090_remove(struct platform_device *pdev)
{
	struct aml_fe *fe = platform_get_drvdata(pdev);
	struct aml_dvb *dvb = aml_get_dvb_device();

	platform_set_drvdata(pdev, NULL);
	
	dib7090_fe_release(dvb, fe);

	return 0;
}

static struct platform_driver dib7090_fe_driver = {
	.probe		= Dib7090_probe,
	.remove		= Dib7090_remove,	
	.driver		= {
		.name	= "DiB7090P",
		.owner	= THIS_MODULE,
	}
};

static int __init dib7090_init(void)
{
	pr_dbg("Dib7090 Init\n");
	return platform_driver_register(&dib7090_fe_driver);
}


static void __exit dib7090_exit(void)
{
	pr_dbg("Dib7090 Exit\n");
	platform_driver_unregister(&dib7090_fe_driver);
}

module_init(dib7090_init);
//late_initcall(dib7090_init);
module_exit(dib7090_exit);


MODULE_AUTHOR("amlogic <?@amlogic.com>");
MODULE_DESCRIPTION("module wrapper for the DiBcom 7090P driver");
MODULE_LICENSE("GPL");
/**/

