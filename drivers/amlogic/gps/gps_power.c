#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gps_power.h>
#include <linux/cdev.h>
#include <linux/fs.h>


#define GPS_POWER_MODULE_NAME   "gps_power"
#define GPS_POWER_DRIVER_NAME "gps_power"
#define GPS_POWER_DEVICE_NAME   "gps_power"
#define GPS_POWER_CLASS_NAME   "gps_power"

static dev_t gps_power_devno;
static struct cdev *gps_power_cdev = NULL;
static struct device *devp=NULL;

static int gps_power_probe(struct platform_device *pdev);
static int gps_power_remove(struct platform_device *pdev);
static ssize_t gps_power_ctrl(struct class *cla,struct class_attribute *attr,const char *buf,size_t count);
static int  gps_power_open(struct inode *inode,struct file *file);
static int  gps_power_release(struct inode *inode,struct file *file);
static int gps_power_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

static struct platform_driver gps_power_driver = {
	.probe = gps_power_probe,
	.remove = gps_power_remove,
	.driver = {
	.name = GPS_POWER_DRIVER_NAME,
	.owner = THIS_MODULE,
	},
};

static const struct file_operations gps_power_fops = {
	.open	= gps_power_open,
	.release	= gps_power_release,
	.ioctl = gps_power_ioctl,
};

static struct class_attribute gps_power_class_attrs[] = {
    __ATTR(gps_powerctrl,S_IWUGO,NULL,gps_power_ctrl),
    __ATTR_NULL
};

static struct class gps_power_class = {
    .name = GPS_POWER_CLASS_NAME,
    .class_attrs = gps_power_class_attrs,
    .owner = THIS_MODULE,
};

static int  gps_power_open(struct inode *inode,struct file *file)
{
	int ret = 0;
	struct cdev * cdevp = inode->i_cdev;
	file->private_data = cdevp;
	return ret;
}

static int  gps_power_release(struct inode *inode,struct file *file)
{
	int ret = 0;
	return ret;
}

static int gps_power_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct gps_power_platform_data *pdata = NULL;
	pdata = (struct gps_power_platform_data*)devp->platform_data;
	if(pdata == NULL){
		printk("%s platform data is required!\n",__FUNCTION__);
		return -1;
	}
	switch(cmd){
		case 0:
			if(pdata->power_off)
				pdata->power_off();
			break;
		case 1:
			if(pdata->power_on)
				pdata->power_on();
			break;
		default:
			break;
	}
	return 0;
}

static ssize_t gps_power_ctrl(struct class *cla,struct class_attribute *attr,const char *buf,size_t count)
{
	struct gps_power_platform_data *pdata = NULL;
	pdata = (struct gps_power_platform_data*)devp->platform_data;
	if(pdata == NULL){
		printk("%s platform data is required!\n",__FUNCTION__);
		return -1;
	}
	if(!strncasecmp(buf,"on",2)){
		if(pdata->power_on)
			pdata->power_on();
	}
	else if (!strncasecmp(buf,"off",3)){
		if(pdata->power_off)
			pdata->power_off();
	}
	else{
		printk( KERN_ERR"%s:%s error!Not support this parameter\n",GPS_POWER_MODULE_NAME,__FUNCTION__);
		return -EINVAL;
	}
	return count;
}

static int gps_power_probe(struct platform_device *pdev)
{
	int ret;
	struct gps_power_platform_data *pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "platform data is required!\n");
		ret = -EINVAL;
		goto out;
	}
	ret = alloc_chrdev_region(&gps_power_devno, 0, 1, GPS_POWER_DRIVER_NAME);
	if (ret < 0) {
		printk(KERN_ERR "%s:%s failed to allocate major number\n",GPS_POWER_MODULE_NAME,__FUNCTION__);
		ret = -ENODEV;
		goto out;
	}
	ret = class_register(&gps_power_class);
	if (ret < 0) {
		printk(KERN_ERR "%s:%s  failed to register class\n",GPS_POWER_MODULE_NAME,__FUNCTION__);
		goto error1;
	}
	gps_power_cdev = cdev_alloc();
	if(!gps_power_cdev){
		printk(KERN_ERR "%s:%s failed to allocate memory\n",GPS_POWER_MODULE_NAME,__FUNCTION__);
		goto error2;
	}
	cdev_init(gps_power_cdev,&gps_power_fops);
	gps_power_cdev->owner = THIS_MODULE;
	ret = cdev_add(gps_power_cdev,gps_power_devno,1);
	if(ret){
		printk(KERN_ERR "%s:%s failed to add device\n",GPS_POWER_MODULE_NAME,__FUNCTION__);
		goto error3;
	}
	devp = device_create(&gps_power_class,NULL,gps_power_devno,NULL,GPS_POWER_DEVICE_NAME);
	if(IS_ERR(devp)){
		printk(KERN_ERR "%s:%s failed to create device node\n",GPS_POWER_MODULE_NAME,__FUNCTION__);
		ret = PTR_ERR(devp);
		goto error3;
	}
	devp->platform_data = pdata;
	return 0;
error3:
	cdev_del(gps_power_cdev);
error2:
	class_unregister(&gps_power_class);
error1:
	unregister_chrdev_region(gps_power_devno,1);
out:
	return ret;
}

static int gps_power_remove(struct platform_device *pdev)
{
	unregister_chrdev_region(gps_power_devno,1);
	class_unregister(&gps_power_class);
	device_destroy(NULL, gps_power_devno);
	cdev_del(gps_power_cdev);
	return 0;
}

static int __init init_gps(void)
{
	int ret = -1;
	ret = platform_driver_register(&gps_power_driver);
	if (ret != 0) {
		printk(KERN_ERR "failed to register gps power module, error %d\n", ret);
		return -ENODEV;
	}
	return ret;
}

module_init(init_gps);

static void __exit unload_gps(void)
{
	platform_driver_unregister(&gps_power_driver);
}
module_exit(unload_gps);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("GPS power driver");


