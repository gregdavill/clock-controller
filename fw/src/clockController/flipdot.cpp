#include "flipdot.h"

#include "sam.h"

flipdot::flipdot(uint16_t w, uint16_t h)
	: Adafruit_GFX(w, h)
{
	/* Setup port */
	// PA22 -> PMUX C
	PORT->Group[0].PMUX[11].reg   = PORT_PMUX_PMUXE_C;
	PORT->Group[0].PINCFG[22].reg = PORT_PINCFG_PMUXEN;
	PORT->Group[0].DIRSET.reg	 = (1 << 22);

	PORT->Group[0].DIRSET.reg = (1 << 27);
	PORT->Group[0].OUTCLR.reg = (1 << 27);

	
	PORT->Group[0].DIRSET.reg = (1 << 2);
	PORT->Group[0].OUTCLR.reg = (1 << 2);

	/* Enable Power */
	PM->APBCMASK.bit.SERCOM1_ = 1;

	/* Setup clocks */
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_ID_SERCOM1_CORE;

	SERCOM1->USART.CTRLA.bit.SWRST = 1;
	while (SERCOM1->USART.CTRLA.reg & SERCOM_USART_CTRLA_SWRST)
		;

	/* Temporary variables  */
	uint64_t ratio			 = 0;
	uint64_t scale			 = 0;
	uint64_t baud_calculated = 0;

	/* Calculate the BAUD value */
	ratio			= ((16 * (uint64_t)9600) << 32) / 1000000UL;
	scale			= ((uint64_t)1 << 32) - ratio;
	baud_calculated = (65536 * scale) >> 32;

	SERCOM1->USART.CTRLA.reg = SERCOM_USART_CTRLA_DORD | SERCOM_USART_CTRLA_TXPO(0) | SERCOM_USART_CTRLA_RXPO(1)
							   | SERCOM_USART_CTRLA_MODE_USART_INT_CLK | SERCOM_USART_CTRLA_SAMPR(0);

	SERCOM1->USART.BAUD.reg  = baud_calculated;
	SERCOM1->USART.CTRLB.reg = SERCOM_USART_CTRLB_TXEN;

	SERCOM1->USART.CTRLA.reg |= SERCOM_USART_CTRLA_ENABLE;
}

void flipdot::drawPixel(int16_t x, int16_t y, uint16_t color)
{
	/* chock bounds */
	if (x < 0 || x >= this->width()) {
		return;
	}
	if (y < 0 || y >= this->height()) {
		return;
	}

	if (color) {
		buffer[x] |= (1 << y);
	} else {
		buffer[x] &= ~(1 << y);
	}
}

void flipdot::invertDisplay(bool value)
{
	bInvert = value;
}

inline void transmit(unsigned char i)
{
	SERCOM1->USART.DATA.reg = i;

	while (SERCOM1->USART.INTFLAG.bit.DRE == 0) {
	}
}

void flipdot::TransmitBuffer()
{
	PORT->Group[0].OUTSET.reg = (1 << 27);
	PORT->Group[0].OUTSET.reg = (1 << 2);

	int i = 3000;
	while(--i)
	{
		asm("nop");
	}

	transmit(0x80);
	transmit(0x83);
	transmit(0xFF);
	for (int i = 0; i < sizeof(buffer); i++) {
		if (bInvert) {
			transmit(buffer[i] ^ 0x7F);
		} else {
			transmit(buffer[i]);
		}
	}
	transmit(0x8F);
	
	while (SERCOM1->USART.INTFLAG.bit.DRE == 0) {
	}

	i = 3000;
	while(--i)
	{
		asm("nop");
	}
	
	PORT->Group[0].OUTCLR.reg = (1 << 27);
	PORT->Group[0].OUTCLR.reg = (1 << 2);
}