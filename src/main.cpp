#define SERIAL_RX_BUFFER_SIZE 255

#include "Arduino.h"
#include <inttypes.h>
#define U8G2_USE_LARGE_FONTS
#include <U8g2lib.h>
#include <SPI.h>
#include <RH_RF95.h>
#include "logo.xbm"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>


#define DEBUG_SERIAL Serial

#define LORA_LEN    255
#define SERIAL_LEN	LORA_LEN + 8 // 255 max LoRa PKT + start + end

#define LED		25

// OLED
// Depending on boards can be CLK 5 or 15 and RST 23 or 16
#define OLED_CLK 	15
#define OLED_DAT 	4
#define OLED_RST 	16

// LoRa
#define LORA_SS		18
#define LORA_INT	26
#define LORA_FREQ   868.7

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
WiFiServer server(80);

// variables
uint8_t pkt_start[4] = {0xAA, 0x55, 0xAA, 0x55};
uint8_t pkt_end[4] = {0x33, 0xCC, 0x33, 0xCC};


const char* page_top =
"<html lang='en'>\n"
"	<head>\n"
"		<meta charset='utf-8'>\n"
"		<title>EKI Telemetry</title>\n"
"		<style>\n"
"			body {\n"
"				font-family: sans-serif;\n"
"				color: gray;\n"
"				font-size: 3em;\n"
"			}\n"
"		</style>\n"
"	</head>\n"
"\n"
"	<body>\n"
"		<h1>EKI Telemetry</h1>\n"
"		<span id='telemetry'>";

const char* page_bottom =
"</span>\n"
"		<script>\n"
"			// get data\n"
"			var telemetry = document.getElementById('telemetry').innerHTML;\n"
"			if (!telemetry.includes('NO TELEMETRY YET')) {\n"
"			// clear it\n"
"				document.getElementById('telemetry').innerHTML = '';\n"
"\n"
"				var fields = telemetry.split('/');\n"
"				var date = fields[8];\n"
"				document.getElementById('telemetry').innerHTML += 'Date: ' +\n"
"					date + '<br />\\n';\n"
"				var time = fields[9];\n"
"				document.getElementById('telemetry').innerHTML += 'Time: ' +\n"
"					time + '<br />\\n';\n"
"				var position = fields[10].split('=')[1];\n"
"				var lat = position.split(',')[0];\n"
"				var lon = position.split(',')[1];\n"
"				document.getElementById('telemetry').innerHTML += 'Position: ' +\n"
"					\"<a href='http://maps.google.com/maps?z=14&t=m&q=loc:\" +\n"
"					lat + '+' + lon + \"'> \" + position + \"</a> <br />\\n\";\n"
"				var altitude = fields[3].split('=')[1];\n"
"				document.getElementById('telemetry').innerHTML += 'Altitude: ' +\n"
"					altitude + ' m <br />\\n';\n"
"				var speed = fields[2];\n"
"				document.getElementById('telemetry').innerHTML += 'Speed: ' +\n"
"					altitude + ' kn <br />\\n';\n"
"				var heading = fields[1].split('O')[1];\n"
"				document.getElementById('telemetry').innerHTML += 'Heading: ' +\n"
"					heading + ' º <br />\\n';\n"
"				var bat = fields[4];\n"
"				document.getElementById('telemetry').innerHTML += 'Battery: ' +\n"
"					altitude + ' V <br />\\n';\n"
"			}\n"
"		</script>\n"
"	</body>\n"
"</html>\n";


// wifi
char ssid [9] = "ASHAB001";
char* password = "hirikizadi";

// lora receive buffer
uint8_t lora_buf[LORA_LEN];
uint8_t lora_len = sizeof(lora_buf);
// Serial buffer
uint8_t serial_buf[SERIAL_LEN];

// telemetry buffer (wifi)
uint8_t last_telem_buf[LORA_LEN];
uint8_t telem_len = 0;

// packet counter
uint16_t pkt_counter = 0;

void setup()
{

	// initialize LED digital pin as an output.
	pinMode(LED, OUTPUT);

	// WiFi
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();

    // and server
    server.begin();

    // prepare telemetry
    memset(last_telem_buf, 0, sizeof(last_telem_buf));
    memcpy(last_telem_buf, "NO TELEMETRY YET", 16);
    telem_len = 16;

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
	u8g2.drawStr(2, LINE8_0, "ASHAB Receiver");
	u8g2.drawStr(2, LINE8_4, "Packets:");
	u8g2.drawStr(2, LINE8_5, "Last RSSI:");
	u8g2.drawStr(2, LINE8_6, "Last Len:");
	u8g2.sendBuffer();

	// Serial port 
	DEBUG_SERIAL.begin(115200);
	//DEBUG_SERIAL.buffer(255);

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
		u8g2.drawStr(2, LINE8_2, myIP.toString().c_str());
		u8g2.sendBuffer();
	}

	// receiver, low power	
	rf95.setFrequency(LORA_FREQ);
	rf95.setTxPower(5,false);
	memset(lora_buf, 0, sizeof(lora_buf));
	memset(serial_buf, 0, sizeof(serial_buf));

}

void loop()
{
	if (rf95.available())
	{
	    lora_len = sizeof(lora_buf);
		// Should be a message for us now   
		if (rf95.recv(lora_buf, &lora_len))
		{
			// packet received
			digitalWrite(LED, HIGH);

			// check for telemetry
			if (lora_buf[0] == '$' && lora_buf[1] == '$') {
			    // clear
			    memset(last_telem_buf, 0, sizeof(last_telem_buf));
			    memcpy(last_telem_buf, lora_buf, lora_len);
			    telem_len = lora_len;
			}

			// create serial packet
			memcpy(serial_buf, pkt_start, 4);
			memcpy(serial_buf+4, lora_buf, lora_len);
			memcpy(serial_buf+4+lora_len, pkt_end, 4);

			// send it through serial port
			DEBUG_SERIAL.write(serial_buf, lora_len+8);
			digitalWrite(LED, LOW);
			// clear buffer
			memset(serial_buf, 0, sizeof(serial_buf));

            // update packet number
            pkt_counter++;
            u8g2.drawStr(2+8*11, LINE8_4, "         ");
            u8g2.setCursor(2+8*11, LINE8_4);
            u8g2.print(pkt_counter);

			// show RSSI
			u8g2.drawStr(2+8*11, LINE8_5, "         ");
			u8g2.setCursor(2+8*11, LINE8_5);
			u8g2.print(rf95.lastRssi());
			// show len
			u8g2.drawStr(2+8*11, LINE8_6, "         ");
			u8g2.setCursor(2+8*11, LINE8_6);
			u8g2.print(lora_len);
			u8g2.sendBuffer();

		}
		else
		{
			// failed packet?
			DEBUG_SERIAL.println("#Recv failed");
		}
	}

	// check for wifi clients
	WiFiClient client = server.available();   // listen for incoming clients

    if (client) {                             // if you get a client
        while (client.connected()) {
            while (client.available()) {             // if there's bytes to read from the client,
                client.read();
            }

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html; charset=utf-8");
            client.println();
            client.println(page_top);
            client.write(last_telem_buf, telem_len);
            client.println(page_bottom);

            client.println();
            break;
        }
        client.stop();
    }

}
