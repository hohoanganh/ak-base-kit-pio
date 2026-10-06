/**
 ******************************************************************************
 * @brief:  W25Qxx SPI NOR on SPI1 (PA5 SCK, PA6 MISO, PA7 MOSI), CS = PB14.
 *          Same wiring/SPI settings as the original ak-base-kit flash driver
 *          (mode 0, PCLK2/8). PORT_KIT_NRF24_CSN = 1 also holds the CSN of an
 *          nRF24 module on the same bus high (AK Base Kit socket).
 ******************************************************************************
**/

#include "stm32l1xx.h"
#include "stm32l1xx_conf.h"

#include "hal.h"
#include "port_cfg.h"
#include "spi_nor.h"

#define CMD_WRITE_ENABLE		(0x06)
#define CMD_READ_SR1			(0x05)
#define CMD_PAGE_PROGRAM		(0x02)
#define CMD_SECTOR_ERASE		(0x20)
#define CMD_READ_DATA			(0x03)
#define CMD_JEDEC_ID			(0x9F)
#define CMD_RELEASE_PD			(0xAB)

#define SR1_BUSY				(0x01)

/* Datasheet maxima (W25Q80/W25Q16/W25Q32): page program 3 ms, sector erase
 * 400 ms. Generous margins; hal_millis() runs from SysTick. */
#define TIMEOUT_PROGRAM_MS		(20U)
#define TIMEOUT_ERASE_MS		(1000U)

/* One byte takes 2 us at 4 MHz; this is some ms. Only a dead SPI peripheral
 * gets here, and then the loops must still end. */
#define XFER_SPIN_MAX			(20000U)

static uint32_t chip_size;
static uint8_t bus_fault;		/* set by xfer() on timeout, cleared per operation */

static inline void cs_low(void) {
	PORT_NOR_CS_PORT->BSRRH = PORT_NOR_CS_PIN;
}

static inline void cs_high(void) {
	PORT_NOR_CS_PORT->BSRRL = PORT_NOR_CS_PIN;
}

static uint8_t xfer(uint8_t b) {
	uint32_t spin = XFER_SPIN_MAX;

	while (!(SPI1->SR & SPI_SR_TXE)) {
		if (--spin == 0) {
			bus_fault = 1;
			return 0xFF;
		}
	}
	SPI1->DR = b;
	while (!(SPI1->SR & SPI_SR_RXNE)) {
		if (--spin == 0) {
			bus_fault = 1;
			return 0xFF;
		}
	}
	return (uint8_t)SPI1->DR;
}

static void send_addr(uint8_t cmd, uint32_t addr) {
	xfer(cmd);
	xfer((uint8_t)(addr >> 16));
	xfer((uint8_t)(addr >> 8));
	xfer((uint8_t)addr);
}

static int wait_ready(uint32_t timeout_ms) {
	uint32_t t0 = hal_millis();
	uint8_t sr;

	do {
		cs_low();
		xfer(CMD_READ_SR1);
		sr = xfer(0xFF);
		cs_high();
		if (!(sr & SR1_BUSY)) {
			return SPI_NOR_OK;
		}
		hal_wdt_kick();
	} while ((hal_millis() - t0) < timeout_ms);

	return SPI_NOR_ERR_TIMEOUT;
}

static void write_enable(void) {
	cs_low();
	xfer(CMD_WRITE_ENABLE);
	cs_high();
}

uint32_t spi_nor_jedec_id(void) {
	uint32_t id;

	cs_low();
	xfer(CMD_JEDEC_ID);
	id = (uint32_t)xfer(0xFF) << 16;
	id |= (uint32_t)xfer(0xFF) << 8;
	id |= xfer(0xFF);
	cs_high();
	return id;
}

uint32_t spi_nor_init(void) {
	GPIO_InitTypeDef gpio;
	SPI_InitTypeDef spi;
	uint32_t id;
	uint8_t cap;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | PORT_NOR_CS_CLK, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

	/* chip selects: output, idle high */
	GPIO_StructInit(&gpio);
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_Speed = GPIO_Speed_10MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	gpio.GPIO_Pin = PORT_NOR_CS_PIN;
	GPIO_Init(PORT_NOR_CS_PORT, &gpio);
	cs_high();
#if PORT_KIT_NRF24_CSN
	RCC_AHBPeriphClockCmd(PORT_NRF_CSN_CLK, ENABLE);
	gpio.GPIO_Pin = PORT_NRF_CSN_PIN;
	GPIO_Init(PORT_NRF_CSN_PORT, &gpio);
	GPIO_SetBits(PORT_NRF_CSN_PORT, PORT_NRF_CSN_PIN);
#endif

	/* SCK/MISO/MOSI */
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_SPI1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_SPI1);
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_Speed = GPIO_Speed_40MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_Init(GPIOA, &gpio);

	SPI_StructInit(&spi);
	spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	spi.SPI_Mode = SPI_Mode_Master;
	spi.SPI_DataSize = SPI_DataSize_8b;
	spi.SPI_CPOL = SPI_CPOL_Low;
	spi.SPI_CPHA = SPI_CPHA_1Edge;
	spi.SPI_NSS = SPI_NSS_Soft;
	spi.SPI_BaudRatePrescaler = PORT_NOR_SPI_PRESCALER;
	spi.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_Init(SPI1, &spi);
	SPI_Cmd(SPI1, ENABLE);

	/* wake up in case an app left the chip in power-down */
	cs_low();
	xfer(CMD_RELEASE_PD);
	cs_high();
	hal_delay_ms(1);

	/* JEDEC: manufacturer, type, capacity = log2(bytes). 0x00/0xFF = no chip. */
	id = spi_nor_jedec_id();
	cap = (uint8_t)id;
	chip_size = 0;
	if ((id >> 16) != 0x00 && (id >> 16) != 0xFF && cap >= 16 && cap <= 32) {
		/* 3-byte addressing (power-up default, also on W25Q256): only the
		 * first 16 MB are reachable */
		chip_size = (cap > 24) ? (1UL << 24) : (1UL << cap);
	}
	return chip_size;
}

int spi_nor_read(uint32_t addr, void* buf, uint32_t len) {
	uint8_t* p = (uint8_t*)buf;

	if (!chip_size) {
		return SPI_NOR_ERR_ABSENT;
	}
	bus_fault = 0;
	cs_low();
	send_addr(CMD_READ_DATA, addr);
	while (len-- && !bus_fault) {
		*p++ = xfer(0xFF);
	}
	cs_high();
	return bus_fault ? SPI_NOR_ERR_TIMEOUT : SPI_NOR_OK;
}

int spi_nor_write(uint32_t addr, const void* data, uint32_t len) {
	const uint8_t* p = (const uint8_t*)data;

	if (!chip_size) {
		return SPI_NOR_ERR_ABSENT;
	}
	bus_fault = 0;
	while (len) {
		/* page program wraps inside a 256 B page: never cross a boundary */
		uint32_t n = SPI_NOR_PAGE_SIZE - (addr % SPI_NOR_PAGE_SIZE);
		if (n > len) {
			n = len;
		}
		write_enable();
		cs_low();
		send_addr(CMD_PAGE_PROGRAM, addr);
		for (uint32_t i = 0; i < n; i++) {
			xfer(p[i]);
		}
		cs_high();
		if (wait_ready(TIMEOUT_PROGRAM_MS) != SPI_NOR_OK || bus_fault) {
			return SPI_NOR_ERR_TIMEOUT;
		}
		addr += n;
		p += n;
		len -= n;
	}
	return SPI_NOR_OK;
}

int spi_nor_erase_sector(uint32_t addr) {
	if (!chip_size) {
		return SPI_NOR_ERR_ABSENT;
	}
	bus_fault = 0;
	write_enable();
	cs_low();
	send_addr(CMD_SECTOR_ERASE, addr);
	cs_high();
	if (wait_ready(TIMEOUT_ERASE_MS) != SPI_NOR_OK || bus_fault) {
		return SPI_NOR_ERR_TIMEOUT;
	}
	return SPI_NOR_OK;
}
