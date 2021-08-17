/*
 Name:		MonitorDCC.ino
 Created:	8/16/2021 9:47:19 PM
 Author:	Rob Antonisse

 MonitorDCC een project voor algemeen monitoring van de DCC poort en een converter die 
 het ontvangen command op logische poorten zet en toont.



*/
char version[] = "V 1.01"; //Openingstekst versie aanduiding


//libraries
//#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

//constanten

//constructers
Adafruit_SSD1306 dp(128, 64, &Wire); // , -1);

//variabelen
byte SW_status=15; //holds the last switch status, start as B00001111;
unsigned long slowtimer;


void setup() {
	//start processen
	Serial.begin(9600);
	dp.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	//poorten	
	DDRC &=~B00001111;
	PORTC |=B00001111;

	//Openings tekst
	dp.clearDisplay();
	dp.setTextSize(1);
	dp.setTextColor(WHITE);
	dp.setCursor(10, 5);
	// Display static text
	dp.println(F("www.wisselmotor.nl"));
	dp.setTextSize(2);
	dp.setCursor(6, 25);
	dp.println(F("MonitorDCC"));
	dp.setTextSize(1);
	dp.setCursor(85, 55);
	dp.print(version);
	dp.display();


}
void loop() {
  //slow events
	if (millis() - slowtimer > 20) {
		slowtimer = millis();
		SW_exe();
	}
}
//schakelaars
void SW_exe() {
	byte changed = 0; byte read = 0;
	read = PINC;
	read = read << 4;
	read = read >> 4; //isoleer bit0~bit3	
	changed = read^SW_status;
	if (changed > 0) {
	
		for (byte i; i < 4; i++) {
			if (changed & (1 << i)) { //status switch i changed
				if (read & (1 << i)) { //switch released
					SW_off(i);
				}
				else { //switch pressed
					SW_on(i);
				}
			}
		}
	}
	SW_status=read;
}
void SW_on(byte sw) {
	dp.clearDisplay();
	dp.setTextColor(WHITE);
	dp.setCursor(10, 30);
	dp.setTextSize(2);
	dp.print("On "); dp.print(sw);
	dp.display();
}
void SW_off(byte sw) {
	dp.clearDisplay();
	dp.setTextColor(WHITE);
	dp.setCursor(10, 30);
	dp.setTextSize(2);
	dp.print("Off "); dp.print(sw);
	dp.display();
}