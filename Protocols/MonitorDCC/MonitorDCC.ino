/*
 Name:		MonitorDCC.ino
 Created:	8/16/2021 9:47:19 PM
 Author:	Rob Antonisse

 MonitorDCC een project voor algemeen monitoring van de DCC poort en een converter die 
 het ontvangen command op logische poorten zet en toont.



*/

//libraries


//#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

//constructers
Adafruit_SSD1306 dp(128, 32, &Wire); // , -1);

void setup() {
	Serial.begin(9600);
	if (dp.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
		Serial.println(F("DP ok!"));
	}
	delay(1000);
	//test tekst
	dp.clearDisplay();
	dp.setTextSize(1);
	dp.setTextColor(WHITE);
	dp.setCursor(0, 0);
	// Display static text
	dp.println("Dit is de 1e regel");
	dp.println("en zowaar de 2e regel");
	dp.println("zeker de derde");
	dp.print("enne de vierde ook...");
	dp.display();

}


void loop() {
  
}
