/*
 Name:		DCC_NMRA.ino
 Created:	8/3/2021 9:11:10 AM
 Author:	gebruiker

 DCC communicatie testen met behulp van de NMRA library: https://github.com/mrrwa/NmraDcc 
 Zie ook: Protocol, werkblad (drive)
 NmraAccessoryDecoder_1 als basis genomen, daarna zo veel mogelijk uitgekleed.
 Deze decoder kan alles van de DCC aan incl.de feedback bij CV instellingen.
 Deze kale versie kan alleen nog basis dingen, voor meer opties, kijk in originele examples.
 

*/

#include <NmraDcc.h>
//***** begin**declaraties decoder NmraDcc
NmraDcc  Dcc;



//***************Call backs, van NmraDcc, voids worden aangeroepen vanuit de library....
//in toepassing is er maar eentje nodig, rest kan eruit
void notifyDccAccTurnoutBoard(uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower){
	//called als CV29 bit6 = false decoderadres,channel,poort,onoff (zie setup 'init')
	Serial.print(F("dec adres: "));
	Serial.print(BoardAddr, DEC);
	Serial.print(',');
	Serial.print(OutputPair, DEC);
	Serial.print(',');
	Serial.print(Direction, DEC);
	Serial.print(',');
	Serial.println(OutputPower, HEX);
}

// This function is called whenever a normal DCC Turnout Packet is received and we're in Output Addressing Mode
void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower){
	//Called als CV29 bit6=true dccadres,poort,onoff (zie init in setup)
	Serial.print(F("DCC adres: "));
	Serial.print(Addr, DEC);
	Serial.print(',');
	Serial.print(Direction, DEC);
	Serial.print(',');
	Serial.println(OutputPower, HEX);
}

// This function is called whenever a DCC Signal Aspect Packet is received
//void notifyDccSigOutputState(uint16_t Addr, uint8_t State)
//deze wordt gebruikt voor seinen, wie weet in de toekomst...

//{
//	Serial.print("notifyDccSigOutputState: ");
//	Serial.print(Addr, DEC);
//	Serial.print(',');
//	Serial.println(State, HEX);
//}
//*******************Einde call backs

void setup(){
	Serial.begin(9600);


	// Setup which External Interrupt, the Pin it's associated with that we're using and enable the Pull-Up 
	Dcc.pin(0, 2, 1); //interupt number 0; pin 2; pullup to pin2
	// Call the main DCC Init function to enable the DCC Receiver. 
	//init van de decoder, vereist, bepaalt hoe en wat van de decoder

	//Dcc.init(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE,0);
	
//		CV29_LOCO_DIR = 0b00000001,	/** bit 0: Locomotive Direction: "0" = normal, "1" = reversed */
//		CV29_F0_LOCATION = 0b00000010,	/** bit 1: F0 location: "0" = bit 4 in Speed and Direction instructions, "1" = bit 4 in function group one instruction */
//		CV29_APS = 0b00000100,	/** bit 2: Alternate Power Source (APS) "0" = NMRA Digital only, "1" = Alternate power source set by CV12 */
//		CV29_RAILCOM_ENABLE = 0b00001000, 	/** bit 3: BiDi ( RailCom ) is active */
//		CV29_SPEED_TABLE_ENABLE = 0b00010000, 	/** bit 4: STE, Speed Table Enable, "0" = values in CVs 2, 4 and 6, "1" = Custom table selected by CV 25 */
//		CV29_EXT_ADDRESSING = 0b00100000,	/** bit 5: "0" = one byte addressing, "1" = two byte addressing */
//		CV29_OUTPUT_ADDRESS_MODE = 0b01000000,	/** bit 6: "0" = Decoder Address Mode "1" = Output Address Mode */
//		CV29_ACCESSORY_DECODER = 0b10000000,	/** bit 7: "0" = Multi-Function Decoder Mode "1" = Accessory Decoder Mode */

	//bit 7 en bit 6 worden bovenstaand dus gezet, met dcc adres
	Dcc.init(MAN_ID_DIY, 10,0b11000000, 0); //idem maar met decoder adres

	//bit7 geeft aan loc decoder of accessory decoder
	//bit 6 true geeft aan of DCCadres terug komt, adres, poort, on/off 
	//of false decoderadres,channel, poort, on/off


	Serial.println(F("DCC"));
}
void loop(){
	Dcc.process();

}
