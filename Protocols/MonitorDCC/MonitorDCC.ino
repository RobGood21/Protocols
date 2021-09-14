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
#define PFsize 15 //aantal programfases
#define autoDelete 10000 //tijd voor autodelete buffer inhoud 10sec

//verkortingen 
#define program GPIOR0 & (1<<2) //programmeer modus aan 


//constructers
Adafruit_SSD1306 dp(128, 64, &Wire); // , -1);
NmraDcc  Dcc;
struct presets {
	byte reg;
	//bit0 True: Toon aan en uit msg, false: toon alleen uit met pulsduur
	//bit1	True : Toon lijst van 5 msg onder elkaar, of false : 1 grote msg.
	//bit2	
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
	//bit0 artikel true: ON; false: off
	//bit1 artikel true: Recht; false: afslaan
	//bit2 true: msg=Drive
	//bit3 true: msg=CV
	//bit4  niet     True: msg=function1  na 13sept in 2 verwerken
	//bit5  niet     True: msg=Function2 
	//bit6
	//bit6 true: accesoire; false: loc
	//bit7 true: bezet; false: vrij

	unsigned int adres; //L&A: bevat adres acc en loc
	unsigned long tijd; //L&A: bevat tijd laatste aanpassing
	byte instructie; //Loc: bevat waarde instructie byte
	byte db[2];
}; buffers bfr[Bsize]; //aantal buffer artikelen

//variabelen
int T_adres = 0; //tijdelijk adres
byte T_instructie = 0; //Tijdelijk opslag instructie byte
byte T_db[2]; //Tijdelijke opslag


byte prgfase;
byte DP_out = 0x00; //output byte
byte lastmsg[MAX_DCC_MESSAGE_LEN]; //length 6
byte data[MAX_DCC_MESSAGE_LEN]; //bevat laatste ontvangen data uit de decoder
byte Bcount; //pointer naar laast verwerkte artikel buffer

unsigned long slowtimer;
//variabelen schakelaars
byte SW_status = 15; //holds the last switch status, start as B00001111;
byte SW_holdcounter[4]; //for scroll functie op buttons
byte SW_scroll = B0000; //masker welke knoppen kunnen scrollen 1=wel 0=niet
//tbv decoder NmrraDCC
byte uniek = 0xFF;
byte slowcount;


//tijdelijke varabelen
byte teller; //gebruikt in display test
byte temp;
//setup functions

void MEM_read() {
	int t;
	//presets laden
	Prst = EEPROM.read(110);
	if (Prst > Psize)Prst = 0;
	for (byte i = 0; i < Psize; i++) {
		//starten vanaf EEPROM adres 200, 20bytes per preset
		t = 200 + (i * 20);
		preset[i].filter = EEPROM.read(t);
		preset[i].reg = EEPROM.read(t + 1);
	}


}
void MEM_write() {
	EEPROM.update(110, Prst);
	EEPROM.update(200 + (Prst * 20) + 0, preset[Prst].filter);
	EEPROM.update(200 + (Prst * 20) + 1, preset[Prst].reg);

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
	if (~PINC & (1 << 0) && ~PINC & (1 << 3))factory();


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
void DP_mon() { //maakt display schoon, voor msg 
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
		slowtimer = millis();

		checkBuffer(); //check status buffer
		slowcount++;
		if (slowcount > 100) {
			slowcount = 0;
			//PORTD &= ~(3 << 3); //set leds off
		}


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
	case 0: //schakelen tussen monitor en programmeren
		GPIOR0 ^= (1 << 2);
		if (program) { //programeer tonen
			DP_prg();
		}
		else { //monitor tonen
			MEM_write();
			DP_mon();

		}
		break;
	case 1:
		if (program) { //program stand
			prgfase++;
			if (prgfase >= PFsize)prgfase = 0;
			DP_prg();
		}
		else {

		}

		break;

	case 2:
		if (program) {
			ParaDown();
			DP_prg();
		}
		else {

		}
		break;
	case 3:
		if (program) {
			ParaUp();
			DP_prg();
		}
		else {
			IO_exe();

		}

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
	byte px; byte py; byte w; byte h;
	byte y = 0; //regel afstand verticaal
	byte x[4] = { 0,26,45,68 };
	dp.clearDisplay();
	dp.setTextSize(1); dp.setTextColor(1);
	//regel 1
	dp.setCursor(0, y);
	dp.print(F("Preset:")); dp.print(Prst + 1);

	if (prgfase == 0) { px = x[0]; py = y + 8; w = x[0] + 47, h = y + 8; }//Cursor preset
	dp.setCursor(x[3], y);
	if (preset[Prst].reg & (1 << 1)) { //lijst
		dp.print(F("Lijst"));
	}
	else { //enkel
		dp.print(F("Apart"));
	}
	if (prgfase == 1) { px = x[3]; py = y + 8; w = x[3] + 27; h = y + 8; }//cursor lijst/apart

	//regel 2 
	y = 12;
	drawCheck(x[0], y, preset[Prst].filter & (1 << 0)); //check toon locs
	drawLoc(x[0] + 8, y, 1);

	if (prgfase == 2) { px = x[0]; py = y + 8; w = x[0] + 20; h = y + 8; }//cursor loc

	if (preset[Prst].filter & (1 << 0)) { //alleen als loc check is true
		drawCheck(x[1], y, preset[Prst].filter & (1 << 1));
		dp.setCursor(x[1] + 8, y); dp.print(F("R")); //rijden msg
		if (prgfase == 3) { px = x[1]; py = y + 8; w = x[1] + 10; h = y + 8; } //cursor loc R
		drawCheck(x[2], y, preset[Prst].filter & (1 << 2));
		dp.setCursor(x[2] + 8, y); dp.print(F("F")); //functies van de loc
		if (prgfase == 4) { px = x[2]; py = y + 8; w = x[2] + 10; h = y + 8; } //cursor Loc F
		drawCheck(x[3], y, preset[Prst].filter & (1 << 3));
		dp.setCursor(x[3] + 8, y); dp.print(F("CV")); //CV voor de loc
		if (prgfase == 5) { px = x[3]; py = y + 8; w = x[3] + 16; h = y + 8; } //cursor Loc CV
	}
	//regel 3
	y = 22;
	drawCheck(x[0], y, preset[Prst].filter & (1 << 4)); //check toon artikelen
	drawWissel(x[0] + 8, y, 1, 0);
	if (prgfase == 6) { px = x[0]; py = y + 8; w = x[0] + 20; h = y + 8; } //cursor Artikelen

	if (preset[Prst].filter & (1 << 4)) { //alleen als artikel check is true
		drawCheck(x[1], y, preset[Prst].filter & (1 << 5));
		dp.setCursor(x[1] + 8, y); dp.print(F("S")); //Switch msg
		if (prgfase == 7) { px = x[1]; py = y + 8; w = x[1] + 10; h = y + 8; } //cursor Artikelen switch
		drawCheck(x[2], y, preset[Prst].filter & (1 << 6));
		dp.setCursor(x[2] + 8, y); dp.print(F("CV")); //CV
		if (prgfase == 8) { px = x[2]; py = y + 8; w = x[2] + 16; h = y + 8; } //cursor Artikelen CV
		drawCheck(x[3], y, preset[Prst].reg & (1 << 0));//drawPuls(x[3] + 8, y, 1);
		dp.setCursor(x[3] + 8, y); dp.print(F("puls")); //puls of aan/uit		
		if (prgfase == 9) { px = x[3]; py = y + 8; w = x[3] + 28; h = y + 8; } //cursor Artikelen CV
	}

	//if (prgfase == 6)dp.drawLine(x[0], y + 8, x[0] + 22, y + 8, 1); //print cursor

	//drawWissel(0, 22, 1, 0);

	//regel 4

	//regel 5
	dp.drawLine(px, py, w, h, 1);
	dp.display();
}
void ParaUp() {
	switch (prgfase) {
	case 0:
		if (Prst < Psize - 1)Prst++; //Psize starts at 1; array at 0
		break;
	case 1:

		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		break;
	case 5:
		break;
	case 6:
		break;
	case 7:
		break;
	case 8:
		break;
	case 9:
		break;
	case 10:
		break;
	case 11:
		break;
	case 12:
		break;
	case 13:
		break;
	case 14:
		break;
	case 15:
		break;

	}

}
void ParaDown() {

	switch (prgfase) {
	case 0:
		if (Prst > 0)Prst--;
		break;
	case 1:
		preset[Prst].reg ^= (1 << 1);//flip  preset[Prst].reg bit1 lijst of apart
		break;
	case 2:
		preset[Prst].filter ^= (1 << 0);//tonen loc msg
		break;
	case 3:
		preset[Prst].filter ^= (1 << 1); //Tonen loc rijden
		break;
	case 4:
		preset[Prst].filter ^= (1 << 2); //Tonen loc functions
		break;
	case 5:
		preset[Prst].filter ^= (1 << 3); //Tonen loc CV
		break;
	case 6:
		preset[Prst].filter ^= (1 << 4); //Tonen Acc
		break;
	case 7:
		preset[Prst].filter ^= (1 << 5); //Tonen Acc switch
		break;
	case 8:
		preset[Prst].filter ^= (1 << 6); //Tonen Acc CV
		break;
	case 9:
		preset[Prst].reg ^= (1 << 0); //tonen puls of aan uit msg
		break;
	case 10:
		break;
	case 11:
		break;
	case 12:
		break;
	case 13:
		break;
	case 14:
		break;
	case 15:
		break;

	}

}
//terugmeldingen (callback) uit libraries (NmraDCC)
void notifyDccMsg(DCC_MSG * Msg) {
	//checkBuffer();
   //msg worden vaak meerdere malen uitgezonden, dubbele eruit filteren

	//stoppen met volle buffer lijkt me niet goed...
	//if (GPIOR0 & (1 << 1))return; //Stop als buffer vol is




	bool nieuw = true;
	for (byte i = 0; i < MAX_DCC_MESSAGE_LEN; i++) {
		if (lastmsg[i] != Msg->Data[i]) nieuw = false;
	}

	if (nieuw)return;


	//Serial.println(Msg->Data[0], BIN);
	//local variables

	for (byte i; i < 6; i++) {
		data[i] = Msg->Data[i];
	}
	byte db;  byte bte;  int adr = 0; byte reg = 0;
	//Filters
	db = data[0];
	if (db == 0) {	//broadcast voor alle decoder bedoeld
	}
	else if (db < 128) {//loc decoder met 7bit adres
		Loc(true);
	}
	else if (db < 192) {//Basic Accessory Decoders with 9 bit addresses and Extended Accessory
		//decoder adres bepalen
		bte = db;
		bte = bte << 2; adr = bte >> 2; //clear bit7 en 6
		bte = data[1];
		if (~bte & (1 << 6))adr += 256;
		if (~bte & (1 << 5))adr += 128;
		if (~bte & (1 << 4))adr += 64;

		//CV of bediening van artikel
		adr = adr * 4;
		if (~bte &(1 << 2))adr -= 2;
		if (~bte & (1 << 1))adr -= 1;
		if (bte & (1 << 3))reg |= (1 << 0); //onoff
		if (bte & (1 << 0))reg |= (1 << 1);//false=rechtdoor, true = afslaan
		if (data[3] > 0)reg |= (1 << 2);  //("CV");
		reg |= (1 << 6); //accessoire
		Write_Acc(adr, reg); // accessoire msg ontvangen
	//}

	}
	else if (db < 232) {
		//Multi-Function (loc) Decoders with 14 bit 60 addresses
		Loc(false);
	}
	else if (db < 255) {
		//Reserved for Future Use
	}
	else {
		//adress=255 idle packett
	}

	for (byte i = 0; i < MAX_DCC_MESSAGE_LEN; i++) {
		lastmsg[i] = data[i];//opslaan huidig msg in lastmsg
	}
}
//functions
void checkBuffer() {
	//check of er te verwerken msg in buffer zitten en of de buffer niet vol is. 
	//verder check of er loc msg inzitten die al getoond zijn maar laatste aanpassing oud, bv > 3000ms en speed op nul

	GPIOR0 &= ~(1 << 0); //flag msg in buffer
	GPIOR0 |= (1 << 1); //flag buffer full

	for (byte i = 0; i < Bsize; i++) {
		if (bfr[i].reg & (1 << 7)) { //|| bfr[i].reg & (1 << 6)) { //tonen of actieve loc
			GPIOR0 |= (1 << 0); //buffer niet leeg(groen)
		}
		else {
			GPIOR0 &= ~(1 << 1);
		}
	}

	PORTD &= ~(3 << 3); //clear leds, periodiek in loop()
	//PORTD &= ~(1 << 4);

	if (GPIOR0 & (1 << 0)) {
		PIND |= (1 << 3);
	}
	else {
		Bcount = 0;
	}
	if (GPIOR0 & (1 << 1))PIND |= (1 << 4);
}
void Loc(bool t) {

	//000, 001, 110 not on this project V1.01 sept 2021 
	//instr = data[2] >> 5; //010=reversed 011=forward 100=F1 101=F2 111=CV 

	if (~preset[Prst].filter & (1 << 0))return; //Filter voor Loc msg
	if (t) { //7 bits adres
		T_adres = data[0];
		T_instructie = data[1];
		T_db[0] = data[2];
		T_db[1] = data[3];
	}
	else { //14 bits adres
		T_adres = data[1];
		if (data[0] & (1 << 0))T_adres += 256;
		if (data[0] & (1 << 1))T_adres += 512;
		if (data[0] & (1 << 2))T_adres += 1024;
		if (data[0] & (1 << 3))T_adres += 2048;
		if (data[0] & (1 << 4))T_adres += 4096;
		if (data[0] & (1 << 5))T_adres += 8192;
		T_instructie = data[2];
		T_db[0] = data[3];
		T_db[1] = data[4];
	}

	//Drive/function or CV
	bool nt = true;
	switch (T_instructie >> 5) {
	case B010:
		if (preset[Prst].filter & (1 << 1)) {
			nt= false;
			LocMsg(1);
		}
		break;
	case B011:
		if (preset[Prst].filter & (1 << 1)) {
			nt= false;
			LocMsg(1);
		}
		break;
	case B100:
		if (preset[Prst].filter & (1 << 2)) {
			nt= false;
			LocMsg(2);
		}
		break;
	case B101:
		if (preset[Prst].filter & (1 << 2)) {
			nt= false;
			LocMsg(3);
		}
		break;
	case B111:
		if (preset[Prst].filter & (1 << 3)) {
			nt= false;
			LocMsgCV(); 
		}
		break;
	}
	if(nt)return; //deze msg niks mee doen
}

void LocMsgCV() {
	//Maak nieuw message Loc CV 
	Serial.print("cv");
	byte buffer = FreeBfr();
	bfr[buffer].adres = T_adres;
	bfr[buffer].reg = 0;
	bfr[buffer].reg |= B10001000; //active, loc, CV
	bfr[buffer].instructie = T_instructie;
	bfr[buffer].db[0] = T_db[0];
	bfr[buffer].db[1] = T_db[1];
	bfr[buffer].tijd = millis();
	Debug_bfr(true,buffer);
}

void LocMsg(byte tiep) { //called from loc() 1=drive 2=function 1 3 = function2
	//000, 001, 110 not on this project V1.01 sept 2021  (misschien wel een melding tonen op display bij zo een msg?)
	//bepaal soort msg 010=reversed 011=forward 100=F1 101=F2 111=CV en filter of getoond moet worden
	//gebruik GPIOR2 as temp byte
	for (byte i = 0; i < Bsize; i++) {
		if ((~bfr[i].reg & (1 << 6)) && (bfr[i].adres == T_adres) && (bfr[i].reg & (1 << 2))) { //drive msg/functions
			//check type ontvangen instructie (RegI)
			switch (tiep) {
			case 1: //drive
				if ((bfr[i].instructie ^ T_instructie) == 0) {
					return;  //herhaalde drive msg
				}
				else {
					//Serial.print(tiep);
					bfr[i].instructie = T_instructie;
					changed(i);
					return; //verlaat function
				}
				break;

			case 2: //Function group 1	
				//db[0] bepaal functies 1 en vergelijk met db[0]
				//5 functies FL (headlights) F1~F4 
				//Serial.println(T_instructie);				
				GPIOR2 = T_instructie << 3;
				GPIOR2 = GPIOR2 >> 3; //isoleer bit 0~bit4 
				//Serial.print(GPIOR2, BIN);
				if (bfr[i].db[0] == GPIOR2) {
					return; //herhaalde functie msg
				}
				else { //Functions 1 veranderd 
					//Serial.println(T_instructie, BIN);
					//uitgaande dat CV29bit1=true (28 snelheidsstappen) geeft bit 4 FL(headlights) bit0 F1 bit3 F3					
					bfr[i].db[0] = GPIOR2;
					changed(i);
				}
				break;

			case 3://Function group 2
				//db[1] bepaal functies 2 en vergelijk met db[1]			
				byte xb = 0;
				GPIOR2 = bfr[i].db[1];
				if (~T_instructie & (1 << 4)) xb = 4;  //F5~F8 db[1] bits 7~4	

				for (byte a = 0; a < 4; a++) {
					GPIOR2 &= ~(1 << a + xb); //clear bit
					if (T_instructie & (1 << a)) GPIOR2 |= (1 << a + xb);
				}
				//Serial.print(GPIOR2);
				if (GPIOR2 == bfr[i].db[1]) {
					return;
				}
				else {
					bfr[i].db[1] = GPIOR2;
					changed(i);
				}
				break;
			}
		}
	}
	//niet uit de functie gesprongen door 'return' dus geen msg gevonden
	//Serial.println("geen msg");
	
	if (tiep == 1)Write_Loc(); //1=drive 2=functions 1 3=functions 2  (alleen nieuwe buffer maken bij drive msg.
}
void changed(byte b) {
	bfr[b].tijd = millis();
	bfr[b].reg |= (1 << 7); //msg weer tonen
	Debug_bfr(false, b); //print buffer inhoud
}
byte FreeBfr() {
	//Hier kan zeker nog aan gewerkt worden voor een zoo slim mogelijke keuze van een vrije buffer	

	//oude niet actieve accesoire(bit6 van reg)  of CV(bit3 van reg) buffer
	for (byte b = 0; b < Bsize; b++) {
		if ((~bfr[b].reg & (1 << 7)) && (bfr[b].reg & (1 << 6) || bfr[b].reg & (1<<3))) return b; 					
	}
	//nu zoeken naar een oude lang niet gebruikte buffer
	for (byte i = 0; i < Bsize; i++) {
		if (bfr[i].tijd == 0) return i; //ongebruikte buffer
		if (millis() - bfr[i].tijd > autoDelete  && ~bfr[i].reg & (1<<7)) return i; //10sec niks gebeurt met deze niet actieve buffer.
	}

	//hier kan nog zoeken naar een hele oude buffer

	return Bsize;
}
void Write_Loc() { //adres en instructiebyte
	byte free = FreeBfr();
	//Serial.println(free);
	if (free == Bsize) return;  //Hier misschien nog een foutafhandeling?
	bfr[free].adres = T_adres;
	bfr[free].instructie = T_instructie; //= eerste byte na adres.
	bfr[free].db[0] = 0;
	bfr[free].db[1] = 0;
	bfr[free].reg = 0;
	bfr[free].reg |= (1 << 7); //7=tonen 6=loc(false)
	bfr[free].reg |= (1 << 2); //type Loc message 2 = drive/functions 3 = CV 
	bfr[free].tijd = millis();
	Debug_bfr(true, free);
}
void Debug_bfr(bool n, byte buffer) {
	/*
	Bij een drive msg wordt nu de checksum in de CV geschreven, maakt denk ik niet uit omdat deze CV
	niet wordt getoond bij deze type msg. uit filteren kan maar geeft extra instructies
	*/
	if (n) Serial.print("New ");
	Serial.print(buffer); Serial.print("  Adres:"); Serial.print(bfr[buffer].adres);
	Serial.print(" instr:"); Serial.print(bfr[buffer].instructie, BIN); Serial.print(" Reg:");
	Serial.print(bfr[buffer].reg, BIN);
	Serial.print(" db0/CV:"); Serial.print(bfr[buffer].db[0], BIN); Serial.print(" db1/CVV:"); Serial.print(bfr[buffer].db[1], BIN);
	Serial.print("  Tijd:"); Serial.println(bfr[buffer].tijd);
}
void Write_Acc(int adr, byte reg) { //called from 'notifyDccMsg()'
	//verwerkt nieuw 'basic accessory decoder packet (artikel)
	//als preset[].reg bit0 true is de aan en de uit msg beide opslaan en verwerken.
	//Tijd opslaan in millis() , verschil geeft pulsduur getoont in de Off msg 
	//bij  bit0 false, alleen de On msg verwerken, opslaan en tonen, simpeler dus
	//kijken of msg nieuw is.	
	//Serial.print("*");

	byte Treg = reg; byte buffer;
	bool nieuw = true; Treg |= (1 << 7);
	for (byte i = 0; i < Bsize; i++) {
		//zoeken naar door artikel bezette buffer
		if (bfr[i].adres == adr && bfr[i].reg == Treg) {
			nieuw = false;
			//Serial.print("n");
		}
	}

	if (nieuw) {
		buffer = FreeBfr();
		if (buffer == Bsize)return; //uitspringen geen vrije buffer gevonden
		bfr[buffer].adres = adr;
		bfr[buffer].reg = reg;
		bfr[buffer].tijd = millis();
		bfr[buffer].reg |= (1 << 7); //set buffer active
	}
	Debug_bfr(false, buffer);
}
//IO alles met de uitvoer van de ontvangen msg. periodiek of manual
void IO_exe() {
	bool read = true; byte count = 0;
	//ervoor zorgen dat alle buffers in volgorde worden gelezen, dus Bcount

//	if (~GPIOR0 & (1 << 0)) return; //stoppen als er geen te verwerken msg zijn.

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
bool IO_dp() { //displays msg's 
	if (~bfr[Bcount].reg & (1 << 7))return true; //exit als niet vrij (alleen bij artikelen maken)
	
	//GPIOR2 = 0; //clear gpr, flags in functie
	
	//dp.setTextColor(WHITE);
	//******Locomotief
	if (~bfr[Bcount].reg & (1 << 6)) { //Locomotief
		return IO_DP_loc();
	}
	else {//*********//Accesoire	artikel
		//Tonen msg van accessoire werkt 10sept
		return IO_DP_art();
	}
}
bool IO_DP_art() {
	if (preset[Prst].reg & (1 << 0)) { //alleen de 'uit' tonen voor leesbaarheid
		if (bfr[Bcount].reg & (1 << 0)) return true; //alleen 'uit' msg verwerken
	}
	GPIOR2 = 0; //reset gpr
	unsigned int puls = 0; byte r = 0; byte d = 0;

	if (preset[Prst].reg & (1 << 0)) { //alleen uit msg verwerken, dus de aan weghalen en tijd uitrekenen			
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
	byte t = 11; //tekst 10=aan, 11=uit

	if (preset[Prst].reg & (1 << 1)) { //toon lijst
		scrolldown(); //bovenste regel vrijmaken
		Symbol(0, 0, d, 1); //teken symbool d (0=wissel cv, 1=wissel R, 2=wissel A)	
		Text(22, 0, 1, bfr[Bcount].adres, 0); //waarde adres, geen tekst
		if (d > 0) { //not(yet) in CV
			if (preset[Prst].reg & (1 << 0)) { //alleen uit msg tonen
				Symbol(49, 0, 10, 1); //symbool 10 puls
				Text(57, 0, 1, puls, 12); //puls met txt12 "ms"
			}
			else { //aan en uit msg tonen
				if (GPIOR2 & (1 << 0)) t = 10;
				Text(57, 0, 1, 0, t);
			}
		}
	}
	else { //toon enkel
		DP_mon(); //clears bovendeel display
		Symbol(3, 0, d, 2);
		Text(42, 0, 2, bfr[Bcount].adres, 0);
		if (d > 0) { //not in CV
			if (preset[Prst].reg & (1 << 0)) { //Alleen uit msg
				Symbol(13, 24, 10, 2);
				Text(30, 24, 2, puls, 12);
			}
			else { //aan en uit tonen				
				if (GPIOR2 & (1 << 0)) t = 10;
				Text(30, 24, 2, 0, t);
			}
		}
	}
	bfr[Bcount].reg &= ~(1 << 7); // buffer vrijgeven
	return false; //dus msg verwerkt
}
void Symbol(byte x, byte y, byte number, byte s) {
	switch (number) {
	case 0:
		drawWissel(x, y, s, 0);
		break;
	case 1: //wissel L
		drawWissel(x, y, s, 1);
		break;
	case 2: //wissel R
		drawWissel(x, y, s, 2);
		break;
	case 10: //puls
		drawPuls(x, y, s); //x-y-size
		break;
	case 20: //Loc
		drawLoc(x, y, s); //teken een loc
		break;
	}
}
void Text(byte x, byte y, byte s, unsigned int value, byte TXTnummer) {
	dp.setTextSize(s);
	dp.setTextColor(1);
	dp.setCursor(x, y);
	if (value > 0) dp.print(value);
	TXT(TXTnummer);
}
bool IO_DP_loc() {
	byte x; byte y; byte s;
	byte x2; byte x3;
	byte y2;
	byte speed = 0;
	byte temp; byte dt;
	// start display 
	if (preset[Prst].reg & (1 << 1)) { //lijst
		scrolldown(); //bovenste regel vrijmaken
		x = 0; y = 0; s = 1;
		x2 = 22; x3 = 49;
		y2 = 0;
	}
	else { //apart
		DP_mon(); //venster vrijmaken
		x = 3; y = 2; s = 2; x2 = 49; x3 = 13; y2 = 24;
	}

	//start display
	Symbol(x, y, 20, s); //teken symbool d (0=wissel cv, 1=wissel R, 2=wissel A)	
	Text(x2, y, s, bfr[Bcount].adres, 0); //waarde adres, geen tekst

	//soort msg
	if (bfr[Bcount].reg & (1 << 2)) { //drive
		//snelheid uitgaande CV29 bit1= true 28steps
		temp = bfr[Bcount].instructie << 4;
		temp = temp >> 4; //clear bit 7,6,5,4
		if (temp > 1) {
			speed = (temp - 1) * 2;
		}
		if (~bfr[Bcount].instructie & (1 << 4))speed--;

		// dir 
		temp = bfr[Bcount].instructie >> 5;
		if (temp == 3) { //forward
			dt = 15;
		}
		else { //reversed
			dt = 16;
		}
		Text(x3, y2, s, speed, dt);
	}

	bfr[Bcount].reg &= ~(1 << 7); // buffer vrijgeven
	return true;
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
void drawCheck(byte x, byte y, bool v) { //v=value
	y = y + 1;
	if (v) {
		dp.fillRect(x, y, 6, 6, 1);
	}
	else {
		dp.drawRect(x, y, 6, 6, 1);
	}
}
void TXT(byte n) {
	switch (n) {
	case 0: //niets
		dp.print(F(""));
		break;
	case 10:
		dp.print(F("aan"));
		break;
	case 11:
		dp.print(F("uit"));
		break;
	case 12:
		dp.print(F("ms"));
		break;
	case 15:
		dp.print(F(">>"));
		break;
	case 16:
		dp.print(F("<<"));
		break;
	}
}

