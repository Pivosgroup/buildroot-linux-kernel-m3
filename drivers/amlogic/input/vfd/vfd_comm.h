
typedef int (*type_vfd_printk)(const char *fmt, ...);

int hardware_init(struct vfd_platform_data *pdev);
int get_vfd_key_value(void);
int set_vfd_led_value(char *display_code);