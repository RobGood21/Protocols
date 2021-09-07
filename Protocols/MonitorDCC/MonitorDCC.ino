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
	//bit4 True: msg=function1
	//bit5 True: msg=Function2 
	//bit6
	//bit6 true: accesoire; false: loc
	//bit7 true: bezet; false: vrij

	unsigned int adres; //L&A: bevat adres acc en loc
	unsigned long tijd; //L&A: bevat tijd laatste aanpassing
	byte instructie; //Loc: bevat waarde instructie byte
	int CV;
	byte CVvalue;
}; buffers bfr[Bsize]; //aantal buffer artikelen

//variabelen

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
	if (GPIOR0 & (1 << 1))return; //Stop als buffer vol is
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
		//accesoire bit6 blijft false


		//bit6 van reg blijft false (artikel)		
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
		if (bfr[i].reg & (1 << 7) || bfr[i].reg & (1 << 6)) { //tonen of actieve loc
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
		//eventueel dit herhalen in de callback anders een volgorde op ingevulde tijd
		//toevoegen als de volgorde van msg fouten geeft.
		Bcount = 0;
	}

	if (GPIOR0 & (1 << 1))PIND |= (1 << 4);
}

void Loc(bool t) {
	if (~preset[Prst].filter & (1 << 0))return; //Filter voor Loc msg

	int adres; byte instr = 0; byte instrByte;
	/*
	000 Decoder and Consist Control Instruction
		001 Advanced Operation Instructions 115
		010 Speed and Direction Instruction for reverse operation
		011 Speed and Direction Instruction for forward operation
		100 Function Group One Instruction
		101 Function Group Two Instruction
		111 Configuration Variable Access Instruction
*/

	if (t) { //7 bits adres
		adres = data[0];
		instr = data[1] >> 5;
		instrByte = 1;
	}
	else { //14 bits adres
		adres = data[1];
		if (data[0] & (1 << 0))adres += 256;
		if (data[0] & (1 << 1))adres += 512;
		if (data[0] & (1 << 2))adres += 1024;
		if (data[0] & (1 << 3))adres += 2048;
		if (data[0] & (1 << 4))adres += 4096;
		if (data[0] & (1 << 5))adres += 8192;
		instr = data[2] >> 5;
		instrByte = 2;
	}

	//Filters en 'bekende' msg weglaten

	if ((instr == 2 || instr == 3) && preset[Prst].filter & (1 << 1)) { //direction and speed
		LocDrive(adres, instrByte); //instrByte geeft aan welk databyte de instructie bevat
	}
	else if (instr == 4) { //function group 1

	}
	else if (instr == 5) { //Function group 2

	}
	else if (instr == 7) { //CV

	}
	else {
		return;// instructions 000 en 001 en 110 niet doorlaten
	}
	//Serial.print("Adres: "); Serial.print(adres);Serial.print("   INstr: "); Serial.println(instr);
}
void LocDrive(int adres, byte instrByte) { //called from loc()
	
	//Verwerkt direction en speed msg van loco
	//zoeken of er een buffer voor deze loc dit type msg is. dus adres en byte 1 of 2
	//Kijken of de instructie byte is veranderd
	//zoja dan instructie byte vervangen en buffer actief stellen

	byte found = 0;
	for (byte i = 0; i < Bsize; i++) {
		if (bfr[i].reg & (1 << 6) && bfr[i].reg & (1<<2) && bfr[i].adres == adres ){
			//actieve loc drive gevonden
			//check for change
			Serial.println("bfr gevonden");
			if (bfr[i].instructie ^ data[instrByte] == 0) {
				return; //geen verandering, herhaalde msg
			}
			else { //msg veranderd aanpassen
				bfr[i].instructie = data[instrByte];
				bfr[i].reg |= (1 << 7); //msg weer tonen
				return; //verlaat function
			}
			found = i;
		}
		else { //geen buffer gevonden
			Serial.println("geen bfr");
			Write_Loc(adres, data[instrByte]);
		}
	}
	
	//if (found > 0) Serial.print("Adres: "); Serial.print(adres); Serial.print(" jo "); //Serial.println(instr);

}
void LocFunction() { //called from loc()
	//
}
void LocCV() { //called from loc()

}
void Write_Loc(int adres, byte instr) { //adres en instructiebyte
	byte free = 0; //vrije buffer
	//vrije buffer zoeken, in stappen
	//Eerst zoeken naar 'oude' loc msg
	for (byte i = 0; i < Bsize; i++) {
		if (millis() - bfr[i].tijd > autoDelete) {//Buffer 10 sec niet gebruikt 
			free = i; //gevonden te gebruiken buffer
			i = Bsize; //for loop verlaten
		}
	}
	if (free > 0) {
		//buffer aanmaken...

	}
	else {
		//hier nog een fout afhandeling als er geen vrije buffer wordt gevonden.
	}
}
void Write_Acc(int adr, byte reg) { //called from 'notifyDccMsg()'
	//verwerkt nieuw 'basic accessory decoder packet (artikel)
	//als preset[].reg bit0 true is de aan en de uit msg beide opslaan en verwerken.
	//Tijd opslaan in millis() , verschil geeft pulsduur getoont in de Off msg 
	//bij  bit0 false, alleen de On msg verwerken, opslaan en tonen, simpeler dus
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


	if (nieuw) {
		//Serial.println("new"); 
		//als geen vrije buffer wordt gevonden dan command niet verwerkt
		for (byte i = 0; i < Bsize; i++) {
			if (~bfr[i].reg & (1 << 7) && ~bfr[i].reg & (1<<6)) { //bit7 false (tonen) bit6 false(actief loc)
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
		if (preset[Prst].reg & (1 << 0)) { //alleen de 'uit' tonen voor leesbaarheid
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
		if (preset[Prst].reg & (1 << 1)) { //toon lijst
			scrolldown();
			dp.setCursor(0, 0);
			dp.setTextSize(1); //6 pixels breed, 8 pixels hoog 10 pixels is regelhoogte
			drawWissel(0, 0, 1, d);
			dp.setCursor(22, 0); dp.print(bfr[Bcount].adres);

			if (d > 0) { //not in CV
			//select aan/uit of alleen uit	

				dp.setCursor(57, 0);
				if (preset[Prst].reg & (1 << 0)) { //alleen uit msg tonen
					drawPuls(49, 0, 1); //x-y-size
					dp.print(puls);
					TXT(12); //"ms"
				}
				else { //aan en uit msg tonen
					if (GPIOR2 & (1 << 0)) {
						TXT(10); //"aan"
					}
					else {
						TXT(11); //"uit"
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

				if (preset[Prst].reg & (1 << 0)) { //Alleen uit msg
					drawPuls(13, 18, 2); //x-y-size
					dp.print(puls);
					TXT(12); //"ms"
				}
				else { //aan en uit tonen
					if (GPIOR2 & (1 << 0)) {
						TXT(10); //"aan"
					}
					else {
						TXT(11); //"uit"
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

	case 10:
		dp.print(F("aan"));
		break;
	case 11:
		dp.print(F("uit"));
		break;
	case 12:
		dp.print(F("ms"));
		break;
	}
}

