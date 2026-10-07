/**
 ******************************************************************************
 * @brief:  hal_rs485.h for STM32L151: USART2 (PA2 TX, PA3 RX) + direction pin
 *          (port_cfg.h). RX: interrupt -> ring. TX: polled, the direction pin
 *          is released when the last stop bit is out (TC), not when the last
 *          byte entered the shift register.
 ******************************************************************************
**/

#include "stm32l1xx.h"
#include "stm32l1xx_conf.h"

#include "hal_rs485.h"
#include "port_cfg.h"

/* one writer (ISR: head), one reader (main loop: tail): no lock needed */
static volatile uint8_t rx_buf[PORT_RS485_RX_BUF];
static volatile uint16_t rx_head, rx_tail;
static volatile uint32_t rx_bytes;

void usart2_irq_handler(void) {
	if (USART2->SR & (USART_SR_RXNE | USART_SR_ORE)) {
		uint8_t c = (uint8_t)USART2->DR;	/* reading DR clears RXNE and ORE */
		uint16_t next = (uint16_t)((rx_head + 1) & (PORT_RS485_RX_BUF - 1));

		rx_bytes++;
		/* full: drop the byte - the frame fails its CRC and the master repeats */
		if (next != rx_tail) {
			rx_buf[rx_head] = c;
			rx_head = next;
		}
	}
}

void hal_rs485_init(uint32_t baudrate) {
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef uart;
	NVIC_InitTypeDef nvic;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | PORT_RS485_DIR_CLK, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	/* direction: output, low = receive */
	GPIO_StructInit(&gpio);
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	gpio.GPIO_Pin = PORT_RS485_DIR_PIN;
	GPIO_Init(PORT_RS485_DIR_PORT, &gpio);
	PORT_RS485_DIR_PORT->BSRRH = PORT_RS485_DIR_PIN;

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;		/* RX idles high while the transceiver output is off */
	gpio.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_Init(GPIOA, &gpio);

	USART_Cmd(USART2, DISABLE);
	USART_StructInit(&uart);
	uart.USART_BaudRate = baudrate;
	uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &uart);

	rx_head = rx_tail = 0;
	rx_bytes = 0;

	nvic.NVIC_IRQChannel = USART2_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 2;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART2, ENABLE);
}

int hal_rs485_getc(void) {
	int c;

	if (rx_tail == rx_head) {
		return -1;
	}
	c = rx_buf[rx_tail];
	rx_tail = (uint16_t)((rx_tail + 1) & (PORT_RS485_RX_BUF - 1));
	return c;
}

uint32_t hal_rs485_rx_bytes(void) {
	return rx_bytes;
}

void hal_rs485_write(const uint8_t* data, uint32_t len) {
	if (len == 0) {
		return;
	}
	PORT_RS485_DIR_PORT->BSRRL = PORT_RS485_DIR_PIN;
	while (len--) {
		while (!(USART2->SR & USART_SR_TXE)) {
		}
		USART2->DR = *data++;
	}
	/* TC, not TXE: the stop bit of the last byte must be on the wire */
	while (!(USART2->SR & USART_SR_TC)) {
	}
	PORT_RS485_DIR_PORT->BSRRH = PORT_RS485_DIR_PIN;
}
