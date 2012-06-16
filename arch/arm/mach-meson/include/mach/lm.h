#ifndef __AMLOGIC_LM_H__
#define __AMLOGIC_LM_H__

/* usb setting header */


enum usb_port_type_e{
    USB_PORT_TYPE_HOST,
    USB_PORT_TYPE_SLAVE,
    USB_PORT_TYPE_OTG
};

enum usb_port_speed_e{
    USB_PORT_SPEED_DEFAULT = 0,
    USB_PORT_SPEED_HIGH,
    USB_PORT_SPEED_FULL
};

enum usb_dma_config_e{
    USB_DMA_DISABLE = 0,
    USB_DMA_BURST_DEFAULT,
    USB_DMA_BURST_SINGLE,
    USB_DMA_BURST_INCR,
    USB_DMA_BURST_INCR4,
    USB_DMA_BURST_INCR8,
    USB_DMA_BURST_INCR16
};
#define LM_DEVICE_TYPE_USB	0
#define LM_DEVICE_TYPE_SATA	1

struct lm_device {
	struct device		dev;
	struct resource		resource;
	unsigned int		irq;
	unsigned int		id;
	unsigned int		type;
	u64				dma_mask_room; // dma mask room for dev->dma_mask
    
	unsigned int		port_type;
	unsigned int		port_speed;
	unsigned int		port_config;
	unsigned int		dma_config;
	void (* set_vbus_power)(char is_power_on);
	void * pdata; 
};

struct lm_driver {
	struct device_driver	drv;
	int			(*probe)(struct lm_device *);
	void			(*remove)(struct lm_device *);
	int			(*suspend)(struct lm_device *, pm_message_t);
	int			(*resume)(struct lm_device *);
};

int lm_driver_register(struct lm_driver *drv);
void lm_driver_unregister(struct lm_driver *drv);

int lm_device_register(struct lm_device *dev);

#define lm_get_drvdata(lm)	dev_get_drvdata(&(lm)->dev)
#define lm_set_drvdata(lm,d)	dev_set_drvdata(&(lm)->dev, d)

#endif
