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
	//bit1	True : Toon lijst onder elkaar, of false : 1 grote msg.
	//bit2	
	byte filter; //welke msg verwerken, true is verwerken, false is overslaan
	//bit0 //loc
	//bit1 //speed, direction 'R''
	//bit2 //functions 'F'
	//bit3 //CV  'CV'
	//bit4 //artikel 
	//bit5 //toon alleen 'AAN'
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
	byte db[3]; //tijdelijk van 2 > 3
}; buffers bfr[Bsize]; //aantal buffer artikelen

//variabelen

int T_adres = 0; //tijdelijk adres
byte T_instructie = 0; //Tijdelijk opslag instructie byte
byte T_db[2]; //Tijdelijke opslag

byte out[2]; //

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
	
	MEM_read();
	//xtra init
	GPIOR2 = 0;

	//tijdelijk
	out[0] = 138; out[1] = 33;

	//displays

DP_monitor();
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
void DP_monitor() { //maakt display schoon, toont output bytes
	dp.clearDisplay();
	byte x = 0; byte bte = 0;
	//onderbalk met 2 plus 4xnibble output bytes
	for (byte i = 0; i < 16; i++) {
		if (i > 7)bte = 1;

		if (out[bte] & (1 << i - (bte * 8))) {

			dp.drawRect(x + (i * 7), 57, 6, 7, 1);
			//dp.fillRect(x+3,60,2,2,1);
		}
		else {
			dp.fillRect(x + (i * 7), 57, 6, 7, 1);
		}

		if (i == 3 || i == 11)x += 3;
		if (i == 7)x += 6;
	}
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
			DP_monitor();
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

	//alleen als artikel check is true
	if (preset[Prst].filter & (1 << 4)) {

		drawCheck(x[1], y, preset[Prst].filter & (1 << 5));
		dp.setCursor(x[1] + 8, y); dp.print(F("S")); //Switch msg 
		if (prgfase == 7) { px = x[1]; py = y + 8; w = x[1] + 12; h = y + 8; } //cursor Artikelen CV

		drawCheck(x[2], y, preset[Prst].reg & (1 << 0)); drawPuls(x[2] + 8, y, 1);
		dp.setCursor(x[2] + 8, y);// dp.print(F("puls")); //puls of aan/uit	
		if (prgfase == 8) { px = x[2]; py = y + 8; w = x[2] + 10; h = y + 8; } //cursor Artikelen switch

		drawCheck(x[3], y, preset[Prst].filter & (1 << 6));
		dp.setCursor(x[3] + 8, y); dp.print(F("CV")); //CV	
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
		preset[Prst].reg ^= (1 << 0); //Tonen Acc CV
		break;
	case 9:
		preset[Prst].filter ^= (1 << 6); //tonen puls of aan uit msg
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
	//direct achter elkaar ontvangen gelijke msg's uitfilteren
	bool nieuw = true;
	for (byte i = 0; i < MAX_DCC_MESSAGE_LEN; i++) {
		if (lastmsg[i] != Msg->Data[i]) nieuw = false;
	}
	if (nieuw)return;
	//databytes uit de library opslaan in tijdelijk geheugen
	for (byte i; i < 6; i++) {
		data[i] = Msg->Data[i];
	}
	//verdelen op soort msg op basis van 1e byte
	byte db;  byte bte;  int adr = 0; byte reg = 0;
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
		reg |= (1 << 6); //accessoire



		if ((data[2] >> 2) == B111011) { //kan niet is nu checksum
			reg |= (1 << 3);  //("CV");
			Write_AccCV(adr);
		}
		else {
			//als switch filter is false alleen 'AAN' msg doorlaten
			if (preset[Prst].filter & (1 << 5) || reg & (1 << 0)) Write_Acc(adr, reg); // accessoire msg ontvangen				
		}
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
			nt = false;
			LocMsg(1);
		}
		break;
	case B011:
		if (preset[Prst].filter & (1 << 1)) {
			nt = false;
			LocMsg(1);
		}
		break;
	case B100:
		if (preset[Prst].filter & (1 << 2)) {
			nt = false;
			LocMsg(2);
		}
		break;
	case B101:
		if (preset[Prst].filter & (1 << 2)) {
			nt = false;
			LocMsg(3);
		}
		break;
	case B111:
		if (preset[Prst].filter & (1 << 3)) {
			nt = false;
			LocMsgCV();
		}
		break;
	}
	if (nt)return; //deze msg niks mee doen
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
	//Debug_bfr(true, buffer);
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
	//Debug_bfr(false, b); //print buffer inhoud
}
byte FreeBfr() {

	for (byte b = 0; b < Bsize; b++) {
		if (~bfr[b].reg & (1 << 7)) {
			if (bfr[b].reg == 0x00) return b;
			if (bfr[b].reg & (1 << 6))return b;
			if (bfr[b].reg & (1 << 3)) return b;
		}
	}

	for (byte i = 0; i < Bsize; i++) {
		if (~bfr[i].reg & (1 << 7)) {
			if (millis() - bfr[i].tijd > autoDelete)return i;
		}
	}

	//hier kan nog zoeken naar een hele oude buffer die is blijven hangen

	return Bsize;
}
void Write_Loc() { //adres en instructiebyte
	byte free = FreeBfr();
	//Serial.println(free);
	if (free == Bsize) return;  //Hier misschien nog een foutafhandeling?
	clearBuffer(free);

	bfr[free].adres = T_adres;
	bfr[free].instructie = T_instructie; //= eerste byte na adres.
	bfr[free].db[0] = 0;
	bfr[free].db[1] = 0;
	bfr[free].reg = 0;
	bfr[free].reg |= (1 << 7); //7=tonen 6=loc(false)
	bfr[free].reg |= (1 << 2); //type Loc message 2 = drive/functions 3 = CV 
	bfr[free].tijd = millis();
	//Debug_bfr(true, free);
}
void clearBuffer(byte buffer) {
	bfr[buffer].reg = 0;
	bfr[buffer].adres = 0;
	bfr[buffer].instructie = 0;
	bfr[buffer].tijd = 0;
	bfr[buffer].db[0] = 0;
	bfr[buffer].db[1] = 0;
	bfr[buffer].db[2] = 0;
}
void Debug_bfr(bool n, byte buffer) {
	/*
	Bij een drive msg wordt nu de checksum in de CV geschreven, maakt denk ik niet uit omdat deze CV
	niet wordt getoond bij deze type msg. uit filteren kan maar geeft extra instructies
	*/
	if (n) Serial.print("New ");
	Serial.print(buffer); Serial.print("  Adres:"); Serial.print(bfr[buffer].adres);
	Serial.print(" Reg:"); Serial.print(bfr[buffer].reg, BIN);
	Serial.print(" instr:"); Serial.print(bfr[buffer].instructie, BIN);

	Serial.print(" db0:"); Serial.print(bfr[buffer].db[0], BIN); Serial.print(" db1:"); Serial.print(bfr[buffer].db[1], BIN);
	Serial.print(" db2:"); Serial.print(bfr[buffer].db[2], BIN);
	Serial.print("  Tijd:"); Serial.println(bfr[buffer].tijd);
}

void Write_AccCV(int adr) {

	Serial.println("cv");

	//schrijft een Accesoire CV in een buffer, deze msg wordt niet herhaald dus geen routine nodig
	//voor uitvangen dubbele
	//databytes en reg is voldoende 
	byte buffer = FreeBfr();
	if (buffer == Bsize)return;
	clearBuffer(buffer);

	bfr[buffer].adres = adr;
	bfr[buffer].reg = B11001000, //bit7 active bit6 accessoire bit 3 CV msg
		bfr[buffer].instructie = data[1];
	bfr[buffer].db[0] = data[2];
	bfr[buffer].db[1] = data[3];
	bfr[buffer].db[2] = data[4];
	bfr[buffer].tijd = millis();
	//Debug_bfr(true, buffer);
	//Serial.println("accCV");
}
void Write_Acc(int adr, byte reg) { //called from 'notifyDccMsg()'

	///Serial.println("**");

	byte Treg = reg; byte buffer;
	bool nieuw = true; Treg |= (1 << 7);
	for (byte i = 0; i < Bsize; i++) {
		//zoeken naar door artikel bezette buffer
		if (bfr[i].adres == adr && bfr[i].reg == Treg) {
			nieuw = false;
		}
	}

	if (nieuw) {
		buffer = FreeBfr();
		if (buffer == Bsize)return; //uitspringen geen vrije buffer gevonden
		clearBuffer(buffer);
		bfr[buffer].adres = adr;
		bfr[buffer].reg = reg;
		bfr[buffer].tijd = millis();
		bfr[buffer].reg |= (1 << 7); //set buffer active
	}
	//Debug_bfr(false, buffer);
}
//IO alles met de uitvoer van de ontvangen msg. periodiek of manual
void IO_exe() { //toont een buffer, called manual, time, or direct from loop.
	bool read = true; byte count = 0;
	while (read) {
		if (bfr[Bcount].reg & (1 << 7)) read = IO_dp(); //Alleen als bfr[].reg bit7=true
		Bcount++;
		if (Bcount == Bsize)Bcount = 0;
		count++;
		if (count > Bsize)read = false; //exit als er geen te tonen buffer is gevonden
	}
	//dp.display();
}
bool IO_dp() { //displays msg's 
	//venster instelling
	byte s = 1; byte speed = 0; bool drive = false; int puls;
	byte symbolE[4]; symbolE[0] = 20; //default loc
	//kolommen
	byte xE[5]; //afstanden horizontaal begin
	//rijen
	byte yR[3]; //rijen 
	int CVadres; byte CVvalue;

	//Type msg en symbol bepalen
	if (bfr[Bcount].reg & (1 << 6)) { //accesoire


		//puls mode
		if (preset[Prst].filter & (1 << 5)) { //switching mode on
			if (preset[Prst].reg & (1 << 0)) { //alleen de 'uit' tonen met pulsduur
				if (bfr[Bcount].reg & (1 << 0)) return true; //buffer is een 'aan' accessoire, overslaan
			}
		}

		//aan/uit mode of een uit msg 
		if (bfr[Bcount].reg & (1 << 3)) { //accessoire CV msg
			symbolE[0] = 0; //CV
			//CVadres en value berekenen
			CVadres = bfr[Bcount].db[1] + 1;
			if (bfr[Bcount].db[0] & (1 << 0))CVadres += 256;
			if (bfr[Bcount].db[0] & (1 << 1))CVadres += 512;
			CVvalue = bfr[Bcount].db[2];
		}
		else if (bfr[Bcount].reg & (1 << 1)) { //Accessoire afslaan
			symbolE[0] = 1;
		}
		else { //Accessoire Rechtdoor
			symbolE[0] = 2;
		}
	}
	else { //locomotief
		drive = true;
		if (bfr[Bcount].reg & (1 << 3)) { //CV instelling
			//cv  berekenen
			CVadres = bfr[Bcount].db[0] + 1;
			CVvalue = bfr[Bcount].db[1];
			if (bfr[Bcount].instructie & (1 << 0))CVadres += 256;
			if (bfr[Bcount].instructie & (1 << 1))CVadres += 512;
		}
		else { //drive aanpassing 			
			if (bfr[Bcount].instructie & (1 << 5)) {
				symbolE[1] = 21;
			}
			else {
				symbolE[1] = 22;
			}
			//snelheidsstap bepalen, altijd denkend CV#29 bit1=true 28snelheidsstappen.
			//Xtra digitale snelheidstrap 128stappen nog niet 16sept2021
			GPIOR2 = bfr[Bcount].instructie << 4;
			GPIOR2 = GPIOR2 >> 4; //clear bit 7,6,5,4
			if (GPIOR2 > 1) {
				speed = (GPIOR2 - 1) * 2;
			}
			if (~bfr[Bcount].instructie & (1 << 4)) speed--;
		}
	}
	//scherm type instellen apart of een lijst
	if (preset[Prst].reg & (1 << 1)) { //lijst
		scrolldown();
		xE[0] = 0; xE[1] = 15; xE[2] = 40; xE[3] = 52; xE[4] = 85;
		yR[0] = 0; yR[1] = 0; yR[2] = 0;
	}
	else { //single msg
		DP_monitor();
		s = 2; //Size objecten
		xE[0] = 2; xE[1] = 40; xE[2] = 5; xE[3] = 30; xE[4] = 85;
		yR[0] = 3; yR[1] = 22; yR[2] = 40;
	}
	//Display
	DP_symbol(xE[0], yR[0], symbolE[0], s);
	setText(xE[1], yR[0], s);
	dp.print(bfr[Bcount].adres); //adres


	if (bfr[Bcount].reg & (1 << 3)) { //CV
		setText(xE[2], yR[1], s);
		dp.print("CV"); dp.print(CVadres);
		setText(xE[4], yR[1], s); dp.print(CVvalue);

		//als in single mode CV value byte binair tonen
		if (~preset[Prst].reg & (1 << 1)) { //lijst
			byte x = xE[0]; byte y = yR[2] + 2;
			byte data = 1; //loc CV

			if (bfr[Bcount].reg & (1 << 6))data = 2; //acc CV 
			for (byte i = 7; i < 255; i--) {
				if (bfr[Bcount].db[data] & (1 << i)) {
					dp.fillRect(x, y, 4, 12, 1);
				}
				else {
					dp.drawLine(x, y + 11, x + 4, y + 11, 1);
				}
				x += 6;
			}
		}
	}
	else { //niet CV
		if (bfr[Bcount].reg & (1 << 6)) { //accesoire

			//alleen tonen als Switching bit5 in preset filter =true
			if (preset[Prst].filter & (1 << 5)) {
				if (preset[Prst].reg & (1 << 0)) { //pulslengte tonen
					byte change;
					for (byte i = 0; i < Bsize; i++) {
						//volgorde belangrijk
						change = bfr[i].reg ^ bfr[Bcount].reg;// reken maar na klopt...alleen bit0 van .reg = verschillend				
						if (bfr[i].adres == bfr[Bcount].adres && change == 1) {
							puls = bfr[Bcount].tijd - bfr[i].tijd;
							bfr[i].reg &= ~(1 << 7);//buffer vrijgeven
							i = Bsize; //exit lus
						}
					}
					DP_symbol(xE[2], yR[1], 10, s);
					setText(xE[3], yR[1], s);
					dp.print(puls); dp.print("ms");
				}

				else { //aan en uit msg tonen
					if (bfr[Bcount].reg & (1 << 0)) {
						dp.fillRect(xE[2], yR[1], s * 10, s * 7, 1);
					}
					else {
						dp.drawRect(xE[2], yR[1], s * 10, s * 7, 1);
					}
				}
			}
		}
		else { //locomotief
			DP_symbol(xE[2], yR[1], symbolE[1], s); //symbool accessoire aan/uit  loc forward/reverse
			setText(xE[3], yR[1], s);
			dp.print(speed);
			if (preset[Prst].filter & (1 << 2)) { //Functions tonen
				//verschil tussen lijst en single, lijst alleen FL, single alle functions (alle 28 dus)
				if (preset[Prst].reg & (1 << 1)) { //lijst
					if (bfr[Bcount].db[0] & (1 << 4)) {
						dp.fillCircle(xE[4], yR[1] + 3, 3, 1); //Veel Program ruimte nodig,letters kleiner 
					}
				}
				else { //single
					functions(xE[0], yR[2]);
				}
			}
		}
	}

	//afsluiting
	bfr[Bcount].reg &= ~(1 << 7); // buffer vrijgeven
	dp.display();
	return false;
}
void functions(byte x, byte y) {
	byte z = 12;
	for (byte i = 0; i < 13; i++) {
		if (Fmap(i)) {
			dp.fillRect(x, y, 4, z, 1);
		}
		else {
			dp.drawLine(x, y + 12, x + 4, y + 12, 1);
		}
		if (i == 0) {
			x += 6;
		}
		if (i == 4 || i == 8)x += 4;
		x += 6;
	}
}
bool Fmap(byte positie) {
	//mapping van de functions positie in de rij van bits
	bool onoff;
	switch (positie) {
	case 0:
		onoff = bfr[Bcount].db[0] & (1 << 4);
		break;
	case 1:
		onoff = bfr[Bcount].db[0] & (1 << 0);
		break;
	case 2:
		onoff = bfr[Bcount].db[0] & (1 << 1);
		break;
	case 3:
		onoff = bfr[Bcount].db[0] & (1 << 2);
		break;
	case 4:
		onoff = bfr[Bcount].db[0] & (1 << 3);
		break;
	case 5:
		onoff = bfr[Bcount].db[1] & (1 << 0);
		break;
	case 6:
		onoff = bfr[Bcount].db[1] & (1 << 1);
		break;
	case 7:
		onoff = bfr[Bcount].db[1] & (1 << 2);
		break;
	case 8:
		onoff = bfr[Bcount].db[1] & (1 << 3);
		break;
	case 9:
		onoff = bfr[Bcount].db[1] & (1 << 4);
		break;
	case 10:
		onoff = bfr[Bcount].db[1] & (1 << 5);
		break;
	case 11:
		onoff = bfr[Bcount].db[1] & (1 << 6);
		break;
	case 12:
		onoff = bfr[Bcount].db[1] & (1 << 7);
		break;
	}
	return onoff;
}
void DP_symbol(byte x, byte y, byte number, byte s) {
	switch (number) {
	case 0: //cv accessoire
		drawWissel(x, y, s, 0);
		break;
	case 1: // Accessoire Afslaan
		drawWissel(x, y, s, 1);
		break;
	case 2: //Accessoire Rechtdoor
		drawWissel(x, y, s, 2);
		break;
	case 10: //puls
		drawPuls(x, y, s); //x-y-size
		break;
	case 20: //Loc
		drawLoc(x, y, s); //teken een loc
		break;
	case 21: //loc forward
		//triangels gebuiken veel program size !!! misschien kleiner te maken?
		dp.drawTriangle(x, y + 3 * s, x + 4 * s, y, x + 4 * s, y + 6 * s, 1);
		dp.fillTriangle(x + 10 * s, y + 3 * s, x + 6 * s, y, x + 6 * s, y + 6 * s, 1);
		break;
	case 22: //loc reversed
		dp.fillTriangle(x, y + 3 * s, x + 4 * s, y, x + 4 * s, y + 6 * s, 1);
		dp.drawTriangle(x + 10 * s, y + 3 * s, x + 6 * s, y, x + 6 * s, y + 6 * s, 1);
		break;

	}
}
void setText(byte x, byte y, byte s) {
	dp.setTextSize(s);
	dp.setTextColor(1);
	dp.setCursor(x, y);
}
void scrolldown() {
	//blok van 50 bovenste lijnen 10 (regelhoogte nog uitzoeken) naar beneden plaatsen
	//regel voor regel onderste regel y=40~y=49 zwart maken
	//blijft dus 15pixels over nu aan onderkant

	for (byte y = 45; y < 56; y++) { //45 kleiner om onderbalk te verhogen (10 hoog)
		for (byte x = 0; x < 129; x++) {
			dp.drawPixel(x, y, BLACK);
		}
	}

	for (byte y = 45; y < 255; y--) { //hier weer de 45
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
	dp.fillRect(x + s * 1, y, s * 5, s * 1, 1);//dak
	dp.fillRect(x + s * 4, (y + 2 * s), s * 8, s * 3, 1); //ketel
	dp.fillRect(x + s * 10, y, s * 1, s * 2, 1); //schoorsteen
	dp.fillRect(x + 5 * s, (y + 1 * s), s * 1, s * 1, 1); //raam
	dp.fillRect(x, (y + 3 * s), s * 2, s * 1, 1); //bok
	dp.fillRect(x, (y + 4 * s), s * 4, s * 1, 1); //vloer
	dp.fillRect(x + 1 * s, (y + 5 * s), s * 5, s * 1, 1); //onderstel
	dp.fillRect(x + 7 * s, (y + 5 * s), s * 5, s * 1, 1); //onderstel voor
	dp.fillRect(x + 2 * s, (y + 6 * s), s * 3, s * 1, 1); //wiel
	dp.fillRect(x + 8 * s, (y + 6 * s), s * 3, s * 1, 1); //wiel voor
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
		//case 10:
		//	dp.print(F("aan"));
		//	break;
		//case 11:
		//	dp.print(F("uit"));
		//	break;
	case 12:
		dp.print(F("ms"));
		break;
		//case 15:
		//	dp.print(F(">>"));
		//	break;
		//case 16:
		//	dp.print(F("<<"));
		//	break;
	}
}

