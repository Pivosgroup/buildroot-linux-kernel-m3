#define MAX_BUFFER_SIZE		144
#define MAX_FINGER_NUMBER	3
#define MAX_PRESSURE		15
#define DEVICE_NAME			"IT7260"
#define DEVICE_VENDOR		0
#define DEVICE_PRODUCT		0
#define DEVICE_VERSION		0
#define IT7260_X_RESOLUTION	1024
#define IT7260_Y_RESOLUTION	600
#define SCREEN_X_RESOLUTION	1024
#define SCREEN_Y_RESOLUTION	600
#define VERSION_ABOVE_ANDROID_20
//If I2C interface on host platform can only transfer 8 bytes at a time,
//uncomment the following line.
//#define HAS_8_BYTES_LIMIT

//If the INT pin use open drain circuit,
//uncomment the following line.
//#define INT_PIN_OPEN_DRAIN

struct ioctl_cmd168 {
	unsigned short bufferIndex;
	unsigned short length;
	unsigned short buffer[MAX_BUFFER_SIZE];
};

#define IOC_MAGIC		'd'
#define IOCTL_SET 		_IOW(IOC_MAGIC, 1, struct ioctl_cmd168)
#define IOCTL_GET 		_IOR(IOC_MAGIC, 2, struct ioctl_cmd168)

struct IT7260_i2c_platform_data {
	int irq_gpio;
	int irq_flag;
	int xres;//resolution
	int yres;
	int swap_xy;
	int xpol;
	int ypol;
};