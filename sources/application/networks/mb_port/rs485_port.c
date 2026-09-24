#include "rs485_port.h"

#if defined (TASK_MBMASTER_EN) || defined (TASK_MBSLAVE_EN)

#include "stm32l1xx.h"
#include "io_cfg.h"
#include "sys_ctrl.h"

#define RS485_RX_BUF_SIZE		(256)

/* Mot nguoi ghi (ngat) - mot nguoi doc (vong chinh): head chi ngat ghi,
 * tail chi vong chinh ghi -> khong can khoa. */
static volatile uint8_t  rx_buf[RS485_RX_BUF_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;

void rs485_port_init(uint32_t baudrate) {
	USART_InitTypeDef USART_InitStructure;

	io_uart_rs485_cfg();			/* clock, GPIO AF, NVIC */
	io_rs485_dir_mode_output();
	io_rs485_dir_low();				/* mac dinh: nhan */

	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART_RS485, &USART_InitStructure);

	rx_head = rx_tail = 0;
	USART_ITConfig(USART_RS485, USART_IT_RXNE, ENABLE);
	USART_Cmd(USART_RS485, ENABLE);
}

void rs485_port_irq(void) {
	if (USART_GetITStatus(USART_RS485, USART_IT_RXNE) != RESET) {
		uint8_t c = (uint8_t)USART_ReceiveData(USART_RS485);	/* doc DR xoa RXNE */
		uint16_t next = (uint16_t)((rx_head + 1) % RS485_RX_BUF_SIZE);
		if (next != rx_tail) {	/* day thi bo byte - khung se hong CRC, master gui lai */
			rx_buf[rx_head] = c;
			rx_head = next;
		}
	}
	/* ORE cung sinh ngat RXNE: doc SR (GetFlagStatus) roi DR de xoa, khong
	 * thi ngat goi lai mai. */
	if (USART_GetFlagStatus(USART_RS485, USART_FLAG_ORE) != RESET) {
		(void)USART_ReceiveData(USART_RS485);
	}
}

int32_t rs485_port_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
	(void)arg;
	uint16_t n = 0;
	uint32_t t0 = sys_ctrl_millis();

	while (n < count) {
		if (rx_tail != rx_head) {
			buf[n++] = rx_buf[rx_tail];
			rx_tail = (uint16_t)((rx_tail + 1) % RS485_RX_BUF_SIZE);
			t0 = sys_ctrl_millis();		/* timeout tinh cho TUNG byte */
			continue;
		}
		if (byte_timeout_ms == 0) {
			break;
		}
		if (byte_timeout_ms > 0 && (uint32_t)(sys_ctrl_millis() - t0) >= (uint32_t)byte_timeout_ms) {
			break;
		}
		sys_ctrl_independent_watchdog_reset();
		sys_ctrl_soft_watchdog_reset();
	}
	return n;
}

int32_t rs485_port_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
	(void)arg; (void)byte_timeout_ms;

	io_rs485_dir_high();
	for (uint16_t i = 0; i < count; i++) {
		while (USART_GetFlagStatus(USART_RS485, USART_FLAG_TXE) == RESET);
		USART_SendData(USART_RS485, buf[i]);
	}
	/* Cho byte cuoi ra het day roi moi tra DIR, khong thi cat mat stop bit. */
	while (USART_GetFlagStatus(USART_RS485, USART_FLAG_TC) == RESET);
	io_rs485_dir_low();
	return count;
}

#endif /* TASK_MBMASTER_EN || TASK_MBSLAVE_EN */
