
#ifndef DSP_IRQ_HEADER
#define  DSP_IRQ_HEADER
#include "audiodsp_module.h"
int audiodsp_init_mailbox(struct audiodsp_priv *priv) ;
int audiodsp_release_mailbox(struct audiodsp_priv *priv);
int dsp_mailbox_send(struct audiodsp_priv *priv,int overwrite,int num,int cmd,const char *data,int len);
#if 0
#define pre_read_mailbox(reg)	\
	dma_cache_inv((unsigned long)reg,sizeof(*reg))
#define after_change_mailbox(reg)	\
	dma_cache_wback((unsigned long)reg,sizeof(*reg))	
#else
#define pre_read_mailbox(reg)	
#define after_change_mailbox(reg)	
#endif
#endif
