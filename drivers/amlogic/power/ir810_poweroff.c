/*
 * drivers/amlogic/power/ir810_poweroff.c
 *
 * Power on/off STV-MBX-xx, platforms
 *
 * Copyright (C) 2012 PivosGroup
 *
 * Written by Scott Davilla <davilla@xbmc.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/module.h>

#define IR810_STATUS_REGISTER  0x00
#define IR810_KEYVAL_REGISTER	(IR810_STATUS_REGISTER+2)
#define IR810_IRMVAL_REGISTER	(IR810_KEYVAL_REGISTER+2)
#define IR810_CODEPR_REGISTER	(IR810_IRMVAL_REGISTER+2)
#define IR810_CODECM_REGISTER	(IR810_CODEPR_REGISTER+2)
#define IR810_TMPVAL_REGISTER	(IR810_CODECM_REGISTER+2)
#define IR810_ADC_T3_REGISTER	(IR810_TMPVAL_REGISTER+2)
#define IR810_ADC_T4_REGISTER	(IR810_ADC_T3_REGISTER+2)
#define IR810_ADC_T5_REGISTER	(IR810_ADC_T4_REGISTER+2)
#define IR810_ADC_T6_REGISTER	(IR810_ADC_T5_REGISTER+2)
#define IR810_ADC_T7_REGISTER	(IR810_ADC_T6_REGISTER+2)
#define IR810_ADC_T8_REGISTER	(IR810_ADC_T7_REGISTER+2)
#define IR810_ADC_T9_REGISTER	(IR810_ADC_T8_REGISTER+2)
#define IR810_TM2ONZ_REGISTER	(IR810_ADC_T9_REGISTER+2)
#define IR810_TM2OFF_REGISTER	(IR810_TM2ONZ_REGISTER+2)
#define IR810_ENCRYP_REGISTER	(IR810_TM2OFF_REGISTER+2)

#define IR810_STATUS_PWR_DISABLE   0x0001
#define IR810_STATUS_PWR_DOWN_NOW  0x0002
#define IR810_STATUS_PWR_HW_BUTTON 0x0004
#define IR810_STATUS_PWR_IR_BUTTON 0x0008

static struct i2c_client *ir810_poweroff_client;

static int show_register(int address, const char *buf)
{
  unsigned int data;
	data = i2c_smbus_read_word_data(ir810_poweroff_client, address);
	return sprintf((char*)buf, "0x%x\n", data);
}

static int store_register(int address, const char *buf)
{
  ssize_t r;
  u16 data16;
  unsigned int data;

  r = sscanf((char*)buf, "0x%x", &data);
  if (r != 1)
    return -EINVAL;

  data16 = data;
	i2c_smbus_write_word_data(ir810_poweroff_client, address, data16);
  return 0;
}

static void ir810_poweroff(void)
{
	/* on shutdown, kernel will call us via pm_power_off */
	u16 data;
	data = i2c_smbus_read_word_data(ir810_poweroff_client, IR810_STATUS_REGISTER);
	/* make sure power control is enabled */
	data &= ~IR810_STATUS_PWR_DISABLE;
	/* set the power down now bit */
	data |= IR810_STATUS_PWR_DOWN_NOW;
	printk("ir810_xx power is going down\n");
	/* power is going down after this write */
	i2c_smbus_write_word_data(ir810_poweroff_client, IR810_STATUS_REGISTER, data);
	return;
}

static ssize_t show_irpowerkey(struct class *class,
			struct class_attribute *attr,	char *buf)
{
  return show_register(IR810_CODEPR_REGISTER, buf);
}

static ssize_t store_irpowerkey(struct class *class,
			struct class_attribute *attr,	const char *buf, size_t count)
{
  store_register(IR810_CODEPR_REGISTER, buf);
	return count;
}

static ssize_t show_status(struct class *class,
			struct class_attribute *attr,	char *buf)
{
  return show_register(IR810_STATUS_REGISTER, buf);
}

static ssize_t store_status(struct class *class,
			struct class_attribute *attr,	const char *buf, size_t count)
{
  store_register(IR810_STATUS_REGISTER, buf);
	return count;
}

static struct class_attribute ir810_class_attrs[] = {
    __ATTR(status,   S_IRUGO | S_IWUSR, show_status,     store_status),
    __ATTR(powerkey, S_IRUGO | S_IWUSR, show_irpowerkey, store_irpowerkey),
    __ATTR_NULL
};

static struct class ir810_class = {
    .name = "ir810",
    .class_attrs = ir810_class_attrs,
};

static int __devinit ir810_poweroff_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	u16 data;
	int res = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		pr_err("%s: i2c_check_functionality failed\n", __FUNCTION__);
		res = -ENODEV;
		goto out;
	}
	/* remember our client */
	ir810_poweroff_client = client;

  /* register our /sys/class API */
  res = class_register(&ir810_class);
	if (res) {
    pr_err(" failed to class register ir810_class\n");
		goto out;
  }

	/* setup power handling */
	data  = i2c_smbus_read_word_data(ir810_poweroff_client, IR810_STATUS_REGISTER);
	printk("ir810_poweroff_probe, status reg = 0x%x\n", data);
	/* enable power control */
	data &= ~IR810_STATUS_PWR_DISABLE;
	/* set hw power button to power off when held for > 3 seconds. */
	data |=  IR810_STATUS_PWR_HW_BUTTON;
	/* set ir power button to power off when held for > 3 seconds. */
	data |=  IR810_STATUS_PWR_IR_BUTTON;
	i2c_smbus_write_word_data(ir810_poweroff_client, IR810_STATUS_REGISTER, data);

	/* hook the poweroff power mananger routine*/
	pm_power_off = ir810_poweroff;
out:
	return res;
}

static int __devexit ir810_poweroff_remove(struct i2c_client *client)
{
	/* unhook the poweroff power mananger routine */
	pm_power_off = NULL;
	return 0;
}

/* see arch/arm/mach-meson/board-xxx.c, aml_i2c_bus_info for i2c slave address */
static const struct i2c_device_id ir810_poweroff_id[] = {
	{ "ir810_poweroff", 0 },
	{ }
};

static struct i2c_driver ir810_poweroff_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name		= "ir810_poweroff",
	},
	.id_table = ir810_poweroff_id,
	.probe		= ir810_poweroff_probe,
	.remove		= ir810_poweroff_remove
};

static int __init ir810_poweroff_init(void)
{
	int res;
	printk("ir810_poweroff_init\n");
	if (ir810_poweroff_client) {
		res = 0;
	} else {
		res = i2c_add_driver(&ir810_poweroff_driver);
		if (res < 0) {
			printk("add ir810_poweroff i2c driver error\n");
		}
	}
	return res;
}

static void __exit ir810_poweroff_exit(void)
{
	i2c_del_driver(&ir810_poweroff_driver);
}

module_init(ir810_poweroff_init);
module_exit(ir810_poweroff_exit);
MODULE_AUTHOR("Scott Davilla <davilla@xbmc.org>");
MODULE_DESCRIPTION("Amlogic IR810 PowerOff Driver");
MODULE_LICENSE("GPL");
