/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 * drivers/char/am_uart.c
 *
 * (C) 2003, ARC International

 * Driver for console on serial interface for the ArcAngel3 board. We
 * use VUART0 as the console device.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/uaccess.h>

#include <mach/hardware.h>
#include <mach/irqs.h>

#include "am_uart.h"

#ifdef CONFIG_UART_USE_FIQ
#define FIQ_UART
#endif

#ifdef FIQ_UART
#include <asm/fiq.h>
#endif
static unsigned long write_lock;

static struct am_uart am_uart_info[NR_PORTS];   //the value of NR_PORTS is given in the .config

struct am_uart *IRQ_ports[NR_IRQS];

#if defined(CONFIG_ARCH_MESON3)
static unsigned int uart_irqs[NR_PORTS] = {INT_UART_AO, INT_UART_0, INT_UART_1, INT_UART_2};
static am_uart_t *uart_addr[NR_PORTS] = {UART_BASEADDRAO, UART_BASEADDR0, UART_BASEADDR1, UART_BASEADDR2};
static unsigned int uart_FIFO_max_cnt[NR_PORTS] = {64,128,64,64};
#else
static unsigned int uart_irqs[NR_PORTS] = { INT_UART,INT_UART_1 };
static am_uart_t *uart_addr[NR_PORTS] = { UART_BASEADDR0,UART_BASEADDR1 };
static unsigned int uart_FIFO_max_cnt[NR_PORTS] = {64,64};
#endif
static int console_inited[2] ={ 0,0};   /* have we initialized the console already? */
static int default_index = 0;   /* have we initialized the console index? */
static struct tty_driver *am_uart_driver;

#ifdef FIQ_UART
char * fiq_buf=NULL;
int fiq_read;
int fiq_write;
volatile int fiq_cnt;
static DEFINE_SPINLOCK(uart_lock);
#define UART_DATA_LOG
#define PRINT_DEBUG
#endif

#define MAX_NAMED_UART 4

#ifndef outl
#define outl(v,addr)    __raw_writel(v,(unsigned long)addr)
#endif

#ifndef inl
#define inl(addr)   __raw_readl((unsigned long)addr)
#endif
#define in_l(addr)  inl((unsigned long )addr)
#define out_l(v,addr)   outl(v,(unsigned long )addr)
#define clear_mask(addr,mask)       out_l(in_l(addr) &(~(mask)),addr)
#define set_mask(addr,mask)     out_l(in_l(addr) |(mask),addr)
/*
 * tmp_buf is used as a temporary buffer by serial_write. We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
//static unsigned char tmp_buf[SERIAL_XMIT_SIZE]; /* This is cheating */
DECLARE_MUTEX(tmp_buf_sem);

#ifndef MIN
#define MIN(a,b)    ((a) < (b) ? (a) : (b))
#endif

static void raw_num(unsigned int num, int zero_ok);
/*
 * ------------------------------------------------------------
 * am_uart_stop() and am_uart_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */

static void am_uart_stop(struct tty_struct *tty)
{
}

/*
 * Output a single character, using UART polled mode.
 * This is used for console output.
 */
void am_uart_put_char(int index,char ch)
{
    am_uart_t *uart = uart_addr[index];
    unsigned int fifo_level = uart_FIFO_max_cnt[index];

    fifo_level = (fifo_level-1)<<8;
    
    /* check if TXEMPTY (bit 7) in the status reg. is 1. Else, we
     * aren't supposed to write right now. Wait for some time.
     */
    while (!((__raw_readl(&uart->status) & 0xff00)< fifo_level)) ;
    __raw_writel(ch, &uart->wdata);
}

void am_uart_print(int index,char *buffer)
{
    while (*buffer) {
        am_uart_put_char(index,*buffer++);
    }
}

static void am_uart_start(struct tty_struct *tty)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;
    am_uart_t *uart = uart_addr[info->line];
    unsigned long mode;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif
    mutex_lock(&info->info_mutex);
    mode = __raw_readl(&uart->mode);
    mode |=UART_TXENB | UART_RXENB;
    __raw_writel(mode, &uart->mode);
    mutex_unlock(&info->info_mutex);
}

static void uart_tasklet_action(unsigned long data)
{
	struct am_uart *info = (struct am_uart *)data;
	tty_wakeup(info->tty);
}

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static inline void am_uart_sched_event(struct am_uart *info, int event)
{
    info->event |= 1 << event;
      schedule_work(&info->tqueue);
}

static void receive_chars(struct am_uart *info, struct pt_regs *regs,
              unsigned short rx)
{
    am_uart_t *uart = uart_addr[info->line];
    struct tty_struct *tty = info->tty;
    int status;
    int mode;
    char ch;
    unsigned long flag = TTY_NORMAL;  
    unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

    fifo_level = (fifo_level-1);
    
	if (!info->tty) {
    	return;
	}
        tty->low_latency = 1;
	status = __raw_readl(&uart->status);
	if (status & UART_OVERFLOW_ERR) {
              info->rx_error |= UART_OVERFLOW_ERR;
              mode = __raw_readl(&uart->mode) | UART_CLEAR_ERR;
		__raw_writel(mode, &uart->mode);
	} else if (status & UART_FRAME_ERR) {
		info->rx_error |= UART_FRAME_ERR;
		//uart->mode|= UART_CLEAR_ERR;  
		mode = __raw_readl(&uart->mode) | UART_CLEAR_ERR;
		__raw_writel(mode, &uart->mode);
	} else if (status & UART_PARITY_ERR) {
		info->rx_error |= UART_PARITY_ERR;
		mode = __raw_readl(&uart->mode) | UART_CLEAR_ERR;
		__raw_writel(mode, &uart->mode);
	}
    	do {    
            ch = (rx & 0x00ff);
            tty_insert_flip_char(tty,ch,flag);
                     
    		info->rx_cnt++;
                       
    		if ((status = __raw_readl(&uart->status) & 0x00ff))
    			rx = __raw_readl(&uart->rdata);
    		/* keep reading characters till the RxFIFO becomes empty */
    	} while (status);
    
	return;
}

static void BH_receive_chars(struct am_uart *info)
{
	struct tty_struct *tty = info->tty;
	unsigned long flag;
	int status;
	int cnt = info->rx_cnt;
    
    if (!tty) {
		//printk("Uart : missing tty on line %d\n", info->line);
        goto clear_and_exit;
    }
       tty->low_latency = 1;	// Originally added by Sameer, serial I/O slow without this 
    flag = TTY_NORMAL;
	status = info->rx_error;
    if (status & UART_OVERFLOW_ERR) {
        printk
            ("Uart %d Driver: Overflow Error while receiving a character\n", info->line);
        flag = TTY_OVERRUN;
    } else if (status & UART_FRAME_ERR) {
        printk
            ("Uart %d Driver: Framing Error while receiving a character\n", info->line);
        flag = TTY_FRAME;
    } else if (status & UART_PARITY_ERR) {
        printk
            ("Uart %d Driver: Parity Error while receiving a character\n", info->line);
        flag = TTY_PARITY;
    }

        info->rx_error = 0;
        if(cnt)
        {
            info->rx_cnt = 0;
            tty_flip_buffer_push(tty);
        }
clear_and_exit:
        if (info->rx_cnt>0)
        am_uart_sched_event(info, 0);
    return;
}

static void transmit_chars(struct am_uart *info)
{
    am_uart_t *uart = uart_addr[info->line];
    unsigned int ch;
    unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

    fifo_level = (fifo_level-1)<<8;
    if(info->line >= 1)
        printk("%s,cnt = %d\n", __FUNCTION__,info->xmit_cnt);
#ifdef UART_DATA_LOG
    if(info->line >=1)
	am_uart_put_char(0,'@');
#endif

    mutex_lock(&info->info_mutex);
    if (info->x_char) {
        __raw_writel(info->x_char, &uart->wdata);
        info->x_char = 0;
        goto clear_and_return;
    }

    if ((info->xmit_cnt <= 0) || info->tty->stopped)
        goto clear_and_return;
    set_bit(info->line, &write_lock);
    while (info->xmit_cnt > 0) {
        if (((__raw_readl(&uart->status) & 0xff00) < fifo_level)) {
            ch = info->xmit_buf[info->xmit_rd];
            __raw_writel(ch, &uart->wdata);
            info->xmit_rd = (info->xmit_rd+1) & (SERIAL_XMIT_SIZE - 1);
            info->xmit_cnt--;
        }
        else
            break;
    }
    clear_bit(info->line, &write_lock);
clear_and_return:
    mutex_unlock(&info->info_mutex);

    if (info->xmit_cnt > 0)
        am_uart_sched_event(info, 0);

    return;
}

#ifdef FIQ_UART
void am_uart_fiq_interrupt(void)
{
    am_uart_t *uart = uart_addr[1];
    char ch;
    int cnt;
    register int reg_write;
    register int reg_cnt,add_cnt;
    unsigned int fifo_level = uart_FIFO_max_cnt[1];

    fifo_level = (fifo_level-1);
    
    if(spin_trylock(&uart_lock))
    {
        reg_write = fiq_write;
	mb();
        reg_cnt = fiq_cnt;
	add_cnt = 0;
        cnt = __raw_readl(&uart->status) & fifo_level;
        while (cnt--){
       	    ch = __raw_readl(&uart->rdata) & 0xff;

	    fiq_buf[reg_write]=ch;
	    reg_write= (reg_write+1) & (SERIAL_XMIT_SIZE - 1);
	    add_cnt++;
	    if ((reg_cnt+add_cnt)>= SERIAL_XMIT_SIZE) {
       	    	am_uart_put_char(0,'^');
        	break;
            }
        }

	fiq_write = reg_write;
	mb();	
	fiq_cnt = fiq_cnt+add_cnt;
	mb();
#ifdef UART_DATA_LOG
        am_uart_put_char(0,'&');
	raw_num(fiq_cnt,1);
#endif	
	spin_unlock(&uart_lock);
    }
    WRITE_MPEG_REG(IRQ_CLR_REG(uart_irqs[1]), 1 << IRQ_BIT(uart_irqs[1]));
}

static void am_uart_timer_sr(unsigned long param)
{
	struct am_uart *info=(struct am_uart *)param;
	struct tty_struct *tty;
	am_uart_t *uart = NULL;
  	int cnt,ch,i;
	unsigned long flags;
	static int last_cnt=0xffff;
	unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

       fifo_level = (fifo_level-1);

       if (!info)
           return;

      	tty = info->tty;
	if (!tty)
	{   
	    goto exit_timer;
	}

	uart = (am_uart_t *) info->port;
	if (!uart)
	    goto exit_timer;

	spin_lock_irqsave(&uart_lock, flags);
	mb();
	cnt = fiq_cnt;
	if(cnt)
        {
            if(fiq_read+cnt > SERIAL_XMIT_SIZE)
            {
                tty_insert_flip_string(tty,fiq_buf+fiq_read,SERIAL_XMIT_SIZE-fiq_read);
		tty_insert_flip_string(tty,fiq_buf,cnt-(SERIAL_XMIT_SIZE-fiq_read));
            }
	    else
	    {
		tty_insert_flip_string(tty,fiq_buf+fiq_read,cnt);
            }
            fiq_read = (fiq_read+cnt) & (SERIAL_XMIT_SIZE - 1);
            mb();
            fiq_cnt = fiq_cnt-cnt; 
            mb();
            spin_unlock_irqrestore(&uart_lock, flags);
            
	    last_cnt = cnt;
    	    tty_flip_buffer_push(tty);
#ifdef UART_DATA_LOG
            am_uart_put_char(0,'*');
	    raw_num(cnt,1);
#endif
       }
       else
       {
	    if(last_cnt==0)
	    {
	    cnt = __raw_readl(&uart->status) & fifo_level;
	    if(cnt)
            {
		    i = cnt;
       	            while (i--){
		    ch = __raw_readl(&uart->rdata) & 0xff;
		    tty_insert_flip_char(tty,ch,TTY_NORMAL);
		}
	        }
            }
            spin_unlock_irqrestore(&uart_lock, flags);

            last_cnt = 0;
	    if(cnt)
            {
		tty_flip_buffer_push(tty);
#ifdef UART_DATA_LOG             
            	am_uart_put_char(0,'%');
	        raw_num(cnt,1);
#endif
	    }
       }

exit_timer:
	mod_timer(&info->timer, jiffies+msecs_to_jiffies(1));

	return;
}
#endif

/*
 * This is the serial driver's generic interrupt routine
 */
static irqreturn_t am_uart_interrupt(int irq, void *dev, struct pt_regs *regs)
{
    struct am_uart *info=(struct am_uart *)dev;

    am_uart_t *uart = NULL;
	struct tty_struct *tty = NULL;
    unsigned int fifo_level = uart_FIFO_max_cnt[info->line];
    unsigned int ch;

    fifo_level = (fifo_level-1)<<8;

    if (!info)
       goto out;

    tty = info->tty;
    if (!tty)
    {   
        goto out;
    }
    uart = (am_uart_t *) info->port;
    if (!uart)
		goto out;
#if 1
    if (__raw_readl(&uart->mode) & UART_TXENB &&
        (__raw_readl(&uart->mode) & UART_TXINT_EN) && !test_bit(info->line, &write_lock)
        /*&& (__raw_readl(&uart->status) & UART_TXEMPTY)*/){
	    /*if(1 == info->line)
            am_uart_put_char(0,'*');
        else if (2 == info->line)
            am_uart_put_char(0,'$');
        else if (3 <= info->line)
            am_uart_put_char(0,'=');*/
        while (info->xmit_cnt > 0) {
            if (((__raw_readl(&uart->status) & 0xff00) < fifo_level)) {
                ch = info->xmit_buf[info->xmit_rd];
                __raw_writel(ch, &uart->wdata);
                info->xmit_rd = (info->xmit_rd+1) & (SERIAL_XMIT_SIZE - 1);
                info->xmit_cnt--;
            }
            else
                break;
        }
        if((SERIAL_XMIT_SIZE - info->xmit_cnt - 1) < WAKEUP_CHARS)
            tasklet_schedule(&info->tlet);
    }
#endif
    if ((__raw_readl(&uart->mode) & UART_RXENB)
	    && !(__raw_readl(&uart->status) & UART_RXEMPTY)) {
		receive_chars(info, 0, __raw_readl(&uart->rdata));
	}

out:	
    am_uart_sched_event(info, 0);
    return IRQ_HANDLED;
}

static void am_uart_workqueue(struct work_struct *work)
{
    struct am_uart *info=container_of(work,struct am_uart,tqueue);
    am_uart_t *uart = NULL;
    struct tty_struct *tty = NULL;
    unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

    fifo_level = (fifo_level-1)<<8;
    
    if (!info)
        goto out;
    tty = info->tty;
    if (!tty)
        goto out;
    uart = (am_uart_t *) info->port;
    if (!uart)
        goto out;

    if (info->rx_cnt>0)
        BH_receive_chars(info);
#if 0
    if ((__raw_readl(&uart->mode) & UART_TXENB)
        /*&&((__raw_readl(&uart->status) & 0xff00) < fifo_level)*/) {
        transmit_chars(info);
    }
#endif
    if (__raw_readl(&uart->status) & UART_FRAME_ERR)
        __raw_writel(__raw_readl(&uart->status) & ~UART_FRAME_ERR,
               &uart->status);
    if (__raw_readl(&uart->status) & UART_OVERFLOW_ERR)
        __raw_writel(__raw_readl(&uart->status) & ~UART_OVERFLOW_ERR,
               &uart->status);
      out:
    return ;
}

static void change_speed(struct am_uart *info, unsigned long newbaud)
{
    am_uart_t *uart = uart_addr[info->line];
    unsigned short port;
    unsigned long tmp;
    unsigned long tmp_intctl;
//    printk("\n\n\n\n\n\n\nchange_speed...\n");
//return ;
#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif

    if (!info->tty || !info->tty->termios)
        return;

    if (!(port = info->port) || newbaud==0)
        return;
    msleep(1);
    printk("Changing baud to %d\n", (int)newbaud);

    while (!(__raw_readl(&uart->status) & UART_TXEMPTY)) {
    }

    tmp = (clk_get_rate(clk_get_sys("clk81", NULL)) / (newbaud * 4)) - 1;
    tmp_intctl = tmp >> 12;
    tmp_intctl &= 0xf;

    info->baud = (int)newbaud;

	if(tmp_intctl != 0x0){
		tmp_intctl = (__raw_readl(&uart->intctl) & 0xff0fffff) | (tmp_intctl <<20);
		__raw_writel(tmp_intctl, &uart->intctl);
	}

	tmp = (__raw_readl(&uart->mode) & ~0xfff) | (tmp & 0xfff);
	__raw_writel(tmp, &uart->mode);
    msleep(1);
}

static int startup(struct am_uart *info)
{
    am_uart_t *uart = uart_addr[info->line];
    unsigned long mode;
    if (info->flags & ASYNC_INITIALIZED)
        return 0;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif

    if (!info->xmit_buf) {
        info->xmit_buf = (unsigned char *)get_zeroed_page(GFP_KERNEL);
        if (!info->xmit_buf)
            return -ENOMEM;
    }

    if (!info->rx_buf) {
	info->rx_buf = (unsigned char *)get_zeroed_page(GFP_KERNEL);
	if (!info->rx_buf)
	    return -ENOMEM;
    }
	
    mutex_lock(&info->info_mutex);

    mode = __raw_readl(&uart->mode);
	mode |= UART_RXENB | UART_TXENB;
	__raw_writel(mode, &uart->mode);

    if (info->tty)
        clear_bit(TTY_IO_ERROR, &info->tty->flags);

    info->xmit_cnt = info->xmit_rd = info->xmit_wr = 0;
    info->rx_cnt = info->rx_head = info->rx_tail = 0;

    mutex_unlock(&info->info_mutex);
    /*
     * and set the speed of the serial port
     */
	//change_speed(info, BASE_BAUD);

    info->flags |= ASYNC_INITIALIZED;

    return 0;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct am_uart *info)
{
    if (!(info->flags & ASYNC_INITIALIZED))
        return;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif

    if (info->xmit_buf) {
        free_page((unsigned long)info->xmit_buf);
        info->xmit_buf = 0;
    }
       if (info->rx_buf) {
		free_page((unsigned long)info->rx_buf);
		info->rx_buf = 0;
	}

    if (info->tty)
        set_bit(TTY_IO_ERROR, &info->tty->flags);

    info->flags &= ~ASYNC_INITIALIZED;
}

static void am_uart_set_ldisc(struct tty_struct *tty)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;
    info->is_cons = (tty->termios->c_line == N_TTY);

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s,is_cons = %d\n", __FUNCTION__,info->is_cons);
#endif
}

static void am_uart_flush_chars(struct tty_struct *tty)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;
    am_uart_t *uart = uart_addr[info->line];
    unsigned long c;
    unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

    fifo_level = (fifo_level-1)<<8;
    
#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s,cnt = %d\n", __FUNCTION__,info->xmit_cnt);
#endif
    
    mutex_lock(&info->info_mutex);
    if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
        !info->xmit_buf)
    {
        mutex_unlock(&info->info_mutex);
        tty_wakeup(tty);
        return;
    }

    /* Enable transmitter */
    set_bit(info->line, &write_lock);
    while (info->xmit_cnt > 0) {
        if (((__raw_readl(&uart->status) & 0xff00) < fifo_level)) {
            c = info->xmit_buf[info->xmit_rd];
            __raw_writel(c, &uart->wdata);
            info->xmit_rd = (info->xmit_rd+1)& (SERIAL_XMIT_SIZE - 1);
            info->xmit_cnt--;
        }
        else
            break;
    }
    clear_bit(info->line, &write_lock);


    mutex_unlock(&info->info_mutex);
    tty_wakeup(tty);
}

static int am_uart_write(struct tty_struct *tty, const unsigned char *buf,
             int count)
{
    int c, total = 0;
    struct am_uart *info = (struct am_uart *)tty->driver_data;
    am_uart_t *uart = uart_addr[info->line];
    unsigned int ch;
    unsigned int fifo_level = uart_FIFO_max_cnt[info->line];
    unsigned int status;
   if(info->line >= 1)
        printk(KERN_DEBUG "%s, count=%d\n", __FUNCTION__, count);
    fifo_level = (fifo_level - 1) << 8;
    
    if (!tty || !info->xmit_buf || (count <= 0))
        return 0;

    //mutex_lock(&info->info_mutex);
    set_bit(info->line, &write_lock);
    total = min_t(int, count, (SERIAL_XMIT_SIZE - info->xmit_cnt - 1));
    c = min_t(int, total, (SERIAL_XMIT_SIZE - info->xmit_wr));
    memcpy(&info->xmit_buf[info->xmit_wr], buf, c);
    if (total > c) {
        memcpy(&info->xmit_buf[0], &buf[c], total - c);
    }

    info->xmit_wr = (info->xmit_wr + total) & (SERIAL_XMIT_SIZE - 1);
    info->xmit_cnt += total;
    if(info->line >= 1)
        printk(KERN_DEBUG ",line=%d,total = %d, cnt = %d\n", info->line, total, info->xmit_cnt);
    //clear_mask(&uart->mode, UART_TXINT_EN);
    
    //if(info->line >= 1)
    //    printk("wstart");
    while (info->xmit_cnt > 0) {
        status = (__raw_readl(&uart->status) );
        if (((status & 0xff00) < fifo_level)) {
            ch = info->xmit_buf[info->xmit_rd];
            __raw_writel(ch, &uart->wdata);
            info->xmit_rd = (info->xmit_rd+1) & (SERIAL_XMIT_SIZE - 1);
            info->xmit_cnt--;
        }
        else {
            //if (info->line >= 1)
			//printk("status=0x%x,fifo_level=0x%x, info->xmit_cnt=%d, info->xmit_rd=%d\n",
			        //status, fifo_level, info->xmit_cnt, info->xmit_rd);
            break; 
		}
    }
    
    //if(info->line >= 1)
    //    printk("wstop\n");
    clear_bit(info->line, &write_lock);
    //set_mask(&uart->mode, UART_TXINT_EN);

    //mutex_unlock(&info->info_mutex);

    if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped) {
        am_uart_sched_event(info, 0);
    }
    return total;
}

static int am_uart_write_room(struct tty_struct *tty)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;
    int ret;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif

    ret = (SERIAL_XMIT_SIZE - info->xmit_cnt) - 1;
    if (ret < 0)
        return 0;
    else
        return ret;
}

static int am_uart_chars_in_buffer(struct tty_struct *tty)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s, %d\n", __FUNCTION__,info->xmit_cnt);
#endif

    return info->xmit_cnt;
}

static void am_uart_flush_buffer(struct tty_struct *tty)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;
    mutex_lock(&info->info_mutex);
    info->xmit_cnt = info->xmit_rd = info->xmit_wr = 0;
    mutex_unlock(&info->info_mutex);
    tty_wakeup(tty);
}

/*
 * ------------------------------------------------------------
 * am_uart_throttle()
 *
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */

static void am_uart_throttle(struct tty_struct *tty)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif

    if (I_IXOFF(tty))
        info->x_char = STOP_CHAR(tty);

    /* Turn off RTS line (do this atomic) */
}

static void am_uart_unthrottle(struct tty_struct *tty)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif

    if (I_IXOFF(tty)) {
        if (info->x_char)
            info->x_char = 0;
        else
            info->x_char = START_CHAR(tty);
    }

    /* Assert RTS line (do this atomic) */
}

/*
 * ------------------------------------------------------------
 * am_uart_ioctl() and friends
 * ------------------------------------------------------------
 */

static int am_uart_ioctl(struct tty_struct *tty, struct file *file,
             unsigned int cmd, unsigned long arg)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;
    unsigned long tmpul;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif

    switch (cmd) {
    case GET_BAUD:
#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("get baud: %ld\n", info->baud);
#endif
        return copy_to_user((void *)arg, &(info->baud),
                    sizeof(unsigned long)) ? -EFAULT : 0;

    case SET_BAUD:
#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("set baud\n");
#endif
        if (copy_from_user
            ((void *)&tmpul, (void *)arg, sizeof(unsigned long)))
            return -EFAULT;

        change_speed(info, tmpul);
        return 0;

    default:
        return -ENOIOCTLCMD;
    }

}

static void am_uart_set_termios(struct tty_struct *tty,
                struct ktermios *old_termios)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;
    am_uart_t *uart = uart_addr[info->line];
    unsigned long tmp;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s,new_cflag = 0x%x,old_cflag = 0x%x\n", __FUNCTION__,tty->termios->c_cflag,old_termios->c_cflag);
#endif

    if (tty->termios->c_cflag == old_termios->c_cflag)
        return;

	change_speed(info, tty->termios->c_ispeed);

    if ((old_termios->c_cflag & CRTSCTS) &&
        !(tty->termios->c_cflag & CRTSCTS)) {
        tty->hw_stopped = 0;
        am_uart_start(tty);
    }

    tmp = __raw_readl(&uart->mode);
    if (tty->termios->c_cflag & PARENB) {
		tmp |= (1 << 19);
		if (tty->termios->c_cflag & PARODD){
	   		tmp |= (1 << 18);
        }
		else{
            tmp &= ~(1 << 18);
        }
    } else {
        tmp &= ~(1 << 19);
    }
	__raw_writel(tmp, &uart->mode);
}

/*
 * ------------------------------------------------------------
 * am_uart_close()
 *
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent. Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void am_uart_close(struct tty_struct *tty, struct file *filp)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;

    if (tty_hung_up_p(filp)) {
        return;
    }
#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif

    if ((tty->count == 1) && (info->count != 1)) {
        /*
         * Uh, oh. tty->count is 1, which means that the tty
         * structure will be freed. Info->count should always
         * be one in these conditions. If it's greater than
         * one, we've got real problems, since it means the
         * serial port won't be shutdown.
         */
        printk("am_uart_close: bad serial port count; tty->count is 1, "
               "info->count is %d\n", info->count);
        info->count = 1;
    }
    if (--info->count < 0) {
        printk("am_uart_close: bad serial port count for ttyS%d: %d\n",
               info->line, info->count);
        info->count = 0;
    }
    if (info->count) {
        return;
    }

    info->flags |= ASYNC_CLOSING;

    /*
     * Now we wait for the transmit buffer to clear; and we notify
     * the line discipline to only process XON/XOFF characters.
     */
    tty->closing = 1;
    if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
        tty_wait_until_sent(tty, info->closing_wait);
    /*
     * At this point we stop accepting input. To do this, we
     * disable the receive line status interrupts, and tell the
     * interrupt driver to stop checking the data ready bit in the
     * line status register.
     */
    info->flags &= ~(ASYNC_NORMAL_ACTIVE | ASYNC_CLOSING);
    wake_up_interruptible(&info->close_wait);

    shutdown(info);
    tty_ldisc_flush(tty);
    tty->closing = 0;
    info->event = 0;
    info->tty = 0;


    if (info->blocked_open) {
        if (info->close_delay) {
            printk("%s\n", __FUNCTION__);
            msleep_interruptible(jiffies_to_msecs
                         (info->close_delay));
        }
        wake_up_interruptible(&info->open_wait);
    }
    info->flags &= ~(ASYNC_NORMAL_ACTIVE | ASYNC_CLOSING);
    wake_up_interruptible(&info->close_wait);

}

/*
 * am_uart_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
void am_uart_hangup(struct tty_struct *tty)
{
    struct am_uart *info = (struct am_uart *)tty->driver_data;


    printk("%s\n", __FUNCTION__);
    am_uart_flush_buffer(tty);
    shutdown(info);
    info->event = 0;
    info->count = 0;
    info->tty = 0;
    wake_up_interruptible(&info->open_wait);
}

/*
 *  am_uart_open and friends
 */
static int block_til_ready(struct tty_struct *tty, struct file *filp,
               struct am_uart *info)
{
    DECLARE_WAITQUEUE(wait, current);
    int retval;

#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif

    /*
     * If the device is in the middle of being closed, then block
     * until it's done, and then try again.
     */

    if (info->flags & ASYNC_CLOSING) {

        interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
        if (info->flags & ASYNC_HUP_NOTIFY)
            return -EAGAIN;
        else
            return -ERESTARTSYS;
#else
        return -EAGAIN;
#endif
    }

    /*
     * If non-blocking mode is set, or the port is not enabled,
     * then make the check up front and then exit.
     */
    if ((filp->f_flags & O_NONBLOCK) || (tty->flags & (1 << TTY_IO_ERROR))) {

#if 0
        if (filp->f_flags & O_NONBLOCK)
            printk("Non blocking return\n");
        if (tty->flags & (1 << TTY_IO_ERROR))
            printk("TTY_IO_ERROR return\n");
#endif
        info->flags |= ASYNC_NORMAL_ACTIVE;
        return 0;
    }

    /*
     * Block waiting for the carrier detect and the line to become
     * free (i.e., not in use by the callout). While we are in
     * this loop, info->count is dropped by one, so that
     * am_uart_close() knows when to free things. We restore it upon
     * exit, either normal or abnormal.
     */

    retval = 0;
    add_wait_queue(&info->open_wait, &wait);

    info->count--;
    info->blocked_open++;

    while (1) {
        set_current_state(TASK_INTERRUPTIBLE);

        if (tty_hung_up_p(filp) || !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
            if (info->flags & ASYNC_HUP_NOTIFY)
                retval = -EAGAIN;
            else
                retval = -ERESTARTSYS;
#else
            retval = -EAGAIN;
#endif
            break;
        }
        if (signal_pending(current)) {
            retval = -ERESTARTSYS;
            break;
        }
        schedule();
    }

    set_current_state(TASK_RUNNING);
    remove_wait_queue(&info->open_wait, &wait);
    if (!tty_hung_up_p(filp))
        info->count++;
    info->blocked_open--;

    if (retval)
        return retval;
    info->flags |= ASYNC_NORMAL_ACTIVE;
    return 0;
}

int am_uart_open(struct tty_struct *tty, struct file *filp)
{
    struct am_uart *info;
    int retval, line;
    line = tty->index;
    if ((line < 0) || (line >= NR_PORTS)) {
        printk("amlogic serial driver: check your lines,in open line=%d\n",line);
        return -ENODEV;
    }

    info = &am_uart_info[line];
#ifdef PRINT_DEBUG
    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif
    mutex_lock(&info->info_mutex);
    info->count++;
    tty->driver_data = info;
    info->tty = tty;
    mutex_unlock(&info->info_mutex);
    /*
     * Start up serial port
     */
    retval = startup(info);

    if (retval)
        return retval;

	//retval = block_til_ready(tty, filp, info);
	//if (retval)
	//	return retval;

    am_uart_sched_event(info,0);
    return 0;
}


void am_uart_wait_until_sent(struct tty_struct *tty, int timeout)
{
#ifdef PRINT_DEBUG
    struct am_uart *info;
    int line;

    line = tty->index;

    info = &am_uart_info[line];

    if(info->line >= 1)
        printk("%s\n", __FUNCTION__);
#endif
    am_uart_flush_chars(tty);
}


static const struct tty_operations am_uart_ops = {
    .open = am_uart_open,
    .close = am_uart_close,
    .write = am_uart_write,
    .wait_until_sent=am_uart_wait_until_sent,
    .flush_chars = am_uart_flush_chars,
    .write_room = am_uart_write_room,
    .chars_in_buffer = am_uart_chars_in_buffer,
    .flush_buffer = am_uart_flush_buffer,
    .ioctl = am_uart_ioctl,
    .throttle = am_uart_throttle,
    .unthrottle = am_uart_unthrottle,
    .set_termios = am_uart_set_termios,
    .stop = am_uart_stop,
    .start = am_uart_start,
    .hangup = am_uart_hangup,
    .set_ldisc = am_uart_set_ldisc,
};

static int __init am_uart_init(void)
{
    struct am_uart *info;
    int i;


    am_uart_driver = alloc_tty_driver(1);
    if (!am_uart_driver)
        return -ENOMEM;

    /* Initialize the tty_driver structure */
    am_uart_driver->owner = THIS_MODULE;
    am_uart_driver->driver_name = "Amlogic Serial Driver";
    am_uart_driver->name = "ttyS";
    am_uart_driver->major = TTY_MAJOR;
    am_uart_driver->minor_start = 64;
    am_uart_driver->num = NR_PORTS;
    am_uart_driver->type = TTY_DRIVER_TYPE_SERIAL;
    am_uart_driver->subtype = SERIAL_TYPE_NORMAL;
    am_uart_driver->init_termios = tty_std_termios;

    am_uart_driver->init_termios.c_cflag =
        B115200 | CS8 | CREAD | HUPCL | CLOCAL;
    am_uart_driver->flags = TTY_DRIVER_REAL_RAW;
    tty_set_operations(am_uart_driver, &am_uart_ops);

    if (tty_register_driver(am_uart_driver)) {
        printk("%s: Couldn't register serial driver\n", __FUNCTION__);
        return (-EBUSY);
    }


    for (i = 0; i < NR_PORTS; i++) {
        am_uart_t *uart = uart_addr[i];
        info = &am_uart_info[i];
        info->magic = SERIAL_MAGIC;
        info->port = (unsigned int)uart_addr[i];

        info->tty = 0;
        info->irq = uart_irqs[i];
        info->custom_divisor = 16;
        info->close_delay = 50;
        info->closing_wait = 100;
        info->x_char = 0;
        info->event = 0;
        info->count = 0;
        info->blocked_open = 0;
        init_waitqueue_head(&info->open_wait);
        init_waitqueue_head(&info->close_wait);
        info->line = i;
        info->is_cons = ((i == 1) ? 1 : 0);

        mutex_init(&info->info_mutex);
        INIT_WORK(&info->tqueue,am_uart_workqueue);
        
        tasklet_init(&info->tlet, uart_tasklet_action,
			     (unsigned long)info);

        set_mask(&uart->mode, UART_RXRST);
        clear_mask(&uart->mode, UART_RXRST);

#ifdef FIQ_UART
        set_mask(&uart->mode, UART_RXINT_EN /*| UART_TXINT_EN*/);
	if (i==1)
		__raw_writel(/*1 << 7 | */0x30, &uart->intctl);
	else
		__raw_writel(/*1 << 7 | */0x1, &uart->intctl);
#else
        set_mask(&uart->mode, UART_RXINT_EN | UART_TXINT_EN);
#if defined(CONFIG_ARCH_MESON3)
        if(1 == i)
            __raw_writel(0x3f << 8 | 1, &uart->intctl);   /* set half full tx fifo to trigger interrupt */
        else
            __raw_writel(0x1f << 8 | 1, &uart->intctl);
#else
        __raw_writel(1 << 7 | 1, &uart->intctl);
#endif
#endif

        clear_mask(&uart->mode, (1 << 19)) ;

        sprintf(info->name,"UART_ttyS%d:",info->line);
#ifdef FIQ_UART
        if (i==1){
            if(!fiq_buf)
            {
                fiq_buf = (unsigned char *)get_zeroed_page(GFP_KERNEL);
            }
            fiq_read = fiq_write=fiq_cnt=0;
            request_fiq(info->irq, am_uart_fiq_interrupt);
            setup_timer(&info->timer, am_uart_timer_sr, (unsigned long)info) ;
            mod_timer(&info->timer, jiffies+msecs_to_jiffies(1));
            printk("request fiq %d done!\n", info->irq);
        }
        else
#endif
        if (request_irq(info->irq, (irq_handler_t) am_uart_interrupt, IRQF_SHARED,
             info->name, info)) {
            printk("request irq error!!!\n");
        }
        printk("%s(irq = %d, address = 0x%x)\n", info->name,
               info->irq, info->port);

        IRQ_ports[info->irq] = info;


    }
    for (i = 0; i < NR_PORTS; i++)
        printk("Port %u = IRQ %u\n", i, uart_irqs[i]);
    return 0;
}

/*
 * register_serial and unregister_serial allows for serial ports to be
 * configured at run-time, to support PCMCIA modems.
 */
int register_serial(struct serial_struct *req)
{
    return -1;
}

void unregister_serial(int line)
{
    return;
}

module_init(am_uart_init);

int am_uart_console_setup(struct console *cp, char *arg)
{
    struct clk *sysclk;
    unsigned long baudrate = 0;
    int baud = 115200;
    int bits = 8;
    int parity = 'n';
    int flow = 'n';
    am_uart_t *uart = uart_addr[cp->index];
    /* TODO: pinmux */
#if 0
    if(cp->index==1)/*PORT B*/
    {
        int uart_bank;
        switch(uart_bank)
        {
            case 0://JTAG=TCK,TDO
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<18)|(1<<22));
                break;
            case 1://GPIO+B0.B1
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<4)|(1<<7));
                break;
            case 2: ///GPIO_C13_C14
                /*6236-SH*/
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<27)|(1<<30));
                break;
            case 3://GPIO+E18.E19
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_5, (1<<23)|(1<<24));
                break;
            default:
                printk("UartB pinmux set error\n");
        }
    }
    else/*PORT_A*/
    {
        int uart_bank=3;
        switch(uart_bank)
        {
            case 0:/*JTAG-TMS/TDI*/
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_1, (1<<10)|(1<<14));
                break;
            case 1:/*GPIO_B2,B3*//*7266-m_SZ*/
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, (1<<11)|(1<<15));
                break;
            case 2:
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_3, (1<<23)|(1<<24));
                break;
            case 3:/*6236m_dpf_sz :GPIOC_21_22*/
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_7,((1 << 11) | (1 << 8)));
                break;
            case 4:
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_7, (1<<18)|(1<<19));
                break;
            case 5:
                SET_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, (1<<0)|(1<<1));
                break;
            default:
                printk("UartA pinmux set error\n");
        }
    }
#endif
    if (arg)
        uart_parse_options(arg, &baud, &parity, &bits, &flow);

    if (baud < 300 || baud > 115200)
        baud = 115200;

    clear_mask(&uart->mode, (1 << 19));

    sysclk = clk_get_sys("clk81", NULL);

    if (!IS_ERR(sysclk)) {
        baudrate = (clk_get_rate(sysclk) / (baud * 4)) - 1;
        clear_mask(&uart->mode, 0xFFF);
        set_mask(&uart->mode, (baudrate & 0xfff));
        set_mask(&uart->mode, (1 << 12) | 1 << 13);
        set_mask(&uart->mode, (7 << 22));
        clear_mask(&uart->mode, (7 << 22));
    }

    console_inited[cp->index]= 1;
    default_index=cp->index;
    return 0;       /* successful initialization */
}

static struct tty_driver *am_uart_console_device(struct console *c, int *index)
{
    *index = c->index;
    return am_uart_driver;
}

void am_uart_console_write(struct console *cp, const char *p, unsigned len)
{
    if (!console_inited[cp->index])
        am_uart_console_setup(cp, NULL);
    while (len-- > 0) {
#if 1
        if (*p == '\n')
            am_uart_put_char(cp->index,'\r');
#endif
        am_uart_put_char(cp->index,*p++);
    }
}

int am_uart_console_read(struct console *cp, const char *p, unsigned len)
{
    return 0;
}

struct console arc_am_uart_console0 = {
    .name = "ttyS",
    .write = am_uart_console_write,
    .read = NULL,
    .device = am_uart_console_device,
    .unblank = NULL,
    .setup = am_uart_console_setup,
    .flags = CON_PRINTBUFFER,
    .index = 0,
    .cflag = 0,
    .next = NULL
};
struct console arc_am_uart_console1 = {
    .name = "ttyS",
    .write = am_uart_console_write,
    .read = NULL,
    .device = am_uart_console_device,
    .unblank = NULL,
    .setup = am_uart_console_setup,
    .flags = CON_PRINTBUFFER,
    .index = 1,
    .cflag = 0,
    .next = NULL
};


int __init am_uart_console_init(void)
{
    register_console(&arc_am_uart_console0);
    register_console(&arc_am_uart_console1);
    return 0;
}

console_initcall(am_uart_console_init);

/************************************************************************
 * Interrupt Safe Raw Printing Routine so we dont have to worry about
 * possible side effects of calling printk( ) such as
 * context switching, semaphores, interrupts getting re-enabled etc
 *
 * Syntax: (1) Doesn't understand Format Specifiers
 *         (2) Can print one @string and one @number (in hex)
 *
 *************************************************************************/
static char dec_to_hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F'
};

static void raw_num(unsigned int num, int zero_ok)
{
    int nibble;
    int i;
    int leading_zeroes = 1;

    /* If @num is Zero don't print it */
    if (num != 0 || zero_ok) {

        /* break num into 8 nibbles */
        for (i = 7; i >= 0; i--) {
            nibble = (num >> (i << 2)) & 0xF;
            if (nibble == 0) {
                if (leading_zeroes)
                    continue;
            } else {
                if (leading_zeroes)
                    leading_zeroes = 0;
            }
            am_uart_put_char(default_index,dec_to_hex[nibble]);
        }
    }

    am_uart_put_char(default_index,' ');
}

static int raw_str(const char *str)
{
    int cr = 0;

    /* Loop until we find a Sentinel in the @str */
    while (*str != '\0') {

        /* If a CR found, make a note so that we
         * print it after printing the number
         */
        if (*str == '\n') {
            cr = 1;
            break;
        }
        am_uart_put_char(default_index,*str++);
    }

    return cr;
}

void raw_printk(const char *str, unsigned int num)
{
    int cr;
    unsigned long flags;

    local_irq_save(flags);

    cr = raw_str(str);
    raw_num(num, 0);

    /* Carriage Return and Line Feed */
    if (cr) {
        am_uart_put_char(default_index,'\r');
        am_uart_put_char(default_index,'\n');
    }

    local_irq_restore(flags);
}

EXPORT_SYMBOL(raw_printk);

void raw_printk5(const char *str, uint n1, uint n2, uint n3, uint n4)
{
    int cr;
    unsigned long flags;

    local_irq_save(flags);

    cr = raw_str(str);
    raw_num(n1, 1);
    raw_num(n2, 1);
    raw_num(n3, 1);
    raw_num(n3, 0);

    /* Carriage Return and Line Feed */
    if (cr) {
        am_uart_put_char(default_index,'\r');
        am_uart_put_char(default_index,'\n');

        local_irq_restore(flags);
    }
}

EXPORT_SYMBOL(raw_printk5);

int get_baud(int line)
{
    struct am_uart *info;

    info = &am_uart_info[line];

    printk("uart%d %s %d\n", line, __FUNCTION__, info->baud);
    return info->baud ? info->baud : 115200;
}
EXPORT_SYMBOL(get_baud);

void set_baud(int line, unsigned long newbaud)
{
    struct am_uart *info;

    info = &am_uart_info[line];

    change_speed(info, newbaud);
    printk("uart%d %s %d\n", line, __FUNCTION__, info->baud);
}
EXPORT_SYMBOL(set_baud);
