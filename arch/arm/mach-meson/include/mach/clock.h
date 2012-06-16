/*
 *  arch/arm/mach-meson/include/mach/clock.h
 *
 *  Copyright (C) 2010 AMLOGIC, INC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef __ARCH_ARM_MESON_CLOCK_H
#define __ARCH_ARM_MESON_CLOCK_H



struct clk {
	const char *name;
	unsigned long rate;
	unsigned long min;
	unsigned long max;
	int source_clk;
	/* for clock gate */
	unsigned char clock_index;
	unsigned clock_gate_reg_adr;
	unsigned clock_gate_reg_mask;
	/**/
	unsigned long	(*get_rate)(struct clk *);
	int	(*set_rate)(struct clk *, unsigned long);
};

extern int set_usb_phy_clk(int rate);
extern int set_sata_phy_clk(int sel);

#define USB_CTL_POR_ON			10
#define USB_CTL_POR_OFF		11
#define USB_CTL_POR_ENABLE	12
#define USB_CTL_POR_DISABLE	13

#define USB_CTL_INDEX_A	0
#define USB_CTL_INDEX_B	1
extern void set_usb_ctl_por(int index,int por_flag);

#endif
