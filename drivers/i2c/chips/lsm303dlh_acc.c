/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
*
* File Name          : lsm303dlh_acc.c
* Authors            : MSH - Motion Mems BU - Application Team
*		     : Carmine Iascone (carmine.iascone@st.com)
*		     : Matteo Dameno (matteo.dameno@st.com)
* Version            : V 1.0.1
* Date               : 19/03/2010
* Description        : LSM303DLH 6D module sensor API
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
*
******************************************************************************/

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h> 

#include <linux/i2c/lsm303dlh.h>

#define NAME			"lsm303dlh_acc"  //?? NAME of device shoudl be setup in BSP file. 

/** Maximum polled-device-reported g value */
#define G_MAX			8000

#define SHIFT_ADJ_2G		4
#define SHIFT_ADJ_4G		3
#define SHIFT_ADJ_8G		2

#define AXISDATA_REG		0x28

/* ctrl 1: pm2 pm1 pm0 dr1 dr0 zenable yenable zenable */
#define CTRL_REG1		0x20	/* power control reg */
#define CTRL_REG2		0x21	/* power control reg */
#define CTRL_REG3		0x22	/* power control reg */
#define CTRL_REG4		0x23	/* interrupt control reg */

#define FUZZ			0
#define FLAT			0
#define I2C_RETRY_DELAY		5
#define I2C_RETRIES		5
#define AUTO_INCREMENT		0x80

#define LSM303DLH_ACC_OPEN_ENABLE

static struct {
	unsigned int cutoff;
	unsigned int mask;
} odr_table[] = {
	{
	3,	LSM303DLH_ACC_PM_NORMAL | LSM303DLH_ACC_ODR1000}, {
	10,	LSM303DLH_ACC_PM_NORMAL | LSM303DLH_ACC_ODR400}, {
	20,	LSM303DLH_ACC_PM_NORMAL | LSM303DLH_ACC_ODR100}, {
	100,	LSM303DLH_ACC_PM_NORMAL | LSM303DLH_ACC_ODR50}, {
	200,	LSM303DLH_ACC_ODR1000 | LSM303DLH_ACC_ODR10}, {
	500,	LSM303DLH_ACC_ODR1000 | LSM303DLH_ACC_ODR5}, {
	1000,	LSM303DLH_ACC_ODR1000 | LSM303DLH_ACC_ODR2}, {
	2000,	LSM303DLH_ACC_ODR1000 | LSM303DLH_ACC_ODR1}, {
	0,	LSM303DLH_ACC_ODR1000 | LSM303DLH_ACC_ODRHALF},};

struct lsm303dlh_acc_data {
	struct i2c_client *client;
	struct lsm303dlh_acc_platform_data *pdata;

	struct mutex lock;

	struct delayed_work input_work;
	struct input_dev *input_dev;

	int hw_initialized;
	atomic_t enabled;
	int on_before_suspend;

	u8 shift_adj;
	u8 resume_state[5];
};

/*
 * Because misc devices can not carry a pointer from driver register to
 * open, we keep this global.  This limits the driver to a single instance.
 */
struct lsm303dlh_acc_data *lsm303dlh_acc_misc_data;

static int lsm303dlh_acc_i2c_read(struct lsm303dlh_acc_data *acc,
				  u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
		 .addr = acc->client->addr,
		 .flags = acc->client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = buf,
		 },
		{
		 .addr = acc->client->addr,
		 .flags = (acc->client->flags & I2C_M_TEN) | I2C_M_RD,
		 .len = len,
		 .buf = buf,
		 },
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&acc->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lsm303dlh_acc_i2c_write(struct lsm303dlh_acc_data *acc,
				   u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
		 .addr = acc->client->addr,
		 .flags = acc->client->flags & I2C_M_TEN,
		 .len = len + 1,
		 .buf = buf,
		 },
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&acc->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lsm303dlh_acc_hw_init(struct lsm303dlh_acc_data *acc)
{
	int err = -1;
	u8 buf[6];

	buf[0] = (AUTO_INCREMENT | CTRL_REG1);
	buf[1] = acc->resume_state[0];
	buf[2] = acc->resume_state[1];
	buf[3] = acc->resume_state[2];
	buf[4] = acc->resume_state[3];
	buf[5] = acc->resume_state[4];
	err = lsm303dlh_acc_i2c_write(acc, buf, 5);
	if (err < 0)
		return err;

	acc->hw_initialized = 1;

	return 0;
}

static void lsm303dlh_acc_device_power_off(struct lsm303dlh_acc_data *acc)
{
	int err;
	u8 buf[2] = { CTRL_REG4, LSM303DLH_ACC_PM_OFF };

	err = lsm303dlh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power off failed\n");

	if (acc->pdata->power_off) {
		acc->pdata->power_off();
		acc->hw_initialized = 0;
	}
}

static int lsm303dlh_acc_device_power_on(struct lsm303dlh_acc_data *acc)
{
	int err;

	if (acc->pdata->power_on) {
		err = acc->pdata->power_on();
		if (err < 0)
			return err;
	}

	if (!acc->hw_initialized) {
		err = lsm303dlh_acc_hw_init(acc);
		if (err < 0) {
			lsm303dlh_acc_device_power_off(acc);
			return err;
		}
	}

	return 0;
}

int lsm303dlh_acc_update_g_range(struct lsm303dlh_acc_data *acc, u8 new_g_range)
{
	int err;
	u8 shift;
	u8 buf[2];

	switch (new_g_range) {
	case LSM303DLH_G_2G:
		shift = SHIFT_ADJ_2G;
		break;
	case LSM303DLH_G_4G:
		shift = SHIFT_ADJ_4G;
		break;
	case LSM303DLH_G_8G:
		shift = SHIFT_ADJ_8G;
		break;
	default:
		return -EINVAL;
	}

	if (atomic_read(&acc->enabled)) {
		/* Set configuration register 4, which contains g range setting
		 *  NOTE: this is a straight overwrite because this driver does
		 *  not use any of the other configuration bits in this
		 *  register.  Should this become untrue, we will have to read
		 *  out the value and only change the relevant bits --XX----
		 *  (marked by X) */
		buf[0] = CTRL_REG4;
		buf[1] = new_g_range;
		err = lsm303dlh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			return err;
	}

	acc->resume_state[3] = new_g_range;
	acc->shift_adj = shift;

	return 0;
}

int lsm303dlh_acc_update_odr(struct lsm303dlh_acc_data *acc, int poll_interval)
{
	int err = -1;
	int i;
	u8 config[2];

	/* Convert the poll interval into an output data rate configuration
	 *  that is as low as possible.  The ordering of these checks must be
	 *  maintained due to the cascading cut off values - poll intervals are
	 *  checked from shortest to longest.  At each check, if the next lower
	 *  ODR cannot support the current poll interval, we stop searching */
	for (i = 0; i < ARRAY_SIZE(odr_table); i++) {
		config[1] = odr_table[i].mask;
		if (poll_interval < odr_table[i].cutoff)
			break;
	}

	config[1] |= LSM303DLH_ACC_ENABLE_ALL_AXES;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&acc->enabled)) {
		config[0] = CTRL_REG1;
		err = lsm303dlh_acc_i2c_write(acc, config, 1);
		if (err < 0)
			return err;
	}

	acc->resume_state[0] = config[1];

	return 0;
}

static int lsm303dlh_acc_get_acceleration_data(struct lsm303dlh_acc_data *acc,
					       int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	int hw_d[3] = { 0 };
	struct lsm303dlh_acc_data *acc_temp;

	acc_data[0] = (AUTO_INCREMENT | AXISDATA_REG);
	err = lsm303dlh_acc_i2c_read(acc, acc_data, 6);
	if (err < 0)
		return err;

	hw_d[0] = (int) (((acc_data[1]) << 8) | acc_data[0]);
	hw_d[1] = (int) (((acc_data[3]) << 8) | acc_data[2]);
	hw_d[2] = (int) (((acc_data[5]) << 8) | acc_data[4]);

	hw_d[0] = (hw_d[0] & 0x8000) ? (hw_d[0] | 0xFFFF0000) : (hw_d[0]);
	hw_d[1] = (hw_d[1] & 0x8000) ? (hw_d[1] | 0xFFFF0000) : (hw_d[1]);
	hw_d[2] = (hw_d[2] & 0x8000) ? (hw_d[2] | 0xFFFF0000) : (hw_d[2]);

	hw_d[0] >>= acc->shift_adj;
	hw_d[1] >>= acc->shift_adj;
	hw_d[2] >>= acc->shift_adj;

	xyz[0] = ((acc->pdata->negate_x) ? (-hw_d[acc->pdata->axis_map_x])
		  : (hw_d[acc->pdata->axis_map_x]));
	xyz[1] = ((acc->pdata->negate_y) ? (-hw_d[acc->pdata->axis_map_y])
		  : (hw_d[acc->pdata->axis_map_y]));
	xyz[2] = ((acc->pdata->negate_z) ? (-hw_d[acc->pdata->axis_map_z])
		  : (hw_d[acc->pdata->axis_map_z]));

//	printk("enter %s %d %d %d %d %d %d \n",__FUNCTION__,acc_data[0],acc_data[1],acc_data[2],acc_data[3],acc_data[4],acc_data[5]); 
	return err;
}

static void lsm303dlh_acc_report_values(struct lsm303dlh_acc_data *acc,
					int *xyz)
{
	input_report_abs(acc->input_dev, ABS_X, xyz[0]);
	input_report_abs(acc->input_dev, ABS_Y, xyz[1]);
	input_report_abs(acc->input_dev, ABS_Z, xyz[2]);
	input_sync(acc->input_dev);
}

static int lsm303dlh_acc_enable(struct lsm303dlh_acc_data *acc)
{
	int err;

	if (!atomic_cmpxchg(&acc->enabled, 0, 1)) {

		err = lsm303dlh_acc_device_power_on(acc);
		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}
		schedule_delayed_work(&acc->input_work,
				      msecs_to_jiffies(acc->
						       pdata->poll_interval));
	}

	return 0;
}

static int lsm303dlh_acc_disable(struct lsm303dlh_acc_data *acc)
{
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		cancel_delayed_work_sync(&acc->input_work);
		lsm303dlh_acc_device_power_off(acc);
	}

	return 0;
}

static int lsm303dlh_acc_misc_open(struct inode *inode, struct file *file)
{
	int err;
	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	file->private_data = lsm303dlh_acc_misc_data;

	return 0;
}

static int lsm303dlh_acc_misc_ioctl(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	u8 buf[4];
	int err;
	int interval;
	struct lsm303dlh_acc_data *acc = file->private_data;

	switch (cmd) {
	case LSM303DLH_ACC_IOCTL_GET_DELAY:
		interval = acc->pdata->poll_interval;
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EFAULT;
		break;

	case LSM303DLH_ACC_IOCTL_SET_DELAY:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval < 0 || interval > 200)
			return -EINVAL;

		acc->pdata->poll_interval =
		    max(interval, acc->pdata->min_interval);
		err = lsm303dlh_acc_update_odr(acc, acc->pdata->poll_interval);
		/* TODO: if update fails poll is still set */
		if (err < 0)
			return err;

		break;

	case LSM303DLH_ACC_IOCTL_SET_ENABLE:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval > 1)
			return -EINVAL;

		if (interval)
			lsm303dlh_acc_enable(acc);
		else
			lsm303dlh_acc_disable(acc);

		break;

	case LSM303DLH_ACC_IOCTL_GET_ENABLE:
		interval = atomic_read(&acc->enabled);
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EINVAL;

		break;

	case LSM303DLH_ACC_IOCTL_SET_G_RANGE:
		if (copy_from_user(&buf, argp, 1))
			return -EFAULT;
		err = lsm303dlh_acc_update_g_range(acc, buf[0]);
		if (err < 0)
			return err;

		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations lsm303dlh_acc_misc_fops = {
	.owner = THIS_MODULE,
	.open = lsm303dlh_acc_misc_open,
	.ioctl = lsm303dlh_acc_misc_ioctl,
};

static struct miscdevice lsm303dlh_acc_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = NAME,
	.fops = &lsm303dlh_acc_misc_fops,
};

static void lsm303dlh_acc_input_work_func(struct work_struct *work)
{
	struct lsm303dlh_acc_data *acc;

	int xyz[3] = { 0 };
	int err;

	acc = container_of((struct delayed_work *)work,
			    struct lsm303dlh_acc_data, input_work);

	mutex_lock(&acc->lock);
	err = lsm303dlh_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		lsm303dlh_acc_report_values(acc, xyz);

	schedule_delayed_work(&acc->input_work,
			      msecs_to_jiffies(acc->pdata->poll_interval));
	mutex_unlock(&acc->lock);
}

#ifdef LSM303DLH_ACC_OPEN_ENABLE
int lsm303dlh_acc_input_open(struct input_dev *input)
{
	struct lsm303dlh_acc_data *acc = input_get_drvdata(input);

	return lsm303dlh_acc_enable(acc);
}

void lsm303dlh_acc_input_close(struct input_dev *dev)
{
	struct lsm303dlh_acc_data *acc = input_get_drvdata(dev);

	lsm303dlh_acc_disable(acc);
}
#endif

static int lsm303dlh_acc_validate_pdata(struct lsm303dlh_acc_data *acc)
{
	acc->pdata->poll_interval = max(acc->pdata->poll_interval,
					acc->pdata->min_interval);

	if (acc->pdata->axis_map_x > 2 ||
	    acc->pdata->axis_map_y > 2 || acc->pdata->axis_map_z > 2) {
		dev_err(&acc->client->dev,
			"invalid axis_map value x:%u y:%u z%u\n",
			acc->pdata->axis_map_x, acc->pdata->axis_map_y,
			acc->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (acc->pdata->negate_x > 1 || acc->pdata->negate_y > 1 ||
	    acc->pdata->negate_z > 1) {
		dev_err(&acc->client->dev,
			"invalid negate value x:%u y:%u z:%u\n",
			acc->pdata->negate_x, acc->pdata->negate_y,
			acc->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (acc->pdata->poll_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lsm303dlh_acc_input_init(struct lsm303dlh_acc_data *acc)
{
	int err;

	INIT_DELAYED_WORK(&acc->input_work, lsm303dlh_acc_input_work_func);

	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocate failed\n");
		goto err0;
	}

#ifdef LSM303DLH_ACC_OPEN_ENABLE
	acc->input_dev->open = lsm303dlh_acc_input_open;
	acc->input_dev->close = lsm303dlh_acc_input_close;
#endif

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);

	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);

	acc->input_dev->name = "accelerometer";

	err = input_register_device(acc->input_dev); //??regist a input event device
	if (err) {
		dev_err(&acc->client->dev,
			"unable to register input polled device %s\n",
			acc->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(acc->input_dev);
err0:
	return err;
}

static void lsm303dlh_acc_input_cleanup(struct lsm303dlh_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}

static int lsm303dlh_acc_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)					//?? be used for Device INIT
{
	struct lsm303dlh_acc_data *acc;
	int err = -1;

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto err0;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto err0;
	}

	acc = kzalloc(sizeof(*acc), GFP_KERNEL);
	if (acc == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto err0;
	}

	mutex_init(&acc->lock);
	mutex_lock(&acc->lock);
	acc->client = client;

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL)
		goto err1;

	memcpy(acc->pdata, client->dev.platform_data, sizeof(*acc->pdata));

	err = lsm303dlh_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto err1_1;
	}

	i2c_set_clientdata(client, acc);

	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0)
			goto err1_1;
	}

	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));

	acc->resume_state[0] = 7;
	acc->resume_state[1] = 0;
	acc->resume_state[2] = 0;
	acc->resume_state[3] = 0;
	acc->resume_state[4] = 0;

	err = lsm303dlh_acc_device_power_on(acc);
	if (err < 0)
		goto err2;

	atomic_set(&acc->enabled, 1);

	err = lsm303dlh_acc_update_g_range(acc, acc->pdata->g_range);
	if (err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto err2;
	}

	err = lsm303dlh_acc_update_odr(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err2;
	}

	err = lsm303dlh_acc_input_init(acc);
	if (err < 0)
		goto err3;

	lsm303dlh_acc_misc_data = acc;

	err = misc_register(&lsm303dlh_acc_misc_device);  //?? register a charactor device 
	if (err < 0) {
		dev_err(&client->dev, "lsm_acc_device register failed\n");
		goto err4;
	}

	lsm303dlh_acc_device_power_off(acc);

	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);

	mutex_unlock(&acc->lock);

	dev_info(&client->dev, "lsm303dlh_acc probed\n");

	return 0;

err4:
	lsm303dlh_acc_input_cleanup(acc);
err3:
	lsm303dlh_acc_device_power_off(acc);
err2:
	if (acc->pdata->exit)
		acc->pdata->exit();
err1_1:
	mutex_unlock(&acc->lock);
	kfree(acc->pdata);
err1:
	kfree(acc);
err0:
	return err;
}

static int __devexit lsm303dlh_acc_remove(struct i2c_client *client)
{
	/* TODO: revisit ordering here once _probe order is finalized */
	struct lsm303dlh_acc_data *acc = i2c_get_clientdata(client);

	misc_deregister(&lsm303dlh_acc_misc_device);
	lsm303dlh_acc_input_cleanup(acc);
	lsm303dlh_acc_device_power_off(acc);
	if (acc->pdata->exit)
		acc->pdata->exit();
	kfree(acc->pdata);
	kfree(acc);

	return 0;
}

static int lsm303dlh_acc_resume(struct i2c_client *client)
{
	struct lsm303dlh_acc_data *acc = i2c_get_clientdata(client);

	if (acc->on_before_suspend)
		return lsm303dlh_acc_enable(acc);
	return 0;
}

static int lsm303dlh_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lsm303dlh_acc_data *acc = i2c_get_clientdata(client);

	acc->on_before_suspend = atomic_read(&acc->enabled);
	return lsm303dlh_acc_disable(acc);
}

static const struct i2c_device_id lsm303dlh_acc_id[] = {
	{NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lsm303dlh_acc_id);

static struct i2c_driver lsm303dlh_acc_driver = {
	.driver = {
		   .name = NAME,
		   },
	.probe = lsm303dlh_acc_probe,
	.remove = __devexit_p(lsm303dlh_acc_remove),
	.resume = lsm303dlh_acc_resume,
	.suspend = lsm303dlh_acc_suspend,
	.id_table = lsm303dlh_acc_id,
};

static int __init lsm303dlh_acc_init(void)
{
	pr_info(KERN_INFO "LSM303DLH_ACC driver for the accelerometer part\n");  //?? driver init , waiting for BSP init 
	return i2c_add_driver(&lsm303dlh_acc_driver);
}

static void __exit lsm303dlh_acc_exit(void)
{
	i2c_del_driver(&lsm303dlh_acc_driver);
	return;
}

module_init(lsm303dlh_acc_init);
module_exit(lsm303dlh_acc_exit);

MODULE_DESCRIPTION("lsm303dlh accelerometer driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

