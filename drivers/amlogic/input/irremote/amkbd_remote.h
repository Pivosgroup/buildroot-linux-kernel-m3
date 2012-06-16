#ifndef   _AMKBD_REMOTE_H
#define   _AMKBD_REMOTE_H
#include  <asm/ioctl.h>
#include <linux/fiq_bridge.h>

//remote config  ioctl  cmd
#define   REMOTE_IOC_RESET_KEY_MAPPING	    _IOW_BAD('I',3,sizeof(short))
#define   REMOTE_IOC_SET_KEY_MAPPING		    _IOW_BAD('I',4,sizeof(short))
#define   REMOTE_IOC_SET_MOUSE_MAPPING	    _IOW_BAD('I',5,sizeof(short))
#define   REMOTE_IOC_SET_REPEAT_DELAY		    _IOW_BAD('I',6,sizeof(short))
#define   REMOTE_IOC_SET_REPEAT_PERIOD	    _IOW_BAD('I',7,sizeof(short))

#define   REMOTE_IOC_SET_REPEAT_ENABLE		_IOW_BAD('I',8,sizeof(short))
#define	REMOTE_IOC_SET_DEBUG_ENABLE			_IOW_BAD('I',9,sizeof(short)) 
#define	REMOTE_IOC_SET_MODE					_IOW_BAD('I',10,sizeof(short)) 

#define   REMOTE_IOC_SET_RELEASE_DELAY		_IOW_BAD('I',99,sizeof(short))
#define   REMOTE_IOC_SET_CUSTOMCODE   			_IOW_BAD('I',100,sizeof(short))
//reg
#define   REMOTE_IOC_SET_REG_BASE_GEN			_IOW_BAD('I',101,sizeof(short))
#define   REMOTE_IOC_SET_REG_CONTROL			_IOW_BAD('I',102,sizeof(short))
#define   REMOTE_IOC_SET_REG_LEADER_ACT 		_IOW_BAD('I',103,sizeof(short))
#define   REMOTE_IOC_SET_REG_LEADER_IDLE 		_IOW_BAD('I',104,sizeof(short))
#define   REMOTE_IOC_SET_REG_REPEAT_LEADER 	_IOW_BAD('I',105,sizeof(short))
#define   REMOTE_IOC_SET_REG_BIT0_TIME		 _IOW_BAD('I',106,sizeof(short))

//sw
#define   REMOTE_IOC_SET_BIT_COUNT			 	_IOW_BAD('I',107,sizeof(short))
#define   REMOTE_IOC_SET_TW_LEADER_ACT		_IOW_BAD('I',108,sizeof(short))
#define   REMOTE_IOC_SET_TW_BIT0_TIME			_IOW_BAD('I',109,sizeof(short))
#define   REMOTE_IOC_SET_TW_BIT1_TIME			_IOW_BAD('I',110,sizeof(short))
#define   REMOTE_IOC_SET_TW_REPEATE_LEADER	_IOW_BAD('I',111,sizeof(short))

#define   REMOTE_IOC_GET_TW_LEADER_ACT		_IOR_BAD('I',112,sizeof(short))
#define   REMOTE_IOC_GET_TW_BIT0_TIME			_IOR_BAD('I',113,sizeof(short))
#define   REMOTE_IOC_GET_TW_BIT1_TIME			_IOR_BAD('I',114,sizeof(short))
#define   REMOTE_IOC_GET_TW_REPEATE_LEADER	_IOR_BAD('I',115,sizeof(short))


#define   REMOTE_IOC_GET_REG_BASE_GEN			_IOR_BAD('I',121,sizeof(short))
#define   REMOTE_IOC_GET_REG_CONTROL			_IOR_BAD('I',122,sizeof(short))
#define   REMOTE_IOC_GET_REG_LEADER_ACT 		_IOR_BAD('I',123,sizeof(short))
#define   REMOTE_IOC_GET_REG_LEADER_IDLE 		_IOR_BAD('I',124,sizeof(short))
#define   REMOTE_IOC_GET_REG_REPEAT_LEADER 	_IOR_BAD('I',125,sizeof(short))
#define   REMOTE_IOC_GET_REG_BIT0_TIME		 	_IOR_BAD('I',126,sizeof(short))
#define   REMOTE_IOC_GET_REG_FRAME_DATA		_IOR_BAD('I',127,sizeof(short))
#define   REMOTE_IOC_GET_REG_FRAME_STATUS	_IOR_BAD('I',128,sizeof(short))

#define   	REMOTE_HW_DECODER_STATUS_MASK			(0xf<<4)
#define   	REMOTE_HW_DECODER_STATUS_OK			(0<<4)
#define	REMOTE_HW_DECODER_STATUS_TIMEOUT		(1<<4)
#define	REMOTE_HW_DECODER_STATUS_LEADERERR	(2<<4)
#define	REMOTE_HW_DECODER_STATUS_REPEATERR	(3<<4)

#define	REMOTE_HW_PATTERN_MASK					(0xf<<4)
#define	REMOTE_HW_NEC_PATTERN					(0x0<<4)
#define	REMOTE_HW_TOSHIBA_PATTERN				(0x1<<4)

#define   REMOTE_WORK_MODE_SW 				(0)
#define   REMOTE_WORK_MODE_HW					(1)
#define   REMOTE_WORK_MODE_FIQ				(2)
#define   REMOTE_WORK_MODE_INV				(3)
#define   REMOTE_WORK_MODE_MASK 				(3)

#define   REMOTE_TOSHIBA_HW		(REMOTE_HW_TOSHIBA_PATTERN|REMOTE_WORK_MODE_HW)
#define   REMOTE_NEC_HW				(REMOTE_HW_NEC_PATTERN|REMOTE_WORK_MODE_HW)


#define REMOTE_STATUS_WAIT       0
#define REMOTE_STATUS_LEADER    1
#define REMOTE_STATUS_DATA       2
#define REMOTE_STATUS_SYNC       3
#define REMOTE_LOG_BUF_LEN		8192
#define REMOTE_LOG_BUF_ORDER		1



typedef  int   (*type_printk)(const char *fmt, ...) ;

struct kp {
	struct input_dev *input;
	struct timer_list timer;
	struct timer_list repeat_timer;
       unsigned long repeat_tick;
	int irq;
	int work_mode ;
	unsigned int cur_keycode;
	unsigned int repeate_flag;
	unsigned int repeat_enable;
	unsigned int debounce;
	unsigned int custom_code;
	unsigned int release_delay;
	unsigned int debug_enable;
//sw
	unsigned int delay;
	unsigned int   step;
	unsigned int   send_data;
	bridge_item_t 		fiq_handle_item;
	
	unsigned int 	bit_count;
	unsigned int   bit_num;
	unsigned int	last_jiffies;
	unsigned int 	time_window[8];
	int			last_pulse_width;
	int			repeat_time_count;
//config 	
	int			config_major;
	char 		config_name[20];
	struct class *config_class;
	struct device *config_dev;
       unsigned int repeat_delay;
       unsigned int repeat_peroid;
};

extern type_printk input_dbg;
extern irqreturn_t remote_bridge_isr(int irq, void *dev_id);

void kp_sw_reprot_key(unsigned long data);
void kp_send_key(struct input_dev *dev, unsigned int scancode, unsigned int type);

#endif   //_AMKBD_REMOTE_H
