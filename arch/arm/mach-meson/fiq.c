/*
 * AMLOGIC FIQ management
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
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>

#include <mach/am_regs.h>

#include <asm/fiq.h>
#include <asm/uaccess.h>

#define MAX_FIQ 4

typedef void (*fiq_routine)(void);

static spinlock_t lock = SPIN_LOCK_UNLOCKED;
static u8 fiq_stack[4096];
static u8 fiq_index[MAX_FIQ];
static fiq_routine fiq_func[MAX_FIQ];

static void __attribute__ ((naked)) fiq_vector(void)
{
	asm __volatile__ ("mov pc, r8 ;");
}

static void __attribute__ ((naked)) fiq_isr(void)
{
    int i;
    unsigned int_status;

	asm __volatile__ (
		"mov    ip, sp;\n"
		"stmfd	sp!, {r0-r12, lr};\n"
		"sub    sp, sp, #256;\n"
		"sub    fp, sp, #256;\n");

    int_status = READ_CBUS_REG(IRQ_STATUS_REG(AM_IRQ0(0)));

    for (i=0;i<MAX_FIQ;i++) {
        if ((fiq_index[i] != 0xff) && (fiq_func[i] != NULL)) {
   	        if (int_status & (1 << IRQ_BIT(fiq_index[i]))) {
   	            fiq_func[i]();
                WRITE_MPEG_REG(IRQ_CLR_REG(AM_IRQ0(0)), 1 << IRQ_BIT(fiq_index[i]));
   	        }
   	    }
    }

	dsb();

	asm __volatile__ (
		"add	sp, sp, #256 ;\n"
		"ldmia	sp!, {r0-r12, lr};\n"
		"subs	pc, lr, #4;\n");
}

int __init init_fiq(void)
{
	struct pt_regs regs;
    int i;

	for (i=0; i<MAX_FIQ; i++) {
	    fiq_index[i] = 0xff;
	    fiq_func[i] = NULL;
	}

	/* prepare FIQ mode regs */
	memset(&regs, 0, sizeof(regs));
	regs.ARM_r8 = (unsigned long)fiq_isr;
	regs.ARM_sp = (unsigned long)fiq_stack + sizeof(fiq_stack) - 4;

	set_fiq_regs(&regs);
	set_fiq_handler(fiq_vector, 8);

    return 0;
}

void request_fiq(unsigned fiq, void (*isr)(void))
{
	int i;
	ulong flags;
	int fiq_share = 0;

    if (fiq >= AM_IRQ0(31)) {
        printk("Invalid fiq number, ignored\n");
        return;
    }

    spin_lock_irqsave(&lock, flags);

    for (i=0;i<MAX_FIQ;i++) {
        if (fiq_index[i] == fiq)
            fiq_share++;
    }

    for (i=0;i<MAX_FIQ;i++){
        if (fiq_index[i]==0xff){
            fiq_index[i] = fiq;
            fiq_func[i] = isr;

            if (fiq_share == 0) {
        	    SET_CBUS_REG_MASK(IRQ_FIQSEL_REG(fiq), 1 << IRQ_BIT(fiq));
        	    dsb();

        	    enable_fiq(fiq);
        	}
            break;
        }
    }
    
    spin_unlock_irqrestore(&lock, flags);
}
EXPORT_SYMBOL(request_fiq);

void free_fiq(unsigned fiq, void (*isr)(void))
{
    int i;
    ulong flags;
	int fiq_share = 0;

    spin_lock_irqsave(&lock, flags);

    for (i=0;i<MAX_FIQ;i++) {
        if (fiq_index[i] == fiq)
            fiq_share++;
    }

    for (i=0;i<MAX_FIQ;i++){
        if ((fiq_index[i] == fiq) && (fiq_func[i] == isr)) {
            if (fiq_share == 1) {
                disable_fiq(fiq);
                CLEAR_CBUS_REG_MASK(IRQ_FIQSEL_REG(fiq), IRQ_BIT(fiq));
            }
            fiq_index[i] = 0xff;
            fiq_func[i] = NULL;
        }
    }

    spin_unlock_irqrestore(&lock, flags);
}
EXPORT_SYMBOL(free_fiq);

arch_initcall(init_fiq);
