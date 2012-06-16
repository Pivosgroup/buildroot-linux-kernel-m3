#ifndef _LINUX_REBOOT_H
#define _LINUX_REBOOT_H

/*
 * Magic values required to use _reboot() system call.
 */

#define	LINUX_REBOOT_MAGIC1	0xfee1dead
#define	LINUX_REBOOT_MAGIC2	672274793
#define	LINUX_REBOOT_MAGIC2A	85072278
#define	LINUX_REBOOT_MAGIC2B	369367448
#define	LINUX_REBOOT_MAGIC2C	537993216


/*
 * Commands accepted by the _reboot() system call.
 *
 * RESTART     Restart system using default command and mode.
 * HALT        Stop OS and give system control to ROM monitor, if any.
 * CAD_ON      Ctrl-Alt-Del sequence causes RESTART command.
 * CAD_OFF     Ctrl-Alt-Del sequence sends SIGINT to init task.
 * POWER_OFF   Stop OS and remove all power from system, if possible.
 * RESTART2    Restart system using given command string.
 * SW_SUSPEND  Suspend system using software suspend if compiled in.
 * KEXEC       Restart system using a previously loaded Linux kernel
 */

#define	LINUX_REBOOT_CMD_RESTART	0x01234567
#define	LINUX_REBOOT_CMD_HALT		0xCDEF0123
#define	LINUX_REBOOT_CMD_CAD_ON		0x89ABCDEF
#define	LINUX_REBOOT_CMD_CAD_OFF	0x00000000
#define	LINUX_REBOOT_CMD_POWER_OFF	0x4321FEDC
#define	LINUX_REBOOT_CMD_RESTART2	0xA1B2C3D4
#define	LINUX_REBOOT_CMD_SW_SUSPEND	0xD000FCE2
#define	LINUX_REBOOT_CMD_KEXEC		0x45584543
#define LINUX_REBOOT_CMD_AML_SUSPEND    0xDFCE0321
/*
 * Commands accepted by the arm_machine_restart() system call.
 *
 * AMLOGIC_NORMAL_BOOT     			Restart system normally.
 * AMLOGIC_FACTORY_RESET_REBOOT      Restart system into recovery factory reset.
 * AMLOGIC_UPDATE_REBOOT			Restart system into recovery update.
 * AMLOGIC_CHARGING_REBOOT     		Restart system into charging.
 * AMLOGIC_CRASH_REBOOT   			Restart system with system crach.
 * AMLOGIC_FACTORY_TEST_REBOOT    	Restart system into factory test.
 * AMLOGIC_SYSTEM_SWITCH_REBOOT  	Restart system for switch other OS.
 * AMLOGIC_SAFE_REBOOT       			Restart system into safe mode.
 * AMLOGIC_LOCK_REBOOT  			Restart system into lock mode.
 * elvis.yu---elvis.yu@amlogic.com
 */
#define	AMLOGIC_NORMAL_BOOT					0x0
#define	AMLOGIC_FACTORY_RESET_REBOOT		0x01010101
#define	AMLOGIC_UPDATE_REBOOT				0x02020202
#define	AMLOGIC_CHARGING_REBOOT				0x03030303
#define	AMLOGIC_CRASH_REBOOT				0x04040404
#define	AMLOGIC_FACTORY_TEST_REBOOT		0x05050505
#define	AMLOGIC_SYSTEM_SWITCH_REBOOT		0x06060606
#define	AMLOGIC_SAFE_REBOOT					0x07070707
#define	AMLOGIC_LOCK_REBOOT					0x08080808

#ifdef __KERNEL__

#include <linux/notifier.h>

extern int register_reboot_notifier(struct notifier_block *);
extern int unregister_reboot_notifier(struct notifier_block *);


/*
 * Architecture-specific implementations of sys_reboot commands.
 */

extern void machine_restart(char *cmd);
extern void machine_halt(void);
extern void machine_power_off(void);

extern void machine_shutdown(void);
struct pt_regs;
extern void machine_crash_shutdown(struct pt_regs *);

/* 
 * Architecture independent implemenations of sys_reboot commands.
 */

extern void kernel_restart_prepare(char *cmd);
extern void kernel_restart(char *cmd);
extern void kernel_halt(void);
extern void kernel_power_off(void);

extern int C_A_D; /* for sysctl */
void ctrl_alt_del(void);

#define POWEROFF_CMD_PATH_LEN	256
extern char poweroff_cmd[POWEROFF_CMD_PATH_LEN];

extern int orderly_poweroff(bool force);

/*
 * Emergency restart, callable from an interrupt handler.
 */

extern void emergency_restart(void);
#include <asm/emergency-restart.h>

#endif

#endif /* _LINUX_REBOOT_H */
