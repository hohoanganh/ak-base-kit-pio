/* Minimal SPI NOR driver (Winbond W25Qxx and JEDEC compatibles), 3-byte
 * addressing, polled SPI. Used for the external STAGING partition. */
#ifndef __SPI_NOR_H__
#define __SPI_NOR_H__

#include <stdint.h>

#define SPI_NOR_SECTOR_SIZE		(4096U)
#define SPI_NOR_PAGE_SIZE		(256U)

#define SPI_NOR_OK				(0)
#define SPI_NOR_ERR_TIMEOUT		(-1)
#define SPI_NOR_ERR_ABSENT		(-2)

/* Init SPI + CS, read JEDEC ID. Returns chip size in bytes, 0 if no chip. */
extern uint32_t spi_nor_init(void);
extern uint32_t spi_nor_jedec_id(void);

extern int spi_nor_read(uint32_t addr, void* buf, uint32_t len);
/* Any length; split at 256 B page boundaries internally. */
extern int spi_nor_write(uint32_t addr, const void* data, uint32_t len);
/* addr aligned to SPI_NOR_SECTOR_SIZE */
extern int spi_nor_erase_sector(uint32_t addr);

#endif
