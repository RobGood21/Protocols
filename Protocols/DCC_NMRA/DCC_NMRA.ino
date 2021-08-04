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
//DCC_MSG  Packet;

// This function is called whenever a normal DCC Turnout Packet is received and we're in Board Addressing Mode
void notifyDccAccTurnoutBoard(uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower)
{
	Serial.print("notifyDccAccTurnoutBoard: ");
	Serial.print(BoardAddr, DEC);
	Serial.print(',');
	Serial.print(OutputPair, DEC);
	Serial.print(',');
	Serial.print(Direction, DEC);
	Serial.print(',');
	Serial.println(OutputPower, HEX);
}

// This function is called whenever a normal DCC Turnout Packet is received and we're in Output Addressing Mode
void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower)
{
	Serial.print("notifyDccAccTurnoutOutput: ");
	Serial.print(Addr, DEC);
	Serial.print(',');
	Serial.print(Direction, DEC);
	Serial.print(',');
	Serial.println(OutputPower, HEX);
}

// This function is called whenever a DCC Signal Aspect Packet is received
void notifyDccSigOutputState(uint16_t Addr, uint8_t State)
{
	Serial.print("notifyDccSigOutputState: ");
	Serial.print(Addr, DEC);
	Serial.print(',');
	Serial.println(State, HEX);
}


void setup()
{
	Serial.begin(9600);
	// Setup which External Interrupt, the Pin it's associated with that we're using and enable the Pull-Up 
	Dcc.pin(0, 2, 1); //interupt number 0; pin 2; pullup to pin2
	// Call the main DCC Init function to enable the DCC Receiver. 
	
	Dcc.init(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE,0);

	// Flag values to be logically ORed together and passed into the init() method
//#define FLAGS_MY_ADDRESS_ONLY        0x01	// Only process DCC Packets with My Address
//#define FLAGS_AUTO_FACTORY_DEFAULT   0x02	// Call notifyCVResetFactoryDefault() if CV 7 & 8 == 255
//#define FLAGS_SETCV_CALLED           0x10   // only used internally !!
//#define FLAGS_OUTPUT_ADDRESS_MODE    0x40  // CV 29/541 bit 6
//#define FLAGS_DCC_ACCESSORY_DECODER  0x80  // CV 29/541 bit 7




	Serial.println(F("DCC"));
}
void loop()
{
	// You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
	Dcc.process();
	//
	//if (FactoryDefaultCVIndex && Dcc.isSetCVReady())
	//{
	//	FactoryDefaultCVIndex--; // Decrement first as initially it is the size of the array 
	//	Dcc.setCV(FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
	//}
}
