#ifndef __BOARD_8626M_W10_H
#define __BOARD_8626M_W10_H

#include <asm/page.h>

#define PHYS_MEM_START		(0x80000000)
#define PHYS_MEM_SIZE		(1024*SZ_1M)
#define PHYS_MEM_END		(PHYS_MEM_START + PHYS_MEM_SIZE -1 )

/******** Reserved memory setting ************************/
#define RESERVED_MEM_START	(0x80000000+64*SZ_1M)	/*start at the second 64M*/

/******** CODEC memory setting ************************/
//	Codec need 16M for 1080p decode
//	4M for sd decode;
#define ALIGN_MSK			((SZ_1M)-1)
#define U_ALIGN(x)			((x+ALIGN_MSK)&(~ALIGN_MSK))
#define D_ALIGN(x)			((x)&(~ALIGN_MSK))

/******** AUDIODSP memory setting ************************/
#define AUDIODSP_ADDR_START	U_ALIGN(RESERVED_MEM_START)	/*audiodsp memstart*/
#define AUDIODSP_ADDR_END	(AUDIODSP_ADDR_START+SZ_1M-1)	/*audiodsp memend*/

/******** Frame buffer memory configuration ***********/
#define OSD_480_PIX			(640*480)
#define OSD_576_PIX			(768*576)
#define OSD_720_PIX			(1280*720)
#define OSD_1080_PIX		(1920*1080)
#define OSD_PANEL_PIX		(800*600)
#define B16BpP	(2)
#define B32BpP	(4)
#define DOUBLE_BUFFER	(2)

#define OSD1_MAX_MEM		U_ALIGN(OSD_1080_PIX*B32BpP*DOUBLE_BUFFER)
#define OSD2_MAX_MEM		U_ALIGN(OSD_1080_PIX*B32BpP*DOUBLE_BUFFER)

/******** Reserved memory configuration ***************/
#define OSD1_ADDR_START		U_ALIGN(AUDIODSP_ADDR_END )
#define OSD1_ADDR_END		(OSD1_ADDR_START+OSD1_MAX_MEM - 1)
#define OSD2_ADDR_START		U_ALIGN(OSD1_ADDR_END)
#define OSD2_ADDR_END		(OSD2_ADDR_START +OSD2_MAX_MEM -1)

#if defined(CONFIG_FB_OSD2_ENABLE)
#define PMEM_START		U_ALIGN(OSD2_ADDR_END)
#else
#define PMEM_START		U_ALIGN(OSD1_ADDR_END)
#endif
#if defined(CONFIG_ANDROID_PMEM) || defined(CONFIG_CMEM)
#define PMEM_SIZE               (32*SZ_1M)
#else
#define PMEM_SIZE               0
#endif
#define PMEM_END			(PMEM_START + PMEM_SIZE-1)

#if defined(CONFIG_AM_VDEC_H264)
#define CODEC_MEM_SIZE		U_ALIGN(CONFIG_AM_VDEC_H264_DECODER_MEM_SIZE*SZ_1M)
#else
#define CODEC_MEM_SIZE		U_ALIGN(16*SZ_1M)
#endif
#define CODEC_ADDR_START	U_ALIGN(PMEM_END)
#define CODEC_ADDR_END		(CODEC_ADDR_START+CODEC_MEM_SIZE-1)

/********VDIN memory configuration ***************/
#ifdef CONFIG_TVIN_VIUIN
#define VDIN_MEM_SIZE			(SZ_1M*15)
#define VDIN_ADDR_START		U_ALIGN(CODEC_ADDR_END)
#define VDIN_ADDR_END		(VDIN_ADDR_START +VDIN_MEM_SIZE -1)
#else
#define VDIN_ADDR_START		U_ALIGN(OSD2_ADDR_END)
#define VDIN_ADDR_END		(VDIN_ADDR_START +CODEC_MEM_SIZE -1)
#endif

#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
#define DI_MEM_SIZE			(SZ_1M*6)
#else
#define DI_MEM_SIZE			(SZ_1M*30)
#endif
#define DI_ADDR_START		U_ALIGN(VDIN_ADDR_END)
#define DI_ADDR_END			(DI_ADDR_START+DI_MEM_SIZE-1)

#ifdef CONFIG_POST_PROCESS_MANAGER
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
#define PPMGR_MEM_SIZE               1920 * 1088*18
#else
#define PPMGR_MEM_SIZE               1920 * 1088*15
#endif
#else
#define PPMGR_MEM_SIZE		0
#endif /* CONFIG_POST_PROCESS_MANAGER */

#define PPMGR_ADDR_START	U_ALIGN(DI_ADDR_END)
#define PPMGR_ADDR_END		(PPMGR_ADDR_START+PPMGR_MEM_SIZE-1)


#define FREESCALE_MEM_SIZE      PPMGR_MEM_SIZE
#define FREESCALE_ADDR_START PPMGR_ADDR_START
#define FREESCALE_ADDR_END   PPMGR_ADDR_END

#define STREAMBUF_MEM_SIZE   		(SZ_1M*15)
#define STREAMBUF_ADDR_START		U_ALIGN(PPMGR_ADDR_END)
#define STREAMBUF_ADDR_END		(STREAMBUF_ADDR_START+STREAMBUF_MEM_SIZE-1)

#define RESERVED_MEM_END	(STREAMBUF_ADDR_END)

#define CONFIG_ENV_OFFSET       0x1000000	//16M
#define CONFIG_ENV_SIZE         0x8000		//32K

#define BOOTLOADER_OFFSET       0
#define BOOTLOADER_SIZE         0x100000	//1M
#endif
