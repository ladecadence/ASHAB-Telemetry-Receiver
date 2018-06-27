#include "Arduino.h"
#define U8G2_USE_LARGE_FONTS
#include <U8g2lib.h>
#include <SPI.h>
#include <RH_RF95.h>
#include "logo.xbm"

#define DEBUG_SERIAL Serial

#define PKT_LEN		255

#define LED		25

// OLED
#define OLED_CLK 	15
#define OLED_DAT 	4
#define OLED_RST 	16

// LoRa
#define LORA_SS		18
#define LORA_INT	26

// INTERFACE
#define LINE8_0	8+1
#define LINE8_1	16+1
#define LINE8_2	24+1
#define LINE8_3 32+1
#define LINE8_4 40+1
#define LINE8_5 48+1
#define LINE8_6 56+1
#define LINE8_7 64+1

#define FONT_7t	u8g2_font_artossans8_8r
#define FONT_16n u8g2_font_logisoso16_tn

// Objects
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ OLED_CLK, /* data=*/ OLED_DAT, /* reset=*/ OLED_RST);
RH_RF95 rf95(LORA_SS, LORA_INT);

// receive buffer
uint8_t buf[PKT_LEN];

void setup()
{

	// initialize LED digital pin as an output.
	pinMode(LED, OUTPUT);

	// init SPI (ESP32)
	pinMode(18, OUTPUT);
	SPI.begin(5, 19, 27, 18);

	// init Display and show splash screen
	u8g2.begin();
	u8g2.drawXBM((128-logo_width)/2, (64-logo_height)/2, logo_width, logo_height, logo_bits);
	u8g2.sendBuffer();
	delay(3000);

	// simple interface
	u8g2.clearBuffer();
	u8g2.setFont(FONT_7t);
	u8g2.drawStr(2, LINE8_0,"ASHAB Receiver");
	u8g2.drawStr(2, LINE8_3,"Last RSSI:");
	u8g2.sendBuffer();

	// Serial port 
	DEBUG_SERIAL.begin(115200);

	// Init LoRa
	if (!rf95.init())
	{
		DEBUG_SERIAL.println("#RF95 Init failed");
		u8g2.drawStr(2, LINE8_1,"LoRa not found");
		u8g2.sendBuffer();
	}
	else
	{
		DEBUG_SERIAL.println("#RF95 Init OK");  
		u8g2.drawStr(2, LINE8_1,"LoRa OK");
		u8g2.sendBuffer();
	}

	// receiver, low power	
	rf95.setFrequency(868.5);
	rf95.setTxPower(5,false);
	memset(buf, 0, sizeof(buf));

}

void loop()
{
	if (rf95.available())
	{
		// Should be a message for us now   
		uint8_t len = sizeof(buf);
		if (rf95.recv(buf, &len))
		{
			// packet received
			digitalWrite(LED, HIGH);
			// send it through serial port
			DEBUG_SERIAL.write(buf, len);
			digitalWrite(LED, LOW);
			// clear buffer
			memset(buf, 0, sizeof(buf));

			// show RSSI
			u8g2.drawStr(2, LINE8_4, "           ");
			u8g2.setCursor(2, LINE8_4);
			u8g2.print(rf95.lastRssi());
			u8g2.sendBuffer();

		}
		else
		{
			// failed packet?
			DEBUG_SERIAL.println("#Recv failed");
		}
	}

}
