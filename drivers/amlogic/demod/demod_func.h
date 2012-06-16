#ifndef _DEMOD_FUNC_H
#define _DEMOD_FUNC_H

#include <mach/am_regs.h>

#ifndef  FALSE
#define  FALSE  0
#endif

#ifndef  TRUE
#define  TRUE   1
#endif

#define Wr(addr, data)   WRITE_CBUS_REG(addr, data) /**(volatile unsigned long *)(0xc1100000|(addr<<2))=data*/
#define Rd(addr)             READ_CBUS_REG(addr)            /**(volatile unsigned long *)(0xc1100000|(addr<<2))*/

#define DEMOD_BASE     APB_REG_ADDR(0x4000)            /*0xd0044000*/
#define DVBT_BASE  (DEMOD_BASE+0x400)
#define DVBC_BASE  (DEMOD_BASE+0x800)

#define OFDM_INT_STS (volatile unsigned long *)(DVBT_BASE+4*0x0d)
#define OFDM_INT_EN  (volatile unsigned long *)(DVBT_BASE+4*0x0e)

#define DEMOD_REG0  (volatile unsigned long *)(DEMOD_BASE+4*0x00)
#define DEMOD_REG1  (volatile unsigned long *)(DEMOD_BASE+4*0x01)
#define DEMOD_REG2  (volatile unsigned long *)(DEMOD_BASE+4*0x02)
                                            
#define P_HHI_DEMOD_PLL_CNTL    (volatile unsigned long *)CBUS_REG_ADDR(HHI_DEMOD_PLL_CNTL)       /*(volatile unsigned long *)0xc11041b4*/
#define P_HHI_DEMOD_PLL_CNTL2  (volatile unsigned long *)CBUS_REG_ADDR(HHI_DEMOD_PLL_CNTL2)     /*(volatile unsigned long *)0xc11041b8*/
#define P_HHI_DEMOD_PLL_CNTL3  (volatile unsigned long *)CBUS_REG_ADDR(HHI_DEMOD_PLL_CNTL3)     /*(volatile unsigned long *)0xc11041bc*/
#define P_HHI_DEMOD_CLK_CNTL    (volatile unsigned long *)CBUS_REG_ADDR(HHI_DEMOD_CLK_CNTL)     /*(volatile unsigned long *)0xc11041d0*/

#define P_PERIPHS_PIN_MUX_2       (volatile unsigned long *)CBUS_REG_ADDR(PERIPHS_PIN_MUX_2)         /*(volatile unsigned long *)0xc11080b8*/
#define P_PERIPHS_PIN_MUX_6       (volatile unsigned long *)CBUS_REG_ADDR(PERIPHS_PIN_MUX_6)         /*(volatile unsigned long *)0xc11080c8*/
 
// demod functions
void apb_write_reg(int mode, int reg, int val); 
u32  apb_read_reg (int mode, int reg);

void demod_set_reg(struct aml_demod_reg *demod_reg);
void demod_get_reg(struct aml_demod_reg *demod_reg);

void demod_msr_clk(int clk_mux);
void demod_calc_clk(struct aml_demod_sta *demod_sta);

int demod_set_sys(struct aml_demod_sta *demod_sta,
		  struct aml_demod_i2c *demod_i2c, 
		  struct aml_demod_sys *demod_sys);
int demod_get_sys(struct aml_demod_i2c *demod_i2c, 
		  struct aml_demod_sys *demod_sys);

int demod_turn_on(struct aml_demod_sta *demod_sta, 
		  struct aml_demod_sys *demod_sys);
int demod_turn_off(struct aml_demod_sta *demod_sta, 
		   struct aml_demod_sys *demod_sys);


int tuner_set_ch (struct aml_demod_sta *demod_sta,
		  struct aml_demod_i2c *demod_i2c);

int dvbc_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c, 
		struct aml_demod_dvbc *demod_dvbc);
int dvbc_status(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c, 
		struct aml_demod_sts *demod_sts);

int dvbt_set_ch(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c, 
		struct aml_demod_dvbt *demod_dvbt);
int dvbt_status(struct aml_demod_sta *demod_sta,
		struct aml_demod_i2c *demod_i2c, 
		struct aml_demod_sts *demod_sts);

void dvbc_get_test_out(u8 sel, u32 len, u32 *buf);
void dvbt_get_test_out(u8 sel, u32 len, u32 *buf);

void dvbc_enable_irq (int dvbc_irq);
void dvbc_disable_irq(int dvbc_irq);
void dvbc_isr(struct aml_demod_sta *demod_sta);
int dvbc_isr_islock(void);

void dvbt_enable_irq (int dvbt_irq);
void dvbt_disable_irq(int dvbt_irq);
void dvbt_isr(struct aml_demod_sta *demod_sta);
int dvbt_isr_islock(void);

int init_tuner_fj2207(struct aml_demod_sta *demod_sta, 
		      struct aml_demod_i2c *adap);
int set_tuner_fj2207(struct aml_demod_sta *demod_sta, 
		     struct aml_demod_i2c *adap);

int get_fj2207_ch_power(void);
int tuner_get_ch_power(struct aml_demod_i2c *adap);

// i2c functions
//int aml_i2c_sw_test_bus(struct aml_demod_i2c *adap, char *name);
int am_demod_i2c_xfer(struct aml_demod_i2c *adap, struct i2c_msg *msgs, int num);

struct dvb_tuner_info * tuner_get_info( int type, int mode);

#endif

