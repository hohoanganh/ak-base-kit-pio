/**
 ******************************************************************************
 * @brief:  demo/kit.h for the AK Base Kit 3I0 (STM32L151CB).
 *
 *   OLED SSD1309 128x64   I2C 0x3C, bit-banged: SCL PB13, SDA PB12, RES PA15
 *   buttons               PB3, PC13, PB4 - to GND, internal pull-up
 *   buzzer (passive)      PB0 = TIM3_CH3
 *   RTC PCF85063          I2C 0x51, bit-banged on the I2C1 pins: SCL PB6, SDA PB7
 *
 *  PA15, PB3 and PB4 are JTAG pins after reset; as plain GPIO they only cost
 *  JTAG, SWD (PA13/PA14) keeps working.
 ******************************************************************************
**/

#include "stm32l1xx.h"
#include "stm32l1xx_conf.h"

#include "hal.h"
#include "kit.h"

/*----------------------------------------------------------------------------
 * bit-banged I2C master (open drain pins, about 400 kHz at 32 MHz)
 *--------------------------------------------------------------------------*/
typedef struct {
	GPIO_TypeDef* port;
	uint16_t scl;
	uint16_t sda;
} i2c_bus_t;

static const i2c_bus_t bus_lcd = { GPIOB, GPIO_Pin_13, GPIO_Pin_12 };
static const i2c_bus_t bus_rtc = { GPIOB, GPIO_Pin_6, GPIO_Pin_7 };

#define LCD_ADDR		(0x3C)
#define RTC_ADDR		(0x51)

static inline void i2c_delay(void) {
	for (volatile uint32_t i = 0; i < 3; i++) {
	}
}

static inline void scl_hi(const i2c_bus_t* b) { b->port->BSRRL = b->scl; i2c_delay(); }
static inline void scl_lo(const i2c_bus_t* b) { b->port->BSRRH = b->scl; i2c_delay(); }
static inline void sda_hi(const i2c_bus_t* b) { b->port->BSRRL = b->sda; }
static inline void sda_lo(const i2c_bus_t* b) { b->port->BSRRH = b->sda; }

static void i2c_pins(const i2c_bus_t* b) {
	GPIO_InitTypeDef gpio;

	b->port->BSRRL = b->scl | b->sda;		/* released = high before becoming outputs */
	GPIO_StructInit(&gpio);
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_OType = GPIO_OType_OD;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	gpio.GPIO_Speed = GPIO_Speed_10MHz;
	gpio.GPIO_Pin = b->scl | b->sda;
	GPIO_Init(b->port, &gpio);
}

static void i2c_start(const i2c_bus_t* b) {
	sda_hi(b);
	scl_hi(b);
	sda_lo(b);
	i2c_delay();
	scl_lo(b);
}

static void i2c_stop(const i2c_bus_t* b) {
	sda_lo(b);
	scl_hi(b);
	sda_hi(b);
	i2c_delay();
}

/* returns 1 if the device acknowledged */
static uint8_t i2c_tx(const i2c_bus_t* b, uint8_t v) {
	uint8_t ack;

	for (uint8_t i = 0; i < 8; i++, v <<= 1) {
		if (v & 0x80) {
			sda_hi(b);
		}
		else {
			sda_lo(b);
		}
		scl_hi(b);
		scl_lo(b);
	}
	sda_hi(b);
	scl_hi(b);
	ack = !(b->port->IDR & b->sda);
	scl_lo(b);
	return ack;
}

static uint8_t i2c_rx(const i2c_bus_t* b, uint8_t send_ack) {
	uint8_t v = 0;

	sda_hi(b);
	for (uint8_t i = 0; i < 8; i++) {
		scl_hi(b);
		v = (uint8_t)((v << 1) | ((b->port->IDR & b->sda) ? 1 : 0));
		scl_lo(b);
	}
	if (send_ack) {
		sda_lo(b);
	}
	scl_hi(b);
	scl_lo(b);
	sda_hi(b);
	return v;
}

/*----------------------------------------------------------------------------
 * OLED SSD1309
 *--------------------------------------------------------------------------*/
/* init sequence of U8g2's SSD1309 128x64 "noname0" (the type this module
 * runs with in the kit's test firmware), page addressing mode */
static const uint8_t lcd_init_seq[] = {
	0xAE,			/* display off */
	0xD5, 0xA0,		/* clock divide / oscillator */
	0x40,			/* start line 0 */
	0x20, 0x02,		/* page addressing mode */
	0xA1,			/* segment remap */
	0xC8,			/* scan direction */
	0xDA, 0x12,		/* COM pins */
	0x81, 0x6F,		/* contrast */
	0xD9, 0xD3,		/* pre-charge */
	0xDB, 0x20,		/* VCOMH */
	0x2E,			/* scroll off */
	0xA4,			/* show RAM */
	0xA6,			/* not inverted */
	0xAF,			/* display on */
};

static uint8_t lcd_cmds(const uint8_t* cmd, uint8_t n) {
	uint8_t ok;

	i2c_start(&bus_lcd);
	ok = i2c_tx(&bus_lcd, LCD_ADDR << 1);
	ok &= i2c_tx(&bus_lcd, 0x00);		/* control byte: commands follow */
	while (n--) {
		ok &= i2c_tx(&bus_lcd, *cmd++);
	}
	i2c_stop(&bus_lcd);
	return ok;
}

uint8_t kit_lcd_write_page(uint8_t page, const uint8_t* data) {
	uint8_t pos[3] = { (uint8_t)(0xB0 | (page & 7)), 0x00, 0x10 };	/* page, column 0 */
	uint8_t ok;

	if (!lcd_cmds(pos, sizeof(pos))) {
		return 0;
	}
	i2c_start(&bus_lcd);
	ok = i2c_tx(&bus_lcd, LCD_ADDR << 1);
	ok &= i2c_tx(&bus_lcd, 0x40);		/* control byte: data follows */
	for (uint8_t x = 0; x < KIT_LCD_W; x++) {
		ok &= i2c_tx(&bus_lcd, data[x]);
	}
	i2c_stop(&bus_lcd);
	return ok;
}

/*----------------------------------------------------------------------------
 * buttons, buzzer
 *--------------------------------------------------------------------------*/
uint8_t kit_buttons(void) {
	uint8_t m = 0;

	if (!(GPIOB->IDR & GPIO_Pin_3)) {
		m |= KIT_BTN_1;
	}
	if (!(GPIOC->IDR & GPIO_Pin_13)) {
		m |= KIT_BTN_2;
	}
	if (!(GPIOB->IDR & GPIO_Pin_4)) {
		m |= KIT_BTN_3;
	}
	return m;
}

/* TIM3 runs from PCLK1 = 32 MHz, prescaled to 1 MHz: ARR = period in us */
void kit_buzzer(uint16_t freq_hz) {
	uint32_t period;

	if (freq_hz < 20) {
		TIM3->CR1 &= (uint16_t)~TIM_CR1_CEN;
		TIM3->CCR3 = 0;
		TIM3->EGR = TIM_EGR_UG;			/* output low, not wherever the wave stopped */
		return;
	}
	period = 1000000UL / freq_hz;
	TIM3->ARR = (uint16_t)(period - 1);
	TIM3->CCR3 = (uint16_t)(period / 2);
	TIM3->EGR = TIM_EGR_UG;
	TIM3->CR1 |= TIM_CR1_CEN;
}

/*----------------------------------------------------------------------------
 * RTC PCF85063: seconds, minutes, hours in BCD from register 0x04
 *--------------------------------------------------------------------------*/
static uint8_t bcd2dec(uint8_t v) {
	return (uint8_t)((v >> 4) * 10 + (v & 0x0F));
}

static uint8_t dec2bcd(uint8_t v) {
	return (uint8_t)(((v / 10) << 4) | (v % 10));
}

uint8_t kit_rtc_get(uint8_t* hh, uint8_t* mm, uint8_t* ss) {
	uint8_t ok;
	uint8_t s, m, h;

	i2c_start(&bus_rtc);
	ok = i2c_tx(&bus_rtc, RTC_ADDR << 1);
	ok &= i2c_tx(&bus_rtc, 0x04);
	i2c_start(&bus_rtc);
	ok &= i2c_tx(&bus_rtc, (RTC_ADDR << 1) | 1);
	s = i2c_rx(&bus_rtc, 1);
	m = i2c_rx(&bus_rtc, 1);
	h = i2c_rx(&bus_rtc, 0);
	i2c_stop(&bus_rtc);

	if (!ok) {
		return 0;
	}
	*ss = bcd2dec(s & 0x7F);
	*mm = bcd2dec(m & 0x7F);
	*hh = bcd2dec(h & 0x3F);
	return (*ss < 60 && *mm < 60 && *hh < 24);
}

uint8_t kit_rtc_set(uint8_t hh, uint8_t mm, uint8_t ss) {
	uint8_t ok;

	i2c_start(&bus_rtc);
	ok = i2c_tx(&bus_rtc, RTC_ADDR << 1);
	ok &= i2c_tx(&bus_rtc, 0x04);
	ok &= i2c_tx(&bus_rtc, dec2bcd(ss));	/* writing seconds also clears the "clock stopped" flag */
	ok &= i2c_tx(&bus_rtc, dec2bcd(mm));
	ok &= i2c_tx(&bus_rtc, dec2bcd(hh));
	i2c_stop(&bus_rtc);
	return ok;
}

/*----------------------------------------------------------------------------
 * init
 *--------------------------------------------------------------------------*/
uint8_t kit_init(void) {
	GPIO_InitTypeDef gpio;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	/* buttons */
	GPIO_StructInit(&gpio);
	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	gpio.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4;
	GPIO_Init(GPIOB, &gpio);
	gpio.GPIO_Pin = GPIO_Pin_13;
	GPIO_Init(GPIOC, &gpio);

	/* buzzer: PB0 = TIM3_CH3, PWM mode 1, 1 MHz time base, stopped */
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_TIM3);
	GPIO_StructInit(&gpio);
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_Pin = GPIO_Pin_0;
	GPIO_Init(GPIOB, &gpio);
	TIM3->CR1 = 0;
	TIM3->PSC = (uint16_t)(SystemCoreClock / 1000000UL - 1);
	TIM3->CCMR2 = TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3PE;
	TIM3->CCER = TIM_CCER_CC3E;
	kit_buzzer(0);

	/* display reset: PA15 high - low - high */
	GPIO_StructInit(&gpio);
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_Pin = GPIO_Pin_15;
	GPIO_Init(GPIOA, &gpio);
	GPIOA->BSRRL = GPIO_Pin_15;
	hal_delay_ms(5);
	GPIOA->BSRRH = GPIO_Pin_15;
	hal_delay_ms(5);
	GPIOA->BSRRL = GPIO_Pin_15;
	hal_delay_ms(10);

	i2c_pins(&bus_lcd);
	i2c_pins(&bus_rtc);

	return lcd_cmds(lcd_init_seq, sizeof(lcd_init_seq));
}
