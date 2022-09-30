/*
 Name:		Knoppen.ino
 Created:	9/17/2022 8:45:45 AM
 Author:	Rob

 Sketch met minimale footprint om 4 aan/uit drukknoppen te maken.
 
 */


//*******van hier.......>>>>>
unsigned long slowtime;
byte laatst;

// the setup function runs once when you press reset or power the board
void setup() {
PINC |= (15 << 0); //pull ups op pinnen A0~A3 (moet eigenlijk portC zijn) 
DDRB |= (15 << 0); //pins 8~11 als output
}

// the loop function runs over and over again until power down or reset
void loop() {

	if (millis() - slowtime > 20) { //gebruik ingebakken timer voor een 50hz timer
		slowtime = millis();
		leesknoppen();
	}  
}
void leesknoppen() {
	byte nu = PINC; //lees de knoppen
	byte veranderd = laatst ^ nu; //xor de twee bytes voor een verandering
	if (veranderd > 0) { //er is wat veranderd
		for (byte i = 0; i < 4; i++) {
			if (veranderd & (1 << i) && (~nu & (1<<i))) { //deze pin is ingedrukt
				knopactie(i);
			}
		}
	}
	laatst = nu; //zet huidige stand knoppen in laatst
}
void knopactie(byte knop) {
//de actie voor  een ingedrukte knop	
PORTB ^= (1 << knop); //flip de output (ledje?)
}

//********Tot hier.....