
/*
 *  am_uart.h
 *
 *  Copyright (C) Amlogic
 *
 * auther:zhouzhi
 */

#ifndef _ASM_AM_UART_H
#define _ASM_AM_UART_H

#ifdef __KERNEL__

/*
 * This is our internal structure for each serial port's state.
 *
 * Many fields are paralleled by the structure used by the serial_struct
 * structure.
 *
 * For definitions of the flags field, see tty.h
 */

struct am_uart{
    char is_cons;       /* Is this our console. */

    /* We need to know the current clock divisor
     * to read the bps rate the chip has currently
     * loaded.
     */
    char        name[16];
    int             baud;
    int         magic;
    int         baud_base;
    int         port;
    int         irq;
    int         flags;      /* defined in tty.h */
    int         type;       /* UART type */
    struct tty_struct   *tty;
    int         read_status_mask;
    int         ignore_status_mask;
    int         timeout;
    int         custom_divisor;
    int         x_char; /* xon/xoff character */
    int         close_delay;
    unsigned short      closing_wait;
    unsigned short      closing_wait2;
    unsigned long       event;
    unsigned long       last_active;
    int         line;
    int         count;      /* # of fd on device */
    int         blocked_open; /* # of blocked opens */
    long            session; /* Session of opening process */
    long            pgrp; /* pgrp of opening process */
    unsigned char       *xmit_buf;
    int         xmit_rd;
    int         xmit_wr;
    int         xmit_cnt;

    unsigned char *rx_buf;
    int                  rx_head;
    int                  rx_tail;
    int                  rx_cnt;
    int                  rx_error;
       
    struct mutex    info_mutex;

    struct tasklet_struct	tlet;
    struct work_struct  tqueue;
    struct work_struct  tqueue_hangup;


/* Sameer: Obsolete structs in linux-2.6 */
/*  struct termios      normal_termios; */
/*  struct termios      callout_termios; */
    wait_queue_head_t   open_wait;
    wait_queue_head_t   close_wait;
    
    struct timer_list timer;
};

typedef volatile struct {
  volatile unsigned long wdata;
  volatile unsigned long rdata;
  volatile unsigned long mode;
  volatile unsigned long status;
  volatile unsigned long intctl;
} am_uart_t;

#define SERIAL_MAGIC 0x5301

/*
 * Events are used to schedule things to happen at timer-interrupt
 * time, instead of at rs interrupt time.
 */
#define ARCSERIAL_EVENT_WRITE_WAKEUP    0

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256
#define CONSOLE_BAUD_RATE 115200
#define BASE_BAUD CONSOLE_BAUD_RATE



/*
 * Define the number of ports supported and their irqs.
 */


//#define UART_IRQ_DEFNS {VUART_INT}

/* #define  CONSOLE_BAUD_RATE CONFIG_ARC700_SERIAL_BAUD
   #define BASE_BAUD CONSOLE_BAUD_RATE */

#if defined(CONFIG_ARCH_MESON3)
#define NR_PORTS         4
#define UART_BASEADDRAO ((void *)AOBUS_REG_ADDR(AO_UART_WFIFO)) //AO UART
#define UART_BASEADDR0  ((void *)CBUS_REG_ADDR(UART0_WFIFO))
#define UART_BASEADDR1  ((void *)CBUS_REG_ADDR(UART1_WFIFO))
#define UART_BASEADDR2  ((void *)CBUS_REG_ADDR(UART2_WFIFO))
#else
#define NR_PORTS         2
#define UART_BASEADDR0  ((void *)CBUS_REG_ADDR(UART0_WFIFO))
#define UART_BASEADDR1  ((void *)CBUS_REG_ADDR(UART1_WFIFO))
#endif

#define UART_OVERFLOW_ERR (0x01<<18)
#define UART_FRAME_ERR  (0x01<<17)
#define UART_PARITY_ERR (0x01<<16)
#define UART_CLEAR_ERR  (0x01<<24)

#define UART_RXENB      (0x01<<13)
#define UART_RXEMPTY    (0x01<<20)
#define UART_RXFULL     (0x01<<19)
#define UART_TXENB      (0x01<<12)
#define UART_TXEMPTY    (0x01<<22)
#define UART_TXFULL     (0x01<<21)

#define UART_TXRST      (0x01<<22)
#define UART_RXRST      (0x01<<23)
#define UART_RXINT_EN   (0x01<<27)
#define UART_TXINT_EN   (0x01<<28)

#endif /* __KERNEL__ */

/* ioctls */

#define GET_BAUD 1
#define SET_BAUD 2

#endif  /* _ASM_ARC_SERIAL_H */

