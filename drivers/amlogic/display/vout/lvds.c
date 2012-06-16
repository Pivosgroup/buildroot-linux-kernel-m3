/*
 * AMLOGIC TCON controller driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:  Tim Yao <timyao@amlogic.com>
 *
 */
#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/vout/tcon.h>
#include <linux/vout/vinfo.h>
#include <linux/vout/vout_notify.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/logo/logo.h>
#include <linux/delay.h>
#include <mach/am_regs.h>
#include <mach/clock.h>
#include <asm/fiq.h>

extern unsigned int clk_util_clk_msr(unsigned int clk_mux);

#define BL_MAX_LEVEL 0x100
#define PANEL_NAME	"panel"

typedef struct {
	lcdConfig_t conf;
	vinfo_t lcd_info;
} tcon_dev_t;

static tcon_dev_t *pDev = NULL;

static void _tcon_init(lcdConfig_t *pConf) ;

#ifdef CONFIG_AM_TV_OUTPUT2
static unsigned int vpp2_sel = 0; /*0,vpp; 1, vpp2 */
#endif

static void set_lvds_gamma_table(u16 *data, u32 rgb_mask)
{
    int i;

    while (!(READ_MPEG_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY)));
    WRITE_MPEG_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
                                    (0x1 << rgb_mask)   |
                                    (0x0 << HADR));
    for (i=0;i<256;i++) {
        while (!( READ_MPEG_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY) )) ;
        WRITE_MPEG_REG(L_GAMMA_DATA_PORT, data[i]);
    }
    while (!(READ_MPEG_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY)));
    WRITE_MPEG_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
                                    (0x1 << rgb_mask)   |
                                    (0x23 << HADR));
}

static inline void _init_tcon_lvds(lcdConfig_t *pConf)
{
    //printk("setup lvds tcon\n");
	set_lvds_gamma_table(pConf->GammaTableR, H_SEL_R);
    set_lvds_gamma_table(pConf->GammaTableG, H_SEL_G);
    set_lvds_gamma_table(pConf->GammaTableB, H_SEL_B);

    WRITE_MPEG_REG(L_GAMMA_CNTL_PORT, pConf->gamma_cntl_port);
    WRITE_MPEG_REG(L_GAMMA_VCOM_HSWITCH_ADDR, pConf->gamma_vcom_hswitch_addr);

    WRITE_MPEG_REG(L_RGB_BASE_ADDR,   pConf->rgb_base_addr);
    WRITE_MPEG_REG(L_RGB_COEFF_ADDR,  pConf->rgb_coeff_addr);
    WRITE_MPEG_REG(L_POL_CNTL_ADDR,   pConf->pol_cntl_addr);
    WRITE_MPEG_REG(L_DITH_CNTL_ADDR,  pConf->dith_cntl_addr);

    // WRITE_MPEG_REG(L_STH1_HS_ADDR,    pConf->sth1_hs_addr);
    // WRITE_MPEG_REG(L_STH1_HE_ADDR,    pConf->sth1_he_addr);
    // WRITE_MPEG_REG(L_STH1_VS_ADDR,    pConf->sth1_vs_addr);
    // WRITE_MPEG_REG(L_STH1_VE_ADDR,    pConf->sth1_ve_addr);    

    // WRITE_MPEG_REG(L_OEH_HS_ADDR,     pConf->oeh_hs_addr);
    // WRITE_MPEG_REG(L_OEH_HE_ADDR,     pConf->oeh_he_addr);
    // WRITE_MPEG_REG(L_OEH_VS_ADDR,     pConf->oeh_vs_addr);
    // WRITE_MPEG_REG(L_OEH_VE_ADDR,     pConf->oeh_ve_addr);

    // WRITE_MPEG_REG(L_VCOM_HSWITCH_ADDR, pConf->vcom_hswitch_addr);
    // WRITE_MPEG_REG(L_VCOM_VS_ADDR,    pConf->vcom_vs_addr);
    // WRITE_MPEG_REG(L_VCOM_VE_ADDR,    pConf->vcom_ve_addr);

    // WRITE_MPEG_REG(L_CPV1_HS_ADDR,    pConf->cpv1_hs_addr);
    // WRITE_MPEG_REG(L_CPV1_HE_ADDR,    pConf->cpv1_he_addr);
    // WRITE_MPEG_REG(L_CPV1_VS_ADDR,    pConf->cpv1_vs_addr);
    // WRITE_MPEG_REG(L_CPV1_VE_ADDR,    pConf->cpv1_ve_addr);    

    // WRITE_MPEG_REG(L_STV1_HS_ADDR,    pConf->stv1_hs_addr);
    // WRITE_MPEG_REG(L_STV1_HE_ADDR,    pConf->stv1_he_addr);
    // WRITE_MPEG_REG(L_STV1_VS_ADDR,    pConf->stv1_vs_addr);
    // WRITE_MPEG_REG(L_STV1_VE_ADDR,    pConf->stv1_ve_addr);    

    // WRITE_MPEG_REG(L_OEV1_HS_ADDR,    pConf->oev1_hs_addr);
    // WRITE_MPEG_REG(L_OEV1_HE_ADDR,    pConf->oev1_he_addr);
    // WRITE_MPEG_REG(L_OEV1_VS_ADDR,    pConf->oev1_vs_addr);
    // WRITE_MPEG_REG(L_OEV1_VE_ADDR,    pConf->oev1_ve_addr);    

    WRITE_MPEG_REG(L_INV_CNT_ADDR,    pConf->inv_cnt_addr);
    WRITE_MPEG_REG(L_TCON_MISC_SEL_ADDR, 	pConf->tcon_misc_sel_addr);
    WRITE_MPEG_REG(L_DUAL_PORT_CNTL_ADDR, pConf->dual_port_cntl_addr);

#ifdef CONFIG_AM_TV_OUTPUT2
    if(vpp2_sel){
        CLEAR_MPEG_REG_MASK(VPP2_MISC, VPP_OUT_SATURATE);
    }
    else
#endif        
    {
        CLEAR_MPEG_REG_MASK(VPP_MISC, VPP_OUT_SATURATE);
    }	
	  //printk("RGB_BASE = %x %x\n", READ_MPEG_REG(L_RGB_BASE_ADDR),    pConf->rgb_base_addr);
    //printk("RGB_COEFF = %x %x\n", READ_MPEG_REG(L_RGB_COEFF_ADDR),  pConf->rgb_coeff_addr);
    //printk("POL_CNTL = %x %x\n", READ_MPEG_REG(L_POL_CNTL_ADDR),    pConf->pol_cntl_addr);
    //printk("DITH_CNTL = %x %x\n", READ_MPEG_REG(L_DITH_CNTL_ADDR),  pConf->dith_cntl_addr);

    // printk("STH1_HS = %x %x\n", READ_MPEG_REG(L_STH1_HS_ADDR),    pConf->sth1_hs_addr);
    // printk("STH1_HE = %x %x\n", READ_MPEG_REG(L_STH1_HE_ADDR),    pConf->sth1_he_addr);
    // printk("STH1_VS = %x %x\n", READ_MPEG_REG(L_STH1_VS_ADDR),    pConf->sth1_vs_addr);
    // printk("STH1_VE = %x %x\n", READ_MPEG_REG(L_STH1_VE_ADDR),     pConf->sth1_ve_addr);

    // printk("OEH_HS = %x %x\n", READ_MPEG_REG(L_OEH_HS_ADDR),     pConf->oeh_hs_addr);
    // printk("OEH_HS = %x %x\n", READ_MPEG_REG(L_OEH_HE_ADDR),     pConf->oeh_he_addr);
    // printk("OEH_HS = %x %x\n", READ_MPEG_REG(L_OEH_VS_ADDR),     pConf->oeh_vs_addr);
    // printk("OEH_HS = %x %x\n", READ_MPEG_REG(L_OEH_VE_ADDR),     pConf->oeh_ve_addr);

    // printk("COM_HSWITCH = %x %x\n", READ_MPEG_REG(L_VCOM_HSWITCH_ADDR), pConf->vcom_hswitch_addr);
    // printk("VCOM_VS = %x %x\n", READ_MPEG_REG(L_VCOM_VS_ADDR),    pConf->vcom_vs_addr);
    // printk("VCOM_VE = %x %x\n", READ_MPEG_REG(L_VCOM_VE_ADDR),    pConf->vcom_ve_addr);

    // printk("CPV1_HS = %x %x\n", READ_MPEG_REG(L_CPV1_HS_ADDR),    pConf->cpv1_hs_addr);
    // printk("CPV1_HE = %x %x\n", READ_MPEG_REG(L_CPV1_HE_ADDR),    pConf->cpv1_he_addr);
    // printk("CPV1_VS = %x %x\n", READ_MPEG_REG(L_CPV1_VS_ADDR),    pConf->cpv1_vs_addr);
    // printk("CPV1_VE = %x %x\n", READ_MPEG_REG(L_CPV1_VE_ADDR),    pConf->cpv1_ve_addr);

    // printk("STV1_HS = %x %x\n", READ_MPEG_REG(L_STV1_HS_ADDR),    pConf->stv1_hs_addr);
    // printk("STV1_HS = %x %x\n", READ_MPEG_REG(L_STV1_HE_ADDR),    pConf->stv1_he_addr);
    // printk("STV1_HS = %x %x\n", READ_MPEG_REG(L_STV1_VS_ADDR),    pConf->stv1_vs_addr);
    // printk("STV1_HS = %x %x\n", READ_MPEG_REG(L_STV1_VE_ADDR),    pConf->stv1_ve_addr);

    // printk("OEV1_HS = %x %x\n", READ_MPEG_REG(L_OEV1_HS_ADDR),    pConf->oev1_hs_addr);
    // printk("OEV1_HE = %x %x\n", READ_MPEG_REG(L_OEV1_HE_ADDR),    pConf->oev1_he_addr);
    // printk("OEV1_VS = %x %x\n", READ_MPEG_REG(L_OEV1_VS_ADDR),    pConf->oev1_vs_addr);
    // printk("OEV1_VE = %x %x\n", READ_MPEG_REG(L_OEV1_VE_ADDR),    pConf->oev1_ve_addr);

    //printk("INV_CNT = %x %x\n", READ_MPEG_REG(L_INV_CNT_ADDR),    pConf->inv_cnt_addr);
    //printk("TCON_MISC_SEL_ADDR = %x %x\n", READ_MPEG_REG(L_TCON_MISC_SEL_ADDR), 	pConf->tcon_misc_sel_addr);
    //printk("DUAL_PORT_CNTL = %x %x\n", READ_MPEG_REG(L_DUAL_PORT_CNTL_ADDR), pConf->dual_port_cntl_addr);
}

static void    clk_util_lvds_set_clk_div(  unsigned long   divn_sel,
                                    unsigned long   divn_tcnt,
                                    unsigned long   div2_en  )
{
    // assign          lvds_div_phy_clk_en     = tst_lvds_tmode ? 1'b1         : phy_clk_cntl[10];
    // assign          lvds_div_div2_sel       = tst_lvds_tmode ? atest_i[5]   : phy_clk_cntl[9];
    // assign          lvds_div_sel            = tst_lvds_tmode ? atest_i[7:6] : phy_clk_cntl[8:7];
    // assign          lvds_div_tcnt           = tst_lvds_tmode ? 3'd6         : phy_clk_cntl[6:4];
    // If dividing by 1, just select the divide by 1 path
    if( divn_tcnt == 1 ) { divn_sel = 0; }

    WRITE_MPEG_REG( LVDS_PHY_CLK_CNTL, ((READ_MPEG_REG(LVDS_PHY_CLK_CNTL) & ~((0x3 << 7) | (1 << 9) | (0x7 << 4))) | ((1 << 10) | (divn_sel << 7) | (div2_en << 9) | (((divn_tcnt-1)&0x7) << 4))) );
}

static void vclk_set_lcd_lvds( int lcd_lvds, int pll_sel, int pll_div_sel, int vclk_sel, 
                   unsigned long pll_reg, unsigned long vid_div_reg, unsigned int xd)
{
	//printk("setup lvds clk\n");
    vid_div_reg |= (1 << 16) ; // turn clock gate on
    vid_div_reg |= (pll_sel << 15); // vid_div_clk_sel    
   
    if(vclk_sel) {
      WRITE_MPEG_REG( HHI_VIID_CLK_CNTL, READ_MPEG_REG(HHI_VIID_CLK_CNTL) & ~(1 << 19) );     //disable clk_div0 
    }
    else {
      WRITE_MPEG_REG( HHI_VID_CLK_CNTL, READ_MPEG_REG(HHI_VID_CLK_CNTL) & ~(1 << 19) );     //disable clk_div0 
      WRITE_MPEG_REG( HHI_VID_CLK_CNTL, READ_MPEG_REG(HHI_VID_CLK_CNTL) & ~(1 << 20) );     //disable clk_div1 
    } 

    // delay 2uS to allow the sync mux to switch over
    //WRITE_MPEG_REG( ISA_TIMERE, 0); while( READ_MPEG_REG(ISA_TIMERE) < 2 ) {}    
	udelay(10);
	
	if(pll_sel){
        WRITE_MPEG_REG( HHI_VIID_PLL_CNTL, pll_reg|(1<<30) );
        WRITE_MPEG_REG( HHI_VIID_PLL_CNTL2, 0x65e31ff );
        WRITE_MPEG_REG( HHI_VIID_PLL_CNTL3, 0x9649a941 );
        WRITE_MPEG_REG( HHI_VIID_PLL_CNTL, pll_reg );
    }    
    else{
        WRITE_MPEG_REG( HHI_VID_PLL_CNTL, pll_reg|(1<<30) );
        WRITE_MPEG_REG( HHI_VID_PLL_CNTL2, 0x65e31ff );
        WRITE_MPEG_REG( HHI_VID_PLL_CNTL3, 0x9649a941 );
        WRITE_MPEG_REG( HHI_VID_PLL_CNTL, pll_reg );
    }

    if(pll_div_sel ) WRITE_MPEG_REG( HHI_VIID_DIVIDER_CNTL,   vid_div_reg);
    else WRITE_MPEG_REG( HHI_VID_DIVIDER_CNTL,   vid_div_reg);

    if(vclk_sel) WRITE_MPEG_REG( HHI_VIID_CLK_DIV, (READ_MPEG_REG(HHI_VIID_CLK_DIV) & ~(0xFF << 0)) | (xd-1) );   // setup the XD divider value
    else WRITE_MPEG_REG( HHI_VID_CLK_DIV, (READ_MPEG_REG(HHI_VID_CLK_DIV) & ~(0xFF << 0)) | (xd-1) );   // setup the XD divider value

    // delay 5uS
    //WRITE_MPEG_REG( ISA_TIMERE, 0); while( READ_MPEG_REG(ISA_TIMERE) < 5 ) {}   
    udelay(10);

    if(vclk_sel) {
      if(pll_div_sel) WRITE_MPEG_REG_BITS (HHI_VIID_CLK_CNTL, 4, 16, 3);  // Bit[18:16] - v2_cntl_clk_in_sel
      else WRITE_MPEG_REG_BITS (HHI_VIID_CLK_CNTL, 0, 16, 3);  // Bit[18:16] - cntl_clk_in_sel
      WRITE_MPEG_REG( HHI_VIID_CLK_CNTL, READ_MPEG_REG(HHI_VIID_CLK_CNTL) |  (1 << 19) );     //enable clk_div0 
    }
    else {
      if(pll_div_sel) WRITE_MPEG_REG_BITS (HHI_VID_CLK_CNTL, 4, 16, 3);  // Bit[18:16] - v2_cntl_clk_in_sel
      else WRITE_MPEG_REG_BITS (HHI_VID_CLK_CNTL, 0, 16, 3);  // Bit[18:16] - cntl_clk_in_sel
      WRITE_MPEG_REG( HHI_VID_CLK_CNTL, READ_MPEG_REG(HHI_VID_CLK_CNTL) |  (1 << 19) );     //enable clk_div0 
      WRITE_MPEG_REG( HHI_VID_CLK_CNTL, READ_MPEG_REG(HHI_VID_CLK_CNTL) |  (1 << 20) );     //enable clk_div1 
    }		
    // delay 2uS

    //WRITE_MPEG_REG( ISA_TIMERE, 0); while( READ_MPEG_REG(ISA_TIMERE) < 2 ) {} 
    udelay(10);

    // set tcon_clko setting
    WRITE_MPEG_REG_BITS (HHI_VID_CLK_CNTL, 
                    (
                    (0 << 11) |     //clk_div1_sel
                    (1 << 10) |     //clk_inv
                    (0 << 9)  |     //neg_edge_sel
                    (0 << 5)  |     //tcon high_thresh
                    (0 << 1)  |     //tcon low_thresh
                    (1 << 0)        //cntl_clk_en1
                    ), 
                    20, 12);

#ifdef CONFIG_AM_TV_OUTPUT2
    if(vclk_sel){
#else        
	if(lcd_lvds){
#endif	    
		WRITE_MPEG_REG_BITS (HHI_VIID_CLK_DIV, 
					 8,      // select v2_clk_div1 
					 12, 4); // [23:20] encl_clk_sel 
	}
	else {
		WRITE_MPEG_REG_BITS (HHI_VID_CLK_DIV, 
					 0,      // select clk_div1 
					 20, 4); // [23:20] enct_clk_sel 
	}

    if(vclk_sel) {
      WRITE_MPEG_REG_BITS (HHI_VIID_CLK_CNTL, 
                   (1<<0),  // Enable cntl_div1_en
                   0, 1    // cntl_div1_en
                   );
      WRITE_MPEG_REG_BITS (HHI_VIID_CLK_CNTL, 1, 15, 1);  //soft reset
      WRITE_MPEG_REG_BITS (HHI_VIID_CLK_CNTL, 0, 15, 1);  //release soft reset
    }
    else {
      WRITE_MPEG_REG_BITS (HHI_VID_CLK_CNTL, 
                   (1<<0),  // Enable cntl_div1_en
                   0, 1    // cntl_div1_en
                   );
      WRITE_MPEG_REG_BITS (HHI_VID_CLK_CNTL, 1, 15, 1);  //soft reset
      WRITE_MPEG_REG_BITS (HHI_VID_CLK_CNTL, 0, 15, 1);  //release soft reset
    }
    
	  //printk("video pl1 clk = %d\n", clk_util_clk_msr(VID_PLL_CLK));
    //printk("video pll2 clk = %d\n", clk_util_clk_msr(VID2_PLL_CLK));
    //printk("cts_encl clk = %d\n", clk_util_clk_msr(CTS_ENCL_CLK));
}

static void set_lvds_pll(lcdConfig_t *pConf)
{
    //printk("setup lvds clk.\n");		
	    
    int pll_div_post;
    int phy_clk_div2;
	int FIFO_CLK_SEL;    
    unsigned long rd_data;
		
	unsigned pll_reg, div_reg, xd;
	int pll_sel, pll_div_sel, vclk_sel;	
	pll_reg = pConf->pll_ctrl;
	div_reg = pConf->div_ctrl | 0x3;	
	xd = pConf->clk_ctrl & 0xf;
	pll_sel = ((pConf->clk_ctrl) >>12) & 0x1;
	pll_div_sel = ((pConf->clk_ctrl) >>8) & 0x1;
	vclk_sel = ((pConf->clk_ctrl) >>4) & 0x1;
        
    pll_div_post = 7;

    phy_clk_div2 = 0;
	
	vclk_set_lcd_lvds(1, pll_sel, pll_div_sel, vclk_sel,  	//int lcd_lvds, int pll_sel, int pll_div_sel, int vclk_sel,
						pll_reg,   	//unsigned long pll_reg,
						(
                            div_reg | 		
                            (1 << 8) |                 // post_sel,  select divide by 3.5 (0 = div1, 1=divN,2 = div3.5)
                            ((pll_div_post-1) << 12) | // divn_tcnt
                            (phy_clk_div2 << 10)
						), 		//unsigned long vid_div_reg,
						xd 		//unsigned int xd
					 ); 

    WRITE_MPEG_REG( LVDS_PHY_CNTL0, 0xffff );

    //    lvds_gen_cntl       <= {10'h0,      // [15:4] unused
    //                            2'h1,       // [5:4] divide by 7 in the PHY
    //                            1'b0,       // [3] fifo_en
    //                            1'b0,       // [2] wr_bist_gate
    //                            2'b00};     // [1:0] fifo_wr mode
    FIFO_CLK_SEL = 1; // div7
    rd_data = READ_MPEG_REG(LVDS_GEN_CNTL);
    rd_data &= 0xffcf | (FIFO_CLK_SEL<< 4);
    WRITE_MPEG_REG( LVDS_GEN_CNTL, rd_data);   
    
    // Set Hard Reset lvds_phy_ser_top
    WRITE_MPEG_REG(LVDS_PHY_CLK_CNTL, READ_MPEG_REG(LVDS_PHY_CLK_CNTL) & 0x7fff);
    // Release Hard Reset lvds_phy_ser_top
    WRITE_MPEG_REG(LVDS_PHY_CLK_CNTL, READ_MPEG_REG(LVDS_PHY_CLK_CNTL) | 0x8000);    
}

static void venc_set_lvds(lcdConfig_t *pConf)
{
    //printk("setup lvds tvencoder.\n");
    
	WRITE_MPEG_REG(ENCL_VIDEO_EN,           0);
	//int havon_begin = 80;
#ifdef CONFIG_AM_TV_OUTPUT2
    if(vpp2_sel){
        WRITE_MPEG_REG_BITS (VPU_VIU_VENC_MUX_CTRL, 0, 2, 2); //viu2 select encl
    }
    else{
        WRITE_MPEG_REG_BITS (VPU_VIU_VENC_MUX_CTRL, 0, 0, 2); //viu1 select encl
    }
#else	
    WRITE_MPEG_REG(VPU_VIU_VENC_MUX_CTRL,
       (0<<0) |    // viu1 select encl
       (0<<2)      // viu2 select encl
       );
#endif
	//WRITE_MPEG_REG(	ENCL_VIDEO_MODE,		0);
 	//WRITE_MPEG_REG(	ENCL_VIDEO_MODE_ADV,	0x0418);
	
	WRITE_MPEG_REG(ENCL_VIDEO_MODE,         0x0040 | (1<<14)); // Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
	WRITE_MPEG_REG(ENCL_VIDEO_MODE_ADV,     0x0008); // Sampling rate: 1

	WRITE_MPEG_REG(	ENCL_VIDEO_MAX_PXCNT,	pConf->max_width - 1);
	WRITE_MPEG_REG(	ENCL_VIDEO_MAX_LNCNT,	pConf->max_height - 1);

	WRITE_MPEG_REG(	ENCL_VIDEO_HAVON_BEGIN,	pConf->video_on_pixel);
	WRITE_MPEG_REG(	ENCL_VIDEO_HAVON_END,	pConf->width - 1 + pConf->video_on_pixel);
	WRITE_MPEG_REG(	ENCL_VIDEO_VAVON_BLINE,	pConf->video_on_line);
	WRITE_MPEG_REG(	ENCL_VIDEO_VAVON_ELINE,	pConf->height + 1  + pConf->video_on_line);

	WRITE_MPEG_REG(	ENCL_VIDEO_HSO_BEGIN,	pConf->sth1_hs_addr);//10);
	WRITE_MPEG_REG(	ENCL_VIDEO_HSO_END,		pConf->sth1_he_addr);//20);
	WRITE_MPEG_REG(	ENCL_VIDEO_VSO_BEGIN,	pConf->stv1_hs_addr);//10);
	WRITE_MPEG_REG(	ENCL_VIDEO_VSO_END,		pConf->stv1_he_addr);//20);
	WRITE_MPEG_REG(	ENCL_VIDEO_VSO_BLINE,	pConf->stv1_vs_addr);//2);
	WRITE_MPEG_REG(	ENCL_VIDEO_VSO_ELINE,	pConf->stv1_ve_addr);//4);
		
	WRITE_MPEG_REG( ENCL_VIDEO_RGBIN_CTRL, 	0);
   
 	// bypass filter
 	WRITE_MPEG_REG(	ENCL_VIDEO_FILT_CTRL	,0x1000);
	
	// enable encl
    WRITE_MPEG_REG( ENCL_VIDEO_EN,           1);
}

static void set_lvds_control(lcdConfig_t *pConf)
{
    //printk("setup lvds control.\n");
	
	unsigned long data32;    

    data32 = (0x00 << LVDS_blank_data_r) |
             (0x00 << LVDS_blank_data_g) |
             (0x00 << LVDS_blank_data_b) ; 
    WRITE_MPEG_REG(LVDS_BLANK_DATA_HI,  (data32 >> 16));
    WRITE_MPEG_REG(LVDS_BLANK_DATA_LO, (data32 & 0xffff));
	
	WRITE_MPEG_REG( LVDS_PHY_CNTL0, 0xffff );
	WRITE_MPEG_REG( LVDS_PHY_CNTL1, 0xff00 );
	
	WRITE_MPEG_REG(ENCL_VIDEO_EN,           1);
	
    WRITE_MPEG_REG(LVDS_PACK_CNTL_ADDR, 
                   ( pConf->lvds_config->lvds_repack<<0 ) | // repack
                   ( 0<<2 ) | // odd_even
                   ( 0<<3 ) | // reserve
                   ( 0<<4 ) | // lsb first
                   ( pConf->lvds_config->pn_swap<<5 ) | // pn swap
                   ( 0<<6 ) | // dual port
                   ( 0<<7 ) | // use tcon control
                   ( pConf->lvds_config->bit_num<<8 ) | // 0:10bits, 1:8bits, 2:6bits, 3:4bits.
                   ( 0<<10 ) | //r_select  //0:R, 1:G, 2:B, 3:0
                   ( 1<<12 ) | //g_select  //0:R, 1:G, 2:B, 3:0
                   ( 2<<14 )); //b_select  //0:R, 1:G, 2:B, 3:0
				   
    WRITE_MPEG_REG( LVDS_GEN_CNTL, (READ_MPEG_REG(LVDS_GEN_CNTL) | (1 << 0)) );  //fifo enable  
    //WRITE_MPEG_REG( LVDS_GEN_CNTL, (READ_MPEG_REG(LVDS_GEN_CNTL) | (1 << 3))); // enable fifo
}

static void init_lvds_phy(lcdConfig_t *pConf)
{
    //printk("init lvds phy.\n");
	
	unsigned tmp_add_data;
    WRITE_MPEG_REG(LVDS_PHY_CNTL4, READ_MPEG_REG(LVDS_PHY_CNTL4)|0xfff);	
	
    //WRITE_MPEG_REG(LVDS_PHY_CNTL3, 0x36);  //0x3f  //0x36
    WRITE_MPEG_REG(LVDS_PHY_CNTL3, 0x16);  //0x3f  //0x36
    
    // tmp_add_data  = 0;
    // tmp_add_data |= 0xf<<0; //LVDS_PREM_CTL<3:0>=<1111>
    // tmp_add_data |= 0x3<<4; //LVDS_SWING_CTL<3:0>=<0011>
    // tmp_add_data |= 0x1<<8 ; //LVDS_VCM_CTL<2:0>=<001>
    // tmp_add_data |= 0xf<<11; //LVDS_REFCTL<4:0>=<01111>
    // WRITE_MPEG_REG(LVDS_PHY_CNTL5, tmp_add_data);
	
	tmp_add_data  = 0;
    tmp_add_data |= (pConf->lvds_config->lvds_phy_control->lvds_prem_ctl & 0xf) << 0; //LVDS_PREM_CTL<3:0>=<1111>
    tmp_add_data |= (pConf->lvds_config->lvds_phy_control->lvds_swing_ctl & 0xf) << 4; //LVDS_SWING_CTL<3:0>=<0011>    
    tmp_add_data |= (pConf->lvds_config->lvds_phy_control->lvds_vcm_ctl & 0x7) << 8; //LVDS_VCM_CTL<2:0>=<001>
    tmp_add_data |= (pConf->lvds_config->lvds_phy_control->lvds_ref_ctl & 0x1f) << 11; //LVDS_REFCTL<4:0>=<01111> 
    WRITE_MPEG_REG(LVDS_PHY_CNTL5, tmp_add_data);

    WRITE_MPEG_REG(LVDS_PHY_CNTL0,0xfff);	    
	WRITE_MPEG_REG(LVDS_PHY_CNTL1,0xffff);	

    WRITE_MPEG_REG(LVDS_PHY_CNTL6,0xcccc);
    WRITE_MPEG_REG(LVDS_PHY_CNTL7,0xcccc);
    WRITE_MPEG_REG(LVDS_PHY_CNTL8,0xcccc);

	WRITE_MPEG_REG(LVDS_PHY_CNTL4, READ_MPEG_REG(LVDS_PHY_CNTL4) & ~(0x7f<<0));  //disable LVDS phy port. wait for power on sequence.	
}

static inline void _init_tvenc(lcdConfig_t *pConf)
{
	set_lvds_pll(pConf);
	//vclk_set_lcd_lvds(1, 1, 1, 1, 0x1021e, 0x001e903, 1);  //0x0018803
	clk_util_lvds_set_clk_div(  1,          // unsigned long   divn_sel,        // select divide by 3.5 (0 = div1, 1=divN,2 = div3.5)
                                7,          // unsigned long   divn_tcnt,       // ignored
                                0 );
    venc_set_lvds(pConf);
	
	set_lvds_control(pConf);
	
	init_lvds_phy(pConf);
}

static inline void _enable_vsync_interrupt(void)
{
    if (READ_MPEG_REG(ENCL_VIDEO_EN) & 1) {
        WRITE_MPEG_REG(VENC_INTCTRL, 0x200);
#if 0
        while ((READ_MPEG_REG(VENC_INTFLAG) & 0x200) == 0) {
            u32 line1, line2;

            line1 = line2 = READ_MPEG_REG(VENC_ENCP_LINE);

            while (line1 >= line2) {
                line2 = line1;
                line1 = READ_MPEG_REG(VENC_ENCP_LINE);
            }

            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
            if (READ_MPEG_REG(VENC_INTFLAG) & 0x200) {
                break;
            }

            WRITE_MPEG_REG(ENCL_VIDEO_EN, 0);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);

            WRITE_MPEG_REG(ENCL_VIDEO_EN, 1);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
            READ_MPEG_REG(VENC_INTFLAG);
        }
#endif
    }
    else{
        WRITE_MPEG_REG(VENC_INTCTRL, 0x2);
    }
}
static void _enable_backlight(u32 brightness_level)
{
    pDev->conf.backlight_on?pDev->conf.backlight_on():0;
}
static void _disable_backlight(void)
{
    pDev->conf.backlight_off?pDev->conf.backlight_off():0;
}
static void _lcd_module_enable(void)
{
  //  printk("\n\n*********************\n");
	printk("LCD module enable.\n");
	//printk("\n*********************\n\n");
	
	BUG_ON(pDev==NULL);
    pDev->conf.power_on?pDev->conf.power_on():0;
    _init_tvenc(&pDev->conf);    		//ENCL
    _init_tcon_lvds(&pDev->conf);	//L_TCON
    _enable_vsync_interrupt();
}

static const vinfo_t *lcd_get_current_info(void)
{
	return &pDev->lcd_info;
}

static int lcd_set_current_vmode(vmode_t mode)
{
	if (mode != VMODE_LCD)
        return -EINVAL;
#ifdef CONFIG_AM_TV_OUTPUT2
    vpp2_sel = 0;
#endif          
    WRITE_MPEG_REG(VPP_POSTBLEND_H_SIZE, pDev->lcd_info.width);
    _lcd_module_enable();
    if (VMODE_INIT_NULL == pDev->lcd_info.mode)
        pDev->lcd_info.mode = VMODE_LCD;
    else
        _enable_backlight(BL_MAX_LEVEL);

	return 0;
	
}

#ifdef CONFIG_AM_TV_OUTPUT2
static int lcd_set_current_vmode2(vmode_t mode)
{
	if (mode != VMODE_LCD)
        return -EINVAL;
    vpp2_sel = 1;
    WRITE_MPEG_REG(VPP2_POSTBLEND_H_SIZE, pDev->lcd_info.width);
    _lcd_module_enable();
    if (VMODE_INIT_NULL == pDev->lcd_info.mode)
        pDev->lcd_info.mode = VMODE_LCD;
    else
        _enable_backlight(BL_MAX_LEVEL);
	return 0;
}
#endif

static vmode_t lcd_validate_vmode(char *mode)
{
	if ((strncmp(mode, PANEL_NAME, strlen(PANEL_NAME))) == 0)
		return VMODE_LCD;
	
	return VMODE_MAX;
}
static int lcd_vmode_is_supported(vmode_t mode)
{
	mode&=VMODE_MODE_BIT_MASK;
	if(mode == VMODE_LCD )
	return true;
	return false;
}
static int lcd_module_disable(vmode_t cur_vmod)
{
	BUG_ON(pDev==NULL);
    _disable_backlight();
	pDev->conf.power_off?pDev->conf.power_off():0;
	return 0;
}
#ifdef  CONFIG_PM
static int lcd_suspend(void)
{
	BUG_ON(pDev==NULL);
    printk("lcd_suspend \n");
    _disable_backlight();
	pDev->conf.power_off?pDev->conf.power_off():0;

	return 0;
}
static int lcd_resume(void)
{
	printk("lcd_resume\n");
	//_lcd_module_enable();
	pDev->conf.power_on?pDev->conf.power_on():0;
    _enable_backlight(BL_MAX_LEVEL);
	return 0;
}
#endif
static vout_server_t lcd_vout_server={
	.name = "lcd_vout_server",
	.op = {	
		.get_vinfo = lcd_get_current_info,
		.set_vmode = lcd_set_current_vmode,
		.validate_vmode = lcd_validate_vmode,
		.vmode_is_supported=lcd_vmode_is_supported,
		.disable=lcd_module_disable,
#ifdef  CONFIG_PM  
		.vout_suspend=lcd_suspend,
		.vout_resume=lcd_resume,
#endif
	},
};

#ifdef CONFIG_AM_TV_OUTPUT2
static vout_server_t lcd_vout2_server={
	.name = "lcd_vout2_server",
	.op = {	
		.get_vinfo = lcd_get_current_info,
		.set_vmode = lcd_set_current_vmode2,
		.validate_vmode = lcd_validate_vmode,
		.vmode_is_supported=lcd_vmode_is_supported,
		.disable=lcd_module_disable,
#ifdef  CONFIG_PM  
		.vout_suspend=lcd_suspend,
		.vout_resume=lcd_resume,
#endif
	},
};
#endif

static void _init_vout(tcon_dev_t *pDev)
{
	pDev->lcd_info.name = PANEL_NAME;
    pDev->lcd_info.mode = VMODE_INIT_NULL;
	pDev->lcd_info.width = pDev->conf.width;
	pDev->lcd_info.height = pDev->conf.height;
	pDev->lcd_info.field_height = pDev->conf.height;
	pDev->lcd_info.aspect_ratio_num = pDev->conf.screen_width;
	pDev->lcd_info.aspect_ratio_den = pDev->conf.screen_height;
	pDev->lcd_info.sync_duration_num = pDev->conf.sync_duration_num;
	pDev->lcd_info.sync_duration_den = pDev->conf.sync_duration_den;

	vout_register_server(&lcd_vout_server);
	
#ifdef CONFIG_AM_TV_OUTPUT2
   vout2_register_server(&lcd_vout2_server);
#endif   	
}

static void _tcon_init(lcdConfig_t *pConf)
{
	logo_object_t  *init_logo_obj=NULL;

	
	_init_vout(pDev);
	init_logo_obj = get_current_logo_obj();	
	if(NULL==init_logo_obj ||!init_logo_obj->para.loaded)
    	_lcd_module_enable();
}

static int tcon_probe(struct platform_device *pdev)
{
    struct resource *s;

    pDev = (tcon_dev_t *)kmalloc(sizeof(tcon_dev_t), GFP_KERNEL);
    if (!pDev) {
        printk("[tcon]: Not enough memory.\n");
        return -ENOMEM;
    }

    s = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!s) {
        printk("[tcon]: No tcon configuration data specified.\n");
        kfree(pDev);
        return -EINVAL;
    }

	pDev->conf = *(lcdConfig_t *)(s->start);

    _tcon_init(&pDev->conf);
    return 0;
}

static int tcon_remove(struct platform_device *pdev)
{
    kfree(pDev);
    
    return 0;
}

static struct platform_driver tcon_driver = {
    .probe      = tcon_probe,
    .remove     = tcon_remove,
    .driver     = {
        .name   = "tcon-dev",
    }
};

static int __init tcon_init(void)
{
    if (platform_driver_register(&tcon_driver)) {
        printk("failed to register tcon driver module\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit tcon_exit(void)
{
    platform_driver_unregister(&tcon_driver);
}

subsys_initcall(tcon_init);
module_exit(tcon_exit);

MODULE_DESCRIPTION("AMLOGIC TCON controller driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");

