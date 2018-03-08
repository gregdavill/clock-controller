#include "sam.h"

#include "flipdot.h"
#include "Fonts/TomThumb.h"
#include "Fonts/apple2.h"
#include "stdio.h"

static flipdot fd(28, 7);

void small_delay()
{
	unsigned int j = 50000;
	while (j--) {
		unsigned int i = 4;
		while (i--) asm("nop");
	}
}

void invert_test()
{
	static bool bInvert;

	//	while (1) {
	//	if (bInvert ^= true) {
	//		fd.fillScreen(0);
	//	} else {
	fd.fillScreen(0);
	//	}

	fd.TransmitBuffer();
	small_delay();
	fd.TransmitBuffer();
	small_delay();
	//	}
}

int hours   = 21;
int minutes = 49;
int seconds = 0;

void text_example()
{
	fd.setFont(&TomThumb);

	static int  counter = 0;
	static char str[32];

	if (++seconds > 59) {
		seconds = 0;
		if (++minutes > 59) {
			minutes = 0;
			if (++hours > 24) {
				hours = 0;
			}
		}
	}

	fd.setFont(&apple2);

	fd.fillScreen(0);
	fd.setCursor(0, 6);
	fd.setTextWrap(false);

	// fd.print(str);

	fd.setCursor(0, 6);
	fd.write(hours / 10 % 10 + '0');

	fd.setCursor(6, 6);
	fd.write(hours % 10 + '0');

	if (seconds & 1) {
		fd.drawPixel(13, 1, 1);
		fd.drawPixel(13, 4, 1);
		fd.drawPixel(14, 2, 1);
		fd.drawPixel(14, 5, 1);

		fd.drawPixel(13, 2, 0);
		fd.drawPixel(13, 5, 0);
		fd.drawPixel(14, 1, 0);
		fd.drawPixel(14, 4, 0);
	} else {
		fd.drawPixel(13, 1, 0);
		fd.drawPixel(13, 4, 0);
		fd.drawPixel(14, 2, 0);
		fd.drawPixel(14, 5, 0);

		fd.drawPixel(13, 2, 1);
		fd.drawPixel(13, 5, 1);
		fd.drawPixel(14, 1, 1);
		fd.drawPixel(14, 4, 1);
	}

	fd.setCursor(15, 6);
	fd.write(minutes / 10 % 10 + '0');
	fd.setCursor(21, 6);
	fd.write(minutes % 10 + '0');

	fd.TransmitBuffer();
	small_delay();
	// fd.TransmitBuffer();
	small_delay();
}

int main()
{
	while (1) {
		text_example();

		small_delay();
	}
}
