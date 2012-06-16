#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/device.h>
#include <linux/spi/flash.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/memory.h>
#include <mach/clock.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <mach/lm.h>
#include <asm/memory.h>
#include <asm/mach/map.h>
#include <mach/nand.h>
#include <linux/i2c.h>
#include <linux/i2c-aml.h>
#include <mach/power_gate.h>
#include <linux/aml_bl.h>
#include <linux/delay.h>
#include <mach/usbclock.h>
#include <mach/am_regs.h>
#include <linux/file.h>
#include <asm/cacheflush.h>

//appf functions
#define APPF_INITIALIZE             0
#define APPF_POWER_DOWN_CPU         1
#define APPF_POWER_UP_CPUS          2
//appf flags
#define APPF_SAVE_PMU          (1<<0)
#define APPF_SAVE_TIMERS       (1<<1)
#define APPF_SAVE_VFP          (1<<2)
#define APPF_SAVE_DEBUG        (1<<3)
#define APPF_SAVE_L2           (1<<4)

void disable_watchdog(void)
{
	//printk(KERN_INFO "** disable watchdog\n");
	WRITE_MPEG_REG(WATCHDOG_RESET, 0);
	CLEAR_MPEG_REG_MASK(WATCHDOG_TC,(1 << WATCHDOG_ENABLE_BIT));
}
EXPORT_SYSMBOL(disable_watchdog);

void enable_watchdog(void)
{
	//printk(KERN_INFO "** enable watchdog\n");
	WRITE_MPEG_REG(WATCHDOG_RESET, 0);
	WRITE_MPEG_REG(WATCHDOG_TC, 1 << WATCHDOG_ENABLE_BIT | 0xFFFFF);//about 10sec
}
EXPORT_SYSMBOL(enable_watchdog);

void reset_watchdog(void)
{
	//printk(KERN_INFO "** reset watchdog\n");
	WRITE_MPEG_REG(WATCHDOG_RESET, 0);	
}
EXPORT_SYSMBOL(reset_watchdog);

int meson_power_suspend()
{
	static int test_flag = 0;
	int i;
	unsigned addr;
	const entry_offset = 0x4400;
	void	(*pwrtest_entry)(unsigned,unsigned,unsigned,unsigned);

	flush_cache_all();

	addr = phys_to_virt(PHYS_OFFSET + CONFIG_AML_SUSPEND_FIRMWARE_BASE);
	addr += entry_offset;
	pwrtest_entry = (void (*)(unsigned,unsigned,unsigned,unsigned))addr;
	if(test_flag != 1234){
		test_flag = 1234;
		printk("initial appf\n");
		pwrtest_entry(APPF_INITIALIZE,0,0,0);
	}
#ifdef CONFIG_SUSPEND_WATCHDOG
	disable_watchdog();
#endif
	printk("power down cpu --\n");
	pwrtest_entry(APPF_POWER_DOWN_CPU,0,0,APPF_SAVE_PMU|APPF_SAVE_VFP|APPF_SAVE_L2);
#ifdef CONFIG_SUSPEND_WATCHDOG
	enable_watchdog();
#endif
	return 0;
}
