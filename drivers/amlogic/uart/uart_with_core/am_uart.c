/*
 *  linux/drivers/char/8250.c
 *
 *  Driver for 8250/16550-type serial ports
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  Copyright (C) 2001 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * A note about mapbase / membase
 *
 *  mapbase is the physical address of the IO port.
 *  membase is an 'ioremapped' cookie.
 */

#ifdef CONFIG_MAGIC_SYSRQ
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_reg.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/nmi.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <mach/hardware.h>
#include <linux/clk.h>
#include <linux/uart-aml.h>

#include <asm/io.h>
#include <asm/irq.h>

#include "am_uart.h"


#include <asm/serial.h>

#if defined(CONFIG_ARCH_MESON3)
#define UART_NR     4
/* --FIXME-- Because the ISA documents. Don't know the AO UART interrupt and UART2 interrupt*/
static unsigned int uart_irqs[UART_NR] = {INT_UART_AO, INT_UART_0, INT_UART_1, INT_UART_2};
static am_uart_t *uart_addr[UART_NR] = {UART_BASEADDRAO, UART_BASEADDR0, UART_BASEADDR1, UART_BASEADDR2};
static unsigned int uart_FIFO_max_cnt[UART_NR] = {64,128,64,64};
#else
#define UART_NR     2
static unsigned int uart_irqs[UART_NR] = { INT_UART,INT_UART_1 };
static am_uart_t *uart_addr[UART_NR] = { UART_BASEADDR0,UART_BASEADDR1 };
static unsigned int uart_FIFO_max_cnt[UART_NR] = {64,64};
#endif


#if defined(CONFIG_AM_UART0_SET_PORT_A) || defined(CONFIG_AM_UART0_SET_PORT_AO)
static int default_index = 0;
#else
static int default_index = 1;
#endif
static int inited_ports_flag = 0;
#ifndef outl
#define outl(v,addr)	__raw_writel(v,(unsigned long)addr)
#endif
#ifndef inl
#define inl(addr)	__raw_readl((unsigned long)addr)
#endif
#define in_l(addr)	inl((unsigned long )addr)
#define out_l(v,addr)	outl(v,(unsigned long )addr)
#define clear_mask(addr,mask)		out_l(in_l(addr) &(~(mask)),addr)
#define set_mask(addr,mask)		out_l(in_l(addr) |(mask),addr)

DECLARE_MUTEX(tmp_buf_sem);
#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

//#define PRINT_DEBUG

struct am_uart_port {
	struct uart_port	port;

	/* We need to know the current clock divisor
	 * to read the bps rate the chip has currently
	 * loaded.
	 */
	char 		name[16];
	int 			baud;
	int			magic;
	int			baud_base;
	int			irq;
	int			flags; 		/* defined in tty.h */
	int			type; 		/* UART type */
	struct tty_struct 	*tty;
	int			read_status_mask;
	int			ignore_status_mask;
	int			timeout;
	int			custom_divisor;
	int			x_char;	/* xon/xoff character */
	int			close_delay;
	unsigned short		closing_wait;
	unsigned short		closing_wait2;
	unsigned long		event;
	unsigned long		last_active;
	int			line;
	int			count;	    /* # of fd on device */
	int			blocked_open; /* # of blocked opens */
	long			session; /* Session of opening process */
	long			pgrp; /* pgrp of opening process */

       int                  rx_cnt;
       int                  rx_error;
       
	struct mutex	info_mutex;
	
	struct work_struct	tqueue;
	struct work_struct	tqueue_hangup;

/* Sameer: Obsolete structs in linux-2.6 */
/* 	struct termios		normal_termios; */
/* 	struct termios		callout_termios; */
	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;
};

struct am_uart_port am_ports[UART_NR];

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

int print_flag = 0;
/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static inline void am_uart_sched_event(struct am_uart_port *info, int event)
{
	info->event |= 1 << event;
      schedule_work(&info->tqueue);
}

static void receive_chars(struct am_uart_port *info, struct pt_regs *regs,
			  unsigned short rx)
{
	struct tty_struct *tty = info->port.state->port.tty;
	am_uart_t *uart = uart_addr[info->line];
	int status;
	int mode;
        char ch;
        unsigned long flag = TTY_NORMAL;  
        unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

       fifo_level = (fifo_level-1);

	if (!tty) {
		//printk("Uart : missing tty on line %d\n", info->line);
		goto clear_and_exit;
	}   
	status = __raw_readl(&uart->status);
	if (status & UART_OVERFLOW_ERR) {
              info->rx_error |= UART_OVERFLOW_ERR;
              mode = __raw_readl(&uart->mode) | UART_CLEAR_ERR;
		__raw_writel(mode, &uart->mode);
	} else if (status & UART_FRAME_ERR) {
		info->rx_error |= UART_FRAME_ERR; 
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
		if ((status = __raw_readl(&uart->status) & fifo_level))
			rx = __raw_readl(&uart->rdata);
	} while (status);

clear_and_exit:
	return;
}

static void BH_receive_chars(struct am_uart_port *info)
{
	struct tty_struct *tty = info->port.state->port.tty;
	unsigned long flag;
	int status;
	int cnt = info->rx_cnt;
    
	if (!tty) {
		printk("Uart : missing tty on line %d\n", info->line);
		goto clear_and_exit;
	}
       tty->low_latency = 1;	// Originally added by Sameer, serial I/O slow without this     
	flag = TTY_NORMAL;
	status = info->rx_error;
	if (status & UART_OVERFLOW_ERR) {
		printk
		    ("Uart  Driver: Overflow Error while receiving a character\n");
		flag = TTY_OVERRUN;
	} else if (status & UART_FRAME_ERR) {
		printk
		    ("Uart  Driver: Framing Error while receiving a character\n");
		flag = TTY_FRAME;
	} else if (status & UART_PARITY_ERR) {
		printk
		    ("Uart  Driver: Parity Error while receiving a character\n");
		flag = TTY_PARITY;
	}
       info->rx_error = 0;
	if(cnt)
       {
            

            info->rx_cnt =0; 
            
	     tty_flip_buffer_push(tty);
       }

clear_and_exit:
        if (info->rx_cnt>0)
		am_uart_sched_event(info, 0);	

        return;
}

static void transmit_chars(struct am_uart_port *info)
{
	am_uart_t *uart = uart_addr[info->line];
  struct uart_port * up = &info->port;
	unsigned int ch;
  struct circ_buf *xmit = &up->state->xmit;

   unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

   fifo_level = (fifo_level-1)<<8;
       
	mutex_lock(&info->info_mutex);
	if (up->x_char) {
		__raw_writel(up->x_char, &uart->wdata);
		up->x_char = 0;
		goto clear_and_return;
	}
    
  if (uart_circ_empty(xmit) || uart_tx_stopped(up))
		goto clear_and_return;

  while(!uart_circ_empty(xmit))
  {
       if(((__raw_readl(&uart->status) & 0xff00) < fifo_level)) {
		ch = xmit->buf[xmit->tail];
		__raw_writel(ch, &uart->wdata);
		xmit->tail = (xmit->tail+1) & (SERIAL_XMIT_SIZE - 1);
		}
	}
clear_and_return:
	mutex_unlock(&info->info_mutex);
	if (!uart_circ_empty(xmit))
		am_uart_sched_event(info, 0);	
	/* Clear interrupt (should be auto) */
	return;
}


/*
 * This is the serial driver's generic interrupt routine
 */
static irqreturn_t am_uart_interrupt(int irq, void *dev, struct pt_regs *regs)
{  
       struct am_uart_port *info=(struct am_uart_port *)dev;    
       am_uart_t *uart = NULL;
	struct tty_struct *tty = NULL;

	if (!info)
           goto out;

       tty = info->port.state->port.tty;
	if (!tty)
       {   
	    goto out;
       }
    
	uart = uart_addr[info->line];
	if (!uart)
		goto out;

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
	struct am_uart_port *info=container_of(work,struct am_uart_port,tqueue);
	am_uart_t *uart = NULL;
	struct tty_struct *tty = NULL;
       unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

       fifo_level = (fifo_level-1)<<8;
       
	if (!info)
           goto out;

  tty = info->port.state->port.tty;
	if (!tty)
  {   
	    goto out;
  }
    
	uart = uart_addr[info->line];
	if (!uart)
		goto out;

  if (info->rx_cnt>0)
            BH_receive_chars(info);
       
	if ((__raw_readl(&uart->mode) & UART_TXENB)
	    &&((__raw_readl(&uart->status) & 0xff00) < fifo_level)) {

		transmit_chars(info);
	}
	if (__raw_readl(&uart->status) & UART_FRAME_ERR)
		__raw_writel(__raw_readl(&uart->status) & ~UART_FRAME_ERR,
			   &uart->status);
	if (__raw_readl(&uart->status) & UART_OVERFLOW_ERR)
		__raw_writel(__raw_readl(&uart->status) & ~UART_OVERFLOW_ERR,
			   &uart->status);
    
      out:
	return ;
}

static void am_uart_stop_tx(struct uart_port *port)
{
  struct am_uart_port * info = &am_ports[port->line];
  am_uart_t *uart = uart_addr[info->line];
	unsigned long mode;

#ifdef PRINT_DEBUG
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
       
	mutex_lock(&info->info_mutex);
	mode = __raw_readl(&uart->mode);
	mode &= ~UART_TXENB;
	__raw_writel(mode, &uart->mode);	
  mutex_unlock(&info->info_mutex);
}

static void am_uart_start_tx(struct uart_port *port)
{
  struct am_uart_port * info = &am_ports[port->line];
  am_uart_t *uart = uart_addr[info->line];
  unsigned long mode;
  struct uart_port * up = &info->port;
	unsigned int ch;
  struct circ_buf *xmit = &up->state->xmit;
   unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

   fifo_level = (fifo_level-1)<<8; 
//	mutex_lock(&info->info_mutex);
	mode = __raw_readl(&uart->mode);
	mode |= UART_TXENB;
	__raw_writel(mode, &uart->mode);	

  while(!uart_circ_empty(xmit))
  {
  if (((__raw_readl(&uart->status) & 0xff00) < fifo_level)) {
		ch = xmit->buf[xmit->tail];
		__raw_writel(ch, &uart->wdata);
		xmit->tail = (xmit->tail+1) & (SERIAL_XMIT_SIZE - 1);
  }
  }
  //mutex_unlock(&info->info_mutex);
}

static void am_uart_stop_rx(struct uart_port *port)
{
  struct am_uart_port * info = &am_ports[port->line];
  am_uart_t *uart = uart_addr[info->line];
	unsigned long mode;

#ifdef PRINT_DEBUG
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif

	mutex_lock(&info->info_mutex);
	mode = __raw_readl(&uart->mode);
	mode &= ~UART_RXENB;
	__raw_writel(mode, &uart->mode);	
  mutex_unlock(&info->info_mutex);
}

static void am_uart_enable_ms(struct uart_port *port)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
      return;
}

static unsigned int am_uart_tx_empty(struct uart_port *port)
{
  struct am_uart_port * info = &am_ports[port->line];
  am_uart_t *uart = uart_addr[info->line];
	unsigned long mode;

#ifdef PRINT_DEBUG
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif

  mutex_lock(&info->info_mutex);
	mode = __raw_readl(&uart->status);
  mutex_unlock(&info->info_mutex);
      
  return ( mode & UART_TXEMPTY) ? TIOCSER_TEMT : 0;
}

static unsigned int am_uart_get_mctrl(struct uart_port *port)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
	return TIOCM_CTS;
}

static void am_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
	return;
}

static void am_uart_break_ctl(struct uart_port *port, int break_state)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
      return;
}



#ifdef CONFIG_CONSOLE_POLL
/*
 * Console polling routines for writing and reading from the uart while
 * in an interrupt or debug context.
 */

static int am_uart_get_poll_char(struct uart_port *port)
{
    struct am_uart_port * info = &am_ports[port->line];
    am_uart_t *uart = uart_addr[info->line];
    int status,rx;
    unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

   fifo_level = (fifo_level-1);

    mutex_lock(&info->info_mutex);
    if ((status = __raw_readl(&uart->status) & fifo_level))
	      rx = __raw_readl(&uart->rdata);
    else
        rx = 0;
    mutex_unlock(&info->info_mutex);

    return rx;
}


static void am_uart_put_poll_char(struct uart_port *port,
			 unsigned char c)
{
    struct am_uart_port * info = &am_ports[port->line];
    am_uart_t *uart = uart_addr[info->line];
    unsigned int fifo_level = uart_FIFO_max_cnt[info->line];

   fifo_level = (fifo_level-1)<<8;
   
    mutex_lock(&info->info_mutex);
    while (!((__raw_readl(&uart->status) & 0xff00)< fifo_level)) ;
	  __raw_writel(c, &uart->wdata);
    mutex_unlock(&info->info_mutex);

    return;
}

#endif /* CONFIG_CONSOLE_POLL */

static int am_uart_startup(struct uart_port *port)
{
  struct am_uart_port * info = &am_ports[port->line];
  am_uart_t *uart = uart_addr[info->line];
	unsigned long mode;
 
#ifdef PRINT_DEBUG
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif

	mutex_lock(&info->info_mutex);
	mode = __raw_readl(&uart->mode);
	mode |= UART_RXENB | UART_TXENB;
	__raw_writel(mode, &uart->mode);	
  mutex_unlock(&info->info_mutex);

  return 0;
}

static void am_uart_shutdown(struct uart_port *port)
{
 // struct am_uart_port * info = &am_ports[port->line];
 // am_uart_t *uart = uart_addr[info->line];
//	unsigned int mode ;
				/* All off! */
//  mutex_lock(&info->info_mutex);
//	mode = __raw_readl(&uart->mode);
//	mode &= ~(UART_TXENB | UART_RXENB);
//	__raw_writel(mode, &uart->mode);
//  mutex_unlock(&info->info_mutex);
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif

}

static void change_speed(struct am_uart_port *info, unsigned long newbaud)
{
    am_uart_t *uart = uart_addr[info->line];
    struct tty_struct *tty;
    unsigned long tmp;
    unsigned long tmp_intctl;
#ifdef PRINT_DEBUG
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
    tty = info->port.state->port.tty;
    if (!tty || !tty->termios)
        return;

    if (newbaud==0)
        return;

    if(newbaud == info->baud)
        return;

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

}

static void
am_uart_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
    struct am_uart_port * info = &am_ports[port->line];
#ifdef PRINT_DEBUG
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif

    //if (termios->c_cflag == old->c_cflag)
    //    return;

    change_speed(info, termios->c_ispeed);

#if 0
  unsigned int mode, fcr = 0;
	unsigned long flags;
	unsigned int baud, quot;
       am_uart_t *uart = uart_addr[port->line];

       printk("am_uart_set_termios, %d\n",port->line);
	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk/16);
	quot = uart_get_divisor(port, baud);

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		mode = UART_CNTL_MASK_CHAR_5BIT;
		break;
	case CS6:
		mode = UART_CNTL_MASK_CHAR_6BIT;
		break;
	case CS7:
		mode = UART_CNTL_MASK_CHAR_7BIT;
		break;
	default:
		mode = UART_CNTL_MASK_CHAR_8BIT;
		break;
	}

       /* stop bits */
	if (termios->c_cflag & CSTOPB)
		mode |= UART_CNTL_MASK_STP_2BIT;

	/* parity */
	if (termios->c_cflag & PARENB) {
              mode|=UART_CNTL_MASK_PRTY_EN;
		if (termios->c_cflag & PARODD)
			mode |= UART_CNTL_MASK_PRTY_ODD;
		else
			mode |= UART_CNTL_MASK_PRTY_EVEN;
	}

	if (port->fifosize > 1)
		fcr = ((port->fifosize&UART_STAT_MASK_RFIFO_CNT)|((port->fifosize<<8)&UART_STAT_MASK_RFIFO_CNT));

	spin_lock_irqsave(&port->lock, flags);

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	/* first, disable everything */
	clear_mask(&uart->mode, (UART_CNTL_MASK_RINT_EN| UART_CNTL_MASK_TINT_EN));

	/* Set baud rate */
       set_mask(&uart->mode, mode);
       set_mask(&uart->status, fcr);
       
       set_mask(&uart->mode, (UART_CNTL_MASK_RINT_EN| UART_CNTL_MASK_TINT_EN));
       
	spin_unlock_irqrestore(&port->lock, flags);
#endif
       return;
}

static void
am_uart_set_ldisc(struct uart_port *port)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
       return;
}

static void
am_uart_pm(struct uart_port *port, unsigned int state,
	      unsigned int oldstate)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
      return;
}

static void am_uart_release_port(struct uart_port *port)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
       return;
}

static int am_uart_request_port(struct uart_port *port)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
     return 0;
}

static void am_uart_config_port(struct uart_port *port, int flags)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
    return;
}

static int
am_uart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
    return 0;
}

static const char *
am_uart_type(struct uart_port *port)
{
#ifdef PRINT_DEBUG
    struct am_uart_port * info = &am_ports[port->line];
    if(info->line == 0)
        printk("%s\n", __FUNCTION__);
#endif
        return NULL;
}

static struct uart_ops am_uart_pops = {
	.tx_empty	= am_uart_tx_empty,
	.set_mctrl	= am_uart_set_mctrl,
	.get_mctrl	= am_uart_get_mctrl,
	.stop_tx	= am_uart_stop_tx,
	.start_tx	= am_uart_start_tx,
	.stop_rx	= am_uart_stop_rx,
	.enable_ms	= am_uart_enable_ms,
	.break_ctl	= am_uart_break_ctl,
	.startup	= am_uart_startup,
	.shutdown	= am_uart_shutdown,
	.set_termios	= am_uart_set_termios,
	.set_ldisc	= am_uart_set_ldisc,
	.pm		= am_uart_pm,
	.type		= am_uart_type,
	.release_port	= am_uart_release_port,
	.request_port	= am_uart_request_port,
	.config_port	= am_uart_config_port,
	.verify_port	= am_uart_verify_port,
#ifdef CONFIG_CONSOLE_POLL	
	.poll_get_char = am_uart_get_poll_char,
	.poll_put_char = am_uart_put_poll_char,
#endif
};


/*
 * Configure the port from the platform device resource info.
 */
static void am_uart_init_port(struct am_uart_port *am_port,int line)
{
	struct uart_port *port = &am_port->port;
  int index = am_port->line;
	    
	port->iotype	= UPIO_PORT;
	port->flags	= UPF_BOOT_AUTOCONF;
	port->ops	= &am_uart_pops;
	port->fifosize	= 1;
	port->line	= line;	
  port->irq	= uart_irqs[index];
  port->type =1;
  port->x_char = 0;
}

static void am_uart_start_port(struct am_uart_port *am_port)
{
  struct uart_port *port = &am_port->port;
  int index = am_port->line;
	am_uart_t *uart = uart_addr[index];
    
  am_port->magic = SERIAL_MAGIC;

	am_port->custom_divisor = 16;
	am_port->close_delay = 50;
	am_port->closing_wait = 100;
	am_port->event = 0;
	am_port->count = 0;
	am_port->blocked_open = 0;

        am_port->rx_cnt= 0;
    
  init_waitqueue_head(&am_port->open_wait);
	init_waitqueue_head(&am_port->close_wait);

	mutex_init(&am_port->info_mutex);
	INIT_WORK(&am_port->tqueue,am_uart_workqueue);

  set_mask(&uart->mode, UART_RXRST);
  clear_mask(&uart->mode, UART_RXRST);

  set_mask(&uart->mode, UART_RXINT_EN | UART_TXINT_EN);
#if defined(CONFIG_ARCH_MESON3)
        __raw_writel(1 << 8 | 1, &uart->intctl);
#else
        __raw_writel(1 << 7 | 1, &uart->intctl);
#endif
  clear_mask(&uart->mode, (1 << 19)) ;
       
	sprintf(am_port->name,"UART_ttyS%d:",am_port->line);
	if (request_irq(port->irq, (irq_handler_t) am_uart_interrupt, IRQF_SHARED,
	     am_port->name, am_port)) {
		printk("request irq error!!!\n");
	}

       printk("%s(irq = %d)\n", am_port->name,
		       port->irq);
}

static void am_uart_register_ports(struct platform_device *pdev)
{
       struct am_uart_port *port;
       struct aml_uart_platform *uart_data = pdev->dev.platform_data;
	     int i;

       if(inited_ports_flag==1)
            return;
       
       for(i=0;i<UART_NR;i++)
       {
	    port = &am_ports[i];
           port->line  = uart_data->uart_line[i];

	    am_uart_init_port(port,i);
       }

       inited_ports_flag = 1;
       return;
}

static int __init am_uart_console_setup(struct console *co, char *options)
{
 	struct clk *sysclk;
	unsigned long baudrate = 0;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
       int index = am_ports[co->index].line;
       am_uart_t *uart;
	if(inited_ports_flag==0)
                     index =default_index;
	 uart = uart_addr[index];
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
			case 2:	///GPIO_C13_C14
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
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

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


	default_index=index;
    
	return 0;		/* successful initialization */
}

static void am_uart_console_write(struct console *co, const char *s, u_int count)
{
       int index = am_ports[co->index].line;

        if(inited_ports_flag==0)
                     index =default_index;      

       if (index!=default_index)
		am_uart_console_setup(co, NULL);
       
	while (count-- > 0) {
		if (*s == '\n')
			am_uart_put_char(index,'\r');
		am_uart_put_char(index,*s++);
	}    
}

struct uart_driver am_uart_reg;

struct console am_uart_console = {
	.name		= "ttyS",
	.write		= am_uart_console_write,
	.device		= uart_console_device,
	.setup		= am_uart_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &am_uart_reg,
};

int console_flag=1;
static int __init am_uart_console_init(void)
{
      register_console(&am_uart_console);
	return 0;
}
console_initcall(am_uart_console_init);

#define AM_UART_CONSOLE	&am_uart_console

struct uart_driver am_uart_reg = {
	.owner			= THIS_MODULE,
	.driver_name		= "serial",
	.dev_name		= "ttyS",
	.major			= 4,
	.minor			= 64,
	.nr                        = UART_NR,
	.cons			= AM_UART_CONSOLE,
};


#define am_uart_suspend NULL
#define am_uart_resume NULL

static int __devinit am_uart_probe(struct platform_device *pdev)
{
	struct am_uart_port *up;
	int i,ret;

       am_uart_register_ports(pdev);

       for(i=0;i<UART_NR;i++)
       {
	    up = &am_ports[i];
           up->port.dev = &pdev->dev;
           ret = uart_add_one_port(&am_uart_reg, &up->port);
	    if (ret)
	   	return ret;

           am_uart_start_port(up);
       }
	return 0;
}

static int __devexit am_uart_remove(struct platform_device *pdev)
{
	struct am_uart_port *up;
       int i,ret = 0;

       for(i=0;i<UART_NR;i++)
       {
           up = &am_ports[i];
	    ret = uart_remove_one_port(&am_uart_reg, &up->port);
       }

       ///?free irq and bh
	
	return ret;
}

struct platform_driver am_uart_isa_driver = {
	.probe		= am_uart_probe,
	.remove		= __devexit_p(am_uart_remove),
	.suspend	=  am_uart_suspend,
	.resume		= am_uart_resume,
	.driver		= {
		.name	= "am_uart",
		.owner	= THIS_MODULE,
	},
};

//int start_flag =3;
static int __init am_uart_init(void)
{
	int ret;

       /*while(start_flag++)
        {
            if(start_flag==0)
                start_flag = 1;
            if(start_flag==0)
                break;
       }*/

	ret = uart_register_driver(&am_uart_reg);
	if (ret)
		return ret;

	ret = platform_driver_register(&am_uart_isa_driver);
	if (ret)
		uart_unregister_driver(&am_uart_reg);

	return ret;
}

static void __exit am_uart_exit(void)
{
       platform_driver_unregister(&am_uart_isa_driver);
	uart_unregister_driver(&am_uart_reg);
}

module_init(am_uart_init);
module_exit(am_uart_exit);


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
    struct am_uart_port * info = &am_ports[line];

    printk("uart%d %s %d\n", line, __FUNCTION__, info->baud);
    return info->baud ? info->baud : 115200;
}
EXPORT_SYMBOL(get_baud);

void set_baud(int line, unsigned long newbaud)
{
      struct am_uart_port * info = &am_ports[line];

      change_speed(info, newbaud);
      printk("uart%d %s %d\n", line, __FUNCTION__, info->baud);
}
EXPORT_SYMBOL(set_baud);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("amlogic uart driver");



