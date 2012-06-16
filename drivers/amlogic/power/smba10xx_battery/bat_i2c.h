#ifndef __BAT_I2C_H__
#define __BAT_I2C_H__

extern int bat_i2c_read_byte(u8 reg_port, u8 *data_buf);
extern int bat_i2c_write_byte(u8 reg_port, u8 *data_buf);
extern int bat_i2c_read_word(u8 reg_port, u16 *data_buf);
extern int bat_i2c_write_word(u8 reg_port, u16 *data_buf);
extern int bat_i2c_block_read(u8 reg_port, u8 *data_buf);

#endif