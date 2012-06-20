/*
 * drivers/amlogic/power/stv-mbx_poweroff.c
 *
 * Power off STV-MBX-xx platform
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

#define STV_MBX_PWR_ADDRESS   0x00
#define ATV_MBX_PWR_DISABLE   0x0001
#define ATV_MBX_PWR_DOWN_NOW  0x0002
#define ATV_MBX_PWR_HW_BUTTON 0x0004
#define ATV_MBX_PWR_IR_BUTTON 0x0008

static struct i2c_client *stv_mbx_poweroff_client;

static void stv_mbx_poweroff(void)
{
	/* on shutdown, kernel will call us via pm_power_off */
	u16 data;
	data = i2c_smbus_read_word_data(stv_mbx_poweroff_client, STV_MBX_PWR_ADDRESS);
	/* make sure power control is enabled */
	data &= ~ATV_MBX_PWR_DISABLE;
	/* set the power down now bit */
	data |= ATV_MBX_PWR_DOWN_NOW;
	printk("stv_mbx_xx power is going down\n");
	/* power is going down after this write */
	i2c_smbus_write_word_data(stv_mbx_poweroff_client, STV_MBX_PWR_ADDRESS, data);
	return;
}

static int __devinit stv_mbx_poweroff_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	u16 data;
	int res = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		pr_err("%s: i2c_check_functionality failed\n", __FUNCTION__);
		res = -ENODEV;
		goto out;
	}
	/* remember our client */
	stv_mbx_poweroff_client = client;

	/* setup power handling */
	data  = i2c_smbus_read_word_data(stv_mbx_poweroff_client, STV_MBX_PWR_ADDRESS);
	printk("stv_mbx_poweroff_probe, status reg = 0x%x\n", data);
	/* enable power control */
	data &= ~ATV_MBX_PWR_DISABLE;
	/* set hw power button to power off when held for > 3 seconds. */
	data |=  ATV_MBX_PWR_HW_BUTTON;
	/* set ir power button to power off when held for > 3 seconds. */
	data |=  ATV_MBX_PWR_IR_BUTTON;
	i2c_smbus_write_word_data(stv_mbx_poweroff_client, STV_MBX_PWR_ADDRESS, data);

	/* hook the poweroff power mananger routine*/
	pm_power_off = stv_mbx_poweroff;
out:
	return res;
}

static int __devexit stv_mbx_poweroff_remove(struct i2c_client *client)
{
	/* unhook the poweroff power mananger routine */
	pm_power_off = NULL;
	return 0;
}

/* see arch/arm/mach-meson/stv-mbx-m3.c for i2c slave address */
static const struct i2c_device_id stv_mbx_poweroff_id[] = {
	{ "stv_mbx_poweroff", 0 },
	{ }
};

static struct i2c_driver stv_mbx_poweroff_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name		= "stv_mbx_poweroff",
	},
	.id_table = stv_mbx_poweroff_id,
	.probe		= stv_mbx_poweroff_probe,
	.remove		= stv_mbx_poweroff_remove
};

static int __init stv_mbx_poweroff_init(void)
{
	int res;
	printk("stv_mbx_poweroff_init\n");
	if (stv_mbx_poweroff_client) {
		res = 0;
	} else {
		res = i2c_add_driver(&stv_mbx_poweroff_driver);
		if (res < 0) {
			printk("add stv_mbx_poweroff i2c driver error\n");
		}
	}
	return res;
}

static void __exit stv_mbx_poweroff_exit(void)
{
	i2c_del_driver(&stv_mbx_poweroff_driver);
}

module_init(stv_mbx_poweroff_init);
module_exit(stv_mbx_poweroff_exit);
MODULE_AUTHOR("Scott Davilla <davilla@xbmc.org>");
MODULE_DESCRIPTION("Amlogic STV-MBX PowerOff Driver");
MODULE_LICENSE("GPL");
