#ifndef UBUNTU
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <NTPClient.h>
//#include "SPIFFS.h"
#include <Wire.h>
#include <SPI.h>
#include <Pinger.h>

#include "OneWireNg_CurrentPlatform.h"
#include "RTClib.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#else
#include "ESP32sim_ubuntu.h"
#endif

#include "jimlib.h"

//#define I2C
#ifdef I2C
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

void snow(); 
#endif // I2C

RTC_DS3231 rtc;
OneWireNg_CurrentPlatform ow(14, false);


const int ledPin = 13;
const int relayPin = 12;
const int buttonPin = 0;

DigitalButton button(buttonPin);
LongShortFilter butFilt(1000,500);

WiFiUDP udp;
WiFiManager wman;
NTPClient ntp(udp);
int i2cDeviceCount = 0;

Pinger ping; 
int secondsSincePing = 0;
void testScreen(); 

void setup() {
	pinMode(ledPin, OUTPUT);
	digitalWrite(ledPin, 0);

	Serial.begin(115200);
	Serial.println("Booting");

	pinMode(relayPin, OUTPUT);
	pinMode(buttonPin, INPUT);

  // turn power on 
  digitalWrite(relayPin, 1);
  digitalWrite(ledPin, 0);

	WiFi.disconnect(true);
	WiFi.mode(WIFI_STA);
	WiFi.begin("ChloeNet", "niftyprairie7");
	
	//wman.autoConnect();  // never has been reliable don't be tricked 

	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
    digitalWrite(ledPin, 1);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin("ChloeNet", "niftyprairie7");
    //ESP.restart();
	}

	ArduinoOTA.begin();
	//ntp.begin();
	//ntp.setUpdateInterval(10 /*minutes*/* 60 * 1000); 
	//ntp.update();

#ifdef I2C
	Wire.begin(3, 1);
	display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
	display.display();

	display.clearDisplay();

	display.setTextSize(3);      // Normal 1:1 pixel scale
	display.setTextColor(SSD1306_WHITE); // Draw white text
	display.setCursor(0, 0);     // Start at top-left corner
	display.cp437(true);         // Use full 256 char 'Code Page 437' font

	display.setCursor(10, 0);
	display.println(F("scroll"));
	display.display();
#endif 

  ping.OnReceive([](const PingerResponse& response) {
    if (response.ReceivedResponse) {
      secondsSincePing = 0;
      digitalWrite(ledPin, 0); //light back on  
      return true;
    }
  });

}

EggTimer sec(1000), slowBlink(20000), minute(60000);
uint64_t lastMillis;

int onTime = -1, offTime = -1;
bool lastOn = false;
std::string disp = "";
int seconds = 0;

void loop() {
	ArduinoOTA.begin();
	ArduinoOTA.handle();

	bool secondTick = sec.tick();

#if 1
  // TMP: quick hack for furnace, just power cycle every 15 minutes, don't do anything else 
  if (0 && secondTick) {
    static int secs = 0;
    secs = (secs + 1) % (15 * 60);
    if (secs > 60 && secs < 70) {
      digitalWrite(relayPin, 0);
      digitalWrite(ledPin, 1);
    } else { 
      digitalWrite(relayPin, 1);
      digitalWrite(ledPin, 0);
    }
#ifdef I2C
    display.setTextSize(4);      // Normal 1:1 pixel scale
    display.setCursor(10, 0);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text		
    display.printf(" %03d ", secs);
    display.display();
#endif
  }
  if (secondTick && butFilt.inProgress() == false)  {
    digitalWrite(relayPin, 1);
    digitalWrite(ledPin, 0);        
		udp.beginPacket("255.255.255.255", 9000);
		char b[128];
		snprintf(b, sizeof(b), "%d %s " __FILE__ " " GIT_VERSION  " 0x%08x ping:%03d\n", (int)(millis() / 1000), WiFi.localIP().toString().c_str(), 
			ESP.getChipId(), secondsSincePing);
  	udp.write((const uint8_t *)b, strlen(b));
		udp.endPacket();
    Serial.print(b);
    seconds++;
    if (seconds % 10 == 0) {
      ping.Ping(IPAddress(4,2,2,1), 2, 1000);
      digitalWrite(ledPin, 1); //blink light off, turned back on in ping recv callback 
    }

    secondsSincePing++;
    if (secondsSincePing > 3 * 60) { 
      secondsSincePing = 0;
      digitalWrite(relayPin, 0);
      digitalWrite(ledPin, 1);        
    }
	}




  return;
#endif 

	button.check();
	butFilt.check(button.duration());
	int now = ((ntp.getHours() - 8 + 24) % 24) * 3600 + ntp.getMinutes() * 60 + ntp.getSeconds();
	bool on = false;
	if (onTime >= 0) { 
		if (offTime > onTime && now >= onTime && now < offTime) on = true;
		if (offTime < onTime && (now >= onTime || now < offTime)) on = true;
	} 
	
	if (lastOn == true && on == false) {
		onTime = offTime = -1;
	}

#ifdef I2C	
	if (butFilt.inProgress() && (onTime < 0 || butFilt.inProgressCount() > 1)) { 
		display.setTextSize(4);      // Normal 1:1 pixel scale
		display.setCursor(10, 0);
		display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text		
		display.printf(" %d ", butFilt.inProgressCount());
		display.display();
	}
#endif
		
		
	if (butFilt.newEvent()) {
		if (onTime >= 0 && butFilt.wasLong == false && butFilt.wasCount == 1 /*on or armed and a single short press?  Turn off*/) { 
			on = false;
			onTime = offTime = -1;
		} else {  
			if (butFilt.wasLong) {
				if (on == true) { 
					ESP.restart();
				}
				onTime = now;
				on = true;
				offTime = (onTime + 4 * 3600) % (24 * 3600);
			} else { 
				int onHour = butFilt.wasCount;
				on = lastOn = false;
				onTime = ((onHour) * 3600) % (24 * 3600);
				offTime = (onTime + 4 * 3600) % (24 * 3600);
			}
		}
	}
	
	if (secondTick && butFilt.inProgress() == false)  {
		float t = 0;
    std::vector<DsTempData> tempData;
#if 0
		tempData = readTemps(&ow);
		if (tempData.size() > 0) 
			t = tempData[0].degC;
#endif 		
		udp.beginPacket("255.255.255.255", 9000);
		char b[128];
		snprintf(b, sizeof(b), "%d %s " __FILE__ " " GIT_VERSION  " 0x%08x %s %f %f %f %d t:%d,%f i2c:%d %d\n", (int)(millis() / 1000), WiFi.localIP().toString().c_str(), 
			ESP.getChipId(), ntp.getFormattedTime().c_str(), onTime/3600.0, offTime/3600.0, now/3600.0, (int)on, (int)tempData.size(), t, i2cDeviceCount,
			(int)ntp.getEpochTime());
		udp.write((const uint8_t *)b, strlen(b));
		udp.endPacket();
    Serial.print(b);
	}

	if (on) {
		disp = "ON";
		digitalWrite(ledPin, 0);
	} else if(onTime >= 0) {
		if (digitalRead(ledPin) == 0) {
			disp = strfmt("%d:%02d", (int)floor(onTime / 3600), (int)floor((onTime % 3600) / 60));
		} else { 
			disp = "";
		}
		if (secondTick) { 
			digitalWrite(ledPin, !digitalRead(ledPin));
		}
	} else { 
		disp = "OFF";
		digitalWrite(ledPin, 1);
	}	
	digitalWrite(relayPin, on);

#ifdef I2C
	if (ntp.getEpochTime() > 10000000) { // check for valid time
		snow();
	}
	if (butFilt.inProgress() == false) { 
		display.setCursor(10, 0);
		display.setTextSize(3);   
		display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Draw 'normal' text		
		display.print(disp.c_str());
		display.display();
	}
	if (0) {
		display.setCursor(90, 22);
		display.setTextSize(1);     
		display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Draw 'normal' text		
		display.printf("%02d:%02d", (int)floor(now/3600), (int)floor((now % 3600) / 60));
		display.display();
	}
#endif
	if (slowBlink.tick()) {
		ntp.update();
		digitalWrite(ledPin, 0);
	}

	lastOn = on;
	lastMillis = millis();
	ArduinoOTA.handle();
	delay(10);
}

#ifdef I2C
static const unsigned char PROGMEM logo_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };


void testdrawline() {
  int16_t i;

  display.clearDisplay(); // Clear display buffer

  for(i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, SSD1306_WHITE);
    display.display(); // Update screen with each newly-drawn line
    delay(1);
  }
  for(i=0; i<display.height(); i+=4) {
    display.drawLine(0, 0, display.width()-1, i, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();

  for(i=0; i<display.width(); i+=4) {
    display.drawLine(0, display.height()-1, i, 0, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  for(i=display.height()-1; i>=0; i-=4) {
    display.drawLine(0, display.height()-1, display.width()-1, i, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();

  for(i=display.width()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, i, 0, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  for(i=display.height()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, 0, i, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();

  for(i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, SSD1306_WHITE);
    display.display();
    delay(1);
  }
  for(i=0; i<display.width(); i+=4) {
    display.drawLine(display.width()-1, 0, i, display.height()-1, SSD1306_WHITE);
    display.display();
    delay(1);
  }

  delay(2000); // Pause for 2 seconds
}

void testdrawrect(void) {
  display.clearDisplay();

  for(int16_t i=0; i<display.height()/2; i+=2) {
    display.drawRect(i, i, display.width()-2*i, display.height()-2*i, SSD1306_WHITE);
    display.display(); // Update screen with each newly-drawn rectangle
    delay(1);
  }

  delay(2000);
}

void testfillrect(void) {
  display.clearDisplay();

  for(int16_t i=0; i<display.height()/2; i+=3) {
    // The INVERSE color is used so rectangles alternate white/black
    display.fillRect(i, i, display.width()-i*2, display.height()-i*2, SSD1306_INVERSE);
    display.display(); // Update screen with each newly-drawn rectangle
    delay(1);
  }

  delay(2000);
}

void testdrawcircle(void) {
  display.clearDisplay();

  for(int16_t i=0; i<max(display.width(),display.height())/2; i+=2) {
    display.drawCircle(display.width()/2, display.height()/2, i, SSD1306_WHITE);
    display.display();
    delay(1);
  }

  delay(2000);
}

void testfillcircle(void) {
  display.clearDisplay();

  for(int16_t i=max(display.width(),display.height())/2; i>0; i-=3) {
    // The INVERSE color is used so circles alternate white/black
    display.fillCircle(display.width() / 2, display.height() / 2, i, SSD1306_INVERSE);
    display.display(); // Update screen with each newly-drawn circle
    delay(1);
  }

  delay(2000);
}

void testdrawroundrect(void) {
  display.clearDisplay();

  for(int16_t i=0; i<display.height()/2-2; i+=2) {
    display.drawRoundRect(i, i, display.width()-2*i, display.height()-2*i,
      display.height()/4, SSD1306_WHITE);
    display.display();
    delay(1);
  }

  delay(2000);
}

void testfillroundrect(void) {
  display.clearDisplay();

  for(int16_t i=0; i<display.height()/2-2; i+=2) {
    // The INVERSE color is used so round-rects alternate white/black
    display.fillRoundRect(i, i, display.width()-2*i, display.height()-2*i,
      display.height()/4, SSD1306_INVERSE);
    display.display();
    delay(1);
  }

  delay(2000);
}

void testdrawtriangle(void) {
  display.clearDisplay();

  for(int16_t i=0; i<max(display.width(),display.height())/2; i+=5) {
    display.drawTriangle(
      display.width()/2  , display.height()/2-i,
      display.width()/2-i, display.height()/2+i,
      display.width()/2+i, display.height()/2+i, SSD1306_WHITE);
    display.display();
    delay(1);
  }

  delay(2000);
}

void testfilltriangle(void) {
  display.clearDisplay();

  for(int16_t i=max(display.width(),display.height())/2; i>0; i-=5) {
    // The INVERSE color is used so triangles alternate white/black
    display.fillTriangle(
      display.width()/2  , display.height()/2-i,
      display.width()/2-i, display.height()/2+i,
      display.width()/2+i, display.height()/2+i, SSD1306_INVERSE);
    display.display();
    delay(1);
  }

  delay(2000);
}

void testdrawchar(void) {
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Not all the characters will fit on the display. This is normal.
  // Library will draw what it can and the rest will be clipped.
  for(int16_t i=0; i<256; i++) {
    if(i == '\n') display.write(' ');
    else          display.write(i);
  }

  display.display();
  delay(2000);
}

void testdrawstyles(void) {
  display.clearDisplay();

  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.println(F("Hello, world!"));

  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
  display.println(3.141592);

  display.setTextSize(2);             // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.print(F("0x")); display.println(0xDEADBEEF, HEX);

  display.display();
  delay(2000);
}

void testscrolltext(void) {
  display.clearDisplay();

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F("scroll"));
  display.display();      // Show initial text
  delay(100);

  // Scroll in various directions, pausing in-between:
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrolldiagright(0x00, 0x07);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x07);
  delay(2000);
  display.stopscroll();
  delay(1000);
}

void testdrawbitmap(void) {
  display.clearDisplay();

  display.drawBitmap(
    (display.width()  - LOGO_WIDTH ) / 2,
    (display.height() - LOGO_HEIGHT) / 2,
    logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  display.display();
  delay(1000);
}

#define XPOS   0 // Indexes into the 'icons' array in function below
#define YPOS   1
#define DELTAY 2

void testanimate(const uint8_t *bitmap, uint8_t w, uint8_t h) {
	int8_t f;
	static int8_t icons[NUMFLAKES][3];
	static int first = 1;

	if (first) { 
		// Initialize 'snowflake' positions
		for(f=0; f< NUMFLAKES; f++) {
			icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
			icons[f][YPOS]   = -LOGO_HEIGHT;
			icons[f][DELTAY] = random(1, 6);
		}
		first = 0;
	} 

	display.clearDisplay(); // Clear the display buffer
	// Draw each snowflake:
	for(f=0; f< NUMFLAKES; f++) {
		display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, SSD1306_WHITE);
	}
	display.display(); // Show the display buffer on the screen

	// Then update coordinates of each flake...
	for(f=0; f< NUMFLAKES; f++) {
	  icons[f][YPOS] += icons[f][DELTAY];
	  // If snowflake is off the bottom of the screen...
	  if (icons[f][YPOS] >= display.height()) {
		// Reinitialize to a random position, just off the top
		icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
		icons[f][YPOS]   = -LOGO_HEIGHT;
		icons[f][DELTAY] = random(1, 6);
	  }
	}


}

void snow() { 
	  testanimate(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
}

void testScreen() {

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    //for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // Draw a single pixel in white
  display.drawPixel(10, 10, SSD1306_WHITE);

  // Show the display buffer on the screen. You MUST call display() after
  // drawing commands to make them visible on screen!
  display.display();


  testdrawchar();      // Draw characters of the default font

  testdrawstyles();    // Draw 'stylized' characters

  testscrolltext();    // Draw scrolling text

  testdrawbitmap();    // Draw a small bitmap image


}

void foo() {  
  delay(2000);
  // display.display() is NOT necessary after every single drawing command,
  // unless that's what you want...rather, you can batch up a bunch of
  // drawing operations and then update the screen all at once by calling
  // display.display(). These examples demonstrate both approaches...

  testdrawline();      // Draw many lines

  testdrawrect();      // Draw rectangles (outlines)

  testfillrect();      // Draw rectangles (filled)

  testdrawcircle();    // Draw circles (outlines)

  testfillcircle();    // Draw circles (filled)

  testdrawroundrect(); // Draw rounded rectangles (outlines)

  testfillroundrect(); // Draw rounded rectangles (filled)

  testdrawtriangle();  // Draw triangles (outlines)

  testfilltriangle();  // Draw triangles (filled)

  testdrawchar();      // Draw characters of the default font

  testdrawstyles();    // Draw 'stylized' characters

  testscrolltext();    // Draw scrolling text

  testdrawbitmap();    // Draw a small bitmap image

  // Invert and restore display, pausing in-between
  display.invertDisplay(true);
  delay(1000);
  display.invertDisplay(false);
  delay(1000);

  testanimate(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT); // Animate bitmaps
}
#endif


#ifdef UBUNTU
class ESP32sim_winglevlr : public ESP32sim_Module {
  void setup() override {
      bm.addPress(buttonPin, 10, 1, true); 
  };
} csim; 
#endif