
#include <linux/types.h>
#include <linux/platform_device.h>
#include <asm/arch/am_regs.h>
#ifdef AML_A1H 
	#include <asm/arch-apollo-h/pehelper.h>
	#include <asm/arch-apollo-h/peripheral_reg_apollo.h>
#else
	#include <asm/arch-apollo/pehelper.h>
	#include <asm/arch-apollo/peripheral_reg_apollo.h>
#endif
/*#define PERI_BASE_ADDR               0xC1200000
    
#define WRITE_PERI_REG(reg, val) *(volatile unsigned *)(PERI_BASE_ADDR + (reg)) = (val)
#define READ_PERI_REG(reg)  *(volatile unsigned *)(PERI_BASE_ADDR + (reg))
    
#define CLEAR_PERI_REG_MASK(reg, mask) WRITE_PERI_REG(reg, (READ_PERI_REG(reg)&(~mask)))
#define SET_PERI_REG_MASK(reg, mask)   WRITE_PERI_REG(reg, (READ_PERI_REG(reg)|(mask)))
    
#define PREG_I2C_SL_CONTROL           (0x180)
#define PREG_I2C_SL_SEND              (0x184)
#define PREG_I2C_SL_RECV              (0x188)
    
#define PREG_I2C_MS_CTRL              (0x1C0)
#define I2C_M_MANUAL_SDA_I        26
#define I2C_M_MANUAL_SCL_I        25
#define I2C_M_MANUAL_SDA_O        24
#define I2C_M_MANUAL_SCL_O        23
#define I2C_M_MANUAL_EN           22
#define I2C_M_DELAY_MSB           21
#define I2C_M_DELAY_LSB           12
#define I2C_M_DATA_CNT_MSB        11
#define I2C_M_DATA_CNT_LSB        8
#define I2C_M_CURR_TOKEN_MSB      7
#define I2C_M_CURR_TOKEN_LSB      4
#define I2C_M_ERROR               3
#define I2C_M_STATUS              2
#define I2C_M_ACK_IGNORE          1
#define I2C_M_START               0
#define  PREG_I2C_MS_SL_ADDR          (0x1C4)
#define  PREG_I2C_MS_TOKEN_LIST_REG0  (0x1C8)
#define  PREG_I2C_MS_TOKEN_LIST_REG1  (0x1CC)
#define  PREG_I2C_MS_TOKEN_WDATA_REG0 (0x1D0)
#define  PREG_I2C_MS_TOKEN_WDATA_REG1 (0x1D4)
#define  PREG_I2C_MS_TOKEN_RDATA_REG0 (0x1D8)
#define  PREG_I2C_MS_TOKEN_RDATA_REG1 (0x1DC)
#define  I2C_END                  0x0
#define  I2C_START                0x1
#define  I2C_SLAVE_ADDR_WRITE     0x2
#define  I2C_SLAVE_ADDR_READ      0x3
#define  I2C_DATA                 0x4
#define  I2C_DATA_LAST            0x5
#define  I2C_STOP                 0x6
    
#define MAX_READ_BYTE   0x08
#define MAX_WRITE_BYTE  0x08*/
    
struct apollo_i2c {
	//unsigned char       id_mask;
	//unsigned char       i2c_flag;    
	//unsigned            max_addr;
	unsigned delay;

	unsigned requency;

	struct i2c_adapter adap;

	wait_queue_head_t queue;

	spinlock_t lock;
};


