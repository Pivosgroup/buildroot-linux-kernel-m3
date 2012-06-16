#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>

#include <linux/amlog.h>
MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0, LOG_DEFAULT_LEVEL_DESC, LOG_DEFAULT_MASK_DESC);

#define MAX_SUBTITLE_PACKET 10

typedef struct {
    int subtitle_size;
    int subtitle_pts;
    char * data;
} subtitle_data_t;
static subtitle_data_t subtitle_data[MAX_SUBTITLE_PACKET];
static int subtitle_enable = 1;
static int subtitle_total = 0;
static int subtitle_width = 0;
static int subtitle_height = 0;
static int subtitle_type = -1;
static int subtitle_current = 0; // no subtitle
//static int subtitle_size = 0;
//static int subtitle_read_pos = 0;
static int subtitle_write_pos = 0;
static int subtitle_start_pts = 0;
static int subtitle_fps = 0;
static int subtitle_subtype = 0;
//static int *subltitle_address[MAX_SUBTITLE_PACKET];

// total
// curr
// bimap
// text
// type
// info
// pts
// duration
// color pallete
// width/height

static ssize_t show_curr(struct class *class,
                         struct class_attribute *attr,
                         char *buf)
{
    return sprintf(buf, "%d: current\n", subtitle_current);
}

static ssize_t store_curr(struct class *class,
                          struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
    unsigned curr;
    ssize_t r;

    r = sscanf(buf, "%d", &curr);
    //if ((r != 1))
    //return -EINVAL;

    subtitle_current = curr;

    return size;
}


static ssize_t show_type(struct class *class,
                         struct class_attribute *attr,
                         char *buf)
{
    return sprintf(buf, "%d: type\n", subtitle_type);
}

static ssize_t store_type(struct class *class,
                          struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
    unsigned type;
    ssize_t r;

    r = sscanf(buf, "%d", &type);
    //if ((r != 1))
    //  return -EINVAL;

    subtitle_type = type;

    return size;
}

static ssize_t show_width(struct class *class,
                          struct class_attribute *attr,
                          char *buf)
{
    return sprintf(buf, "%d: width\n", subtitle_width);
}

static ssize_t store_width(struct class *class,
                           struct class_attribute *attr,
                           const char *buf,
                           size_t size)
{
    unsigned width;
    ssize_t r;

    r = sscanf(buf, "%d", &width);
    //if ((r != 1))
    //return -EINVAL;

    subtitle_width = width;

    return size;
}

static ssize_t show_height(struct class *class,
                           struct class_attribute *attr,
                           char *buf)
{
    return sprintf(buf, "%d: height\n", subtitle_height);
}

static ssize_t store_height(struct class *class,
                            struct class_attribute *attr,
                            const char *buf,
                            size_t size)
{
    unsigned height;
    ssize_t r;

    r = sscanf(buf, "%d", &height);
    //if ((r != 1))
    //return -EINVAL;

    subtitle_height = height;

    return size;
}

static ssize_t show_total(struct class *class,
                          struct class_attribute *attr,
                          char *buf)
{
    return sprintf(buf, "%d: num\n", subtitle_total);
}

static ssize_t store_total(struct class *class,
                           struct class_attribute *attr,
                           const char *buf,
                           size_t size)
{
    unsigned total;
    ssize_t r;

    r = sscanf(buf, "%d", &total);
    if ((r <= 0)) {
        return -EINVAL;
    }
    printk("subtitle num is %d\n", total);
    subtitle_total = total;

    return size;
}

static ssize_t show_enable(struct class *class,
                           struct class_attribute *attr,
                           char *buf)
{
    if (subtitle_enable) {
        return sprintf(buf, "1: enabled\n");
    }

    return sprintf(buf, "0: disabled\n");
}

static ssize_t store_enable(struct class *class,
                            struct class_attribute *attr,
                            const char *buf,
                            size_t size)
{
    unsigned mode;
    ssize_t r;

    r = sscanf(buf, "%d", &mode);
    if ((r != 1)) {
        return -EINVAL;
    }
    printk("subtitle enable is %d\n", mode);
    subtitle_enable = mode ? 1 : 0;

    return size;
}

static ssize_t show_size(struct class *class,
                         struct class_attribute *attr,
                         char *buf)
{
    if (subtitle_enable) {
        return sprintf(buf, "1: size\n");
    }

    return sprintf(buf, "0: size\n");
}

static ssize_t store_size(struct class *class,
                          struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
    unsigned ssize;
    ssize_t r;

    r = sscanf(buf, "%d", &ssize);
    if ((r <= 0)) {
        return -EINVAL;
    }
    printk("subtitle size is %d\n", ssize);
    subtitle_data[subtitle_write_pos].subtitle_size = ssize;

    return size;
}

static ssize_t show_startpts(struct class *class,
                             struct class_attribute *attr,
                             char *buf)
{
    return sprintf(buf, "%d: pts\n", subtitle_start_pts);
}

static ssize_t store_startpts(struct class *class,
                              struct class_attribute *attr,
                              const char *buf,
                              size_t size)
{
    unsigned spts;
    ssize_t r;

    r = sscanf(buf, "%d", &spts);
    if ((r <= 0)) {
        return -EINVAL;
    }
    printk("subtitle start pts is %x\n", spts);
    subtitle_start_pts = spts;

    return size;
}

static ssize_t show_data(struct class *class,
                         struct class_attribute *attr,
                         char *buf)
{
    if (subtitle_data[subtitle_write_pos].data) {
        return sprintf(buf, "%d\n", (int)(subtitle_data[subtitle_write_pos].data));
    }

    return sprintf(buf, "0: disabled\n");
}

static ssize_t store_data(struct class *class,
                          struct class_attribute *attr,
                          const char *buf,
                          size_t size)
{
    unsigned address;
    ssize_t r;

    r = sscanf(buf, "%d", &address);
    if ((r == 0)) {
        return -EINVAL;
    }
    if (subtitle_data[subtitle_write_pos].subtitle_size > 0) {
        subtitle_data[subtitle_write_pos].data = vmalloc((subtitle_data[subtitle_write_pos].subtitle_size));
        if (subtitle_data[subtitle_write_pos].data)
            memcpy(subtitle_data[subtitle_write_pos].data, (char *)address,
                   subtitle_data[subtitle_write_pos].subtitle_size);
    }
    printk("subtitle data address is %x", (unsigned int)(subtitle_data[subtitle_write_pos].data));
    subtitle_write_pos++;
    if (subtitle_write_pos >= MAX_SUBTITLE_PACKET) {
        subtitle_write_pos = 0;
    }
    return 1;
}

static ssize_t show_fps(struct class *class,
                        struct class_attribute *attr,
                        char *buf)
{
    return sprintf(buf, "%d: fps\n", subtitle_fps);
}

static ssize_t store_fps(struct class *class,
                         struct class_attribute *attr,
                         const char *buf,
                         size_t size)
{
    unsigned ssize;
    ssize_t r;

    r = sscanf(buf, "%d", &ssize);
    if ((r <= 0)) {
        return -EINVAL;
    }
    printk("subtitle fps is %d\n", ssize);
    subtitle_fps = ssize;

    return size;
}

static ssize_t show_subtype(struct class *class,
                            struct class_attribute *attr,
                            char *buf)
{
    return sprintf(buf, "%d: subtype\n", subtitle_subtype);
}

static ssize_t store_subtype(struct class *class,
                             struct class_attribute *attr,
                             const char *buf,
                             size_t size)
{
    unsigned ssize;
    ssize_t r;

    r = sscanf(buf, "%d", &ssize);
    if ((r <= 0)) {
        return -EINVAL;
    }
    printk("subtitle subtype is %d\n", ssize);
    subtitle_subtype = ssize;

    return size;
}

static struct class_attribute subtitle_class_attrs[] = {
    __ATTR(enable,     S_IRUGO | S_IWUSR, show_enable,  store_enable),
    __ATTR(total,     S_IRUGO | S_IWUSR, show_total,  store_total),
    __ATTR(width,     S_IRUGO | S_IWUSR, show_width,  store_width),
    __ATTR(height,     S_IRUGO | S_IWUSR, show_height,  store_height),
    __ATTR(type,     S_IRUGO | S_IWUSR, show_type,  store_type),
    __ATTR(curr,     S_IRUGO | S_IWUSR, show_curr,  store_curr),
    __ATTR(size,     S_IRUGO | S_IWUSR, show_size,  store_size),
    __ATTR(data,     S_IRUGO | S_IWUSR, show_data,  store_data),
    __ATTR(startpts,     S_IRUGO | S_IWUSR, show_startpts,  store_startpts),
    __ATTR(fps,     S_IRUGO | S_IWUSR, show_fps,  store_fps),
    __ATTR(subtype,     S_IRUGO | S_IWUSR, show_subtype,  store_subtype),
    __ATTR_NULL
};

static struct class subtitle_class = {
        .name = "subtitle",
        .class_attrs = subtitle_class_attrs,
    };

static int __init subtitle_init(void)
{
    int r;

    r = class_register(&subtitle_class);

    if (r) {
        amlog_level(LOG_LEVEL_ERROR, "subtitle class create fail.\n");
        return r;
    }


    return (0);
}

static void __exit subtitle_exit(void)
{
    class_unregister(&subtitle_class);
}

module_init(subtitle_init);
module_exit(subtitle_exit);

MODULE_DESCRIPTION("AMLOGIC Subtitle management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Wang <kevin.wang@amlogic.com>");
