#ifndef __POWER_MGR_HEADER_
#define __POWER_MGR_HEADER_

#include <mach/am_regs.h>
#include <mach/clock.h>
/* clock gate control */

#define CLK_GATE_ON(_MOD) \
    do{                     \
        if(GCLK_ref[GCLK_IDX_##_MOD]++ == 0){ \
            if (0) printk(KERN_INFO "gate on %s %x, %x\n", GCLK_NAME_##_MOD, GCLK_REG_##_MOD, GCLK_MASK_##_MOD); \
            SET_CBUS_REG_MASK(GCLK_REG_##_MOD, GCLK_MASK_##_MOD); \
        } \
    }while(0)
            

#define CLK_GATE_OFF(_MOD) \
    do{                             \
        if(GCLK_ref[GCLK_IDX_##_MOD] == 0)    \
            break;                  \
        if(--GCLK_ref[GCLK_IDX_##_MOD] == 0){ \
            if (0) printk(KERN_INFO "gate off %s %x, %x\n", GCLK_NAME_##_MOD, GCLK_REG_##_MOD, GCLK_MASK_##_MOD); \
            CLEAR_CBUS_REG_MASK(GCLK_REG_##_MOD, GCLK_MASK_##_MOD); \
        } \
    }while(0)

#define IS_CLK_GATE_ON(_MOD) (READ_CBUS_REG(GCLK_REG_##_MOD) & (GCLK_MASK_##_MOD))
#define GATE_INIT(_MOD) GCLK_ref[GCLK_IDX_##_MOD] = IS_CLK_GATE_ON(_MOD)?1:0

#define GCLK_IDX_DDR         0
#define GCLK_NAME_DDR      "DDR"
#define GCLK_DEV_DDR      "CLKGATE_DDR"
#define GCLK_REG_DDR      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_DDR      (1<<0)

#define GCLK_IDX_VLD_CLK         1
#define GCLK_NAME_VLD_CLK      "VLD_CLK"
#define GCLK_DEV_VLD_CLK      "CLKGATE_VLD_CLK"
#define GCLK_REG_VLD_CLK      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_VLD_CLK      (1<<1)

#define GCLK_IDX_IQIDCT_CLK         2
#define GCLK_NAME_IQIDCT_CLK      "IQIDCT_CLK"
#define GCLK_DEV_IQIDCT_CLK      "CLKGATE_IQIDCT_CLK"
#define GCLK_REG_IQIDCT_CLK      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_IQIDCT_CLK      (1<<2)

#define GCLK_IDX_MC_CLK         3
#define GCLK_NAME_MC_CLK      "MC_CLK"
#define GCLK_DEV_MC_CLK      "CLKGATE_MC_CLK"
#define GCLK_REG_MC_CLK      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_MC_CLK      (1<<3)

#define GCLK_IDX_AHB_BRIDGE         4
#define GCLK_NAME_AHB_BRIDGE      "AHB_BRIDGE"
#define GCLK_DEV_AHB_BRIDGE      "CLKGATE_AHB_BRIDGE"
#define GCLK_REG_AHB_BRIDGE      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_AHB_BRIDGE      (1<<4)

#define GCLK_IDX_ISA         5
#define GCLK_NAME_ISA      "ISA"
#define GCLK_DEV_ISA      "CLKGATE_ISA"
#define GCLK_REG_ISA      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_ISA      (1<<5)

#define GCLK_IDX_MMC_DDR         6
#define GCLK_NAME_MMC_DDR      "MMC_DDR"
#define GCLK_DEV_MMC_DDR      "CLKGATE_MMC_DDR"
#define GCLK_REG_MMC_DDR      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_MMC_DDR      (1<<6)

#define GCLK_IDX__1200XXX       7
#define GCLK_NAME__1200XXX      "_1200XXX"
#define GCLK_DEV__1200XXX      "CLKGATE__1200XXX"
#define GCLK_REG__1200XXX      (HHI_GCLK_MPEG0)   
#define GCLK_MASK__1200XXX      (1<<7)

#define GCLK_IDX_IR_REMOTE         8
#define GCLK_NAME_IR_REMOTE      "IR_REMOTE"
#define GCLK_DEV_IR_REMOTE      "CLKGATE_IR_REMOTE"
#define GCLK_REG_IR_REMOTE      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_IR_REMOTE      (1<<8)

#define GCLK_IDX_I2C         9
#define GCLK_NAME_I2C      "I2C"
#define GCLK_DEV_I2C      "CLKGATE_I2C"
#define GCLK_REG_I2C      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_I2C      (1<<9)

#define GCLK_IDX_SAR_ADC         10
#define GCLK_NAME_SAR_ADC      "SAR_ADC"
#define GCLK_DEV_SAR_ADC      "CLKGATE_SAR_ADC"
#define GCLK_REG_SAR_ADC      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_SAR_ADC      (1<<10)

#define GCLK_IDX_SMART_CARD_MPEG_DOMAIN         11
#define GCLK_NAME_SMART_CARD_MPEG_DOMAIN      "SMART_CARD_MPEG_DOMAIN"
#define GCLK_DEV_SMART_CARD_MPEG_DOMAIN      "CLKGATE_SMART_CARD_MPEG_DOMAIN"
#define GCLK_REG_SMART_CARD_MPEG_DOMAIN      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_SMART_CARD_MPEG_DOMAIN      (1<<11)

#define GCLK_IDX_RANDOM_NUM_GEN         12
#define GCLK_NAME_RANDOM_NUM_GEN      "RANDOM_NUM_GEN"
#define GCLK_DEV_RANDOM_NUM_GEN      "CLKGATE_RANDOM_NUM_GEN"
#define GCLK_REG_RANDOM_NUM_GEN      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_RANDOM_NUM_GEN      (1<<12)

#define GCLK_IDX_UART0         13
#define GCLK_NAME_UART0      "UART0"
#define GCLK_DEV_UART0      "CLKGATE_UART0"
#define GCLK_REG_UART0      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_UART0      (1<<13)

#define GCLK_IDX_RTC         14
#define GCLK_NAME_RTC      "RTC"
#define GCLK_DEV_RTC      "CLKGATE_RTC"
#define GCLK_REG_RTC      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_RTC      (1<<14)

#define GCLK_IDX_STREAM         15
#define GCLK_NAME_STREAM      "STREAM"
#define GCLK_DEV_STREAM      "CLKGATE_STREAM"
#define GCLK_REG_STREAM      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_STREAM      (1<<15)

#define GCLK_IDX_ASYNC_FIFO         16
#define GCLK_NAME_ASYNC_FIFO      "ASYNC_FIFO"
#define GCLK_DEV_ASYNC_FIFO      "CLKGATE_ASYNC_FIFO"
#define GCLK_REG_ASYNC_FIFO      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_ASYNC_FIFO      (1<<16)

#define GCLK_IDX_SDIO         17
#define GCLK_NAME_SDIO      "SDIO"
#define GCLK_DEV_SDIO      "CLKGATE_SDIO"
#define GCLK_REG_SDIO      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_SDIO      (1<<17)

#define GCLK_IDX_AUD_BUF         18
#define GCLK_NAME_AUD_BUF      "AUD_BUF"
#define GCLK_DEV_AUD_BUF      "CLKGATE_AUD_BUF"
#define GCLK_REG_AUD_BUF      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_AUD_BUF      (1<<18)

#define GCLK_IDX_HIU_PARSER         19
#define GCLK_NAME_HIU_PARSER      "HIU_PARSER"
#define GCLK_DEV_HIU_PARSER      "CLKGATE_HIU_PARSER"
#define GCLK_REG_HIU_PARSER      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_HIU_PARSER      (1<<19)

#define GCLK_IDX_AHB_SRAM         20
#define GCLK_NAME_AHB_SRAM      "AHB_SRAM"
#define GCLK_DEV_AHB_SRAM      "CLKGATE_AHB_SRAM"
#define GCLK_REG_AHB_SRAM      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_AHB_SRAM      (1<<20)

#define GCLK_IDX_AMRISC         21
#define GCLK_NAME_AMRISC      "AMRISC"
#define GCLK_DEV_AMRISC      "CLKGATE_AMRISC"
#define GCLK_REG_AMRISC      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_AMRISC      (1<<21)

#define GCLK_IDX_BT656_IN         22
#define GCLK_NAME_BT656_IN      "BT656_IN"
#define GCLK_DEV_BT656_IN      "CLKGATE_BT656_IN"
#define GCLK_REG_BT656_IN      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_BT656_IN      (1<<22)

#define GCLK_IDX_ASSIST_MISC         23
#define GCLK_NAME_ASSIST_MISC      "ASSIST_MISC"
#define GCLK_DEV_ASSIST_MISC      "CLKGATE_ASSIST_MISC"
#define GCLK_REG_ASSIST_MISC      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_ASSIST_MISC      (1<<23)

#define GCLK_IDX_VENC_MISC         24
#define GCLK_NAME_VENC_MISC      "VENC_MISC"
#define GCLK_DEV_VENC_MISC      "CLKGATE_VENC_MISC"
#define GCLK_REG_VENC_MISC      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_VENC_MISC      (1<<24)

#define GCLK_IDX_ENC480I         25
#define GCLK_NAME_ENC480I      "ENC480I"
#define GCLK_DEV_ENC480I      "CLKGATE_ENC480I"
#define GCLK_REG_ENC480I      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_ENC480I      (1<<25)

#define GCLK_IDX_ENC480P_MPEG_DOMAIN         26
#define GCLK_NAME_ENC480P_MPEG_DOMAIN      "ENC480P_MPEG_DOMAIN"
#define GCLK_DEV_ENC480P_MPEG_DOMAIN      "CLKGATE_ENC480P_MPEG_DOMAIN"
#define GCLK_REG_ENC480P_MPEG_DOMAIN      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_ENC480P_MPEG_DOMAIN      (1<<26)

#define GCLK_IDX_LCD         27
#define GCLK_NAME_LCD      "LCD"
#define GCLK_DEV_LCD      "CLKGATE_LCD"
#define GCLK_REG_LCD      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_LCD      (1<<27)

#define GCLK_IDX_VI_CORE         28
#define GCLK_NAME_VI_CORE      "VI_CORE"
#define GCLK_DEV_VI_CORE      "CLKGATE_VI_CORE"
#define GCLK_REG_VI_CORE      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_VI_CORE      (1<<28)

#define GCLK_IDX_SPI2         29
#define GCLK_NAME_SPI2      "SPI2"
#define GCLK_DEV_SPI2      "CLKGATE_SPI2"
#define GCLK_REG_SPI2      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_SPI2      (1<<30)

#define GCLK_IDX_MDEC_CLK_ASSIST         30
#define GCLK_NAME_MDEC_CLK_ASSIST      "MDEC_CLK_ASSIST"
#define GCLK_DEV_MDEC_CLK_ASSIST      "CLKGATE_MDEC_CLK_ASSIST"
#define GCLK_REG_MDEC_CLK_ASSIST      (HHI_GCLK_MPEG0)   
#define GCLK_MASK_MDEC_CLK_ASSIST      (1<<31)

/***********************************************************/

#define GCLK_IDX_MDEC_CLK_PSC         31
#define GCLK_NAME_MDEC_CLK_PSC      "MDEC_CLK_PSC"
#define GCLK_DEV_MDEC_CLK_PSC      "CLKGATE_MDEC_CLK_PSC"
#define GCLK_REG_MDEC_CLK_PSC      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_MDEC_CLK_PSC      (1<<0)

#define GCLK_IDX_SPI1         32
#define GCLK_NAME_SPI1      "SPI1"
#define GCLK_DEV_SPI1      "CLKGATE_SPI1"
#define GCLK_REG_SPI1      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_SPI1      (1<<1)

#define GCLK_IDX_AUD_IN         33
#define GCLK_NAME_AUD_IN      "AUD_IN"
#define GCLK_DEV_AUD_IN      "CLKGATE_AUD_IN"
#define GCLK_REG_AUD_IN      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AUD_IN      (1<<2)

#define GCLK_IDX_ETHERNET         34
#define GCLK_NAME_ETHERNET      "ETHERNET"
#define GCLK_DEV_ETHERNET      "CLKGATE_ETHERNET"
#define GCLK_REG_ETHERNET      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_ETHERNET      (1<<3)

#define GCLK_IDX_DEMUX         35
#define GCLK_NAME_DEMUX      "DEMUX"
#define GCLK_DEV_DEMUX      "CLKGATE_DEMUX"
#define GCLK_REG_DEMUX      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_DEMUX      (1<<4)

#define GCLK_IDX_AIU_AI_TOP_GLUE         36
#define GCLK_NAME_AIU_AI_TOP_GLUE      "AIU_AI_TOP_GLUE"
#define GCLK_DEV_AIU_AI_TOP_GLUE      "CLKGATE_AIU_AI_TOP_GLUE"
#define GCLK_REG_AIU_AI_TOP_GLUE      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AIU_AI_TOP_GLUE      (1<<6)

#define GCLK_IDX_AIU_IEC958         37
#define GCLK_NAME_AIU_IEC958      "AIU_IEC958"
#define GCLK_DEV_AIU_IEC958      "CLKGATE_AIU_IEC958"
#define GCLK_REG_AIU_IEC958      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AIU_IEC958      (1<<7)

#define GCLK_IDX_AIU_I2S_OUT         38
#define GCLK_NAME_AIU_I2S_OUT      "AIU_I2S_OUT"
#define GCLK_DEV_AIU_I2S_OUT      "CLKGATE_AIU_I2S_OUT"
#define GCLK_REG_AIU_I2S_OUT      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AIU_I2S_OUT      (1<<8)

#define GCLK_IDX_AIU_AMCLK_MEASURE         39
#define GCLK_NAME_AIU_AMCLK_MEASURE      "AIU_AMCLK_MEASURE"
#define GCLK_DEV_AIU_AMCLK_MEASURE      "CLKGATE_AIU_AMCLK_MEASURE"
#define GCLK_REG_AIU_AMCLK_MEASURE      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AIU_AMCLK_MEASURE      (1<<9)

#define GCLK_IDX_AIU_AIFIFO2         40
#define GCLK_NAME_AIU_AIFIFO2      "AIU_AIFIFO2"
#define GCLK_DEV_AIU_AIFIFO2      "CLKGATE_AIU_AIFIFO2"
#define GCLK_REG_AIU_AIFIFO2      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AIU_AIFIFO2      (1<<10)

#define GCLK_IDX_AIU_AUD_MIXER         41
#define GCLK_NAME_AIU_AUD_MIXER      "AIU_AUD_MIXER"
#define GCLK_DEV_AIU_AUD_MIXER      "CLKGATE_AIU_AUD_MIXER"
#define GCLK_REG_AIU_AUD_MIXER      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AIU_AUD_MIXER      (1<<11)

#define GCLK_IDX_AIU_MIXER_REG         42
#define GCLK_NAME_AIU_MIXER_REG      "AIU_MIXER_REG"
#define GCLK_DEV_AIU_MIXER_REG      "CLKGATE_AIU_MIXER_REG"
#define GCLK_REG_AIU_MIXER_REG      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AIU_MIXER_REG      (1<<12)

#define GCLK_IDX_AIU_ADC         43
#define GCLK_NAME_AIU_ADC      "AIU_ADC"
#define GCLK_DEV_AIU_ADC      "CLKGATE_AIU_ADC"
#define GCLK_REG_AIU_ADC      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AIU_ADC      (1<<13)

#define GCLK_IDX_BLK_MOV         44
#define GCLK_NAME_BLK_MOV      "BLK_MOV"
#define GCLK_DEV_BLK_MOV      "CLKGATE_BLK_MOV"
#define GCLK_REG_BLK_MOV      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_BLK_MOV      (1<<14)

#define GCLK_IDX_UART1         45
#define GCLK_NAME_UART1      "UART1"
#define GCLK_DEV_UART1      "CLKGATE_UART1"
#define GCLK_REG_UART1      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_UART1      (1<<16)

#define GCLK_IDX_LED_PWM         46
#define GCLK_NAME_LED_PWM      "LED_PWM"
#define GCLK_DEV_LED_PWM      "CLKGATE_LED_PWM"
#define GCLK_REG_LED_PWM      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_LED_PWM      (1<<17)

#define GCLK_IDX_VGHL_PWM         47
#define GCLK_NAME_VGHL_PWM      "VGHL_PWM"
#define GCLK_DEV_VGHL_PWM      "CLKGATE_VGHL_PWM"
#define GCLK_REG_VGHL_PWM      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_VGHL_PWM      (1<<18)

#define GCLK_IDX_RESERVED0         48
#define GCLK_NAME_RESERVED0      "RESERVED0"
#define GCLK_DEV_RESERVED0      "CLKGATE_RESERVED0"
#define GCLK_REG_RESERVED0      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_RESERVED0      (1<<19)

#define GCLK_IDX_GE2D         49
#define GCLK_NAME_GE2D      "GE2D"
#define GCLK_DEV_GE2D      "CLKGATE_GE2D"
#define GCLK_REG_GE2D      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_GE2D      (1<<20)

#define GCLK_IDX_USB0         50
#define GCLK_NAME_USB0      "USB0"
#define GCLK_DEV_USB0      "CLKGATE_USB0"
#define GCLK_REG_USB0      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_USB0      (1<<21)

#define GCLK_IDX_USB1         51
#define GCLK_NAME_USB1      "USB1"
#define GCLK_DEV_USB1      "CLKGATE_USB1"
#define GCLK_REG_USB1      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_USB1      (1<<22)

#define GCLK_IDX_RESET         52
#define GCLK_NAME_RESET      "RESET"
#define GCLK_DEV_RESET      "CLKGATE_RESET"
#define GCLK_REG_RESET      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_RESET      (1<<23)

#define GCLK_IDX_NAND         53
#define GCLK_NAME_NAND      "NAND"
#define GCLK_DEV_NAND      "CLKGATE_NAND"
#define GCLK_REG_NAND      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_NAND      (1<<24)

#define GCLK_IDX_HIU_PARSER_TOP         54
#define GCLK_NAME_HIU_PARSER_TOP      "HIU_PARSER_TOP"
#define GCLK_DEV_HIU_PARSER_TOP      "CLKGATE_HIU_PARSER_TOP"
#define GCLK_REG_HIU_PARSER_TOP      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_HIU_PARSER_TOP      (1<<25)

#define GCLK_IDX_MDEC_CLK_DBLK         55
#define GCLK_NAME_MDEC_CLK_DBLK      "MDEC_CLK_DBLK"
#define GCLK_DEV_MDEC_CLK_DBLK      "CLKGATE_MDEC_CLK_DBLK"
#define GCLK_REG_MDEC_CLK_DBLK      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_MDEC_CLK_DBLK      (1<<26)

#define GCLK_IDX_MDEC_CLK_PIC_DC         56
#define GCLK_NAME_MDEC_CLK_PIC_DC      "MDEC_CLK_PIC_DC"
#define GCLK_DEV_MDEC_CLK_PIC_DC      "CLKGATE_MEDC_CLK_PIC_DC"
#define GCLK_REG_MDEC_CLK_PIC_DC      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_MDEC_CLK_PIC_DC      (1<<27)

#define GCLK_IDX_VIDEO_IN         57
#define GCLK_NAME_VIDEO_IN      "VIDEO_IN"
#define GCLK_DEV_VIDEO_IN      "CLKGATE_VIDEO_IN"
#define GCLK_REG_VIDEO_IN      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_VIDEO_IN      (1<<28)

#define GCLK_IDX_AHB_ARB0         58
#define GCLK_NAME_AHB_ARB0      "AHB_ARB0"
#define GCLK_DEV_AHB_ARB0      "CLKGATE_AHB_ARB0"
#define GCLK_REG_AHB_ARB0      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_AHB_ARB0      (1<<29)

#define GCLK_IDX_EFUSE         59
#define GCLK_NAME_EFUSE      "EFUSE"
#define GCLK_DEV_EFUSE      "CLKGATE_EFUSE"
#define GCLK_REG_EFUSE      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_EFUSE      (1<<30)

#define GCLK_IDX_ROM_CLK         60
#define GCLK_NAME_ROM_CLK      "ROM_CLK"
#define GCLK_DEV_ROM_CLK      "CLKGATE_ROM_CLK"
#define GCLK_REG_ROM_CLK      (HHI_GCLK_MPEG1)   
#define GCLK_MASK_ROM_CLK      (1<<31)

/**************************************************************/

#define GCLK_IDX_AXI_BUS         61
#define GCLK_NAME_AXI_BUS      "AXI_BUS"
#define GCLK_DEV_AXI_BUS      "CLKGATE_AXI_BUS"
#define GCLK_REG_AXI_BUS      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_AXI_BUS      (1<<0)

#define GCLK_IDX_AHB_DATA_BUS         62
#define GCLK_NAME_AHB_DATA_BUS      "AHB_DATA_BUS"
#define GCLK_DEV_AHB_DATA_BUS      "CLKGATE_AHB_DATA_BUS"
#define GCLK_REG_AHB_DATA_BUS      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_AHB_DATA_BUS      (1<<1)

#define GCLK_IDX_AHB_CONTROL_BUS         63
#define GCLK_NAME_AHB_CONTROL_BUS      "AHB_CONTROL_BUS"
#define GCLK_DEV_AHB_CONTROL_BUS      "CLKGATE_AHB_CONTROL_BUS"
#define GCLK_REG_AHB_CONTROL_BUS      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_AHB_CONTROL_BUS      (1<<2)

#define GCLK_IDX_HDMI_MPEG_DOMAIN         64
#define GCLK_NAME_HDMI_MPEG_DOMAIN      "HDMI_MPEG_DOMAIN"
#define GCLK_DEV_HDMI_MPEG_DOMAIN      "CLKGATE_HDMI_MPEG_DOMAIN"
#define GCLK_REG_HDMI_MPEG_DOMAIN      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_HDMI_MPEG_DOMAIN      (1<<3)

#define GCLK_IDX_MEDIA_CPU         65
#define GCLK_NAME_MEDIA_CPU      "MEDIA_CPU"
#define GCLK_DEV_MEDIA_CPU      "CLKGATE_MEDIA_CPU"
#define GCLK_REG_MEDIA_CPU      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_MEDIA_CPU      (1<<4)

#define GCLK_IDX_WIFI         66
#define GCLK_NAME_WIFI      "WIFI"
#define GCLK_DEV_WIFI      "CLKGATE_WIFI"
#define GCLK_REG_WIFI      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_WIFI      (1<<5)

#define GCLK_IDX_SATA         67
#define GCLK_NAME_SATA      "SATA"
#define GCLK_DEV_SATA      "CLKGATE_SATA"
#define GCLK_REG_SATA      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_SATA      (1<<6)

#define GCLK_IDX_MISC_SATA_TO_DDR         68
#define GCLK_NAME_MISC_SATA_TO_DDR      "MISC_SATA_TO_DDR"
#define GCLK_DEV_MISC_SATA_TO_DDR      "CLKGATE_MISC_SATA_TO_DDR"
#define GCLK_REG_MISC_SATA_TO_DDR      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_MISC_SATA_TO_DDR      (1<<7)

#define GCLK_IDX_MISC_USB1_TO_DDR         69
#define GCLK_NAME_MISC_USB1_TO_DDR      "MISC_USB1_TO_DDR"
#define GCLK_DEV_MISC_USB1_TO_DDR      "CLKGATE_MISC_USB1_TO_DDR"
#define GCLK_REG_MISC_USB1_TO_DDR      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_MISC_USB1_TO_DDR      (1<<8)

#define GCLK_IDX_MISC_USB0_TO_DDR         70
#define GCLK_NAME_MISC_USB0_TO_DDR      "MISC_USB0_TO_DDR"
#define GCLK_DEV_MISC_USB0_TO_DDR      "CLKGATE_MISC_USB0_TO_DDR"
#define GCLK_REG_MISC_USB0_TO_DDR      (HHI_GCLK_MPEG2)   
#define GCLK_MASK_MISC_USB0_TO_DDR      (1<<9)

/**************************************************************/

#define GCLK_IDX_VCLK1_VENC_BIST         71
#define GCLK_NAME_VCLK1_VENC_BIST      "VCLK1_VENC_BIST"
#define GCLK_DEV_VCLK1_VENC_BIST      "CLKGATE_VCLK1_VENC_BIST"
#define GCLK_REG_VCLK1_VENC_BIST      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK1_VENC_BIST      (1<<1)

#define GCLK_IDX_VCLK1_VENC_ENCI         72
#define GCLK_NAME_VCLK1_VENC_ENCI      "VCLK1_VENC_ENCI"
#define GCLK_DEV_VCLK1_VENC_ENCI      "CLKGATE_VCLK1_VENC_ENCI"
#define GCLK_REG_VCLK1_VENC_ENCI      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK1_VENC_ENCI      (1<<2)

#define GCLK_IDX_VCLK1_VENC_DVI         73
#define GCLK_NAME_VCLK1_VENC_DVI      "VCLK1_VENC_DVI"
#define GCLK_DEV_VCLK1_VENC_DVI      "CLKGATE_VCLK1_VENC_DVI"
#define GCLK_REG_VCLK1_VENC_DVI      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK1_VENC_DVI      (1<<3)

#define GCLK_IDX_VCLK1_VENC_656         74
#define GCLK_NAME_VCLK1_VENC_656      "VCLK1_VENC_656"
#define GCLK_DEV_VCLK1_VENC_656      "CLKGATE_VCLK1_VENC_656"
#define GCLK_REG_VCLK1_VENC_656      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK1_VENC_656      (1<<4)

#define GCLK_IDX_VCLK2_VENC_BIST         75
#define GCLK_NAME_VCLK2_VENC_BIST      "VCLK2_VENC_BIST"
#define GCLK_DEV_VCLK2_VENC_BIST      "CLKGATE_VCLK2_VENC_BIST"
#define GCLK_REG_VCLK2_VENC_BIST      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK2_VENC_BIST      (1<<6)

#define GCLK_IDX_VCLK2_VENC_ENC480P         76
#define GCLK_NAME_VCLK2_VENC_ENC480P      "VCLK2_VENC_ENC480P"
#define GCLK_DEV_VCLK2_VENC_ENC480P      "CLKGATE_VCLK2_VENC_ENC480P"
#define GCLK_REG_VCLK2_VENC_ENC480P      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK2_VENC_ENC480P      (1<<7)

#define GCLK_IDX_VCLK2_VENC_DVI         77
#define GCLK_NAME_VCLK2_VENC_DVI      "VCLK2_VENC_DVI"
#define GCLK_DEV_VCLK2_VENC_DVI      "CLKGATE_VCLK2_VENC_DVI"
#define GCLK_REG_VCLK2_VENC_DVI      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK2_VENC_DVI      (1<<8)

#define GCLK_IDX_VCLK2_VIU         78
#define GCLK_NAME_VCLK2_VIU      "VCLK2_VIU"
#define GCLK_DEV_VCLK2_VIU      "CLKGATE_VCLK2_VIU"
#define GCLK_REG_VCLK2_VIU      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK2_VIU      (1<<9)

#define GCLK_IDX_VCLK3_DVI         79
#define GCLK_NAME_VCLK3_DVI      "VCLK3_DVI"
#define GCLK_DEV_VCLK3_DVI      "CLKGATE_VCLK3_DVI"
#define GCLK_REG_VCLK3_DVI      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK3_DVI      (1<<10)

#define GCLK_IDX_VCLK3_MISC         80
#define GCLK_NAME_VCLK3_MISC      "VCLK3_MISC"
#define GCLK_DEV_VCLK3_MISC      "CLKGATE_VCLK3_MISC"
#define GCLK_REG_VCLK3_MISC      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK3_MISC      (1<<11)

#define GCLK_IDX_VCLK3_DAC         81
#define GCLK_NAME_VCLK3_DAC      "VCLK3_DAC"
#define GCLK_DEV_VCLK3_DAC      "CLKGATE_VCLK3_DAC"
#define GCLK_REG_VCLK3_DAC      (HHI_GCLK_OTHER)   
#define GCLK_MASK_VCLK3_DAC      (1<<12)

#define GCLK_IDX_AIU_AUD_DAC_CLK         82
#define GCLK_NAME_AIU_AUD_DAC_CLK      "AIU_AUD_DAC_CLK"
#define GCLK_DEV_AIU_AUD_DAC_CLK      "CLKGATE_AIU_AUD_DAC_CLK"
#define GCLK_REG_AIU_AUD_DAC_CLK      (HHI_GCLK_OTHER)   
#define GCLK_MASK_AIU_AUD_DAC_CLK      (1<<13)

#define GCLK_IDX_AIU_I2S_SLOW         83
#define GCLK_NAME_AIU_I2S_SLOW      "AIU_I2S_SLOW"
#define GCLK_DEV_AIU_I2S_SLOW      "CLKGATE_AIU_I2S_SLOW"
#define GCLK_REG_AIU_I2S_SLOW      (HHI_GCLK_OTHER)   
#define GCLK_MASK_AIU_I2S_SLOW      (1<<14)

#define GCLK_IDX_AIU_I2S_DAC_AMCLK         84
#define GCLK_NAME_AIU_I2S_DAC_AMCLK      "AIU_I2S_DAC_AMCLK"
#define GCLK_DEV_AIU_I2S_DAC_AMCLK      "CLKGATE_AIU_I2S_DAC_AMCLK"
#define GCLK_REG_AIU_I2S_DAC_AMCLK      (HHI_GCLK_OTHER)   
#define GCLK_MASK_AIU_I2S_DAC_AMCLK      (1<<15)

#define GCLK_IDX_AIU_ICE958_AMCLK         85
#define GCLK_NAME_AIU_ICE958_AMCLK      "AIU_ICE958_AMCLK"
#define GCLK_DEV_AIU_ICE958_AMCLK      "CLKGATE_AIU_ICE958_AMCLK"
#define GCLK_REG_AIU_ICE958_AMCLK      (HHI_GCLK_OTHER)   
#define GCLK_MASK_AIU_ICE958_AMCLK      (1<<16)

#define GCLK_IDX_HDMI         86
#define GCLK_NAME_HDMI      "HDMI"
#define GCLK_DEV_HDMI      "CLKGATE_HDMI"
#define GCLK_REG_HDMI      (HHI_GCLK_OTHER)   
#define GCLK_MASK_HDMI      (1<<17)

#define GCLK_IDX_AIU_AUD_DAC         87
#define GCLK_NAME_AIU_AUD_DAC      "AIU_AUD_DAC"
#define GCLK_DEV_AIU_AUD_DAC      "CLKGATE_AIU_AUD_DAC"
#define GCLK_REG_AIU_AUD_DAC      (HHI_GCLK_OTHER)   
#define GCLK_MASK_AIU_AUD_DAC      (1<<19)

#define GCLK_IDX_ENC480P         88
#define GCLK_NAME_ENC480P      "ENC480P"
#define GCLK_DEV_ENC480P      "CLKGATE_ENC480P"
#define GCLK_REG_ENC480P      (HHI_GCLK_OTHER)   
#define GCLK_MASK_ENC480P      (1<<20)

#define GCLK_IDX_SMART_CARD                      89
#define GCLK_NAME_SMART_CARD      "SMART_CARD"
#define GCLK_DEV_SMART_CARD      "CLKGATE_SMART_CARD"
#define GCLK_REG_SMART_CARD                   (HHI_GCLK_OTHER)   
#define GCLK_MASK_SMART_CARD                      (1<<21)

#define GCLK_IDX_MAX 90
extern unsigned char GCLK_ref[GCLK_IDX_MAX];

#define REGISTER_CLK(_MOD) \
static struct clk CLK_##_MOD = {            \
	.name		= GCLK_NAME_##_MOD,             \
	.clock_index = GCLK_IDX_##_MOD,          \
	.clock_gate_reg_adr = GCLK_REG_##_MOD,  \
	.clock_gate_reg_mask = GCLK_MASK_##_MOD,    \
}

#define CLK_LOOKUP_ITEM(_MOD) \
    {           \
	        .dev_id = GCLK_DEV_##_MOD, \
	        .con_id = GCLK_NAME_##_MOD, \
	        .clk    = &CLK_##_MOD,   \
    }



/**********************/
/* internal audio dac control */
#define ADAC_RESET                		(0x5000+0x00*4)
#define ADAC_LATCH                		(0x5000+0x01*4)
#define ADAC_POWER_CTRL_REG1      		(0x5000+0x10*4)
#define ADAC_POWER_CTRL_REG2      		(0x5000+0x11*4)

extern int audio_internal_dac_disable(void);

/* video dac control */
extern int  video_dac_enable(unsigned char enable_mask);

extern int  video_dac_disable(void);


#endif
