#ifndef _LINUX_IT7230_H_
#define _LINUX_IT7230_H_

#define PAGE_0                      0
#define PAGE_1                      1

//typedef  UCHAR BYTE;

//--- Calibration Engine (Page number = 0) ---//
#define CAPS_CE_Base                0x00
#define CAPS_PSR                    (CAPS_CE_Base+0x00)
#define CAPS_SIR                    (CAPS_CE_Base+0x01)
#define CAPS_SXPOSR                 (CAPS_CE_Base+0x02)
#define CAPS_SXCHSR                 (CAPS_CE_Base+0x03)
#define CAPS_SXCLSR                 (CAPS_CE_Base+0x04)
#define CAPS_SXDSR                  (CAPS_CE_Base+0x05)
#define CAPS_SXCSR                  (CAPS_CE_Base+0x06)
#define CAPS_SLR                    (CAPS_CE_Base+0x07)

#define CAPS_S0CCDCVR               (CAPS_CE_Base+0x20)
#define CAPS_S1CCDCVR               (CAPS_CE_Base+0x21)
#define CAPS_S2CCDCVR               (CAPS_CE_Base+0x22)
#define CAPS_S3CCDCVR               (CAPS_CE_Base+0x23)
#define CAPS_S4CCDCVR               (CAPS_CE_Base+0x24)
#define CAPS_S5CCDCVR               (CAPS_CE_Base+0x25)
#define CAPS_S6CCDCVR               (CAPS_CE_Base+0x26)
#define CAPS_S7CCDCVR               (CAPS_CE_Base+0x27)
#define CAPS_S8CCDCVR               (CAPS_CE_Base+0x28)
#define CAPS_S9CCDCVR               (CAPS_CE_Base+0x29)
#define CAPS_S10CCDCVR              (CAPS_CE_Base+0x2A)
#define CAPS_S11CCDCVR              (CAPS_CE_Base+0x2B)
#define CAPS_S12CCDCVR              (CAPS_CE_Base+0x2C)

// Stage 0 register

#define CAPS_S0FR0                  (CAPS_CE_Base+0x30)
#define CAPS_S0FR1                  (CAPS_CE_Base+0x31)
#define CAPS_S0FR2                  (CAPS_CE_Base+0x32)
#define CAPS_S0DLR                  (CAPS_CE_Base+0x33)
#define CAPS_S0CVR                  (CAPS_CE_Base+0x34)
#define CAPS_S0OHCR                 (CAPS_CE_Base+0x35)
#define CAPS_S0OLCR                 (CAPS_CE_Base+0x36)
#define CAPS_S0OHR                  (CAPS_CE_Base+0x37)
#define CAPS_S0OLR                  (CAPS_CE_Base+0x38)
#define CAPS_S0OHUR                 (CAPS_CE_Base+0x39)
#define CAPS_S0OLUR                 (CAPS_CE_Base+0x3A)
#define CAPS_S0HTR                  (CAPS_CE_Base+0x3B)
#define CAPS_S0LTR                  (CAPS_CE_Base+0x3C)
#define CAPS_S0SR                   (CAPS_CE_Base+0x3D)
#define CAPS_S0CDC_CMP              (CAPS_CE_Base+0x3E)
#define CAPS_S0CSR                  (CAPS_CE_Base+0x3F)

// Stage 1 register

#define CAPS_S1FR0                  (CAPS_CE_Base+0x40)
#define CAPS_S1FR1                  (CAPS_CE_Base+0x41)
#define CAPS_S1FR2                  (CAPS_CE_Base+0x42)
#define CAPS_S1DLR                  (CAPS_CE_Base+0x43)
#define CAPS_S1CVR                  (CAPS_CE_Base+0x44)
#define CAPS_S1OHCR                 (CAPS_CE_Base+0x45)
#define CAPS_S1OLCR                 (CAPS_CE_Base+0x46)
#define CAPS_S1OHR                  (CAPS_CE_Base+0x47)
#define CAPS_S1OLR                  (CAPS_CE_Base+0x48)
#define CAPS_S1OHUR                 (CAPS_CE_Base+0x49)
#define CAPS_S1OLUR                 (CAPS_CE_Base+0x4A)
#define CAPS_S1HTR                  (CAPS_CE_Base+0x4B)
#define CAPS_S1LTR                  (CAPS_CE_Base+0x4C)
#define CAPS_S1SR                   (CAPS_CE_Base+0x4D)
#define CAPS_S1CDC_CMP              (CAPS_CE_Base+0x4E)
#define CAPS_S1CSR                  (CAPS_CE_Base+0x4F)

// Stage 2 register

#define CAPS_S2FR0                  (CAPS_CE_Base+0x50)
#define CAPS_S2FR1                  (CAPS_CE_Base+0x51)
#define CAPS_S2FR2                  (CAPS_CE_Base+0x52)
#define CAPS_S2DLR                  (CAPS_CE_Base+0x53)
#define CAPS_S2CVR                  (CAPS_CE_Base+0x54)
#define CAPS_S2OHCR                 (CAPS_CE_Base+0x55)
#define CAPS_S2OLCR                 (CAPS_CE_Base+0x56)
#define CAPS_S2OHR                  (CAPS_CE_Base+0x57)
#define CAPS_S2OLR                  (CAPS_CE_Base+0x58)
#define CAPS_S2OHUR                 (CAPS_CE_Base+0x59)
#define CAPS_S2OLUR                 (CAPS_CE_Base+0x5A)
#define CAPS_S2HTR                  (CAPS_CE_Base+0x5B)
#define CAPS_S2LTR                  (CAPS_CE_Base+0x5C)
#define CAPS_S2SR                   (CAPS_CE_Base+0x5D)
#define CAPS_S2CDC_CMP              (CAPS_CE_Base+0x5E)
#define CAPS_S2CSR                  (CAPS_CE_Base+0x5F)

// Stage 3 register

#define CAPS_S3FR0                  (CAPS_CE_Base+0x60)
#define CAPS_S3FR1                  (CAPS_CE_Base+0x61)
#define CAPS_S3FR2                  (CAPS_CE_Base+0x62)
#define CAPS_S3DLR                  (CAPS_CE_Base+0x63)
#define CAPS_S3CVR                  (CAPS_CE_Base+0x64)
#define CAPS_S3OHCR                 (CAPS_CE_Base+0x65)
#define CAPS_S3OLCR                 (CAPS_CE_Base+0x66)
#define CAPS_S3OHR                  (CAPS_CE_Base+0x67)
#define CAPS_S3OLR                  (CAPS_CE_Base+0x68)
#define CAPS_S3OHUR                 (CAPS_CE_Base+0x69)
#define CAPS_S3OLUR                 (CAPS_CE_Base+0x6A)
#define CAPS_S3HTR                  (CAPS_CE_Base+0x6B)
#define CAPS_S3LTR                  (CAPS_CE_Base+0x6C)
#define CAPS_S3SR                   (CAPS_CE_Base+0x6D)
#define CAPS_S3CDC_CMP              (CAPS_CE_Base+0x6E)
#define CAPS_S3CSR                  (CAPS_CE_Base+0x6F)

// Stage 4 register

#define CAPS_S4FR0                  (CAPS_CE_Base+0x70)
#define CAPS_S4FR1                  (CAPS_CE_Base+0x71)
#define CAPS_S4FR2                  (CAPS_CE_Base+0x72)
#define CAPS_S4DLR                  (CAPS_CE_Base+0x73)
#define CAPS_S4CVR                  (CAPS_CE_Base+0x74)
#define CAPS_S4OHCR                 (CAPS_CE_Base+0x75)
#define CAPS_S4OLCR                 (CAPS_CE_Base+0x76)
#define CAPS_S4OHR                  (CAPS_CE_Base+0x77)
#define CAPS_S4OLR                  (CAPS_CE_Base+0x78)
#define CAPS_S4OHUR                 (CAPS_CE_Base+0x79)
#define CAPS_S4OLUR                 (CAPS_CE_Base+0x7A)
#define CAPS_S4HTR                  (CAPS_CE_Base+0x7B)
#define CAPS_S4LTR                  (CAPS_CE_Base+0x7C)
#define CAPS_S4SR                   (CAPS_CE_Base+0x7D)
#define CAPS_S4CDC_CMP              (CAPS_CE_Base+0x7E)
#define CAPS_S4CSR                  (CAPS_CE_Base+0x7F)

// Stage 5 register

#define CAPS_S5FR0                  (CAPS_CE_Base+0x80)
#define CAPS_S5FR1                  (CAPS_CE_Base+0x81)
#define CAPS_S5FR2                  (CAPS_CE_Base+0x82)
#define CAPS_S5DLR                  (CAPS_CE_Base+0x83)
#define CAPS_S5CVR                  (CAPS_CE_Base+0x84)
#define CAPS_S5OHCR                 (CAPS_CE_Base+0x85)
#define CAPS_S5OLCR                 (CAPS_CE_Base+0x86)
#define CAPS_S5OHR                  (CAPS_CE_Base+0x87)
#define CAPS_S5OLR                  (CAPS_CE_Base+0x88)
#define CAPS_S5OHUR                 (CAPS_CE_Base+0x89)
#define CAPS_S5OLUR                 (CAPS_CE_Base+0x8A)
#define CAPS_S5HTR                  (CAPS_CE_Base+0x8B)
#define CAPS_S5LTR                  (CAPS_CE_Base+0x8C)
#define CAPS_S5SR                   (CAPS_CE_Base+0x8D)
#define CAPS_S5CDC_CMP              (CAPS_CE_Base+0x8E)
#define CAPS_S5CSR                  (CAPS_CE_Base+0x8F)

// Stage 6 register

#define CAPS_S6FR0                  (CAPS_CE_Base+0x90)
#define CAPS_S6FR1                  (CAPS_CE_Base+0x91)
#define CAPS_S6FR2                  (CAPS_CE_Base+0x92)
#define CAPS_S6DLR                  (CAPS_CE_Base+0x93)
#define CAPS_S6CVR                  (CAPS_CE_Base+0x94)
#define CAPS_S6OHCR                 (CAPS_CE_Base+0x95)
#define CAPS_S6OLCR                 (CAPS_CE_Base+0x96)
#define CAPS_S6OHR                  (CAPS_CE_Base+0x97)
#define CAPS_S6OLR                  (CAPS_CE_Base+0x98)
#define CAPS_S6OHUR                 (CAPS_CE_Base+0x99)
#define CAPS_S6OLUR                 (CAPS_CE_Base+0x9A)
#define CAPS_S6HTR                  (CAPS_CE_Base+0x9B)
#define CAPS_S6LTR                  (CAPS_CE_Base+0x9C)
#define CAPS_S6SR                   (CAPS_CE_Base+0x9D)
#define CAPS_S6CDC_CMP              (CAPS_CE_Base+0x9E)
#define CAPS_S6CSR                  (CAPS_CE_Base+0x9F)

// Stage 7 register

#define CAPS_S7FR0                  (CAPS_CE_Base+0xA0)
#define CAPS_S7FR1                  (CAPS_CE_Base+0xA1)
#define CAPS_S7FR2                  (CAPS_CE_Base+0xA2)
#define CAPS_S7DLR                  (CAPS_CE_Base+0xA3)
#define CAPS_S7CVR                  (CAPS_CE_Base+0xA4)
#define CAPS_S7OHCR                 (CAPS_CE_Base+0xA5)
#define CAPS_S7OLCR                 (CAPS_CE_Base+0xA6)
#define CAPS_S7OHR                  (CAPS_CE_Base+0xA7)
#define CAPS_S7OLR                  (CAPS_CE_Base+0xA8)
#define CAPS_S7OHUR                 (CAPS_CE_Base+0xA9)
#define CAPS_S7OLUR                 (CAPS_CE_Base+0xAA)
#define CAPS_S7HTR                  (CAPS_CE_Base+0xAB)
#define CAPS_S7LTR                  (CAPS_CE_Base+0xAC)
#define CAPS_S7SR                   (CAPS_CE_Base+0xAD)
#define CAPS_S7CDC_CMP              (CAPS_CE_Base+0xAE)
#define CAPS_S7CSR                  (CAPS_CE_Base+0xAF)

// Stage 8 register

#define CAPS_S8FR0                  (CAPS_CE_Base+0xB0)
#define CAPS_S8FR1                  (CAPS_CE_Base+0xB1)
#define CAPS_S8FR2                  (CAPS_CE_Base+0xB2)
#define CAPS_S8DLR                  (CAPS_CE_Base+0xB3)
#define CAPS_S8CVR                  (CAPS_CE_Base+0xB4)
#define CAPS_S8OHCR                 (CAPS_CE_Base+0xB5)
#define CAPS_S8OLCR                 (CAPS_CE_Base+0xB6)
#define CAPS_S8OHR                  (CAPS_CE_Base+0xB7)
#define CAPS_S8OLR                  (CAPS_CE_Base+0xB8)
#define CAPS_S8OHUR                 (CAPS_CE_Base+0xB9)
#define CAPS_S8OLUR                 (CAPS_CE_Base+0xBA)
#define CAPS_S8HTR                  (CAPS_CE_Base+0xBB)
#define CAPS_S8LTR                  (CAPS_CE_Base+0xBC)
#define CAPS_S8SR                   (CAPS_CE_Base+0xBD)
#define CAPS_S8CDC_CMP              (CAPS_CE_Base+0xBE)
#define CAPS_S8CSR                  (CAPS_CE_Base+0xBF)

// Stage 9 register

#define CAPS_S9FR0                  (CAPS_CE_Base+0xC0)
#define CAPS_S9FR1                  (CAPS_CE_Base+0xC1)
#define CAPS_S9FR2                  (CAPS_CE_Base+0xC2)
#define CAPS_S9DLR                  (CAPS_CE_Base+0xC3)
#define CAPS_S9CVR                  (CAPS_CE_Base+0xC4)
#define CAPS_S9OHCR                 (CAPS_CE_Base+0xC5)
#define CAPS_S9OLCR                 (CAPS_CE_Base+0xC6)
#define CAPS_S9OHR                  (CAPS_CE_Base+0xC7)
#define CAPS_S9OLR                  (CAPS_CE_Base+0xC8)
#define CAPS_S9OHUR                 (CAPS_CE_Base+0xC9)
#define CAPS_S9OLUR                 (CAPS_CE_Base+0xCA)
#define CAPS_S9HTR                  (CAPS_CE_Base+0xCB)
#define CAPS_S9LTR                  (CAPS_CE_Base+0xCC)
#define CAPS_S9SR                   (CAPS_CE_Base+0xCD)
#define CAPS_S9CDC_CMP              (CAPS_CE_Base+0xCE)
#define CAPS_S9CSR                  (CAPS_CE_Base+0xCF)

// Stage 10 register

#define CAPS_S10FR0                 (CAPS_CE_Base+0xD0)
#define CAPS_S10FR1                 (CAPS_CE_Base+0xD1)
#define CAPS_S10FR2                 (CAPS_CE_Base+0xD2)
#define CAPS_S10DLR                 (CAPS_CE_Base+0xD3)
#define CAPS_S10CVR                 (CAPS_CE_Base+0xD4)
#define CAPS_S10OHCR                (CAPS_CE_Base+0xD5)
#define CAPS_S10OLCR                (CAPS_CE_Base+0xD6)
#define CAPS_S10OHR                 (CAPS_CE_Base+0xD7)
#define CAPS_S10OLR                 (CAPS_CE_Base+0xD8)
#define CAPS_S10OHUR                (CAPS_CE_Base+0xD9)
#define CAPS_S10OLUR                (CAPS_CE_Base+0xDA)
#define CAPS_S10HTR                 (CAPS_CE_Base+0xDB)
#define CAPS_S10LTR                 (CAPS_CE_Base+0xDC)
#define CAPS_S10SR                  (CAPS_CE_Base+0xDD)
#define CAPS_S10CDC_CMP             (CAPS_CE_Base+0xDE)
#define CAPS_S10CSR                 (CAPS_CE_Base+0xDF)

// Stage 11 register

#define CAPS_S11FR0                 (CAPS_CE_Base+0xE0)
#define CAPS_S11FR1                 (CAPS_CE_Base+0xE1)
#define CAPS_S11FR2                 (CAPS_CE_Base+0xE2)
#define CAPS_S11DLR                 (CAPS_CE_Base+0xE3)
#define CAPS_S11CVR                 (CAPS_CE_Base+0xE4)
#define CAPS_S11OHCR                (CAPS_CE_Base+0xE5)
#define CAPS_S11OLCR                (CAPS_CE_Base+0xE6)
#define CAPS_S11OHR                 (CAPS_CE_Base+0xE7)
#define CAPS_S11OLR                 (CAPS_CE_Base+0xE8)
#define CAPS_S11OHUR                (CAPS_CE_Base+0xE9)
#define CAPS_S11OLUR                (CAPS_CE_Base+0xEA)
#define CAPS_S11HTR                 (CAPS_CE_Base+0xEB)
#define CAPS_S11LTR                 (CAPS_CE_Base+0xEC)
#define CAPS_S11SR                  (CAPS_CE_Base+0xED)
#define CAPS_S11CDC_CMP             (CAPS_CE_Base+0xEE)
#define CAPS_S11CSR                 (CAPS_CE_Base+0xEF)

// Stage 12 register

#define CAPS_S12FR0                 (CAPS_CE_Base+0xF0)
#define CAPS_S12FR1                 (CAPS_CE_Base+0xF1)
#define CAPS_S12FR2                 (CAPS_CE_Base+0xF2)
#define CAPS_S12DLR                 (CAPS_CE_Base+0xF3)
#define CAPS_S12CVR                 (CAPS_CE_Base+0xF4)
#define CAPS_S12OHCR                (CAPS_CE_Base+0xF5)
#define CAPS_S12OLCR                (CAPS_CE_Base+0xF6)
#define CAPS_S12OHR                 (CAPS_CE_Base+0xF7)
#define CAPS_S12OLR                 (CAPS_CE_Base+0xF8)
#define CAPS_S12OHUR                (CAPS_CE_Base+0xF9)
#define CAPS_S12OLUR                (CAPS_CE_Base+0xFA)
#define CAPS_S12HTR                 (CAPS_CE_Base+0xFB)
#define CAPS_S12LTR                 (CAPS_CE_Base+0xFC)
#define CAPS_S12SR                  (CAPS_CE_Base+0xFD)
#define CAPS_S12CDC_CMP             (CAPS_CE_Base+0xFE)
#define CAPS_S12CSR                 (CAPS_CE_Base+0xFF)

//--- Calibration Engine (Page number = 1) ---//

#define CAPS_PCR                    (CAPS_CE_Base+0x01)
#define CAPS_PMR                    (CAPS_CE_Base+0x02)
#define CAPS_CFER                   (CAPS_CE_Base+0x03)
#define CAPS_RTR                    (CAPS_CE_Base+0x04)
#define CAPS_CTR                    (CAPS_CE_Base+0x05)
#define CAPS_CRMR                   (CAPS_CE_Base+0x06)
#define CAPS_PDR                    (CAPS_CE_Base+0x07)
#define CAPS_DR                     (CAPS_CE_Base+0x08)

// Hardware slider 0 register

#define CAPS_S0UBR                  (CAPS_CE_Base+0x09)
#define CAPS_S0LBR                  (CAPS_CE_Base+0x0A)
#define CAPS_S0OSR                  (CAPS_CE_Base+0x0B)

// Hardware slider 1 register

#define CAPS_S1UBR                  (CAPS_CE_Base+0x0C)
#define CAPS_S1LBR                  (CAPS_CE_Base+0x0D)
#define CAPS_S1OSR                  (CAPS_CE_Base+0x0E)

// Connection register

#define CAPS_S0CR                   (CAPS_CE_Base+0x10)
#define CAPS_S1CR                   (CAPS_CE_Base+0x11)
#define CAPS_S2CR                   (CAPS_CE_Base+0x12)
#define CAPS_S3CR                   (CAPS_CE_Base+0x13)
#define CAPS_S4CR                   (CAPS_CE_Base+0x14)
#define CAPS_S5CR                   (CAPS_CE_Base+0x15)
#define CAPS_S6CR                   (CAPS_CE_Base+0x16)
#define CAPS_S7CR                   (CAPS_CE_Base+0x17)
#define CAPS_S8CR                   (CAPS_CE_Base+0x18)
#define CAPS_S9CR                   (CAPS_CE_Base+0x19)
#define CAPS_S10CR                  (CAPS_CE_Base+0x1A)
#define CAPS_S11CR                  (CAPS_CE_Base+0x1B)
#define CAPS_S12CR                  (CAPS_CE_Base+0x1C)


#define CAPS_GODCR0                 (CAPS_CE_Base+0x40)
#define CAPS_GODCR1                 (CAPS_CE_Base+0x41)
#define CAPS_I2CODCR                (CAPS_CE_Base+0x42)
#define CAPS_IODCR                  (CAPS_CE_Base+0x43)
#define CAPS_GPIOOSRR               (CAPS_CE_Base+0x44)
#define CAPS_GPIOICR                (CAPS_CE_Base+0x45)
#define CAPS_GPIOCR0                (CAPS_CE_Base+0x46)
#define CAPS_GPIOCR1                (CAPS_CE_Base+0x47)
#define CAPS_GPIOCR2                (CAPS_CE_Base+0x48)

//--- Interrupt Controller (Page number = 0) ---//

#define CAPS_INTC_Base              0x08
#define CAPS_ISR                    (CAPS_INTC_Base+0x00)
#define CAPS_SXCHAIER               (CAPS_INTC_Base+0x01)
#define CAPS_SXCHRIER               (CAPS_INTC_Base+0x02)
#define CAPS_SXCLAIER               (CAPS_INTC_Base+0x03)
#define CAPS_SXCLRIER               (CAPS_INTC_Base+0x04)
#define CAPS_SXCIER                 (CAPS_INTC_Base+0x05)

//--- General Purpose I/O Port (Page number = 1) ---// 

#define CAPS_GPIO_Base              0x50
#define CAPS_GPIOMSR                (CAPS_GPIO_Base+0x00)
#define CAPS_GPIODR                 (CAPS_GPIO_Base+0x01)
#define CAPS_GPIOIR                 (CAPS_GPIO_Base+0x02)
#define CAPS_GPIOOR                 (CAPS_GPIO_Base+0x03)
#define CAPS_GPIOMR                 (CAPS_GPIO_Base+0x04)
#define CAPS_GPIOLR                 (CAPS_GPIO_Base+0x05)
#define CAPS_GPIOER                 (CAPS_GPIO_Base+0x06)
#define CAPS_GPIOISR                (CAPS_GPIO_Base+0x07)
#define CAPS_GPIOIMR                (CAPS_GPIO_Base+0x08)
#define CAPS_GPIOPCR                (CAPS_GPIO_Base+0x09)
#define CAPS_GPIONPCR               (CAPS_GPIO_Base+0x0A)
#define CAPS_LEDCMR0                (CAPS_GPIO_Base+0x0B)
#define CAPS_LEDCMR1                (CAPS_GPIO_Base+0x0C)
#define CAPS_LEDCMR2                (CAPS_GPIO_Base+0x0D)
#define CAPS_LEDCMR3                (CAPS_GPIO_Base+0x0E)
#define CAPS_LEDRPR                 (CAPS_GPIO_Base+0x0F)
#define CAPS_LEDBR                  (CAPS_GPIO_Base+0x10)
#define CAPS_LEDCGCR                (CAPS_GPIO_Base+0x11)
#define CAPS_LEDPR0                 (CAPS_GPIO_Base+0x12)
#define CAPS_LEDPR1                 (CAPS_GPIO_Base+0x13)
#define CAPS_LEDPR2                 (CAPS_GPIO_Base+0x14)
#define CAPS_LEDPR3                 (CAPS_GPIO_Base+0x15)

//--- Capacitance-to-digital Converter (Page number = 1) ---//

#define CAPS_CDC_Base               0x20
#define CAPS_C0COR                  (CAPS_CDC_Base+0x00)
#define CAPS_C1COR                  (CAPS_CDC_Base+0x01)
#define CAPS_C2COR                  (CAPS_CDC_Base+0x02)
#define CAPS_C3COR                  (CAPS_CDC_Base+0x03)
#define CAPS_C4COR                  (CAPS_CDC_Base+0x04)
#define CAPS_C5COR                  (CAPS_CDC_Base+0x05)
#define CAPS_C6COR                  (CAPS_CDC_Base+0x06)
#define CAPS_C7COR                  (CAPS_CDC_Base+0x07)
#define CAPS_C8COR                  (CAPS_CDC_Base+0x08)
#define CAPS_C9COR                  (CAPS_CDC_Base+0x09)
#define CAPS_C10COR                 (CAPS_CDC_Base+0x0A)
#define CAPS_C11COR                 (CAPS_CDC_Base+0x0B)
#define CAPS_C12COR                 (CAPS_CDC_Base+0x0C)
#define CAPS_ICR0                   (CAPS_CDC_Base+0x0D)
#define CAPS_ICR1                   (CAPS_CDC_Base+0x0E)
#define CAPS_COER0                  (CAPS_CDC_Base+0x0F)
#define CAPS_COER1                  (CAPS_CDC_Base+0x10)
#define CAPS_PDCR                   (CAPS_CDC_Base+0x11)
#define CAPS_CGCR                   (CAPS_CDC_Base+0x12)

//--- On-chip OTP Memory (Page number = 2) ---//

#define D7230_ADDR      0x8C

typedef struct InitCapSReg
{
    unsigned char page;
    unsigned char reg;
    unsigned short value;
}sInitCapSReg;

struct cap_key{
    int code; /* input key code */
    int mask;
    unsigned char *name;
};

struct it7230_platform_data{
    int (*init_irq)(void);
    int (*get_irq_level)(void);
    struct cap_key *key;
    int key_num;
};

#endif //_LINUX_IT7230_H_