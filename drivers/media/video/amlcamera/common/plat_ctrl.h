#ifndef _AMLOGIC_CAMERA_PLAT_CTRL_H
#define _AMLOGIC_CAMERA_PLAT_CTRL_H

typedef struct aml_camera_i2c_fig_s{
    unsigned short   addr;
    unsigned char    val;
} aml_camera_i2c_fig_t;

typedef struct aml_camera_i2c_fig0_s{
    unsigned short   addr;
    unsigned short    val;
} aml_camera_i2c_fig0_t;

typedef struct aml_camera_i2c_fig1_s{
    unsigned char   addr;
    unsigned char    val;
} aml_camera_i2c_fig1_t;


extern int i2c_get_byte(struct i2c_client *client,unsigned short addr);
extern int i2c_get_word(struct i2c_client *client,unsigned short addr);
extern int i2c_get_byte_add8(struct i2c_client *client,unsigned short addr);
extern int i2c_put_byte(struct i2c_client *client, unsigned short addr, unsigned char data);
extern int i2c_put_word(struct i2c_client *client, unsigned short addr, unsigned short data);
extern int i2c_put_byte_add8(struct i2c_client *client,char *buf, int len);


#endif /* _AMLOGIC_CAMERA_PLAT_CTRL_H. */
