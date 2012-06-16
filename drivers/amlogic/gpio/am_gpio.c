/*
 * Amlogic general gpio driver
 *
 *
 * Copyright (C) 2009 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 *  docment for gpio usage  :
 *        http://openlinux.amlogic.com/wiki/index.php/GPIO
*/


#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/uio_driver.h>  //mmap to user space . to raise frequency.

#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/vout/vinfo.h>
#include <mach/am_regs.h>
#include <asm/uaccess.h>
#include <mach/gpio.h>
#include "am_gpio.h"
#include "am_gpio_log.h"
#include <linux/amlog.h>

MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0xff, LOG_LEVEL_DESC, LOG_MASK_DESC);

static void set_power_led_onoff(char *onoff)// 1:on; 0:off
{
	/* GPIO AO_10 */
    if (0 == strcmp(onoff, "powerkey led on")) { //led on
        set_gpio_val(GPIOAO_bank_bit0_11(10), GPIOAO_bit_bit0_11(10), 1);
        set_gpio_mode(GPIOAO_bank_bit0_11(10), GPIOAO_bit_bit0_11(10), GPIO_OUTPUT_MODE);
    } else {
        set_gpio_val(GPIOAO_bank_bit0_11(10), GPIOAO_bit_bit0_11(10), 0);
        set_gpio_mode(GPIOAO_bank_bit0_11(10), GPIOAO_bit_bit0_11(10), GPIO_OUTPUT_MODE);
    }
}

static inline int _gpio_setup_bank_bit(cmd_t  *op)
{
    switch (op->bank) {
    case 'a': //bank a
        op->bank = GPIOA_bank_bit0_27(0);
        if (op->bit < 28) { //bit0..27
            op->bit = GPIOA_bit_bit0_27(op->bit);
        } else {
            return -1;
        }
        break;
    case 'b': //bank b
        op->bank = GPIOB_bank_bit0_23(0);
        if (op->bit < 24) { //bit0..23
            op->bit = GPIOB_bit_bit0_23(op->bit);
        } else {
            return -1;
        }
        break;
    case 'c': //bank c
        op->bank = GPIOC_bank_bit0_15(0);
        if (op->bit < 16) { //bit0..15
            op->bit = GPIOC_bit_bit0_15(op->bit);
        } else {
            return -1;
        }
        break;
    case 'd': //bank d
        op->bank = GPIOD_bank_bit0_9(0);
        if (op->bit < 10) { //bit0..9
            op->bit = GPIOD_bit_bit0_9(op->bit);
        } else {
            return -1;
        }
        break;
    case 'x': //bank x
        if (op->bit < 32) { //bit0..31 ,bit no change .
            op->bank = GPIOX_bank_bit0_31(0); //bit 0..15 16..21 share one bank
		} else if (op->bit <36) { //bit 32..35
            op->bank = GPIOX_bank_bit32_35(0);
			op->bit = GPIOX_bit_bit32_35(op->bit);
		} else {
            return -1;
        }
        break;
    case 'y': //bank y
        if (op->bit < 23) { //bit0..22 ,bit no change .
            op->bank = GPIOY_bank_bit0_22(0); //bit 0..15 16..21 share one bank
		} else {
            return -1;
        }
        break;
	/* FIXME AO/BOOT/CARD GPIO can not controle todo */
    }
    return 0;
}
static  inline int _gpio_bank_write(cmd_t  *op)
{
    if (0 > _gpio_setup_bank_bit(op)) {
        return -1;
    }

    spin_lock(&gpio_lock);
    set_gpio_mode(op->bank, op->bit, GPIO_OUTPUT_MODE);
    set_gpio_val(op->bank, op->bit, op->val);
    spin_unlock(&gpio_lock);

    return op->val;
}
static inline int _gpio_bank_read(cmd_t  *op)
{
    u32 write_bit = op->bit;
    char bank = op->bank;

    if (0 > _gpio_setup_bank_bit(op)) {
        return -1;
    }

    spin_lock(&gpio_lock);
    set_gpio_mode(op->bank, op->bit, GPIO_INPUT_MODE);
    op->val = get_gpio_val(op->bank, op->bit);
    spin_unlock(&gpio_lock);
    printk("GPIO_%s_bit_%d(input bit:%d) = %d \n", &bank, op->bit, write_bit, op->val);

    return op->val ;
}
static inline int _gpio_run_cmd(const char *cmd, cmd_t  *op)
{
#define  MAX_NUMBER_PARA    10

    if (strlen(cmd) < 5) {
        return -1;
    }

    //trim space out
    cmd = skip_spaces(cmd);
    op->cmd = cmd[0] | 0x20;
    op->bank = cmd[2] | 0x20;

    if (isdigit(cmd[5])) { //pin number double bit
        op->bit = (cmd[4] - '0') * 10 + cmd[5] - '0' ;
        switch (op->cmd) {
        case 'w':
            op->val = cmd[7] - '0';
            return _gpio_bank_write(op);
        case 'r':
            return _gpio_bank_read(op);
        default:
            break;
        }
    } else {
        op->bit = cmd[4] - '0';
        switch (op->cmd) {
        case 'w':
            op->val = cmd[6] - '0';
            return _gpio_bank_write(op);
        case 'r':
            return _gpio_bank_read(op);
        default:
            break;
        }
    }
    return 0;
}

static int am_gpio_open(struct inode * inode, struct file * file)
{
    cmd_t  *op;

    op = (cmd_t*)kmalloc(sizeof(cmd_t), GFP_KERNEL);
    if (IS_ERR(op)) {
        amlog_mask_level(LOG_MASK_INIT, LOG_LEVEL_HIGH, "cant alloc an op_target struct\n");;
        return -1;
    }
    file->private_data = op;
    return 0;
}
static ssize_t am_gpio_read(struct file *file, char __user *buf, size_t size, loff_t*o)
{
    cmd_t  *op = file->private_data;
    char  tmp[10];
    char  cmd[10];
    int  val;

    if (buf == NULL || op == NULL) {
        return -1;
    }
    copy_from_user(tmp, buf, size);

    strcpy(cmd, "r:");
    strcat(cmd, tmp);
    val = _gpio_run_cmd(cmd, op);
    if (0 > val) {
        return -1;
    }
    sprintf(tmp, ":%d", val);
    strcat(cmd, tmp);
    return copy_to_user(buf, cmd, strlen(cmd));
}
static ssize_t am_gpio_write(struct file *file, const char __user *buf,
                             size_t bytes, loff_t *off)
{
    cmd_t  *op = file->private_data;
    char  tmp[10];
    char  cmd[10];
    int  val;

    if (buf == NULL || op == NULL) {
        return -1;
    }
    copy_from_user(tmp, buf, bytes);

    strcpy(cmd, "w:");
    strcat(cmd, tmp);
    val = _gpio_run_cmd(cmd, op);
    if (0 > val) {
        return -1;
    }
    return strlen(cmd);


}
static int am_gpio_ioctl(struct inode *inode, struct file *file,
                         unsigned int ctl_cmd, unsigned long arg)
{
    cmd_t  *op = file->private_data;
    char  cmd[10];
    cmd_t  op_target;
    cmd_t  ret_target;

    switch (ctl_cmd) {
    case GPIO_CMD_OP:
        if (copy_from_user(&op_target, (cmd_t*)arg, sizeof(cmd_t))) {
            return -EFAULT;
        }
        memcpy(&ret_target, &op_target, sizeof(cmd_t));
        sprintf(cmd, "%c:%c:%d:%d", op_target.cmd, op_target.bank, op_target.bit, op_target.val);
        _gpio_run_cmd(cmd, op);
        ret_target.val = op_target.val;
        if (copy_to_user((cmd_t*)arg, &ret_target, sizeof(cmd_t))) {
            return  -EFAULT;
        }
        break;
    default:
        break;
    }
    return 0;
}
static int am_gpio_release(struct inode *inode, struct file *file)
{
    cmd_t  *op = file->private_data;

    if (op) {
        kfree(op);
    }
    return 0;
}
static const struct file_operations am_gpio_fops = {
    .open   = am_gpio_open,
    .read   = am_gpio_read,
    .write  = am_gpio_write,
    .ioctl      = am_gpio_ioctl,
    .release    = am_gpio_release,
    .poll       = NULL,
};

ssize_t gpio_cmd_restore(struct class *cla, struct class_attribute *attr, const char *buf, size_t count)
{
    cmd_t   op;
    _gpio_run_cmd(buf, &op);
    return strlen(buf);
}
int  create_gpio_device(gpio_t *gpio)
{
    int ret ;

    ret = class_register(&gpio_class);
    if (ret < 0) {
        amlog_level(LOG_LEVEL_HIGH, "error create gpio class\r\n");
        return ret;
    }
    gpio->cla = &gpio_class ;
    gpio->dev = device_create(gpio->cla, NULL, MKDEV(gpio->major, 0), NULL, strcat(gpio->name, "_dev"));
    if (IS_ERR(gpio->dev)) {
        amlog_level(LOG_LEVEL_HIGH, "create gpio device error\n");
        class_unregister(gpio->cla);
        gpio->cla = NULL;
        gpio->dev = NULL;
        return -1 ;
    }

    if (uio_register_device(gpio->dev, &gpio_uio_info)) {
        amlog_level(LOG_LEVEL_HIGH, "gpio UIO device register fail.\n");
        return -1;
    }

    amlog_level(LOG_LEVEL_HIGH, "create gpio device success\n");
    return  0;
}
/*****************************************************************
**
**  module   entry and exit port
**
******************************************************************/
static int __init gpio_init_module(void)
{
    sprintf(am_gpio.name, "%s", GPIO_DEVCIE_NAME);
    am_gpio.major = register_chrdev(0, GPIO_DEVCIE_NAME, &am_gpio_fops);
    if (am_gpio.major < 0) {
        amlog_mask_level(LOG_MASK_INIT, LOG_LEVEL_HIGH, "register char dev gpio error\r\n");
    } else {
        amlog_mask_level(LOG_MASK_INIT, LOG_LEVEL_HIGH, "gpio dev major number:%d\r\n", am_gpio.major);
        if (0 > create_gpio_device(&am_gpio)) {
            unregister_chrdev(am_gpio.major, am_gpio.name);
            am_gpio.major = -1;
        }
    }

    return am_gpio.major;
}
static __exit void gpio_remove_module(void)
{
    if (0 > am_gpio.major) {
        return ;
    }
    uio_unregister_device(&gpio_uio_info);
    device_destroy(am_gpio.cla, MKDEV(am_gpio.major, 0));
    class_unregister(am_gpio.cla);
    return ;
}
/****************************************/
module_init(gpio_init_module);
module_exit(gpio_remove_module);

MODULE_DESCRIPTION("AMLOGIC gpio driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wangjf <jianfeng.wang@amlogic.com>");

