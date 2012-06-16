#ifndef  LOGO_DEV_OSD_H
#define LOGO_DEV_OSD_H

static int osd0_init(logo_object_t *plogo);
static int osd1_init(logo_object_t *plogo);
static  int  osd_enable_set(int  enable);
static int osd_deinit(void);
static  int  osd_transfer(logo_object_t *plogo);

#endif
