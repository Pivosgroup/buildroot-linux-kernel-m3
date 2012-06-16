/*
 * (C) Copyright 2010, Amlogic, Inc. http://www.amlogic.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR /PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <mach/memory.h>
#include <mach/pctl.h>
#include <mach/dmc.h>
#include "sleep.h"

#define HIU_GCLK_MPEG       (0x50<<2)
#define HHI_MPEG_CLK_CNTL   (0x5d<<2)
#define HIU_DDR_PLL_CTRL    (0x68<<2)
#define A9_AUTO_CLK0        (0x78<<2)
#define HIU_DDR_RESET       (0x102<<2)
#define LED_PWM_REG0        (0x11da<<2)

    .text
/*
 * Move Meson into deep sleep state
 *
 * Note: This code is copied to internal SRAM by PM code. When the Meson
 *     wakes up it continues execution at the point it went to sleep.
 */
ENTRY(meson_cpu_suspend)
    stmfd   sp!, {r0-r12, lr}        @ save registers on stack

    ldmia   r0, {r0-r6}
    /*
        r0: pctl_reg_base;
        r1: mmc_reg_base;
        r2: hiu_reg_base;
        r3: power_key;
        r4: ddr_clk;
        r5: ddr_reg_backup;
        r6: core_voltage_adjust;
    */

    /* r9: led_pwm_reg0, r10: value of led_pwm_reg0*/
    mov     r9, #0
    movw    r9, #LED_PWM_REG0
    add     r9, r9, r2
    ldr     r10, [r9]

    /* turn on auto byte bypass */
    ldr     r8, [r0, #PCTL_IOCR_ADDR]
    orr     r8, r8, #(1<<25)
    str     r8, [r0, #PCTL_IOCR_ADDR]

    /*
     * Switch DDR to self-refresh mode.
     */
    mov     r8, #0x3
    str     r8, [r0, #PCTL_SCTL_ADDR]

    mov     r8, #MS_DLY
0:  subs    r8, r8, #0x1
    bne     0b

    /* DDR PHY power saving*/
    ldr     r8, [r1, #MMC_PHY_CTRL-0x1000]
    orr     r8, r8, #(1<<0)
    orr     r8, r8, #(1<<8)
    orr     r8, r8, #(1<<13)
    str     r8, [r1, #MMC_PHY_CTRL-0x1000]

    /* hold dll reset */
    ldr     r8, [r0, #PCTL_PHYCR_ADDR]
    bic     r8, r8, #(1<<6)
    str     r8, [r0, #PCTL_PHYCR_ADDR]

    /* enable dll bypass */
    ldr     r8, [r0, #PCTL_DLLCR9_ADDR]
    orr     r8, r8, #(1<<31)
    str     r8, [r0, #PCTL_DLLCR9_ADDR]

#ifdef TURN_OFF_DDR_PLL
    /* turn off ddr pll */
    ldr     r8, [r2, #HIU_DDR_PLL_CTRL]
    orr     r8, r8, #(1<<15)
    str     r8, [r2, #HIU_DDR_PLL_CTRL]
#else
    /* lower ddr pll */    
    ldr     r7, [r2, #HIU_DDR_PLL_CTRL]
    str     r4, [r2, #HIU_DDR_PLL_CTRL]
#endif

    /* ddr power gate */
    ldr     r8, [r2, #HIU_GCLK_MPEG]
    bic     r8, r8, #0x41
    str     r8, [r2, #HIU_GCLK_MPEG]

    /* lower core voltage */
    bic     r8, r10, #0x0f
    orr     r8, r8, r6
    str     r8, [r9]

    /* System goes to sleep beyond after this instruction */
    ldr     r8, [r2, #A9_AUTO_CLK0]
    orr     r8, r8, #(1<<1)
    str     r8, [r2, #A9_AUTO_CLK0]
    bic     r8, r8, #(1<<1)
    str     r8, [r2, #A9_AUTO_CLK0]
    wfi
    ldr     r8, [r2, #A9_AUTO_CLK0]
    bic     r8, r8, #(1<<0)
    str     r8, [r2, #A9_AUTO_CLK0]
    /* Wake up from sleep */

    /* restore core voltage */
    str     r10, [r9]
    mov     r8, #VOLTAGE_DLY
2:  subs    r8, r8, #0x1
    bne     2b

    /* ddr power gate */
    ldr     r8, [r2, #HIU_GCLK_MPEG]
    orr     r8, r8, #0x41
    str     r8, [r2, #HIU_GCLK_MPEG]

#ifdef TURN_OFF_DDR_PLL
    /* turn on ddr pll */
    ldr     r8, [r2, #HIU_DDR_PLL_CTRL]
    bic     r8, r8, #(1<<15)
    str     r8, [r2, #HIU_DDR_PLL_CTRL] 
#else
    /* restore ddr pll */
    str     r7, [r2, #HIU_DDR_PLL_CTRL]
#endif

    /* Wait for PLL to lock */
    mov     r8, #MS_DLY
3:  subs    r8, r8, #0x1
    bne     3b

    /* disable dll bypass */
    ldr     r8, [r0, #PCTL_DLLCR9_ADDR]
    bic     r8, r8, #(1<<31)
    str     r8, [r0, #PCTL_DLLCR9_ADDR]

    /* release reset */
    ldr     r8, [r0, #PCTL_PHYCR_ADDR]
    orr     r8, r8, #(1<<6)
    str     r8, [r0, #PCTL_PHYCR_ADDR]

    /* DDR PHY leave power saving mode*/
    ldr     r8, [r1, #MMC_PHY_CTRL-0x1000]
    bic     r8, r8, #(1<<8)
    str     r8, [r1, #MMC_PHY_CTRL-0x1000]

    /* ddr back to normal mode */
    mov     r8, #0x4
    str     r8, [r0, #PCTL_SCTL_ADDR]

    mov     r8, #MS_DLY
9:  subs    r8, r8, #0x1
    bne     9b

    /* Restore registers and return */
    ldmfd   sp!, {r0-r12, pc}
ENDPROC(meson_cpu_suspend)

ENTRY(meson_cpu_suspend_sz)
    .word    . - meson_cpu_suspend
ENDPROC(meson_cpu_suspend_sz)
