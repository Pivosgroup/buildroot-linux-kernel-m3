
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include "am_regs.h"

#define WRITE_PERI_REG              WRITE_CBUS_REG
#define READ_PERI_REG               READ_CBUS_REG
#define SET_PERI_REG_MASK           SET_CBUS_REG_MASK
#define CLEAR_PERI_REG_MASK         CLEAR_CBUS_REG_MASK
#define PREG_SPI_FLASH_CMD          SPI_FLASH_CMD
#define PREG_SPI_FLASH_ADDR         SPI_FLASH_ADDR
#define PREG_SPI_FLASH_CTRL         SPI_FLASH_CTRL
#define PREG_SPI_FLASH_CTRL1        SPI_FLASH_CTRL1
#define PREG_SPI_FLASH_STATUS       SPI_FLASH_STATUS
#define PREG_SPI_FLASH_C0           SPI_FLASH_C0

#define AMLOGIC_SPI_MAX_FREQ        25000000
#define SPI_DEV_NAME                "spi_nor"

#define FLASH_PAGESIZE      256

/* Flash opcodes. */
#define OPCODE_WREN     0x06    /* Write enable */
#define OPCODE_RDSR     0x05    /* Read status register */
#define OPCODE_WRSR     0x01    /* Write status register */
#define OPCODE_NORM_READ    0x03    /* Read data bytes (low frequency) */
#define OPCODE_FAST_READ    0x0b    /* Read data bytes (high frequency) */
#define OPCODE_PP       0x02    /* Page program (up to 256 bytes) */
#define OPCODE_SE_4K        0x20    /* Erase 4KiB block */
#define OPCODE_SE_32K       0x52    /* Erase 32KiB block */
#define OPCODE_BE       0xd8    /* Sector erase (usually 64KiB) */
#define OPCODE_RDID     0x9f    /* Read JEDEC ID */

/* Status Register bits. */
#define SR_WIP          1   /* Write in progress */
#define SR_WEL          2   /* Write enable latch */
/* meaning of other SR_* bits may differ between vendors */
#define SR_BP0          4   /* Block protect 0 */
#define SR_BP1          8   /* Block protect 1 */
#define SR_BP2          0x10    /* Block protect 2 */
#define SR_SRWD         0x80    /* SR write protect */

/* Define max times to check status register before we give up. */
#define MAX_READY_WAIT_COUNT    100000
#define CMD_SIZE        4

#ifdef CONFIG_SPI_USE_FAST_READ
#define OPCODE_READ     OPCODE_FAST_READ
#define FAST_READ_DUMMY_BYTE 1
#else
#define OPCODE_READ     OPCODE_NORM_READ
#define FAST_READ_DUMMY_BYTE 0
#endif

#ifdef CONFIG_MTD_PARTITIONS
#define mtd_has_partitions()    (1)
#else
#define mtd_has_partitions()    (0)
#endif

