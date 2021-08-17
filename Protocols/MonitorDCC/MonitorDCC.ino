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
#include <NmraDcc.h>

//constanten

//constructers
Adafruit_SSD1306 dp(128, 64, &Wire); // , -1);
NmraDcc  Dcc;

//variabelen
unsigned long slowtimer;

//variabelen schakelaars
byte SW_status = 15; //holds the last switch status, start as B00001111;
byte SW_holdcounter[4]; //for scroll functie op buttons
byte SW_scroll = B0011; //masker welke knoppen kunnen scrollen 1=wel 0=niet

//tijdelijke varabelen
byte teller; //gebruikt in display test


void setup() {

	//start processen
	Serial.begin(9600);
	dp.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	Dcc.pin(0, 2, 1); //interupt number 0; pin 2; pullup to pin2

	Dcc.init(MAN_ID_DIY, 10, 0b10000000, 0); //bit6 false, decoder adres callback 'notifyDccAccTurnoutBoard'
	//bit 7 maakt er een loc decoder van

	//poorten	
	DDRC &= ~B00001111;
	PORTC |= B00001111;

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
	//processen
	Dcc.process();
	//slow events
	if (millis() - slowtimer > 30) {
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

	//pressed or released
	changed = read ^ SW_status;
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
	//hold pressed for scroll function
	for (byte i = 0; i < 4; i++) {
		if (~read & (1 << i) && SW_scroll & (1<<i)) {
			if (SW_holdcounter[i] > 30) { //tijdxslowtimer voor scroll begint, tempo scroll komt uit loop, slowevents slowtimer
				SW_on(i);
			}
			else {
				SW_holdcounter[i] ++;
			}
		}
	}

	SW_status = read;
}
void SW_on(byte sw) {
	switch (sw) {
	case 0:
Dcc.init(MAN_ID_DIY, 10, 0b10000000, 0); //accesoire decoder
		break;
	case 1:
		Dcc.init(MAN_ID_DIY, 10, 0b00000000, 0); //loc decoder
		break;
	}
	
	
	



	//test schakelaars
	teller++;
	dp.clearDisplay();
	dp.setTextColor(WHITE);
	dp.setCursor(10, 30);
	dp.setTextSize(2);
	dp.print("On "); dp.print(sw);
	dp.setCursor(85, 50);
	dp.setTextSize(1);
	dp.print(teller);
	dp.display();

}

void SW_off(byte sw) {
	SW_holdcounter[sw] = 0; //reset counter for scroll function 


	//Test schakelaars
	dp.clearDisplay();
	dp.setTextColor(WHITE);
	dp.setCursor(10, 30);
	dp.setTextSize(2);
	dp.print("Off "); dp.print(sw);
	dp.display();
}

//terugmeldingen (callback) uit libraries (NmraDCC)
void notifyDccAccTurnoutBoard(uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower) {
	//called als CV29 bit6 = false decoderadres,channel,poort,onoff (zie setup 'init')
	Serial.print("Artikel adres: "); Serial.print(BoardAddr);Serial.print("  ch: "); Serial.print(OutputPair);
	Serial.print("  R-on/off: "); Serial.print(Direction); Serial.print("-"); Serial.println(OutputPower);
}
void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower) {
	Serial.println("jo");
}
void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps) {
	Serial.print("Loc adres: "); Serial.print(Addr); Serial.print("  type: "); Serial.print(AddrType);
	Serial.print("  speed "); Serial.print(Speed); Serial.print("  dir:"); Serial.print(Dir); Serial.print("Steps: "); Serial.println(SpeedSteps);
}