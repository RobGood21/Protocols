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
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <NmraDcc.h>

//constanten
#define Bsize 8 //hoe groot is de artikel buffer?
#define AutoDelete 2000 //aantal ms dat een buffer max. bezet bljft.
//constructers
Adafruit_SSD1306 dp(128, 64, &Wire); // , -1);
NmraDcc  Dcc;

/*
//strucs
struct locs {
	byte reg; //register
	int adres;


}; locs loc[4]; //4 locomotieven kunnen worden gemonitoord
*/


struct buffers {
	byte reg;//
	//bit0 artikel true ON, false OFF
	//bit1 artikel dir true Recht; false afslaan
	//bit6 is klaar om te tonen. MEM_reg bit0 ???nodig
	//bit7 is artikel vrij, bezet true, vrij false (tonen?)

	unsigned int adres;
	unsigned long tijd;
}; buffers bfr[Bsize]; //aantal buffer artikelen

//variabelen
byte MEM_reg;
byte lastmsg[6];
byte Bcount; //pointer naar laast verwerkte artikel buffer

unsigned long slowtimer;
//variabelen schakelaars
byte SW_status = 15; //holds the last switch status, start as B00001111;
byte SW_holdcounter[4]; //for scroll functie op buttons
byte SW_scroll = B0011; //masker welke knoppen kunnen scrollen 1=wel 0=niet
//tbv decoder NmrraDCC
byte uniek = 0xFF;
//tijdelijke varabelen
byte teller; //gebruikt in display test
byte temp;
//setup functions



void MEM_read() {
	//factory eerst maken....
	MEM_reg = 0xFF;//EEPROM.read(10);
	//bit0=Voor artikelen, true, alleen uit tonen met pulsduur, false aan en uit msg tonen 
}
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
	DDRD |= (1 << 3); DDRD |= (1 << 4); //groene en rode  leds
	DP_welcome(); //toon opening text
	delay(2000);
	DP_start();
	MEM_read();
}
void DP_welcome() {
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
void DP_start() {
	dp.clearDisplay();
	dp.drawLine(0, 50, 128, 50, 1);
	dp.setTextSize(1);
	dp.setTextColor(WHITE);
	dp.setCursor(10, 53);

	// Display static text
	dp.println(F("Hier onderbalk"));
	dp.display();
}
void loop() {
	//processen
	Dcc.process();
	//slow events
	if (millis() - slowtimer > 30) {
		checkBuffer(); //check status buffer
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
		if (~read & (1 << i) && SW_scroll & (1 << i)) {
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

		for (byte i = 0; i < Bsize; i++) {
			Serial.print(bfr[i].adres); Serial.print(" ");
			Serial.println(bfr[i].reg);
		}
		Serial.println("----");
		break;
	case 1:
		IO_exe();
		break;

	case 2:
		scrolldown();
		break;
	case 3:
		DP_start();
		break;

	}
	/*
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
*/
}
void SW_off(byte sw) {
	SW_holdcounter[sw] = 0; //reset counter for scroll function 
/*
	//Test schakelaars
	dp.clearDisplay();
	dp.setTextColor(WHITE);
	dp.setCursor(10, 30);
	dp.setTextSize(2);
	dp.print("Off "); dp.print(sw);
	dp.display();
*/
}

//terugmeldingen (callback) uit libraries (NmraDCC)
void notifyDccMsg(DCC_MSG * Msg) {
	//msg worden vaak meerdere malen uitgezonden, dubbele eruit filteren
	bool nieuw = true;
	for (byte i = 0; i < MAX_DCC_MESSAGE_LEN; i++) {
		if (lastmsg[i] != Msg->Data[i]) nieuw = false;
	}

	if (nieuw)return;
	//local variables
	byte db;  byte bte;  int adr = 0; byte reg = 0;


	//Filters
	db = Msg->Data[0];
	if (db == 0) {	//broadcast voor alle decoder bedoeld
	}
	else if (db < 128) {//loc decoder met 7bit adres
	}
	else if (db < 192) {//Basic Accessory Decoders with 9 bit addresses and Extended Accessory
		//decoder adres bepalen
		bte = db;
		bte = bte << 2; adr = bte >> 2; //clear bit7 en 6
		bte = Msg->Data[1];
		if (~bte & (1 << 6))adr += 256;
		if (~bte & (1 << 5))adr += 128;
		if (~bte & (1 << 4))adr += 64;

		//CV of bediening van artikel
		if (Msg->Data[3] > 0) { //artikel CV instelling
			//hier in theorie weer twee opties, alle channels of selectieve channel
			Serial.println("CV");
		}
		else { //artikel msg 2 bytes
			//dubbele na elkaar gestuurde boodschappen uitfilteren.
			//if (lastmsg[0] != Msg->Data[0] || lastmsg[1] != Msg->Data[1]) {
				//dcc adres bepalen
			adr = adr * 4;
			if (~bte &(1 << 2))adr -= 2;
			if (~bte & (1 << 1))adr -= 1;
			if (bte & (1 << 3))reg |= (1 << 0); //onoff
			if (bte & (1 << 0))reg |= (1 << 1);//false=rechtdoor, true = afslaan
			//bit6 van reg blijft false (artikel)				
			ART_write(adr, reg); //Zet msg in buffer
		//}
		}
	}
	else if (db < 232) {
		//Multi-Function Decoders with 14 bit 60 addresses
	}
	else if (db < 255) {
		//Reserved for Future Use
	}
	else {
		//adress=255 idle packett
	}


	for (byte i = 0; i < MAX_DCC_MESSAGE_LEN; i++) {
		lastmsg[i] = Msg->Data[i];//opslaan huidig msg in lastmsg
	}
}


//functions
void checkBuffer() {
	//check of er te verwerken msg in buffer zitten en of de buffer niet vol is. 
	GPIOR0 &= ~(1 << 0); //flag msg in buffer
	GPIOR0 |= (1 << 1); //flag buffer full
	for (byte i = 0; i < Bsize; i++) {
		if (bfr[i].reg & (1 << 7)) {
			//ghost boodschappen...(langer dan (instelbaar xtal sec in de buffer) automatisch leegmaken		
			GPIOR0 |= (1 << 0); //buffer niet leeg(groen)
		}
		else {
			GPIOR0 &= ~(1 << 1); //Buffer niet vol (rood)
		}
	}

	PORTD &= ~(3 << 3); //clear leds
	//PORTD &= ~(1 << 4);
	if (GPIOR0 & (1 << 0))PIND |= (1 << 3);
	if (GPIOR0 & (1 << 1))PIND |= (1 << 4);
}


void ART_write(int adr, byte reg) {
	//verwerkt nieuw 'basic accessory decoder packet (artikel)
	//als mem_reg bit0 true is de aan en de uit msg beide opslaan en verwerken.
	//Tijd opslaan in millis() , verschil geeft pulsduur getoont in de Off msg 
	//bij memreg bit0 false, alleen de On msg verwerken, opslaan en tonen, simpeler dus

	//kijken of msg nieuw is.	
	bool nieuw = true; reg |= (1 << 7);
	for (byte i = 0; i < Bsize; i++) {
		//zoeken naar door artikel bezette buffer
		if (bfr[i].adres == adr && bfr[i].reg == reg) {
			nieuw = false;
			Serial.print("*");
		}
	}

	if (nieuw) {
		//Serial.println("new");
		for (byte i = 0; i < Bsize; i++) {
			if (~bfr[i].reg & (1 << 7)) { //vrij gevonden	
				bfr[i].adres = adr;
				bfr[i].reg = reg;
				bfr[i].tijd = millis();
				//Serial.println(i);
				i = Bsize; //uitspringen
			}
		}
	}
}

//IO alles met de uitvoer van de ontvangen msg. periodiek of manual
void IO_exe() {
	//ervoor zorgen dat alle buffers in volgorde worden gelezen, dus Bcount
	if (~GPIOR0 & (1 << 0))return; //stoppen als er geen te verwerken msg zijn.

	//eventueel te maken 
	if (MEM_reg & (1 << 1)) { //Toon lijst
		scrolldown();
	}
	else { //toon 1 msg
		dp.clearDisplay();
	}

	
	byte count = 0; bool read = true;
	while (read) { //zolang read =true herhalen, volgorde belangrijk
		read = IO_dp(); //dp=displays msg 
		Bcount++;
		if (Bcount == Bsize)Bcount = 0;
		//count++; //niet nodig er is zeker een te vererken buffer
		//if (count == Bsize)read = false; //geen te verwerken buffer gevonden
	}
	dp.display();
}

bool IO_dp() {
	bool read = true;
	if (~bfr[Bcount].reg & (1 << 7))return true;
	
	//te tonen buffer = Bcount
	dp.setTextColor(WHITE);
	dp.setCursor(0, 0);
	dp.setTextSize(1); //6 pixels breed, 8 pixels hoog 10 pixels is regelhoogte

	if (bfr[Bcount].reg & (1 << 6)) { //Locomotief

	}
	else { //Accesoire
		if (MEM_reg & (1 << 0)) { //alleen de 'uit' tonen voor leesbaarheid
			if (bfr[Bcount].reg & (1 << 0)) return true; //alleen 'uit' msg verwerken
		}

		dp.print(F("Art.")); dp.print(bfr[Bcount].adres);
		if (bfr[Bcount].reg & (1 << 1)) {
			dp.print(F(" R"));
		}
		else {
			dp.print(F(" A"));
		}
		if (MEM_reg & (1 << 0)) { //alleen uit msg verwerken, dus de aan weghalen en tijd uitrekenen

			unsigned int puls; byte r;
			for (byte i = 0; i < Bsize; i++) {
				//volgorde belangrijk
				r = bfr[i].reg ^ bfr[Bcount].reg;// reken maar na klopt...alleen bit0 van .reg = verschillend
				if (bfr[i].adres == bfr[Bcount].adres && r == 1) {
					puls = bfr[Bcount].tijd - bfr[i].tijd;
					dp.print(F(" <>")); dp.print(puls); dp.println(F("ms"));
					bfr[i].reg &= ~(1 << 7);//buffer vrijgeven
					i = Bsize; //exit lus
				}
			}
		}
		else { //Beide verwerken aan en uit
			if (bfr[Bcount].reg & (1 << 0)) {
				dp.println("+");
			}
			else {
				dp.println("-");
			}
		}
		bfr[Bcount].reg &= ~(1 << 7); // buffer vrijgeven
	}
	return false;
}

void scrolldown1() {
	temp++;
	dp.ssd1306_command(0x40+temp);
}
void scrolldown() {
	//blok van 50 bovenste lijnen 10 (regelhoogte nog uitzoeken) naar beneden plaatsen
	//regel voor regel onderste regel y=40~y=49 zwart maken
	//blijft dus 15pixels over nu aan onderkant
	for (byte y = 40; y < 50; y++) {
		for (byte x = 0; x < 128; x++) {
			dp.drawPixel(x, y, BLACK);
		}
	}	
	for (byte y = 39; y < 255; y--) {
		for (byte x = 0; x < 128; x++) {
			if (dp.getPixel(x, y)) {
				dp.drawPixel(x,y, BLACK);
				dp.drawPixel(x,y + 10, WHITE);
			}
			else {
				dp.drawPixel(x,y + 10, BLACK);
			}
		}
	}
	dp.display();
}


