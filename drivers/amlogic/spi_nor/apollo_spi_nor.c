#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <linux/spi/spi.h>
#include <mach/spi_nor.h>
#include <mach/pinmux.h>

struct amlogic_spi_user_crtl {
	unsigned char	user_def_cmd;
	unsigned char	cmd_have_addr;
	unsigned char	cmd_have_dummy;
	unsigned char	cmd_have_data_in;
	unsigned char	cmd_have_data_out;
	unsigned char	cmd_dummy_num;
	unsigned char	user_def_cmd_value;
	unsigned		addr;
	int 		tx_data_len;
	int 		rx_data_len;
	unsigned char	*data_buf;
};

struct amlogic_spi {
	spinlock_t		lock;
	struct list_head	msg_queue;
	struct spi_master	*master;
	struct spi_device	spi_dev;

	unsigned char		*map_base;
	unsigned char		*map_end;
};

static void spi_hw_init(struct amlogic_spi	*amlogic_spi)
{
#if  defined(ONFIG_AMLOGIC_BOARD_APOLLO) || defined(CONFIG_AMLOGIC_BOARD_APOLLO_H)
	CLEAR_PERI_REG_MASK(PREG_PIN_MUX_REG4, ((1 << 1)|(1 << 2)|(1 << 4)|(1 << 5)|(1 << 6)));
	if (((READ_MPEG_REG(ASSIST_POR_CONFIG) & (1 << 1)) ? 1 : 0) && ((READ_MPEG_REG(ASSIST_POR_CONFIG) & (1 << 6)) ? 1 : 0)) {
		amlogic_spi->map_base = (unsigned char *)(0xC1800000);
		amlogic_spi->map_end = (unsigned char *)(0xC1FFFFFF);
	}
	else {
		amlogic_spi->map_base = (unsigned char *)(0x00000000);
		amlogic_spi->map_end = (unsigned char *)(0x00FFFFFF);		
	}
#endif
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	WRITE_PERI_REG(PREG_SPI_FLASH_CTRL, 0xea525);
	WRITE_PERI_REG(PREG_SPI_FLASH_CTRL1, 0xf0280100);
	SET_PERI_REG_MASK(SPI_FLASH_USER,(1<<2));
}

static void spi_hw_enable(void)
{
#ifdef CONFIG_ARCH_MESON3
  clear_mio_mux(2,7<<19);
  clear_mio_mux(5,(0xf<<6));
	set_mio_mux(5, 0xf);
#else
	clear_mio_mux(6,0x7fff);
	SET_PERI_REG_MASK(PERIPHS_PIN_MUX_1, ((1 << 23)|(1 <<25)|(1 << 27)|(1 << 29)));
#endif	
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
}

static void spi_hw_disable(void)
{
	CLEAR_PERI_REG_MASK(PERIPHS_PIN_MUX_1, ((1 << 23)|(1 <<25)|(1 << 27)|(1 << 29)));
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
}

static int spi_add_dev(struct amlogic_spi *amlogic_spi, struct spi_master	*master)
{
	amlogic_spi->spi_dev.master = master;
	device_initialize(&amlogic_spi->spi_dev.dev);
	amlogic_spi->spi_dev.dev.parent = &amlogic_spi->master->dev;
	amlogic_spi->spi_dev.dev.bus = &spi_bus_type;

	strcpy((char *)amlogic_spi->spi_dev.modalias, SPI_DEV_NAME);
	dev_set_name(&amlogic_spi->spi_dev.dev, "%s:%d", "apollospi", master->bus_num);
	return device_add(&amlogic_spi->spi_dev.dev);
}

static  void amlogic_spi_cleanup(struct spi_device	*spi)
{
	if (spi->modalias)
		kfree(spi->modalias);
}

static int spi_receive_cycle(struct amlogic_spi	*amlogic_spi, struct amlogic_spi_user_crtl *amlogic_user_spi)
{
	unsigned spi_cmd_reg = ((amlogic_user_spi->user_def_cmd<<SPI_FLASH_USR) |
							(amlogic_user_spi->cmd_have_addr<<SPI_FLASH_USR_ADDR) |
							(amlogic_user_spi->cmd_have_dummy<<SPI_FLASH_USR_DUMMY) |
							(amlogic_user_spi->cmd_have_data_in<<SPI_FLASH_USR_DIN) |
							(amlogic_user_spi->cmd_have_data_out<<SPI_FLASH_USR_DOUT) |
							(amlogic_user_spi->cmd_dummy_num<<SPI_FLASH_USR_DUMMY_BLEN) |
							(amlogic_user_spi->user_def_cmd_value<<SPI_FLASH_USR_CMD));
	unsigned read_addr = amlogic_user_spi->addr;
	int spi_rx_len = amlogic_user_spi->rx_data_len;
	unsigned *data_buf = (unsigned *)amlogic_user_spi->data_buf;
	int i, read_len;
	unsigned temp_buf[8], data_offset = 0;

	if (amlogic_user_spi->user_def_cmd == 0) {
		//don`t know why memcpy from the nor map failed 
		CLEAR_PERI_REG_MASK(PREG_SPI_FLASH_CTRL, SPI_ENABLE_AHB);
		/*if ((read_addr + spi_rx_len) <= (unsigned)amlogic_spi->map_end) {
			memcpy(amlogic_user_spi->data_buf, (amlogic_spi->map_base+read_addr), spi_rx_len);
			SET_PERI_REG_MASK(PREG_SPI_FLASH_CTRL, SPI_ENABLE_AHB);
			return 0;
		}*/

		while (spi_rx_len > 0 )
		{
			if(spi_rx_len >= 32)
				read_len = 32;
			else
				read_len = spi_rx_len;

			switch (amlogic_user_spi->user_def_cmd_value) {
				case OPCODE_RDID:
					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_RDID));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);
					break;

				case OPCODE_RDSR:
					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_RDSR));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);
					break;

				case OPCODE_NORM_READ:				
					WRITE_PERI_REG(PREG_SPI_FLASH_ADDR, ((read_addr & 0xffffff)|(read_len << SPI_FLASH_BYTES_LEN)));
					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_READ));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);
					break;

				default:
					break;
			}

			for (i=0; i<8; i++) {
				if (spi_rx_len <= 0)
					break;
				if (amlogic_user_spi->user_def_cmd_value == OPCODE_RDSR)
					*(temp_buf+i) = READ_PERI_REG(PREG_SPI_FLASH_STATUS);
				else
					*(temp_buf+i) = READ_PERI_REG(PREG_SPI_FLASH_C0+i);
				spi_rx_len -= 4;
			}

			memcpy((unsigned char *)data_buf+data_offset, (unsigned char *)temp_buf, read_len);
			data_offset += read_len;
			read_addr += read_len;
		}
		SET_PERI_REG_MASK(PREG_SPI_FLASH_CTRL, SPI_ENABLE_AHB);
	}
	else {
		CLEAR_PERI_REG_MASK(PREG_SPI_FLASH_CTRL, SPI_ENABLE_AHB);
		while (spi_rx_len > 0 )
		{
			if(spi_rx_len >= 32)
				read_len = 32;
			else
				read_len = spi_rx_len;

			WRITE_PERI_REG(PREG_SPI_FLASH_ADDR, (read_len << SPI_FLASH_BYTES_LEN));
			WRITE_PERI_REG(PREG_SPI_FLASH_CMD, spi_cmd_reg);
			udelay(1);

			for (i=0; i<8; i++) {
				if (spi_rx_len <= 0)
					break;
				*(temp_buf+i) = READ_PERI_REG(PREG_SPI_FLASH_C0+i);
				spi_rx_len -= 4;
			}
			memcpy((unsigned char *)data_buf+data_offset, (unsigned char *)temp_buf, read_len);
			data_offset += read_len;
		}
		SET_PERI_REG_MASK(PREG_SPI_FLASH_CTRL, SPI_ENABLE_AHB);
	}

	return 0;
}

static int spi_transmit_cycle(struct amlogic_spi *amlogic_spi, struct amlogic_spi_user_crtl *amlogic_user_spi)
{
	unsigned spi_cmd_reg = ((amlogic_user_spi->user_def_cmd<<SPI_FLASH_USR) |
							(amlogic_user_spi->cmd_have_addr<<SPI_FLASH_USR_ADDR) |
							(amlogic_user_spi->cmd_have_dummy<<SPI_FLASH_USR_DUMMY) |
							(amlogic_user_spi->cmd_have_data_in<<SPI_FLASH_USR_DIN) |
							(amlogic_user_spi->cmd_have_data_out<<SPI_FLASH_USR_DOUT) |
							(amlogic_user_spi->cmd_dummy_num<<SPI_FLASH_USR_DUMMY_BLEN) |
							(amlogic_user_spi->user_def_cmd_value<<SPI_FLASH_USR_CMD));
	unsigned write_addr = amlogic_user_spi->addr;
	int spi_tx_len = amlogic_user_spi->tx_data_len;
	unsigned *data_buf = (unsigned *)amlogic_user_spi->data_buf;
	int i, temp, write_len;
	unsigned temp_buf[8], data_offset = 0;

	CLEAR_PERI_REG_MASK(PREG_SPI_FLASH_CTRL, SPI_ENABLE_AHB);

	if (amlogic_user_spi->user_def_cmd == 0) {
		do {
			if (spi_tx_len > 32)
				write_len = 32;
			else
				write_len = spi_tx_len;

			memcpy((unsigned char *)temp_buf, (unsigned char *)data_buf+data_offset, write_len);
			for (i=0; i<write_len; i+=4) {
				if (spi_tx_len <= 0)
					break;
				if (amlogic_user_spi->user_def_cmd_value == OPCODE_WRSR)
					WRITE_PERI_REG(PREG_SPI_FLASH_STATUS, (*(temp_buf+i/4) & 0xff));
				else
					WRITE_PERI_REG((PREG_SPI_FLASH_C0+i/4), *(temp_buf+i/4));
			}

			switch (amlogic_user_spi->user_def_cmd_value) {
				case OPCODE_WREN:
					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_WREN));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);
					break;

				case OPCODE_WRSR:
					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_WREN));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);

					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_WRSR));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);
					break;

				case OPCODE_SE_4K:
					WRITE_PERI_REG(PREG_SPI_FLASH_ADDR, (write_addr & 0xffffff));
					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_WREN));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);

					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_SE));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);
					break;

				case OPCODE_BE:
					WRITE_PERI_REG(PREG_SPI_FLASH_ADDR, (write_addr & 0xffffff));
					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_WREN));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);

					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_BE));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);
					break;

				case OPCODE_PP:				
					WRITE_PERI_REG(PREG_SPI_FLASH_ADDR, ((write_addr & 0xffffff) | (write_len << SPI_FLASH_BYTES_LEN )));
					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_WREN));
					while(READ_PERI_REG(PREG_SPI_FLASH_CMD) != 0);

					WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_PP));
					while ( READ_PERI_REG(PREG_SPI_FLASH_CMD ) !=0 );
					break;

				default:
					break;
			}

			temp = 1;
			while ( (temp & 1) == 1 )
			{
				WRITE_PERI_REG(PREG_SPI_FLASH_CMD, (1<<SPI_FLASH_RDSR));
				while ( READ_PERI_REG(PREG_SPI_FLASH_CMD ) !=0 );

				temp = READ_PERI_REG(PREG_SPI_FLASH_STATUS);
			}
			write_addr += write_len;
			data_offset += write_len;
			spi_tx_len -= write_len;

		}while (spi_tx_len > 0 );
	}
	else {
		do {
			if(spi_tx_len >= 32)
				write_len = 32;
			else
				write_len = spi_tx_len;

			for (i=0; i<8; i++) {
				if (spi_tx_len <= 0)
					break;
				WRITE_PERI_REG((PREG_SPI_FLASH_C0+i), *data_buf++);
				spi_tx_len -= 4;
			}

			WRITE_PERI_REG(PREG_SPI_FLASH_ADDR, (write_len << SPI_FLASH_BYTES_LEN));
			WRITE_PERI_REG(PREG_SPI_FLASH_CMD, spi_cmd_reg);
			udelay(10);
		}while (spi_tx_len > 0 );
	}

	SET_PERI_REG_MASK(PREG_SPI_FLASH_CTRL, SPI_ENABLE_AHB);
	return 0;
}


static int amlogic_spi_transfer(struct spi_device *spi, struct spi_message *m)
{
	struct amlogic_spi	*amlogic_spi;
	struct amlogic_spi_user_crtl amlogic_hw_ctl;
	unsigned long		flags;
	struct spi_transfer	*t;
	unsigned command_index = 0;
	spin_lock_irqsave(&amlogic_spi->lock, flags);
	amlogic_spi = spi_master_get_devdata(spi->master);
	m->actual_length = 0;
	m->status = 0;
	memset(&amlogic_hw_ctl, 0x0, sizeof(struct amlogic_spi_user_crtl));

	/* reject invalid messages and transfers */
	if (list_empty(&m->transfers) || !m->complete)
		return -EINVAL;
	list_for_each_entry(t, &m->transfers, transfer_list) {
		unsigned char *tx_buf = (unsigned char *)t->tx_buf;
		unsigned char *rx_buf = (unsigned char *)t->rx_buf;
		unsigned	len = t->len;

		if (tx_buf != NULL) {
			if(command_index) {
				amlogic_hw_ctl.cmd_have_data_out = 1;
				amlogic_hw_ctl.tx_data_len = len;
				amlogic_hw_ctl.data_buf = tx_buf;
				m->actual_length += len;
			}
			else {
				amlogic_hw_ctl.user_def_cmd_value = tx_buf[0];
				//for distinguish user command or apollo spi hw already exist command
				if ((amlogic_hw_ctl.user_def_cmd_value == OPCODE_WREN) ||
					(amlogic_hw_ctl.user_def_cmd_value == OPCODE_RDSR) ||
					(amlogic_hw_ctl.user_def_cmd_value == OPCODE_NORM_READ) ||
					(amlogic_hw_ctl.user_def_cmd_value == OPCODE_PP) ||
					(amlogic_hw_ctl.user_def_cmd_value == OPCODE_SE_4K) ||
					(amlogic_hw_ctl.user_def_cmd_value == OPCODE_BE) ||
					(amlogic_hw_ctl.user_def_cmd_value == OPCODE_RDID) ||
					(amlogic_hw_ctl.user_def_cmd_value == OPCODE_WRSR) )
					amlogic_hw_ctl.user_def_cmd = 0;
				else
					amlogic_hw_ctl.user_def_cmd = 1;

				if ((len > 1) && (len < 4)) {
					amlogic_hw_ctl.tx_data_len = (len - 1);
					amlogic_hw_ctl.data_buf = &tx_buf[1];
				}
				else if (len == 4) {
					amlogic_hw_ctl.addr = (tx_buf[3] + (tx_buf[2]<<8) + (tx_buf[1]<<16))&0xffffff;
					amlogic_hw_ctl.cmd_have_addr = 1;
				}
				else if (len > 4) {
					amlogic_hw_ctl.cmd_have_dummy = 1;
					amlogic_hw_ctl.cmd_dummy_num = (len - 4);
				}
				command_index = 1;
				m->actual_length += len;
			}
		}
		if (rx_buf != NULL) {
			if(command_index) {
				amlogic_hw_ctl.cmd_have_data_in = 1;
				amlogic_hw_ctl.rx_data_len = len;
				amlogic_hw_ctl.data_buf = rx_buf;
				m->actual_length += len;
			}
		}
	}

	if ((amlogic_hw_ctl.rx_data_len > 0) && (amlogic_hw_ctl.tx_data_len > 0)) {
		printk("can`t do read and write simultaneous\n");
		BUG();
		spin_unlock_irqrestore(&amlogic_spi->lock, flags);
		return -1;
	}

	//spin_lock_irqsave(&amlogic_spi->lock, flags);
	spi_hw_enable();
	if (amlogic_hw_ctl.cmd_have_data_in)
		spi_receive_cycle(amlogic_spi, &amlogic_hw_ctl);
	else
		spi_transmit_cycle(amlogic_spi, &amlogic_hw_ctl);
	spi_hw_disable();
	spin_unlock_irqrestore(&amlogic_spi->lock, flags);

	return 0;
}

static int amlogic_spi_nor_probe(struct platform_device *pdev)
{
	struct spi_master	*master;
	struct amlogic_spi	*amlogic_spi;
	struct resource		*r;
	int			status = 0;
	unsigned		num_chipselect = 1;

	master = spi_alloc_master(&pdev->dev, sizeof *amlogic_spi);
	if (master == NULL) {
		dev_dbg(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}

	if (pdev->id != -1)
		master->bus_num = pdev->id;

	//master->setup = amlogic_spi_setup;
	master->transfer = amlogic_spi_transfer;
	master->cleanup = amlogic_spi_cleanup;
	master->num_chipselect = num_chipselect;

	dev_set_drvdata(&pdev->dev, master);

	amlogic_spi = spi_master_get_devdata(master);
	amlogic_spi->master = master;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		status = -ENODEV;
		goto err1;
	}

	//amlogic_spi->map_base = (unsigned char *)(r->start);
	//amlogic_spi->map_end = (unsigned char *)(r->end);
	amlogic_spi->spi_dev.dev.platform_data = pdev->dev.platform_data;

	spin_lock_init(&amlogic_spi->lock);
	INIT_LIST_HEAD(&amlogic_spi->msg_queue);

	status = spi_register_master(master);
	if (status < 0)
		goto err1;

	spi_hw_init(amlogic_spi);

	status = spi_add_dev(amlogic_spi, master);
	return status;

err1:
	spi_master_put(master);
	return status;
}

static int amlogic_spi_nor_remove(struct platform_device *pdev)
{
	struct spi_master	*master;
	struct amlogic_spi	*amlogic_spi;

	master = dev_get_drvdata(&pdev->dev);
	amlogic_spi = spi_master_get_devdata(master);

	spi_unregister_master(master);

	return 0;
}

static struct platform_driver amlogic_spi_nor_driver = { 
	.probe = amlogic_spi_nor_probe, 
	.remove = amlogic_spi_nor_remove, 
	.driver =
	    {
			.name = "AMLOGIC_SPI_NOR", 
			.owner = THIS_MODULE, 
		}, 
};

static int __init amlogic_spi_nor_init(void) 
{	
	return platform_driver_register(&amlogic_spi_nor_driver);
}

static void __exit amlogic_spi_nor_exit(void) 
{	
	platform_driver_unregister(&amlogic_spi_nor_driver);
} 

module_init(amlogic_spi_nor_init);
module_exit(amlogic_spi_nor_exit);

MODULE_DESCRIPTION("Amlogic Spi Nor Flash driver");
MODULE_LICENSE("GPL");
