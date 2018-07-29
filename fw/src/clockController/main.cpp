#include "sam.h"

#include "flipdot.h"
#include "i2c.h"
#include "Fonts/TomThumb.h"
#include "Fonts/apple2.h"
#include "stdio.h"

static flipdot fd(28, 7);

void small_delay()
{
	unsigned int j = 20000;
	while (j--) asm("nop");
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

void text_example()
{
	int hours   = 21;
	int minutes = 49;
	int seconds = 0;

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

void printTime(uint8_t* buf)
{
	fd.setFont(&TomThumb);

	fd.setFont(&apple2);

	fd.fillScreen(0);
	fd.setCursor(0, 6);
	fd.setTextWrap(false);

	// fd.print(str);

	fd.setCursor(0, 6);
	fd.write((buf[3] >> 4) + '0');

	fd.setCursor(6, 6);
	fd.write((buf[3] & 0xF) + '0');

	if (buf[1] & 1) {
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
	fd.write((buf[2] >> 4) + '0');
	fd.setCursor(21, 6);
	fd.write((buf[2] & 0xF) + '0');

	fd.TransmitBuffer();
}

void print_number(uint8_t num)
{
	fd.setFont(&TomThumb);

	static int  counter = 0;
	static char str[32];

	fd.setFont(&apple2);

	fd.fillScreen(0);
	fd.setCursor(0, 6);
	fd.setTextWrap(false);

	fd.write(num / 100 % 10 + '0');
	fd.write(num / 10 % 10 + '0');
	fd.write(num / 1 % 10 + '0');

	fd.TransmitBuffer();
	small_delay();
	// fd.TransmitBuffer();
	small_delay();
}

int main()
{
	i2c i2c_rtc;

	i2c_master_packet pkt;
	uint8_t			  buf[7];
	pkt.address		= (0xD2 >> 1);
	pkt.data_length = 1;
	pkt.data		= buf;


	uint8_t old_seconds;

	i2c_rtc.i2c_master_enable();

	if (0) {
		/* Write Time */
		buf[0] = 1;
		buf[1] = 0;	// seconds
		buf[2] = 0x12; // minuets
		buf[3] = 0x20; // hours

		pkt.data_length = 4;
		pkt.data		= buf;
		status_code ret = i2c_rtc.i2c_master_write_packet_wait(&pkt);
	}
	small_delay();

	/* Enable Trickle charger on SuperCap */
	if (1) {
		/* Unlock Conf */
		buf[0] = 0x1F;
		buf[1] = 0x9D;	// Unlock

		pkt.data_length = 2;
		pkt.data		= buf;
		status_code ret = i2c_rtc.i2c_master_write_packet_wait(&pkt);


	small_delay();
		/* Unlock Conf */
		buf[0] = 0x20;
		buf[1] = 0xA5;	// Enable Trickle with Schottky + 3k

		pkt.data_length = 2;
		pkt.data		= buf;
		 ret = i2c_rtc.i2c_master_write_packet_wait(&pkt);
	}
	small_delay();

	while (1) {
		pkt.data_length = 1;
		pkt.data		= buf;
		buf[0]			= 0x00;
		status_code ret = i2c_rtc.i2c_master_write_packet_wait(&pkt);

		pkt.data_length = 4;
		pkt.data		= buf;
		ret				= i2c_rtc.i2c_master_read_packet_wait(&pkt);

		if(old_seconds != buf[1])
		{ 
		old_seconds = buf[1];

		printTime(buf);
		small_delay();
		}

	}
}
