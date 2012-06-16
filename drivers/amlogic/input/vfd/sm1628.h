#ifndef SM_1628_H
#define	SM_1628_H
//def GIEC_LED_DISPLAY

#if 0//def GK-HD200T
#define	COLON_ENABLE				0	// Maybe cause error of the display of 1bit LED
#define	FP_LED_MODE					0x02
// -------------- DIGITAL LED MACRO -----------------
#define LED_NUM						4
#define FP_DOT1_DIG				2
#define FP_DOT2_DIG				4
#define FP_DOT_SEG_NUM				6
#define LED_WORD1_ADDR				0
#define	LED_WORD2_ADDR				2
#define	LED_WORD3_ADDR				4
#define	LED_WORD4_ADDR				6
#define	LED_WORD5_ADDR				10
#define	LED_COLON_ADDR				8
#define	LED_COLON_ENABLE_LOW		0xFF
#define	LED_COLON_ENABLE_HIGH		0xFF
#define	LED_COLON_DISABLE_LOW		0
#define	LED_COLON_DISABLE_HIGH		0

#define FRONTPNL_START_TIME_MS		3	//((1000 / 50) / LED_NUM)
#define FRONTPNL_PERIOD_TIME_MS		150


typedef struct {
    char u8Char;
    char u8SegmentLowByte;
    char u8SegmentHighByte;
} Char2Segment;
#define BIT_A		(1 << 0)	//            a
#define BIT_B		(1 << 1)	//         -------
#define BIT_C		(1 << 2)	//        |       |
#define BIT_D		(1 << 3)	//    //f |       | b
#define BIT_E		(1 << 4)	//         ---g---
#define BIT_F		(1 << 5)	//        |       |	c
#define BIT_G		(1 << 6)	//    //e |       |
#define BIT_P		(1 << 7)	//         ---d---   p
#else  //B203_PANEL
#define	COLON_ENABLE				0	// Maybe cause error of the display of 1bit LED
#define	FP_LED_MODE					0x03
// -------------- DIGITAL LED MACRO -----------------
#define LED_NUM								5
#define LED_BYTE_NUM					7
#define FP_DOT1_DIG						2
#define FP_DOT2_DIG						4
#define FP_DOT_ENABLE					0
#define FP_DOT_SEG_NUM				6
#define LED_WORD1_ADDR				0
#define	LED_WORD2_ADDR				2
#define	LED_WORD3_ADDR				4
#define	LED_WORD4_ADDR				6
#define	LED_WORD5_ADDR				8
#define	LED_WORD6_ADDR				10
#define	LED_COLON_ADDR				12
#define	LED_COLON_ENABLE_LOW		0xFF
#define	LED_COLON_ENABLE_HIGH		0xFF
#define	LED_COLON_DISABLE_LOW		0
#define	LED_COLON_DISABLE_HIGH		0

#define FRONTPNL_START_TIME_MS		3	//((1000 / 50) / LED_NUM)
#define FRONTPNL_PERIOD_TIME_MS		150


typedef struct {
    char u8Char;
    char u8SegmentLowByte;
    char u8SegmentHighByte;
} Char2Segment;
#define BIT_A		(1 << 0)	//            a
#define BIT_B		(1 << 1)	//         -------
#define BIT_C		(1 << 2)	//        |       |
#define BIT_D		(1 << 5)	//    //f |       | b
#define BIT_E		(1 << 4)	//         ---g---
#define BIT_F		(1 << 3)	//        |       |	c
#define BIT_G		(1 << 6)	//    //e |       |
#define BIT_P		(1 << 7)	//         ---d---   p

#endif
#define LED_DATA_LIGHT		1	// open or close this when choose light mode
#if (LED_DATA_LIGHT == 0)
#define DATA_NOT	~	// data_reverse
#else
#define DATA_NOT		// no reverse
#endif
// 定义字符编码，此处无需随方案不同而改动，只需改动上面配置的BIT_A、BIT_B等
#define DATA_0		(DATA_NOT (BIT_A | BIT_B | BIT_C | BIT_D | BIT_E | BIT_F))
#define DATA_1		(DATA_NOT (BIT_B | BIT_C))
#define DATA_2		(DATA_NOT (BIT_A | BIT_B | BIT_D | BIT_E | BIT_G))
#define DATA_3		(DATA_NOT (BIT_A | BIT_B | BIT_C | BIT_D | BIT_G))
#define DATA_4		(DATA_NOT (BIT_B | BIT_C | BIT_F | BIT_G))
#define DATA_5		(DATA_NOT (BIT_A | BIT_C | BIT_D | BIT_F | BIT_G))
#define DATA_6		(DATA_NOT (BIT_A | BIT_C | BIT_D | BIT_E | BIT_F | BIT_G))
#define DATA_7		(DATA_NOT (BIT_A | BIT_B | BIT_C))
#define DATA_8		(DATA_NOT (BIT_A | BIT_B | BIT_C | BIT_D | BIT_E | BIT_F | BIT_G))
#define DATA_9		(DATA_NOT (BIT_A | BIT_B | BIT_C | BIT_D | BIT_F | BIT_G))
#define DATA_A		(DATA_NOT (BIT_A | BIT_B | BIT_C | BIT_E | BIT_F | BIT_G))
#define DATA_b		(DATA_NOT (BIT_C | BIT_D | BIT_E | BIT_F | BIT_G))
#define DATA_C		(DATA_NOT (BIT_A | BIT_D | BIT_E | BIT_F))
#define DATA_c		(DATA_NOT (BIT_D | BIT_E | BIT_G))
#define DATA_d		(DATA_NOT (BIT_B | BIT_C | BIT_D | BIT_E | BIT_G))
#define DATA_E		(DATA_NOT (BIT_A | BIT_D | BIT_E | BIT_F | BIT_G))
#define DATA_F		(DATA_NOT (BIT_A | BIT_E | BIT_F | BIT_G))
#define DATA_H		(DATA_NOT (BIT_B | BIT_C | BIT_E | BIT_F | BIT_G))
#define DATA_h		(DATA_NOT (BIT_C | BIT_E | BIT_F | BIT_G))
#define DATA_I		(DATA_NOT (BIT_E | BIT_F))
#define DATA_i		(DATA_NOT (BIT_E))
#define DATA_L		(DATA_NOT (BIT_D | BIT_E | BIT_F))
#define DATA_N		(DATA_NOT (BIT_A | BIT_B | BIT_C | BIT_E | BIT_F))
#define DATA_n		(DATA_NOT (BIT_C | BIT_E | BIT_G))
#define DATA_O		(DATA_NOT (BIT_A | BIT_B | BIT_C | BIT_D | BIT_E | BIT_F))
#define DATA_o		(DATA_NOT (BIT_C | BIT_D | BIT_E | BIT_G))
#define DATA_P		(DATA_NOT (BIT_A | BIT_B | BIT_E | BIT_F | BIT_G))
#define DATA_R		(DATA_NOT (BIT_D | BIT_E | BIT_G))
#define DATA_r		(DATA_NOT (BIT_D | BIT_E | BIT_G))
#define DATA_S		(DATA_NOT (BIT_A | BIT_C | BIT_D | BIT_F | BIT_G))
#define DATA_s		(DATA_NOT (BIT_A | BIT_C | BIT_D | BIT_F | BIT_G))
#define DATA_T		(DATA_NOT (BIT_D | BIT_E | BIT_F | BIT_G))
#define DATA_t		(DATA_NOT (BIT_D | BIT_E | BIT_F | BIT_G))
#define DATA_U		(DATA_NOT (BIT_B | BIT_C | BIT_D | BIT_E | BIT_F))
#define DATA_u		(DATA_NOT (BIT_C | BIT_D | BIT_E))
#define DATA_DP		(DATA_NOT (BIT_P))
#define DATA_HYPH	(DATA_NOT (BIT_G))
#define DATA_DARK	(DATA_NOT (0x00))
static const char FP_LED_BOOT[] =
{
	0xF0,
	0x00,
	0xD0,
	0x01,
	0xD0,
	0x01,
	0xF0,
	0x01
};

static const char FP_LED_STANDBY[]=
{
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x04
};


//  ---1---
//    |     |
//    2     3
//    |     |
//  ---4---
//    |     |
//    8     5
//    |     |
//  ---7---

static const Char2Segment _char2SegmentTable[] =
{
#if 0
	// char, low Byte, High Byte
	{'S', 0x5B, 0x00},
	{'0', 0xD7, 0x00},
	{'1', 0x14, 0x00},	//{'1',0x00,0x03},
	{'2', 0xCD, 0x00},
	{'3', 0x5D, 0x00},
	{'4', 0x1E, 0x00},
	{'5', 0x5B, 0x00},
	{'6', 0xDB, 0x00},
	{'7', 0x15, 0x00},
	{'8', 0xDF, 0x00},
	{'9', 0x5F, 0x00},
	{'A', 0x9F, 0x00},
	{'B', 0xDA, 0x00},
	{'b', 0xDA, 0x00},
	{'C', 0xC3, 0x00},
	{'c', 0xC8, 0x00},
	{'D', 0xDC, 0x00},
	{'d', 0xDC, 0x00},
	{'E', 0xCB, 0x00},
	{'F', 0x8B, 0x00},
	{'H', 0x9E, 0x00},	//{'H',0x70,0x03},
	{'h', 0x9A, 0x00},
	{'I', 0x14, 0x00},
	{'i', 0x14, 0x00},
	{'L', 0xC2, 0x00},
	{'n', 0x98, 0x00},
	{'N', 0x97, 0x00},
	{'O', 0xD8, 0x00},
	{'o', 0xD8, 0x00},
	{'P', 0x8F, 0x00},
	{'R', 0x9F, 0x00},
	{'r', 0x9F, 0x00},
	{'T', 0xCA, 0x00},
	{'t', 0xCA, 0x00},
	{'U', 0xD6, 0x00},
	{'V', 0xD6, 0x00},
	{'-', 0x08, 0x00},
	{' ', 0x00, 0x00},

#else	// for HENAG by scares
	// char, low Byte, High Byte
	{'0', DATA_0, 0x00},
	{'1', DATA_1, 0x00},
	{'2', DATA_2, 0x00},
	{'3', DATA_3, 0x00},
	{'4', DATA_4, 0x00},
	{'5', DATA_5, 0x00},
	{'6', DATA_6, 0x00},
	{'7', DATA_7, 0x00},
	{'8', DATA_8, 0x00},
	{'9', DATA_9, 0x00},
	{'A', DATA_A, 0x00},
	{'B', DATA_b, 0x00},
	{'b', DATA_b, 0x00},
	{'C', DATA_C, 0x00},
	{'c', DATA_c, 0x00},
	{'D', DATA_d, 0x00},
	{'d', DATA_d, 0x00},
	{'E', DATA_E, 0x00},
	{'F', DATA_F, 0x00},
	{'H', DATA_H, 0x00},
	{'h', DATA_h, 0x00},
	{'I', DATA_I, 0x00},
	{'i', DATA_i, 0x00},
	{'L', DATA_L, 0x00},
	{'n', DATA_n, 0x00},
	{'N', DATA_N, 0x00},
	{'O', DATA_O, 0x00},
	{'o', DATA_o, 0x00},
	{'P', DATA_P, 0x00},
	{'R', DATA_R, 0x00},
	{'r', DATA_r, 0x00},
	{'S', DATA_S, 0x00},
	{'T', DATA_T, 0x00},
	{'t', DATA_t, 0x00},
	{'U', DATA_U, 0x00},
	{'V', DATA_u, 0x00},
	{'-', DATA_HYPH, 0x00},
	{' ', DATA_DARK, 0x00},
#endif
};
// NOTE: in 6964 the CHAR arrayed like 'hgfedcba', and the 'h' is the MSB.

//----------------- IO MACRO ------------------------
/*
#define REG(addr)				(*(volatile MS_U32 *)(addr))
#define REG_CHIP_TOP_BASE		0xBF803C00

#define REG_CHIP_TOP2C			(0x2C*4 + (REG_CHIP_TOP_BASE))
#define REG_CHIP_TOP06			(0x06*4 + (REG_CHIP_TOP_BASE))
#define REG_CHIP_TOP0E			(0x0E*4 + (REG_CHIP_TOP_BASE))

#define REG_GPIO_CI_IN			(0x5A*4 + (REG_CHIP_TOP_BASE))
#define REG_GPIO_CI_OUT			(0x57*4 + (REG_CHIP_TOP_BASE))
#define REG_GPIO_CI_OEN			(0x4C*4 + (REG_CHIP_TOP_BASE))

#define REG_PM_BASE				0xBF809F00  //(0x27C0 * 4) + 0xBF800000
#define REG_PMCEC_EN			(0x00*4 + (REG_PM_BASE))
#define REG_PM_GPIO_OUT_PM2_EN	(0x01*4 + (REG_PM_BASE))
#define REG_PM_GPIO_OEN			(0x02*4 + (REG_PM_BASE))
#define REG_PM_GPIO_IN			(0x03*4 + (REG_PM_BASE))

#define KEYPAD_CHANNEL_START	1           ///< Define the start keypad channel
#define KEYPAD_CHANNEL_END		2           ///< Define the end keypad channel
#define KEYPAD_CHANNEL_NUM		(KEYPAD_CHANNEL_END - KEYPAD_CHANNEL_START + 1) ///< Define the number of keypad channel.
*/
#define PT6964_CLK_H			vfd_set_clock_pin_value(1)
#define PT6964_CLK_L			vfd_set_clock_pin_value(0)

#define PT6964_DIN_H			vfd_set_do_pin_value(1)
#define PT6964_DIN_L			vfd_set_do_pin_value(0)

#define PT6964_DOUT_GET			vfd_get_di_pin_value()

#define	PT6964_STB_H			vfd_set_stb_pin_value(1)
#define	PT6964_STB_L			vfd_set_stb_pin_value(0)

//------------ KEY PAD macro ------------------

//-------------------------------------------------------------------------------------------------
// Defines
//-------------------------------------------------------------------------------------------------
#define PT6964_KEY_L0			1 //
#define PT6964_KEY_L1			4 //
#define PT6964_KEY_L2			5 //
#define PT6964_KEY_L3			6 //
#define PT6964_KEY_L4			7 //
#define PT6964_KEY_L5			8 //

typedef struct {
	char keyMapData;
	char keyMapLevel;
} VFD_KEYMAP;

typedef     unsigned char       U8;
typedef     unsigned short      U16;
//typedef 		_Bool								bool;

enum {
    FALSE = 0,
    TRUE  = 1
};

#define DELAY(x) { mdelay(32 * x); }
char dig_value[LED_BYTE_NUM];
//char bit2seg[LED_NUM] = {4,3,1,0,2};
char bit2seg[LED_NUM] = {2,0,1,3,4};

#endif

