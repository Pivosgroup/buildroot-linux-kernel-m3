/*
 *
 * arch/arm/mach-meson/common.c
 *
 *  Copyright (C) 2010 AMLOGIC, INC.
 *
 * License terms: GNU General Public License (GPL) version 2
 * CPU frequence management.
 *
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/clock.h>
#include <asm/system.h>
#include <linux/io.h>
#include <linux/ioport.h>



static int __init chip_version_init(void)
{
    char  *version_map;
    unsigned int version;

    system_serial_high = 0; /*0 for meson*/
    /*0x49800000*/
    /*system_serial_low=,1,2,3,4,5 for REVA,B,C*/
    version_map = ioremap(0x49800000, 1024);

    if (!version_map) {
        printk("ioremap version_map failed\n");
        return -1;
    }

    version = inl((unsigned long)&version_map[0x1a0]);
    printk("chip version=%x\n", version);
    switch (version) {
    case 0xe7e0c653:
        system_serial_low = 0xA;
        break;
    case 0xe5832fe0:
        system_serial_low = 0xB;
        break;
    default:/*changed?*/
        system_serial_low = 0xC;
    }

    iounmap(version_map);
    return 0;
}

arch_initcall(chip_version_init);


