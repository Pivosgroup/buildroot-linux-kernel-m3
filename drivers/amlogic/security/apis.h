#ifndef __APIS_H__
#define __APIS_H__


/* encrypt interface (RSA1024/SHA256) */
int secure_runtime_call(char *pSRC, int nDataLen,char *pOUT,int *pOUTLen,int procedure, int cmd);
/**********************************************************************************/
/* int cmd definition:*/
#define SECURE_CMD_INIT		0
#define SECURE_CMD_GAIN_SN		1
#define SECURE_CMD_RSA_PROC_DEC_ME		2
#define SECURE_CMD_SHA_PROC_ENC_IV		3
#define SECURE_CMD_BURN_SN		4
#define SECURE_CMD_BURN_IN  5
// #define SECURE_CMD_UPDATE_AUTH_RA  6
#define SECURE_CMD_RSA_DEC_VIA_SECU_KEY 6
#define SECURE_CMD_CHECK_IN_FREE 7
/* pSRC data must be little-endian */
#define SECURE_CMD_BURN_SN_RAW       8
/* pSRC data must be little-endian */
#define SECURE_CMD_BURN_IN_RAW       9
/* pSRC data must be little-endian */
#define SECURE_CMD_BURN_MAC     10
/* pSRC data must be little-endian */
#define SECURE_CMD_GAIN_MAC     11

/**********************************************************************************/

/* each command definition*/
/**********************************************************************************/
/* function: secure runtime init, obtain SN/RA/CI/IN */
/* secure_runtime_call(NULL, 0, NULL, NULL, -1, SECURE_CMD_INIT) */
/* return 0:ok 1:fail  */
/**********************************************************************************/

/**********************************************************************************/
/* function: secure runtime gain SN */
/* secure_runtime_call(NULL, 0, pOUT, pOUTLen, -1, SECURE_CMD_GAIN_SN) */
/* return 0:ok 1:fail  */
/**********************************************************************************/

/**********************************************************************************/
/* function: secure runtime RSA decrypt ME */
/* secure_runtime_call(pSRC, nDataLen, pOUT, pOUTLen,	procedure, SECURE_CMD_RSA_PROC_DEC_ME) */
/* return 0:ok 1:fail  */
//
/* return 0:ok 1:fail  */
//pSRC : buffer for process
//nDataLen : valid data length in pSRC
//       enc : nDataLen <= 117
//       dec : nDataLen = 128
//pOUT : buffer to contain the result
//       enc : length >= 128
//       dec : length >= 127
//pOUTLen : valid data length in pOUT
//procedure : rsa option
//			ENC_WITH_PUB  : encode with public key
//			ENC_WITH_PRIV : encode with private key
//			DEC_WITH_PRIV : decode with private key
//			DEC_WITH_PUB  : decode with public key
//			RSA_BUF_IN_TYPE_LITTLE : source file is little endian 
//			RSA_BUF_IN_TYPE_BIG : source file is big endian 
//			RSA_BUF_OUT_TYPE_LITTLE : output file is little endian 
//			RSA_BUF_OUT_TYPE_BIG : source file is big endian 
/**********************************************************************************/
/* int procedure bit definition*/
#define RSA_ENC_DEC_MASK (0x0F)
#define ENC_WITH_PUB	 (0)
#define ENC_WITH_PRIV	 (1)
#define DEC_WITH_PRIV  	 (3)
#define DEC_WITH_PUB   	 (2)

#define RSA_BUF_IN_TYPE_MASK    (0x30)
#define RSA_BUF_IN_TYPE_LITTLE  (0x00)
#define RSA_BUF_IN_TYPE_BIG     (0x10)
#define RSA_BUF_IN_TYPE_IGNORE (0x30)
#define RSA_BUF_OUT_TYPE_MASK    (0xC0)
#define RSA_BUF_OUT_TYPE_LITTLE  (0x00)
#define RSA_BUF_OUT_TYPE_BIG     (0x80)
#define RSA_BUF_OUT_TYPE_IGNORE (0xC0)


/**********************************************************************************/
/* function: secure runtime SHA256 generate */
/* secure_runtime_call(pSRC, nDataLen, pOUT, pOUTLen, -1, SECURE_CMD_SHA_PROC_ENC_IV) */
/* return 0:ok 1:fail  */
/**********************************************************************************/

/**********************************************************************************/
/* Structure */
/**********************************************************************************/
typedef struct secure_sn_s { // SN, 24 bytes
  char factory_id[4];            // 厂商代码
  char product_model[4];         // 终端型号
  char product_batch[8];         // 产品批次
  char product_sn[8];            // 产品序号
  int status;
} secure_sn_t;

typedef struct secure_ra_s { // RA (RSA1024 Algorithm factors), 517 bytes
  char e[  5];
  char n[256];
  char d[256];
  int status;
} secure_ra_t;

typedef struct secure_in_s { // IN, 24 bytes
  char factory_id[4];            // 厂商代码
  char chip_batch[8];            // 芯片批次
  char chip_sn[12];              // 芯片序号
  int status;
} secure_in_t;

typedef struct secure_ci_s { // CI (ChipID), 4 bytes
  char data[4];
  int status;
} secure_ci_t;


#endif /*__APIS_H__*/
