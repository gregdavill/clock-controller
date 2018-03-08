#pragma once

#include "Adafruit_GFX.h"

class flipdot : public Adafruit_GFX
{
protected:
public:
	flipdot(uint16_t w, uint16_t h);

	// Required Non-Transaction
	void drawPixel(int16_t x, int16_t y, uint16_t color);

	void TransmitBuffer();
	void invertDisplay(bool value);

protected:
	unsigned char buffer[28];
	bool		  bInvert = false;
};
