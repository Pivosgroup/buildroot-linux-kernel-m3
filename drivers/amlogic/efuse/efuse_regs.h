#ifndef __EFUSE_REG_H                                
#define __EFUSE_REG_H
                       
//#define EFUSE_DEBUG                       
                                                     
#define EFUSE_CNTL0 0x2158
#define EFUSE_CNTL1 0x2159                           
#define EFUSE_CNTL2 0x215a                           
#define EFUSE_CNTL3 0x215b                           
#define EFUSE_CNTL4 0x215c                           
                                                     
/* EFUSE_CNTL0 */                                   
                                                    
                                                    
/**                                                 
EFUSE_CNTL1                                         
                                                    
bit[31-27]                                          
bit[26]     AUTO_RD_BUSY                            
bit[25]     AUTO_RD_START                           
bit[24]     AUTO_RD_ENABLE                          
bit[23-16]  BYTE_WR_DATA                            
bit[15]                                             
bit[14]     AUTO_WR_BUSY                            
bit[13]     AUTO_WR_START                           
bit[12]     AUTO_WR_ENABLE                          
bit[11]     BYTE_ADDR_SET                           
bit[10]                                             
bit[9-0]    BYTE_ADDR
**/

#define CNTL1_AUTO_RD_BUSY_BIT              26
#define CNTL1_AUTO_RD_BUSY_SIZE             1
  
#define CNTL1_AUTO_RD_START_BIT             25
#define CNTL1_AUTO_RD_START_SIZE						1

#define CNTL1_AUTO_RD_ENABLE_BIT            24
#define CNTL1_AUTO_RD_ENABLE_SIZE           1
#define CNTL1_AUTO_RD_ENABLE_ON             1
#define CNTL1_AUTO_RD_ENABLE_OFF            0
                                                    
                                                    
                                                    
#define CNTL1_BYTE_WR_DATA_BIT							16
#define CNTL1_BYTE_WR_DATA_SIZE							8

#define CNTL1_AUTO_WR_BUSY_BIT              14
#define CNTL1_AUTO_WR_BUSY_SIZE             1
                                                    
#define CNTL1_AUTO_WR_START_BIT             13
#define CNTL1_AUTO_WR_START_SIZE            1
#define CNTL1_AUTO_WR_START_ON              1
#define CNTL1_AUTO_WR_START_OFF             0
                                                    
                                                    
                                                    
#define CNTL1_AUTO_WR_ENABLE_BIT            12
#define CNTL1_AUTO_WR_ENABLE_SIZE           1
#define CNTL1_AUTO_WR_ENABLE_ON             1
#define CNTL1_AUTO_WR_ENABLE_OFF            0

                                                    
                                                    
#define CNTL1_BYTE_ADDR_SET_BIT             11
#define CNTL1_BYTE_ADDR_SET_SIZE            1
#define CNTL1_BYTE_ADDR_SET_ON              1
#define CNTL1_BYTE_ADDR_SET_OFF             0
                                                    
                                                    
                                                    
                                                    
#define CNTL1_BYTE_ADDR_BIT                 0
#define CNTL1_BYTE_ADDR_SIZE                10
                                                    
                                                    
/* EFUSE_CNTL2 */                                   
                                                    
                                                    
/* EFUSE_CNTL3 */                                   
                                                    
                                                    
/**                                                 
EFUSE_CNTL4                                         
                                                    
bit[31-24]                                          
bit[23-16]  RD_CLOCK_HIGH                           
bit[15-11]                                          
bit[10]     Encrypt enable                          
bit[9]      Encrypt reset                           
bit[8]      XOR_ROTATE                              
bit[7-0]    XOR                                     
**/                                                 
#define CNTL4_RD_CLOCK_HIGH_BIT             23
#define CNTL4_RD_CLOCK_HIGH_SIZE            8
                                                    
#define CNTL4_ENCRYPT_ENABLE_BIT            10
#define CNTL4_ENCRYPT_ENABLE_SIZE           1
#define CNTL4_ENCRYPT_ENABLE_ON             1
#define CNTL4_ENCRYPT_ENABLE_OFF            0
                                                    
#define CNTL4_ENCRYPT_RESET_BIT             9
#define CNTL4_ENCRYPT_RESET_SIZE            1
#define CNTL4_ENCRYPT_RESET_ON              1
#define CNTL4_ENCRYPT_RESET_OFF             0
                                                   
                                                   
#define CNTL4_XOR_ROTATE_BIT                8
#define CNTL4_XOR_ROTATE_SIZE               1
                                                   
#define CNTL4_XOR_BIT                       0
#define CNTL4_XOR_SIZE                      8
                                                   
#if 0
/* defined in am_regs.h */                         
#define WRITE_MPEG_REG(reg, val)	\
        WRITE_CBUS_REG(reg, val)                   
#define READ_MPEG_REG(reg) \
        READ_CBUS_REG(reg)                         
#define WRITE_MPEG_REG_BITS(reg, val, start, len) \
        WRITE_CBUS_REG_BITS(reg, val, start, len)  
#define READ_MPEG_REG_BITS(reg, start, len) \
        READ_CBUS_REG_BITS(reg, start, len)        
#endif                                             
                             
#define 	EFUSE_VERSION_OFFSET     		380
#define 	EFUSE_VERSION_ENC_LEN  		4
#define 	EFUSE_VERSION_DATA_LEN 		3
#define 	EFUSE_VERSION_BCH_EN			1
#define	EFUSE_VERSION_BCH_REVERSE		0

#define	V1_EFUSE_USID_OFFSET				4
#define	V1_EFUSE_USID_ENC_LEN				35
#define	V1_EFUSE_USID_DATA_LEN			35
#define 	V1_EFUSE_USID_BCH_EN				0
#define	V1_EFUSE_USID_BCH_REVERSE		0
                                                  
                                                  
#endif                                             
                                                   