#ifndef _SDIO_HW_H
#define _SDIO_HW_H

#include <linux/types.h>

#pragma pack(1)

typedef struct SDHW_CMD_Argument_Reg
{
	unsigned long arg;
} SDHW_CMD_Argument_Reg_t;

typedef struct MSHW_CMD_Argument_Reg
{
	unsigned long short_data_write;
} MSHW_CMD_Argument_Reg_t;

typedef struct SDHW_CMD_Send_Reg
{
	unsigned cmd_data: 8;							// Bit 7:0
	unsigned cmd_res_bits: 8;						// Bit 15:8
	unsigned res_without_crc7: 1;					// Bit 16
	unsigned res_with_data: 1;						// Bit 17
	unsigned res_crc7_from_8: 1;					// Bit 18
	unsigned check_dat0_busy: 1;					// Bit 19
	unsigned cmd_send_data: 1;						// Bit 20
	unsigned use_int_window: 1;						// Bit 21
	unsigned reserved: 2;						    // Bit 22:23
	unsigned repeat_package_times: 8;				// Bit 31:24
} SDHW_CMD_Send_Reg_t;

typedef struct MSHW_CMD_Send_Reg
{
	unsigned tpc_data: 8;							// Bit 7:0
	unsigned short_data_read_nums: 8;				// Bit 15:8
	unsigned short_data_without_crc16: 1;			// Bit 16
	unsigned have_long_data_read: 1;				// Bit 17
	unsigned res_crc7_from_8: 1;					// Bit 18
	unsigned check_dat0_busy: 1;					// Bit 19
	unsigned have_long_data_write: 1;				// Bit 20
	unsigned use_int_window: 1;						// Bit 21
	unsigned reserved: 2;						    // Bit 22:23
	unsigned short_data_write_nums: 8;				// Bit 31:24
} MSHW_CMD_Send_Reg_t;

typedef struct SDIO_Config_Reg
{
	unsigned cmd_clk_divide: 10;					// Bit 9:0
	unsigned cmd_disable_crc: 1;					// Bit 10
	unsigned cmd_out_at_posedge: 1;					// Bit 11
	unsigned cmd_argument_bits: 6;					// Bit 17:12
	unsigned res_latch_at_negedge: 1;				// Bit 18
	unsigned data_latch_at_negedge: 1;				// Bit 19
	unsigned bus_width: 1;							// Bit 20
	unsigned m_endian: 2;							// Bit 22:21
	unsigned write_Nwr: 6;							// Bit 28:23
	unsigned write_crc_ok_status: 3;				// Bit 31:29
} SDIO_Config_Reg_t;

typedef struct SDIO_Status_IRQ_Reg
{
	unsigned status: 4;								// Bit 3:0
	unsigned cmd_busy: 1;							// Bit 4
	unsigned res_crc7_ok: 1;						// Bit 5
	unsigned data_read_crc16_ok: 1;					// Bit 6
	unsigned data_write_crc16_ok: 1;				// Bit 7
	unsigned if_int: 1;								// Bit 8
	unsigned cmd_int: 1;							// Bit 9
	unsigned soft_int: 1;							// Bit 10
	unsigned set_soft_int: 1;						// Bit 11
	unsigned status_info: 4;						// Bit 15:12
	unsigned timing_out_int: 1;						// Bit 16
	unsigned amrisc_timing_out_int_en:1;				// Bit 17
	unsigned arc_timing_out_int_en: 1;				// Bit 18
	unsigned timing_out_count: 13;					// Bit 31:19
} SDIO_Status_IRQ_Reg_t;

typedef struct SDHW_IRQ_Config_Reg
{
	unsigned amrisc_if_int_en: 1;					// Bit 0
	unsigned amrisc_cmd_int_en: 1;					// Bit 1
	unsigned amrisc_soft_int_en: 1;					// Bit 2
	unsigned arc_if_int_en: 1;						// Bit 3
	unsigned arc_cmd_int_en: 1;						// Bit 4
	unsigned arc_soft_int_en: 1;					// Bit 5
	unsigned if_int_config: 2;						// Bit 7:6
	unsigned data: 6;								// Bit 13:8
	unsigned force_enable: 1;						// Bit 14
	unsigned soft_reset: 1;							// Bit 15
	unsigned force_output_en: 6;					// Bit 21:16
	unsigned diable_mem_halt: 2;					// Bit 23:22
	unsigned force_data_read: 6;					// Bit 29:24
	unsigned force_halt: 1;							// Bit 30
	unsigned halt_hole:1;								// Bit 31
} SDHW_IRQ_Config_Reg_t;

typedef struct MSHW_IRQ_Config_Reg
{
	unsigned amrisc_if_int_en: 1;					// Bit 0
	unsigned amrisc_cmd_int_en: 1;					// Bit 1
	unsigned amrisc_soft_int_en: 1;					// Bit 2
	unsigned arc_if_int_en: 1;						// Bit 3
	unsigned arc_cmd_int_en: 1;						// Bit 4
	unsigned arc_soft_int_en: 1;					// Bit 5
	unsigned if_int_config: 2;						// Bit 7:6
	unsigned data0: 1;								// Bit 8
	unsigned data1: 1;								// Bit 9
	unsigned data2: 1;								// Bit 10
	unsigned data3: 1;								// Bit 11
	unsigned bs: 1;									// Bit 12
	unsigned sclk: 1;								// Bit 13
	unsigned force_enable: 1;						// Bit 14
	unsigned soft_reset: 1;							// Bit 15
	unsigned force_output_en: 6;					// Bit 21:16
	unsigned diable_mem_halt: 2;					// Bit 23:22
	unsigned force_data_read: 6;					// Bit 29:24
	unsigned force_halt: 1;							// Bit 30
	unsigned halt_hole:1;								// Bit 31
} MSHW_IRQ_Config_Reg_t;

typedef struct SDIO_Multi_Config_Reg
{
	unsigned port_sel: 2;							// Bit 1:0
	unsigned ms_enable: 1;							// Bit 2
	unsigned ms_sclk_always: 1;						// Bit 3
	unsigned stream_enable: 1;						// Bit 4
	unsigned stream_8_bits_mode: 1;					// Bit 5
	unsigned data_catch_level: 2;					// Bit 7:6
	unsigned write_read_out_index: 1;				// Bit 8
	unsigned data_catch_out_en: 1;					// Bit 9
	unsigned reserved: 2;							// Bit 11:10
	unsigned res_read_index: 4;						// Bit 15:12
	unsigned data_catch_finish_point: 12;			// Bit 27:16
	unsigned reserved1: 4;							// Bit 31:28
} SDIO_Multi_Config_Reg_t;

typedef struct SDIO_M_Addr_Reg
{
	unsigned long m_addr;
} SDIO_M_Addr_Reg_t;

typedef struct SDHW_Extension_Reg
{
	unsigned cmd_arg_ext: 16;						// Bit 15:0
	unsigned data_rw_number: 14;					// Bit 29:16
	unsigned data_rw_without_crc16: 1;				// Bit 30
	unsigned crc_status_4line: 1;					// Bit 31
} SDHW_Extension_Reg_t;                             	
                                                    	
typedef struct MSHW_Extension_Reg                   	
{                                                   	
	unsigned short_data_ext: 16;					// Bit 15:0
	unsigned long_data_nums: 14;					// Bit 29:16
	unsigned long_data_without_crc16: 1;			// Bit 30
	unsigned crc_status_4line: 1;					// Bit 31
} MSHW_Extension_Reg_t;

#pragma pack()

#endif //_SDIO_HW_H
