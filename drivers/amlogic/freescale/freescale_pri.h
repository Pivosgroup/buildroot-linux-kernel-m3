#ifndef _FREESCALE_PRI_INCLUDE__
#define _FREESCALE_PRI_INCLUDE__

/* for freescale device op. */
extern int  init_freescale_device(void);
extern int uninit_freescale_device(void);

/* for freescale device class op. */
extern struct class* init_freescale_cls(void);

/* for thread of freescale. */
static int start_freescale_task(void);
extern void stop_freescale_task(void);

/* for freescale private member. */
extern void set_freescale_buf_info(char* start,unsigned int size);
extern void get_freescale_buf_info(char** start,unsigned int* size);

/*  freescale buffer op. */
extern int freescale_buffer_init(void);

extern int freescale_register();

#endif /* _FREESCALE_PRI_INCLUDE__ */
