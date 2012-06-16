
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
    
#include <asm/io.h>
#include <linux/fsl_devices.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include "i2c-apollo.h"
    
#include "asm/bsp.h"
    
#define MAX_READ_BYTE   0x08
#define MAX_WRITE_BYTE  0x08
//static struct platform_driver apollo_i2c_driver;
struct apollo_i2c apollo_i2c_para = { 
	// .id_mask = 0xff,
	.delay = 300, 
	.requency = 400, 
};

static void apollo_i2c_delay(unsigned int delay_count) 
{
	//udelay(delay_count);	
	//if (delay_count)	
		//while (delay_count--) ;
	msleep((delay_count >> 10) + 1);
}

//#define I2C_dbg()     
#ifdef DEBUG
	#define I2C_DBG(fmt,args... ) printk(fmt,##args) 
#else
	#define I2C_DBG(fmt,args...)	
#endif

#define I2C_INFO(fmt,args... ) printk(fmt,##args) 

static void apollo_i2c_set_clk(void) 
{	
	int i2c_clock_set;
	int host_clock = 168;

	host_clock = get_mpeg_clk() / 100000;
	printk("apollo_i2c_set_clk clk %d\n", host_clock);

	SET_PERIPHS_REG_BITS(PREG_PIN_MUX_REG0, ((1<<20)|(1<<24)));

	i2c_clock_set = (25 * host_clock) / apollo_i2c_para.requency;
	WRITE_PERI_REG(PREG_I2C_MS_CTRL, (((READ_PERI_REG(PREG_I2C_MS_CTRL)) & 0xffc00ffd) | (i2c_clock_set << I2C_M_DELAY_LSB)));	//set the i2c clock frequency and enable ack detecting 
	//WRITE_PERI_REG(PREG_I2C_MS_CTRL, (((READ_PERI_REG(PREG_I2C_MS_CTRL)) |0x2) | (i2c_clock_set << I2C_M_DELAY_LSB)));
} 

static int apollo_i2c_detect_ack(void)	//This read only bit is set if the I2C device generates a NACK during writing
{
	return (((READ_PERI_REG(PREG_I2C_MS_CTRL)) & 0x8) >> 3);
}

static void apollo_i2c_stop(void) 
{
	int count_ = 1000;

	WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, ((I2C_END << 28) | 
						       (I2C_END << 24) | 
						       (I2C_END << 20) | 
						       (I2C_END << 16) | 
						       (I2C_END << 12) | 
						       (I2C_END << 8) | 
						       (I2C_END << 4) | 
						       (I2C_STOP << 0)));
	
	// Toggle start low then high 
	WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) & 0xfffffffe));
	WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) | 0x1));
	apollo_i2c_delay(apollo_i2c_para.delay << 4);
	
	while (((READ_PERI_REG(PREG_I2C_MS_CTRL)) & (1 << I2C_M_STATUS)) && (count_--)) ;	// Wait for the transfer to complete 
	CLEAR_PERI_REG_MASK(PREG_PIN_MUX_REG0, ((1<<20)|(1<<24)));   
}


int apollo_i2c_read_1(unsigned char *rw_buff, unsigned int rw_bytes, unsigned int slave_addr) 
{
	int tmp_count1, tmp_count2, i, count1, temp_data0 = 0, temp_data1 = 0;

	WRITE_PERI_REG(PREG_I2C_MS_SL_ADDR, slave_addr << 1);	
	WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, 0);	
	WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG1, 0);
	
	WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, ((I2C_END << 28) | 
								(I2C_END << 24) | 
								(I2C_END << 20) |	// don't stop the bus, leave it in a read state
								(I2C_END << 16) | 
								(I2C_END << 12) |	// send Start Repeat for read
								(I2C_DATA << 8) | 
								(I2C_SLAVE_ADDR_WRITE <<4) | 
								(I2C_START <<0)));
	

	WRITE_PERI_REG(PREG_I2C_MS_TOKEN_WDATA_REG0, rw_buff[0]);	//send register address                                         
	WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) & 0xfffffffe));
	WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) | 0x1));
	
	apollo_i2c_delay(apollo_i2c_para.delay << 4);	/*there is enough delay in order to make apolloware I2C to work normal */
	
	count1 = 1000;
	while (((READ_PERI_REG(PREG_I2C_MS_CTRL)) & (1 << I2C_M_STATUS)) && (count1--)) ;	// Wait for the transfer to complete  
	
	if (apollo_i2c_detect_ack())	//error 
	{	
		apollo_i2c_stop();
		return -1;
	}
	
	I2C_DBG("in apollo_i2c_read_1: slave_addr and register address send ok!\n");

	WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, ((I2C_END << 28) | 
							(I2C_END << 24) | 
							(I2C_END << 20) | 
							(I2C_END << 16) | 
							(I2C_END << 12) | 
							(I2C_END << 8) | 
							(I2C_SLAVE_ADDR_READ << 4) |	// send Start Repeat for read
							(I2C_START << 0)));
	
	// Toggle start "low then high"
	WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) & 0xfffffffe));
	WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) | 0x1));
	
	apollo_i2c_delay(apollo_i2c_para.delay << 4);	//there is enough delay in order to make apolloware I2C to work normal
	count1 = 1000;

	while (((READ_PERI_REG(PREG_I2C_MS_CTRL)) & (1 << I2C_M_STATUS)) && (count1--)) ;	// Wait for the transfer to complete  
	
	if (apollo_i2c_detect_ack())	//error 
	{	
		apollo_i2c_stop();
		return -1;
	}
	I2C_DBG("in apollo_i2c_read_1: slave_addr send ok again!\n");
	
	while (rw_bytes)	
	{	
		I2C_DBG("in apollo_i2c_read_1: start reading now---\n");
		
		// -------------------------------------------------------------------
		// Read data in blocks of 8
		// -------------------------------------------------------------------
		if (rw_bytes > MAX_READ_BYTE)	
			tmp_count1 = MAX_READ_BYTE;
		else	
			tmp_count1 = rw_bytes;
		// Pre-Fill the token lists with TOKEN_END
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, 0);
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG1, 0);
		
		// Fill Data and Token Lists
		for (i = 0; i < tmp_count1; i++)		
		{	
			if (rw_bytes == 1)
				WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0,((READ_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0))& (~(0xf << (i * 4)))) |(I2C_DATA_LAST << (i * 4)));
			else
				WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0,((READ_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0))& (~(0xf << (i * 4)))) |(I2C_DATA << (i * 4)));
			rw_bytes--;
			
		}
		// Toggle start "low then high"
		WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) &0xfffffffe));
		WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) | 0x1));
		
		apollo_i2c_delay(apollo_i2c_para.delay * 25 * tmp_count1);
		count1 = 10000;

		while (((READ_PERI_REG(PREG_I2C_MS_CTRL)) & (1 << I2C_M_STATUS)) && (count1--)) ;	// Wait for the transfer to complete  
		// Read the bytes just read
		tmp_count2 = ((READ_PERI_REG(PREG_I2C_MS_CTRL)) >> 8) & 0xf;
		if (tmp_count1 == tmp_count2)	//read data success
		{	
			apollo_i2c_delay(apollo_i2c_para.delay << 5);	/*there is enough delay in order to read data correctly, about 100us */
			temp_data0 = READ_PERI_REG(PREG_I2C_MS_TOKEN_RDATA_REG0);
			if (tmp_count2 >= MAX_READ_BYTE / 2)
				temp_data1 = READ_PERI_REG(PREG_I2C_MS_TOKEN_RDATA_REG1);
			
			for (i = 0; i < tmp_count2; i++)	
			{	
				if (i >= MAX_READ_BYTE / 2)	
				{	
					(*rw_buff++) = ((temp_data1 >>((i -MAX_READ_BYTE / 2) *8)) & 0xff);	
				}
				else	
				{	
					(*rw_buff++) = ((temp_data0 >> (i * 8)) & 0xff);
					    //printk("%s\n",rw_buff);
				}
			}
		}
		else	
 		{	
			apollo_i2c_stop();
			return -1;
		}
	}
	apollo_i2c_stop();
	I2C_DBG("reading %d bytes\n", tmp_count2);

	return tmp_count2;
}

int apollo_i2c_write_1(unsigned char *rw_buff, unsigned int rw_bytes, unsigned int slave_addr) 
{
	int count1 = 0;
	I2C_DBG("in apollo_i2c_write_0 .........\n");

	//AVMutexPend(hwi2c_mutex, 0, &err); 
	if (rw_bytes == 0)	//when address prob
	{
		WRITE_PERI_REG(PREG_I2C_MS_SL_ADDR, slave_addr << 1);	// Set the I2C Address,D0=0:W  
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, 0);
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG1, 0);

		//send the device id(write)
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, ((I2C_END << 28) | 
							(I2C_END << 24) |	// don't stop the bus, leave it in a read state
							(I2C_END << 20) | 
							(I2C_END << 16) |	// send Start Repeat for read
							(I2C_END << 12) | 
							(I2C_END << 8) |
							(I2C_SLAVE_ADDR_WRITE << 4) | 
							(I2C_START << 0)));

		// Toggle start "low then high"     
		WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) &0xfffffffe));
		WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) | 0x1));

		apollo_i2c_delay(apollo_i2c_para.delay << 4);	/*there is enough delay in order to make apolloware I2C to work normal */
		count1 = 1000;

		while (((READ_PERI_REG(PREG_I2C_MS_CTRL)) & (1 << I2C_M_STATUS)) && (count1--)) ;	// Wait for the transfer to complete  
		if (apollo_i2c_detect_ack())	//error 
		{
			apollo_i2c_stop();
			return -1;
		}	
		I2C_DBG("send the device id(write):%d OK!\n", slave_addr << 1);
	}
	else	
	{	
		WRITE_PERI_REG(PREG_I2C_MS_SL_ADDR, slave_addr << 1);	// Set the I2C Address,D0=0:W  
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, 0);
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG1, 0);

		//send the device id(write)
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, ((I2C_END << 28) | 
							(I2C_END << 24) |	// don't stop the bus, leave it in a read state
							(I2C_END << 20) | 
							(I2C_END << 16) |	// send Start Repeat for read
							(I2C_END << 12) | 
							(I2C_DATA << 8) |
							(I2C_SLAVE_ADDR_WRITE << 4) |
							(I2C_START << 0)));
		// Toggle start "low then high" 
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_WDATA_REG0, rw_buff[0]);
		WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) & 0xfffffffe));
		WRITE_PERI_REG(PREG_I2C_MS_CTRL,((READ_PERI_REG(PREG_I2C_MS_CTRL)) | 0x1));

		apollo_i2c_delay(apollo_i2c_para.delay << 4);	/*there is enough delay in order to make apolloware I2C to work normal */	
		count1 = 1000;

		while (((READ_PERI_REG(PREG_I2C_MS_CTRL)) & (1 << I2C_M_STATUS)) && (count1--)) ;	// Wait for the transfer to complete  
		if (apollo_i2c_detect_ack())	//error 
		{	
			apollo_i2c_stop();	
			return -1;
		}
		I2C_DBG("send the device id(write):%d OK!\n", slave_addr << 1);
		--rw_bytes;
		++rw_buff;
	}

	while (rw_bytes)	
	{	
		I2C_DBG("writing data now.....\n");
		// -------------------------------------------------------------------
		// write data to  I2C_TOKEN_WDATA_REG0 , I2C_TOKEN_WDATA_REG1 
		// ------------------------------------------------------------------- 
		// Pre-Fill the token lists with TOKEN_END
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, 0);
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_WDATA_REG0, 0);

		// Fill Data and Token Lists
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_LIST_REG0, (I2C_DATA));
		WRITE_PERI_REG(PREG_I2C_MS_TOKEN_WDATA_REG0, (*rw_buff));

		rw_buff++;
		rw_bytes--;

		// Toggle start "low then high"
		WRITE_PERI_REG(PREG_I2C_MS_CTRL, ((READ_PERI_REG(PREG_I2C_MS_CTRL)) & 0xfffffffe));
		WRITE_PERI_REG(PREG_I2C_MS_CTRL, ((READ_PERI_REG(PREG_I2C_MS_CTRL)) | 0x1));

		apollo_i2c_delay(apollo_i2c_para.delay << 4);
		count1 = 10000;

		while (((READ_PERI_REG(PREG_I2C_MS_CTRL)) & (1 << I2C_M_STATUS)) && (count1--)) ;	// Wait for the transfer to complete  
		if (apollo_i2c_detect_ack())	//error 
		{	
			apollo_i2c_stop();	
			return -1;
		}
		if (rw_bytes <= 0)
			rw_bytes = 0;
		I2C_DBG("writing data finished.....\n");	
	}

	// -------------------------------------------------------------------
	// Send STOP
	// -------------------------------------------------------------------
	apollo_i2c_stop();
	return rw_bytes;
}



static u32 apollo_i2c_func(struct i2c_adapter *adap) 
{	
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static int apollo_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num) 
{
	struct i2c_msg *pmsg;
	//struct apollo_i2c *i2c = i2c_get_adapdata(adap);
	int ret = 0, i;
	SET_PERIPHS_REG_BITS(PREG_PIN_MUX_REG0, ((1<<20)|(1<<24)));

	for (i = 0; i < num; i++)
	{
		pmsg = &msgs[i];

		 //printk("in apollo_i2c_xfer now ......\n");
		pr_debug("Doing %s %d bytes to 0x%02x - %d of %d messages\n", pmsg->flags & I2C_M_RD ? "read" : "write", pmsg->len,pmsg->addr, i + 1, num);

		if (pmsg->flags & I2C_M_RD)	
			ret = apollo_i2c_read_1(pmsg->buf, pmsg->len, pmsg->addr);
		else
			ret = apollo_i2c_write_1(pmsg->buf, pmsg->len, pmsg->addr);
	}

	return (ret < 0) ? ret : num;
}

static const struct i2c_algorithm apollo_i2c_algorithm = { 
	.master_xfer = apollo_i2c_xfer, 
	.functionality = apollo_i2c_func, 
};


static struct i2c_adapter apollo_i2c_adapter = { 
	.owner = THIS_MODULE, 
	.name = "APOLLO_i2c_adapter", 
	.class = I2C_CLASS_HWMON, 
	.algo = &apollo_i2c_algorithm, 
};

static int apollo_i2c_probe(struct platform_device *pdev) 
{
	int ret;
	apollo_i2c_set_clk();

	ret = i2c_add_adapter(&apollo_i2c_adapter);
	if (ret < 0) 
	{
		dev_err(&pdev->dev, "Adapter %s registration failed\n",	 
		apollo_i2c_adapter.name);
	}

	dev_info(&pdev->dev, "apollo i2c bus driver.\n");
	return 0;
}

static int apollo_i2c_remove(struct platform_device *pdev) 
{
	i2c_del_adapter(&apollo_i2c_adapter);
	return 0;
}

static struct platform_driver apollo_i2c_driver = { 
	.probe = apollo_i2c_probe, 
	.remove = apollo_i2c_remove, 
	.driver = {
			.name = "apollo-i2c",						
			.owner = THIS_MODULE,						
	}, 
};

static int __init apollo_i2c_init(void) 
{
#if 0///this platform_device_register move to bsp.c becase we only need to init once... 
	//some resource can move to it;
	//apollo_i2c_set_clk();
	int ret;

	ret = platform_device_register(&apollo_device_i2c);
	if (ret)
	{
		printk(KERN_ERR"apollo failed to add board device %s (%d) @%p\n", apollo_device_i2c.name, ret, &apollo_device_i2c);
	}
#endif
	return platform_driver_register(&apollo_i2c_driver);
}

static void __exit apollo_i2c_exit(void) 
{
	platform_driver_unregister(&apollo_i2c_driver);
	//i2c_del_adapter(&apollo_i2c_adapter);
} 

module_init(apollo_i2c_init);

module_exit(apollo_i2c_exit);

MODULE_AUTHOR("AMLOGIC");

MODULE_DESCRIPTION("I2C driver for apollo");

MODULE_LICENSE("GPL");

