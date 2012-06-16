#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/nmi.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>			/* For in_interrupt() */
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/security.h>
#include <linux/bootmem.h>
#include <linux/syscalls.h>
#include <linux/kexec.h>
#include <linux/ratelimit.h>
#include <linux/kmsg_dump.h>
#include <linux/syslog.h>
#include <linux/timer.h>

#include <asm/uaccess.h>


#define MAX_PRINT_SIZE 1024
static unsigned char fiqprint_buf[MAX_PRINT_SIZE+4];
static unsigned char fiqprint_buf1[MAX_PRINT_SIZE+4];

static int fiqprint_buf_off=0;

#define FIQ_PREF "<FIQ>:"
static struct timer_list fiq_print_timer;


int fiq_vprintk(const char *fmt, va_list args);
int fiq_printk(const char *fmt, ...){
	va_list args;
	int r;
	va_start(args, fmt);
	r=fiq_vprintk(fmt,args);
	va_end(args);
	return r;
}
EXPORT_SYMBOL(fiq_printk);
int fiq_vprintk(const char *fmt, va_list args)
{
	/*must call in FIQ mode.*/
	int r;
	char *bufp=fiqprint_buf+fiqprint_buf_off;
	int buf_len=MAX_PRINT_SIZE-fiqprint_buf_off;

	if(buf_len>sizeof(FIQ_PREF)+16){
		strcpy(bufp,FIQ_PREF);
		bufp+=strlen(FIQ_PREF);
		buf_len-=strlen(FIQ_PREF);
	}
	r=vscnprintf(bufp,MAX_PRINT_SIZE-fiqprint_buf_off,fmt,args);
	fiqprint_buf_off+=r;
	if(fiqprint_buf_off>MAX_PRINT_SIZE)
		fiqprint_buf_off=0;/*overflow,drop all.*/
	return r;

}

EXPORT_SYMBOL(fiq_vprintk);

static void fiq_printk_timer(unsigned long arg){
	unsigned long flags;
	int len=0;
	if(fiqprint_buf_off>0){
		raw_local_save_flags(flags);
		local_fiq_disable();
		memcpy(fiqprint_buf1,fiqprint_buf,fiqprint_buf_off);
		len=fiqprint_buf_off;
		fiqprint_buf_off=0;
		fiqprint_buf1[len]='\n';
		fiqprint_buf1[len+1]='0';
		raw_local_irq_restore(flags);
	}
	if(len>0)
		printk("%s\n",fiqprint_buf1);
	mod_timer(&fiq_print_timer,jiffies+HZ/10);
	///printk("fiq timer fired,waper data len=%d\n",len);
}

static __init int fiq_printk_init(void)
{
	init_timer(&fiq_print_timer);
	fiq_print_timer.function = fiq_printk_timer;
    fiq_print_timer.expires = jiffies + 1;
	fiq_print_timer.data=0;
	add_timer(&fiq_print_timer);
	return 0;
}

module_init(fiq_printk_init);

