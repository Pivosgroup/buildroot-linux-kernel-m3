/*
 *
 * arch/arm/mach-meson/cpu.c
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

static struct clk *cpu_clk;

int meson_cpufreq_verify(struct cpufreq_policy *policy)
{
    if (policy->cpu) {
        return -EINVAL;
    }

    cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
                                 policy->cpuinfo.max_freq);

    policy->min = clk_round_rate(cpu_clk, policy->min * 1000) / 1000;
    policy->max = clk_round_rate(cpu_clk, policy->max * 1000) / 1000;
    cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
                                 policy->cpuinfo.max_freq);
    return 0;
}

static int meson_cpufreq_target(struct cpufreq_policy *policy,
                                unsigned int target_freq,
                                unsigned int relation)
{
    struct cpufreq_freqs freqs;
    int ret = 0;

    /* Ensure desired rate is within allowed range.  Some govenors
     * (ondemand) will just pass target_freq=0 to get the minimum. */
    if (target_freq < policy->min) {
        target_freq = policy->min;
    }
    if (target_freq > policy->max) {
        target_freq = policy->max;
    }

    freqs.old = clk_get_rate(cpu_clk) / 1000;
    freqs.new = clk_round_rate(cpu_clk, target_freq * 1000) / 1000;
    freqs.cpu = 0;

    if (freqs.old == freqs.new) {
        return ret;
    }

    cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
#ifdef CONFIG_CPU_FREQ_DEBUG
    printk(KERN_DEBUG "cpufreq-meson: transition: %u --> %u\n",
           freqs.old, freqs.new);
#endif
    ret = clk_set_rate(cpu_clk, freqs.new * 1000);
    cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

    return ret;
}

unsigned int meson_cpufreq_get(unsigned int cpu)
{
    unsigned long rate;

    if (cpu) {
        return 0;
    }

    rate = clk_get_rate(cpu_clk) / 1000;
    return rate;
}

static int __init meson_cpufreq_init(struct cpufreq_policy *policy)
{
    cpu_clk = clk_get_sys("a9_clk", NULL);
    if (IS_ERR(cpu_clk)) {
        return PTR_ERR(cpu_clk);
    }

    if (policy->cpu != 0) {
        return -EINVAL;
    }

    policy->cur = clk_get_rate(cpu_clk) / 1000;
    policy->min = policy->cpuinfo.min_freq = clk_round_rate(cpu_clk, 0) / 1000;
    policy->max = policy->cpuinfo.max_freq = clk_round_rate(cpu_clk, 0xffffffff) / 1000;

    /* FIXME: what's the actual transition time? */
    policy->cpuinfo.transition_latency = 20 * 1000;

    return 0;
}

static struct freq_attr *meson_cpufreq_attr[] = {
    &cpufreq_freq_attr_scaling_available_freqs,
    NULL,
};

static struct cpufreq_driver meson_cpufreq_driver = {
    .flags      = CPUFREQ_STICKY,
    .verify     = meson_cpufreq_verify,
    .target     = meson_cpufreq_target,
    .get        = meson_cpufreq_get,
    .init       = meson_cpufreq_init,
    .name       = "meson_cpufreq",
    .attr       = meson_cpufreq_attr,
};

static int __init meson_cpu_init(void)
{
    return cpufreq_register_driver(&meson_cpufreq_driver);
}

arch_initcall(meson_cpu_init);
