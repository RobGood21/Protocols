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
#define Psize 4 //aantal presets
#define aan "aan"
#define uit "uit"
//constructers
Adafruit_SSD1306 dp(128, 64, &Wire); // , -1);
NmraDcc  Dcc;
struct presets {
	byte filter; //welke msg verwerken, true is verwerken, false is overslaan
	//bit0 //loc
	//bit1 //speed, direction 'R''
	//bit2 //functions 'F'
	//bit3 //CV  'CV'
	//bit4 //artikel 
	//bit5 //switching '<>'
	//bit6 //CV 'CV'
	//bit7
};
presets preset[Psize];

byte Prst; //huidig actief preset

struct buffers {
	byte reg;//
	//bit0 artikel true ON, false OFF
	//bit1 artikel dir true Recht; false afslaan
	//bit2 msg=CV
	//bit3 
	//bit4
	//bit5
	//bit6
	//bit6 true: accesoire; false: loc
	//bit7 true: bezet; false vrij

	unsigned int adres;
	unsigned long tijd;
}; buffers bfr[Bsize]; //aantal buffer artikelen

//variabelen
byte DP_out = 0x00; //output byte
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
	Prst = EEPROM.read(110);
	if (Prst > Psize)Prst = 0;
	//factory eerst maken....
	MEM_reg = B11111100;//EEPROM.read(10);
	//bit0=Voor artikelen, true, alleen uit tonen met pulsduur, false aan en uit msg tonen 
	//bit1=singel false
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

	//reset, factory knop 0+3
	delay(10);
	if (~PINC & (1<<0) && ~PINC & (1<<3))factory();


	DP_welcome(); //toon opening text
	delay(500);
	DP_mon();
	MEM_read();
	//xtra init
	GPIOR2 = 0;
}

void factory() {
	for (int i = 0; i < EEPROM.length(); i++) {
		EEPROM.update(i, 0xFF);
	}
	dp.clearDisplay();
	dp.setCursor(10, 20);
	dp.setTextSize(2);
	dp.setTextColor(1);
	dp.print(F("Factory"));
	dp.display();
	delay(1000);
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
void DP_mon() {
	dp.clearDisplay();
	dp.drawLine(0, 54, 128, 54, 1);
	dp.setTextSize(1);
	dp.setTextColor(WHITE);
	dp.setCursor(10, 55);
	// Display static text
	dp.println(F("Hier onderbalk"));
	dp.display();
}
void loop() {
	//processen
	Dcc.process();
	//slow events
	if (millis() - slowtimer > 30) {
		//checkBuffer(); //check status buffer
		PORTD &= ~(3 << 3);
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
	bool p;
	switch (sw) {
	case 0: //schakelen tussen monitor en programmeren
		GPIOR0 ^= (1 << 2);
		p = (GPIOR0 & (1 << 2));
		if (p) { //programeer tonen
			DP_prg();
		}
		else { //monitor tonen
			DP_mon();
		}
		break;
	case 1:
		if (p) {
		} else{
	IO_exe();
		}
	
		break;

	case 2:
		drawPuls(3, 25, 2); //x-y-size;
		dp.display();
		//scrolldown();
		break;
	case 3:
		DP_mon();
		break;

	}
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

void DP_prg() { //iedere keer geheel vernieuwen?
	//toont programmeer blad (6 regels?)
	dp.clearDisplay();
	dp.setTextSize(1); dp.setTextColor(1);
	//regel 1
	dp.setCursor(0, 0);	
	dp.print("Preset: ");

	//regel 2
	drawLoc(0, 12, 1);

	//regel 3
	drawWissel(0, 22, 1, 0);

	//regel 4

	//regel 5



	dp.display();
}
//terugmeldingen (callback) uit libraries (NmraDCC)
void notifyDccMsg(DCC_MSG * Msg) {
	checkBuffer();
	//msg worden vaak meerdere malen uitgezonden, dubbele eruit filteren
	if (GPIOR0 & (1 << 1))return; //Stop als buffer vol is
	bool nieuw = true;
	for (byte i = 0; i < MAX_DCC_MESSAGE_LEN; i++) {
		if (lastmsg[i] != Msg->Data[i]) nieuw = false;
	}

	if (nieuw)return;
	//Serial.println(Msg->Data[0], BIN);
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
		adr = adr * 4;
		if (~bte &(1 << 2))adr -= 2;
		if (~bte & (1 << 1))adr -= 1;
		if (bte & (1 << 3))reg |= (1 << 0); //onoff
		if (bte & (1 << 0))reg |= (1 << 1);//false=rechtdoor, true = afslaan
		if (Msg->Data[3] > 0)reg |= (1 << 2);  //("CV");

		//bit6 van reg blijft false (artikel)		
		ART_write(adr, reg); //Zet msg in buffer
	//}

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
			GPIOR0 &= ~(1 << 1);
		}
	}



	//PORTD &= ~(3 << 3); //clear leds, periodiek in loop()
	//PORTD &= ~(1 << 4);
	if (GPIOR0 & (1 << 0)) {
		PIND |= (1 << 3);
	}
	else {
		Bcount = 0;
	}

	if (GPIOR0 & (1 << 1))PIND |= (1 << 4);
}


void ART_write(int adr, byte reg) { //called from 'notifyDccMsg()'
	//verwerkt nieuw 'basic accessory decoder packet (artikel)
	//als mem_reg bit0 true is de aan en de uit msg beide opslaan en verwerken.
	//Tijd opslaan in millis() , verschil geeft pulsduur getoont in de Off msg 
	//bij memreg bit0 false, alleen de On msg verwerken, opslaan en tonen, simpeler dus
	//kijken of msg nieuw is.	
	byte Treg = reg;

	bool nieuw = true; Treg |= (1 << 7);
	for (byte i = 0; i < Bsize; i++) {
		//zoeken naar door artikel bezette buffer
		if (bfr[i].adres == adr && bfr[i].reg == Treg) {
			nieuw = false;
			//Serial.print("*");
		}
	}


/*
	//Als er een 'aan' msg komt voor een artikel, zoeken of er nog een uit te voeren 'uit' msg is,
		//dan geen nieuw buffer aanmaken. kijken of dit sneller kan...?

	Treg &= ~(1 << 0); //zoeken naar actieve 'uit'msg
	//if (reg & (1 << 0)) { //dus een 'aan'msg
	for (byte i = 0; i < Bsize; i++) {
		if (bfr[i].adres == adr && bfr[i].reg == Treg) { //dus een actieve msg
			if(bfr[i].reg ^Treg==1)	nieuw = false;
		}
	}

	//}
*/


	if (nieuw) {
		//Serial.println("new");
		for (byte i = 0; i < Bsize; i++) {
			if (~bfr[i].reg & (1 << 7)) { //vrij gevonden	
				bfr[i].adres = adr;
				bfr[i].reg = reg;
				bfr[i].tijd = millis();
				bfr[i].reg |= (1 << 7); //set buffer active
				i = Bsize; //uitspringen
			}
		}
	}
}

//IO alles met de uitvoer van de ontvangen msg. periodiek of manual
void IO_exe() {
	bool read = true; byte count = 0;
	//ervoor zorgen dat alle buffers in volgorde worden gelezen, dus Bcount
	if (~GPIOR0 & (1 << 0))return; //stoppen als er geen te verwerken msg zijn.
	while (read) {
		//Serial.print("|");
		//zolang read =true herhalen, volgorde belangrijk
		//deze constructie omdat er meerdere redenen zijn waarom
		//deze buffer niet kan worden getoond. 
		read = IO_dp(); //dp=displays msg 
		Bcount++;
		if (Bcount == Bsize)Bcount = 0;

		count++;
		if (count > Bsize)read = false; //exit als buffer blijft hangen. Misschien een autodelete er op los laten?
	}
	dp.display();
}

bool IO_dp() { //displays msg
	bool read = true;
	if (~bfr[Bcount].reg & (1 << 7))return true; //exit als niet vrij (alleen bij artikelen maken)
	GPIOR2 = 0; //clear gpr, flags in functie
	dp.setTextColor(WHITE);

	//******Locomotief
	if (bfr[Bcount].reg & (1 << 6)) { //Locomotief

	}
	//*********//Accesoire	artikel
	else {
		if (MEM_reg & (1 << 0)) { //alleen de 'uit' tonen voor leesbaarheid
			if (bfr[Bcount].reg & (1 << 0)) return true; //alleen 'uit' msg verwerken
		}
		else { //aan en uit tonen
			if (~bfr[Bcount].reg & (1 << 0)) { //uit msg			


				/*
				//zoeken of er nog een 'aan' msg actief is, zoja deze buffer overslaan
				//hopelijk oplossing voor het probleem dat 'aan' msg blijven hangen en in de buffer blijven
				//Nog steeds niet helemaal goed...wel beter  ff verder gaan kijken of verderop een oplossing komt

				byte count = 0;
				GPIOR2 = bfr[Bcount].reg;
				GPIOR2 |= (1 << 0); //set reg naar 'aan'
				for (byte i = 0; i < Bsize; i++) {
					if (bfr[i].adres == bfr[Bcount].adres && bfr[i].reg == GPIOR2) {
						count++;
						if (count > 1) bfr[i].reg &= ~(1 << 7); //set buffer free, meer dan 1 'aan' msg
					}
					if (count > 0)	return true;
				}
				*/
			}
		}
		//Algemeen waardes bepalen
		GPIOR2 = 0; //reset gpr
		unsigned int puls = 0; byte r = 0; byte d = 0;

		if (MEM_reg & (1 << 0)) { //alleen uit msg verwerken, dus de aan weghalen en tijd uitrekenen			
			for (byte i = 0; i < Bsize; i++) {
				//volgorde belangrijk
				r = bfr[i].reg ^ bfr[Bcount].reg;// reken maar na klopt...alleen bit0 van .reg = verschillend				
				if (bfr[i].adres == bfr[Bcount].adres && r == 1) {
					puls = bfr[Bcount].tijd - bfr[i].tijd;
					bfr[i].reg &= ~(1 << 7);//buffer vrijgeven
					i = Bsize; //exit lus
				}
			}
		}
		else {
			if (bfr[Bcount].reg & (1 << 0)) GPIOR2 |= (1 << 0); //zet flag POWEROUT=on		
		}

		//type msg bepalen
		if (bfr[Bcount].reg & (1 << 2)) { //CV msg
			d = 0;
		}
		else if (bfr[Bcount].reg & (1 << 1)) {//rechtdoor
			d = 1;
		}
		else {//afslaand
			d = 2;
		}

		// Display
		if (MEM_reg & (1 << 1)) { //toon lijst
			scrolldown();
			dp.setCursor(0, 0);
			dp.setTextSize(1); //6 pixels breed, 8 pixels hoog 10 pixels is regelhoogte
			drawWissel(0, 0, 1, d);
			dp.setCursor(22, 0); dp.print(bfr[Bcount].adres);

			if (d > 0) { //not in CV
			//select aan/uit of alleen uit	

				dp.setCursor(57, 0);
				if (MEM_reg & (1 << 0)) { //alleen uit msg tonen
					drawPuls(49, 0, 1); //x-y-size
					dp.print(puls); dp.println(F("ms"));
				}
				else { //aan en uit msg tonen
					if (GPIOR2 & (1 << 0)) {
						dp.println(aan);
					}
					else {
						dp.println(uit);
					}
				}
			}
		}
		else { //toon enkel
			DP_mon(); //clears bovendeel display
			drawWissel(3, 0, 2, d);
			dp.setCursor(42, 0);
			dp.setTextSize(2);
			dp.print(bfr[Bcount].adres);
			if (d > 0) { //not in CV
				dp.setCursor(42, 18);

				if (MEM_reg & (1 << 0)) { //Alleen uit msg
					drawPuls(13, 18, 2); //x-y-size
					dp.print(puls); dp.println(F("ms"));
				}
				else { //aan en uit tonen
					if (GPIOR2 & (1 << 0)) {
						dp.println(aan);
					}
					else {
						dp.println(uit);
					}
				}
				//byte weergeven
				byte x;
				for (byte i = 0; i < 8; i++) {
					x = 15 * i;
					if (i > 3)x += 4;
					dp.drawRect(x, 38, 12, 12, 1);
				}
			}
		}

		bfr[Bcount].reg &= ~(1 << 7); // buffer vrijgeven

	}
	return false;
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
				dp.drawPixel(x, y, BLACK);
				dp.drawPixel(x, y + 10, WHITE);
			}
			else {
				dp.drawPixel(x, y + 10, BLACK);
			}
		}
	}
	dp.display();
}
void drawLoc(byte x, byte y, byte s) {
	//tekend loc icon
	dp.fillRect(s*(x + 1), s*y, s * 5, s * 1, 1);//dak
	dp.fillRect(s*(x + 4), s*(y + 2), s * 8, s * 3, 1); //ketel
	dp.fillRect(s*(x + 10), s*y, s * 1, s * 2, 1); //schoorsteen
	dp.fillRect(s*(x + 5), s*(y + 1), s * 1, s * 1, 1); //raam
	dp.fillRect(s*x, s*(y + 3), s * 2, s * 1, 1); //bok
	dp.fillRect(s*x, s*(y + 4), s * 4, s * 1, 1); //vloer
	dp.fillRect(s*(x + 1), s*(y + 5), s * 5, s * 1, 1); //onderstel
	dp.fillRect(s*(x + 7), s*(y + 5), s * 5, s * 1, 1); //onderstel voor
	dp.fillRect(s*(x + 2), s*(y + 6), s * 3, s * 1, 1); //wiel
	dp.fillRect(s*(x + 8), s*(y + 6), s * 3, s * 1, 1); //wiel voor
}
void drawWissel(byte x, byte y, byte s, byte t) { //t=0 CV; t=1 rechtdoor t=2 afslaand
	switch (t) {
	case 0:
		dp.drawRect(s*x, y, s * 5, s * 7, 1);
		dp.drawRect(s*(x + 7), y, s * 5, s * 7, 1);
		break;
	case 1:
		dp.fillRect(s*x, y, s * 5, s * 7, 1);
		dp.drawRect(s*(x + 7), y, s * 5, s * 7, 1);
		break;
	case 2:
		dp.drawRect(s*x, y, s * 5, s * 7, 1);
		dp.fillRect(s*(x + 7), y, s * 5, s * 7, 1);
		break;
	}
}
void drawPuls(byte x, byte y, byte s) {
	dp.fillRect(x, y + (s * 6), s * 3, s * 1, 1);//onder

	dp.fillRect(x + (s * 2), y + (s * 2), s * 1, s * 5, 1); //naar boven

	dp.fillRect(x + (s * 3), y + (s * 2), s * 2, s * 1, 1); //boven
}



