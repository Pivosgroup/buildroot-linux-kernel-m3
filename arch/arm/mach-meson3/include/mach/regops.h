/*
 *
 * arch/arm/arch-meson3/include/mach/regops.h
 *
 *  Copyright (C) 2011 AMLOGIC, INC.
 *
 *	Common interface for aml CBUS/AHB/APB/AXI operation
 *
 * License terms: GNU General Public License (GPL) version 2
 * Basic platform init and mapping functions.
 */
#ifndef __REGOPS_H__
#define __REGOPS_H__
#include <linux/types.h>
#include <linux/io.h>
#ifndef __ASSEMBLY__
#ifndef REGOPS_DEBUG
static __inline__ uint32_t aml_read_reg32(uint32_t _reg)
{
	return readl_relaxed(_reg);
};
static __inline__ void aml_write_reg32(uint32_t _reg, const uint32_t _value)
{
	writel(_value, _reg);
};
static __inline__ void aml_set_reg32_bits(uint32_t _reg, const uint32_t _value, const uint32_t _start, const uint32_t _len)
{
	writel(((readl_relaxed(_reg) & ~(((1L << (_len)) - 1) << (_start))) | ((unsigned)((_value) & ((1L << (_len)) - 1)) << (_start))), _reg);
}
static __inline__ void aml_clrset_reg32_bits(uint32_t _reg, const uint32_t clr, const uint32_t set)
{
	writel((readl_relaxed(_reg) & ~(clr)) | (set), _reg);
}

static __inline__ uint32_t aml_get_reg32_bits(uint32_t _reg, const uint32_t _start, const uint32_t _len)
{
	return	((readl_relaxed(_reg) >> (_start)) & ((1L << (_len)) - 1));
}
static __inline__ void aml_set_reg32_mask(uint32_t _reg, const uint32_t _mask)
{
	writel((readl_relaxed(_reg) | (_mask)), _reg);
}
static __inline__ void aml_clr_reg32_mask(uint32_t _reg, const uint32_t _mask)
{
	writel((readl_relaxed(_reg) & (~(_mask))), _reg);
}
#else
static __inline__ uint32_t aml_read_reg32(uint32_t _reg)
{
	uint32_t _val;

	_val = readl_relaxed(_reg);
	if (g_regops_dbg_lvl) {
		printk(KERN_DEBUG "rd:%X = %X\n", _reg, _val);
	}
	return val;
};
static __inline__ void aml_write_reg32(uint32_t _reg, const uint32_t _value)
{
	if (g_regops_dbg_lvl) {
		printk(KERN_DEBUG "wr:%X = %X\n", _reg, _value);
	}
	writel(_value, _reg);
};
static __inline__ void aml_set_reg32_bits(uint32_t _reg, const uint32_t _value, const uint32_t _start, const uint32_t _len)
{
	if (g_regops_dbg_lvl) {
		printk(KERN_DEBUG "setbit:%X = (%X,%X,%X)\n", _reg, _value, _start, _len);
	}
	writel((readl_relaxed(_reg) & ~(((1L << (_len)) - 1) << (_start)) | ((unsigned)((_value) & ((1L << (_len)) - 1)) << (_start))), _reg);
}
static __inline__ void aml_clrset_reg32_bits(uint32_t _reg, const uint32_t clr, const uint32_t set)
{
	if (g_regops_dbg_lvl) {
		printk(KERN_DEBUG "clrsetbit:%X = (%X,%X)\n", _reg, clr, set);
	}
	writel((readl_relaxed(_reg) & ~(clr)) | (set), _reg);
}

static __inline__ uint32_t aml_get_reg32_bits(uint32_t _reg, const uint32_t _start, const uint32_t _len)
{
	uint32_t _val;

	_val = ((readl_relaxed(_reg) >> (_start)) & ((1L << (_len)) - 1));

	if (g_regops_dbg_lvl) {
		printk(KERN_DEBUG "getbit:%X = (%X,%X,%X)\n", _reg, _val, _start, _len);
	}

	return	_val;
}
static __inline__ void aml_set_reg32_mask(uint32_t _reg, const uint32_t _mask)
{
	uint32_t _val;

	_val = readl_relaxed(_reg) | _mask;

	if (g_regops_dbg_lvl) {
		printk(KERN_DEBUG "setmsk:%X |= %X, %X\n", _reg, _mask, _val);
	}

	writel(val , reg);
}
static __inline__ void aml_clr_reg32_mask(uint32_t _reg, const uint32_t _mask)
{
	uint32_t _val;

	_val = readl_relaxed(_reg) & (~ _mask);

	if (g_regops_dbg_lvl) {
		printk(KERN_DEBUG "setmsk:%X |= %X, %X\n", _reg, _mask, _val);
	}

	writel(_val , _reg);
}
#endif
#endif

#endif
