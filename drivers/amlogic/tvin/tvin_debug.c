/*
 * TVIN attribute debug
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Standard Linux Headers */
#include <linux/string.h>
#include <linux/kernel.h>

/* Amlogic Headers */
#include <mach/am_regs.h>

/* Local Headers */
#include "tvin_debug.h"


static void tvin_dbg_usage(void)
{
    pr_info("Usage: rc address or wc address value\n");
    pr_info("Notes: address in hexadecimal and prefix 0x\n");
    return;
}


/*
* rc 0x12345678
* rw 0x12345678 1234
* adress must be hexadecimal and prefix with ox.
*/
ssize_t vdin_dbg_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{

    char strcmd[16];
    char straddr[10];
    char strval[32];
    int i = 0;
    int j = 0;
    unsigned long addr;
    unsigned int value=0;
    unsigned int retval;


    /* get parameter command string */
    j = 0;
    while( (i < count) && (buf[i]) && (buf[i] != ',') && (buf[i] != ' ') && (buf[i] != 10)){
        strcmd[j] = buf[i];
        i++;
        j++;
    }
    strcmd[j] = '\0';

    /* ignore */
    while( (i < count) && (buf[i]) && ((buf[i] ==',') || (buf[i] == ' ') || (buf[i] == 10))){
        i++;
    }

    /* check address */
    if (strncmp(&buf[i], "0x", 2) != 0){
        pr_info("invalid parameter address\n");
        tvin_dbg_usage();
        return 32;
    }


    /* get parameter address string */
    j = 0;
    while( (i < count) && (buf[i]) && (buf[i] != ',') && (buf[i] != ' ' && (buf[i] != 10))){
        straddr[j] = buf[i];
        i++;
        j++;
    }
    straddr[j] = 0;
    addr = simple_strtoul(straddr, NULL, 16);       //hex data

    /* rc read cbus */
    if (strncmp(strcmd, "rc", 2) == 0){
        retval = READ_CBUS_REG(addr);
        pr_info("%s: 0x%x --> 0x%x\n", strcmd, addr, retval);
        return 32;
    }
    /* rp read apb */
    else if (strncmp(strcmd, "rp", 2) == 0){
        retval = READ_APB_REG(addr);
        pr_info("%s: 0x%x --> 0x%x\n", strcmd, addr, retval);
        return 32;
    }
    /* wc write cbus */
    else if (strncmp(strcmd, "wc", 2) == 0){
        /* get parameter value string*/
        /* ignore */
        while( (i < count) && (buf[i]) && ((buf[i] ==',') || (buf[i] == ' ') || (buf[i] == 10))){
            i++;
        }
        if (!buf[i]){
            pr_info("no parameter value\n");
            tvin_dbg_usage();
            return 32;
        }

        j = 0;
        while( (i < count) && (buf[i]) && (buf[i] != ',') && (buf[i] != ' ')&& (buf[i] != 10)){
            strval[j] = buf[i];
            i++;
            j++;
        }
        strval[j] = '\0';
        value = simple_strtol(strval, NULL, 16);    //hex data

        WRITE_CBUS_REG(addr, value);
        pr_info("%s: 0x%x <-- 0x%x\n", strcmd, addr, value);
        return 32;
    }
    /* wp write apb */
    else if (strncmp(strcmd, "wp", 2) == 0){
        /* get parameter value string*/
        /* ignore */
        while( (i < count) && (buf[i]) && ((buf[i] ==',') || (buf[i] == ' ') || (buf[i] == 10))){
            i++;
        }
        if (!buf[i]){
            pr_info("no parameter value\n");
            tvin_dbg_usage();
            return 32;
        }

        j = 0;
        while( (i < count) && (buf[i]) && (buf[i] != ',') && (buf[i] != ' ')&& (buf[i] != 10)){
            strval[j] = buf[i];
            i++;
            j++;
        }
        strval[j] = '\0';
        value = simple_strtol(strval, NULL, 16);    //hex data

        WRITE_APB_REG(addr, value);
        pr_info("%s: 0x%x <-- 0x%x\n", strcmd, addr, value);
        return 32;
    }
    else{
        pr_info("invalid parameter\n");
        tvin_dbg_usage();
        return 32;
    }

    return 32;
}


