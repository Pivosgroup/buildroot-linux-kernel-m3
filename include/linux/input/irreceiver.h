#ifndef IRRECEIVER_H
#define IRRECEIVER_H

#define MAX_PLUSE 1024

enum{
    PWM_A = 0,
    PWM_B,
    PWM_C,
    PWM_D,
};

struct irreceiver_platform_data{
    int pwm_no;
    int freq;
    int (*init_pwm_pinmux)(void);
    int (*init_ir_pinmux)(void);
};

struct ir_window {
    unsigned int winNum;
    unsigned int winArray[MAX_PLUSE];
};

#define IRRECEIVER_IOC_SEND     0x5500
#define IRRECEIVER_IOC_RECV     0x5501
#define IRRECEIVER_IOC_STUDY_S  0x5502
#define IRRECEIVER_IOC_STUDY_E  0x5503


#endif //IRRECEIVER_H

