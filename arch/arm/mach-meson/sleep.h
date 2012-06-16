#define ADJUST_CORE_VOLTAGE
#define TURN_OFF_DDR_PLL
//#define SAVE_DDR_REGS
//#define SYSTEM_16K

#ifdef SYSTEM_16K
#define VOLTAGE_DLY     0x400
#define MS_DLY          0x200
#else // 24M/128
#define VOLTAGE_DLY     0x4000
#define MS_DLY          0x2000
#endif

