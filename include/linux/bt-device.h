/*
 *
 * arch/arm/mach-meson/bcm-bt.c
 *
 *  Copyright (C) 2010 AMLOGIC, INC.
 *
 * License terms: GNU General Public License (GPL) version 2
 * Platform machine definition.
 */

#ifndef __BT_DEVICE_H
#define __BT_DEVICE_H

struct bt_dev_data {
    void (*bt_dev_init)(void);
    void (*bt_dev_on)(void);
    void (*bt_dev_off)(void);
    void (*bt_dev_suspend)(void);
    void (*bt_dev_resume)(void);
};

#endif    
