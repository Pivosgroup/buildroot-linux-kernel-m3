/*
 * mach-meson/sram.c - simple SRAM allocator
 *
 * Copyright (C) 2010 Robin Zhu
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/genalloc.h>
#include <linux/io.h>

#include <mach/sram.h>

static struct gen_pool *sram_pool;

void *sram_alloc(size_t len)
{
	unsigned long vaddr;

	if (!sram_pool)
		return NULL;
	vaddr = gen_pool_alloc(sram_pool, len);
	if (!vaddr)
		return NULL;
	return (void *)vaddr;

}
EXPORT_SYMBOL(sram_alloc);

int sram_vaddr;
void sram_free(void *addr, size_t len)
{
	gen_pool_free(sram_pool, (unsigned long) addr, len);
}
EXPORT_SYMBOL(sram_free);

static int __init sram_init(void)
{
	int status = 0;
	sram_vaddr = ioremap(0xc9000000, SRAM_SIZE);
	printk("sram vaddr = %x", sram_vaddr);

	sram_pool = gen_pool_create(ilog2(SRAM_GRANULARITY), -1);
	if (!sram_pool){
		status = -ENOMEM;
	}
	if (sram_pool)
		status = gen_pool_add(sram_pool, sram_vaddr /*SRAM_VIRT*/, SRAM_SIZE, -1);
	WARN_ON(status < 0);
	return status;
}
core_initcall(sram_init);

