#include <Wire.h>
#include "Adafruit_MPR121.h"

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

Adafruit_MPR121 cap = Adafruit_MPR121();

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

#define COMMON_ANODE

#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif


#define wipeB A4     // Button pin for WipeMode

#define stepPin 5
#define dirPin  6
#define enPin   8
#define RECV_PIN 7
#define ledStrip 3

#include <EEPROM.h>     // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>
#include <IRremote.h>
#include <Servo.h>
#include <NeoPixelBus.h>

const uint16_t PixelCount = 24; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 3; 

IRrecv irrecv(RECV_PIN);
decode_results results;

#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);

long numre[] = {0xE0E08877,0xE0E020DF,0xE0E0A05F,0xE0E0609F,0xE0E010EF,0xE0E0906F,0xE0E050AF,0xE0E030CF,0xE0E0B04F,0xE0E0708F};
int kode[6];
int kodeTjek[] = {1,2,3,4,5,6};         
int tjekKoden = 0;

Servo myservo;
NeoPixelBus<NeoRgbwFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

void setup() {
    if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");
  pinMode(wipeB, INPUT_PULLUP);
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enPin, OUTPUT);
  pinMode(ledStrip, OUTPUT);
  digitalWrite(dirPin, LOW);
  digitalWrite(enPin, HIGH); 
  irrecv.enableIRIn();
  
  
  Serial.begin(115200);
  setupRfid();
  strip.Begin();
  strip.Show();
  drej(0,0);
  }

String inputString = "";

long nyTid;
long gammelTid;
int tal;
char streng[200];
bool lys;
bool stringComplete = false;
int hastighed = 1000;
bool programMode = false;
bool erKodeRigtig[] = {false,false,false,false,false,false};
bool infrarodKlaret = false;
bool matchKlaret = false;

int tryk;
int i=0;
int pos = 0;
uint8_t successRead;
int kodeTal;

byte storedCard[4];   // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module
byte masterCard[4];

byte colorSaturation = 255;
RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

bool boolLys = true;

int x = 150;
int y = 120;
int gammelHoz;
int vert;
int hoz;
int xArray[3];
int yArray[3];
String xString;
String yString;
float posHoz;
float posVert;
int forrigeHoz;
int forrigeVert;
int nuHoz = 0;
int nuVert = 60;
bool numpad = false;

void loop() {
  loopRfid();
  //infraRod();
}

String tjek = "";
int tjekMatch = 0;

void loopRfid(){ 
  do {
    successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0
    // When device is in use if wipe button pressed for 10 seconds initialize Master Card wiping
    if (digitalRead(wipeB) == LOW) { // Check if button is pressed
      // Visualize normal operation is iterrupted by pressing wipe button Red is like more Warning to user
    strip.ClearTo(RgbColor(0,100,0)); 
    strip.Show();
      // Give some feedback
      Serial.println(F("Wipe Button Pressed"));
      Serial.println(F("Master Card will be Erased! in 10 seconds"));
      bool buttonState = monitorWipeButton(10000); // Give user enough time to cancel operation
      if (buttonState == true && digitalRead(wipeB) == LOW) {    // If button still be pressed, wipe EEPROM
        EEPROM.write(1, 0);                  // Reset Magic Number.
        Serial.println(F("Master Card Erased from device"));
        Serial.println(F("Please reset to re-program Master Card"));
        while (1);
      }
      Serial.println(F("Master Card Erase Cancelled"));
    }
    if (programMode) {
      cycleLeds();// Program Mode cycles through Red Green Blue waiting to read a new card
    }
    else {
      normalModeOn();     // Normal mode, blue Power LED is on, all others are off
    }
  }
  while (!successRead);   //the program will not go further while you are not getting a successful read
  if (programMode) {
    if ( isMaster(readCard) ) { //When in program mode check First If master card scanned again to exit program mode
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      i=0;
      programMode = false;
      infrarodKlaret = false;
      matchKlaret = false;
      tjekKoden = 0;
      for (int i=0; i<6; i++){
        erKodeRigtig[i] = false;
      }
    }
    else {
      if ( findID(readCard) ) { // If scanned card is known delete it
        Serial.println(F("I know this PICC, removing..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
      else {                    // If scanned card is not known add it
        Serial.println(F("I do not know this PICC, adding..."));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
    }
  }
  else {
    if ( isMaster(readCard)) {
      strip.ClearTo(RgbColor(0,20,0));
      strip.Show();// If scanned card's ID matches Master Card's ID - enter program mode
      programMode = true;
      Serial.println(F("Hello Master - Entered Program Mode"));
      uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that
      Serial.print(F("I have "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      Serial.println(F("Scan Master Card again to Exit Program Mode"));
      Serial.println(F("-----------------------------"));
      while (infrarodKlaret==false){
        infraRod();
        if (infrarodKlaret==true){
          drej(60,0);  
        for (int i=0; i<255; i++){
            strip.ClearTo(RgbwColor(0,0,0,i));
            strip.Show();
            delay(5);
         }
        }
      } 
      while (matchKlaret == false){
        esp();
      }
      strip.ClearTo(RgbwColor(0,0,0,10));
      strip.Show();
    }
    else {
      if ( findID(readCard) ) { // If not, see if the card is in the EEPROM
        Serial.println(F("Welcome, You shall pass"));
        granted(300);         // Open the door lock for 300 ms
      }
      else {      // If not, show that the ID was not valid
        Serial.println(F("You shall not pass"));
        denied();
      }
    }
  }
}
int light;

bool lysTilstand;
bool start = false;

void esp(){
  tjekMatch = 0;
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    streng[tal] = inChar;
    tal++;
    inputString += inChar;
    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
      if (stringComplete) {
    if (inputString == "Match Face ID: 0\n" || inputString == "Match Face ID: 1\n" || inputString == "Match Face ID: 2\n" || inputString == "Match Face ID: 3\n" || inputString == "Match Face ID: 4\n"){
      drej(0,0);
      strip.ClearTo(RgbwColor(0,0,0,0));
      strip.Show();
      light = 0;
      Serial.println("MATCH!!");
      Serial.end();
      myservo.attach(4);
        for (pos = 0; pos <= 90; pos += 1) {
          myservo.write(pos);              // tell servo to go to position in variable 'pos'
          delay(15);                       // waits 15ms for the servo to reach the position
    }
    delay(10000);
        for (pos = 90; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
          myservo.write(pos);              // tell servo to go to position in variable 'pos'
          delay(15);                       // waits 15ms for the servo to reach the position
      }
        myservo.detach();
        Serial.begin(115200);
        matchKlaret = true;
    }
    else{
      if ('x'==streng[0]){
        xString += streng[1];
        xString += streng[2];
        xString += streng[3];
        x = xString.toInt();
        Serial.print("x er ");
        Serial.print(x);
        xString = "";
      }
    if ('y'==streng[0]){
        yString += streng[1];
        yString += streng[2];
        yString += streng[3];
        y = yString.toInt();
        Serial.print("y er ");
        Serial.println(y);
        yString = "";
      }
    if (y < 30 || y > 110){
        Serial.end();
        nuVert = nuVert + map(x, 0, 240, -5, 5);
        drej(nuVert, nuHoz);
        y = 100;
        Serial.begin(115200);      
        Serial.println("y er");
    }
    if (x < 130 || x > 190){
        Serial.end();
        nuHoz = nuHoz + map(x,0,320,-115,115);
        drej(55, nuHoz);
        x = 150;
        Serial.begin(115200);
        Serial.print("nuhoz er ");
        Serial.println(nuHoz);  
    }
    for (int i=0; i<100; i++){
      Serial.print(streng[i]);
      streng[i] = 0;      
    }
    tal = 0;
    }
    // clear the string:
    inputString = "";
    tjek = "";
    tjekMatch = 0;
    stringComplete = false;
  }
}


void infraRod(){ //tjek om den har modtaget et nummer
    touchTjek();  
      if (numpad==true){
        numpad=false;
        Serial.print("nummer ");
        Serial.print(tryk);
        Serial.println(" er trykket");
        kode[kodeTal] = tryk; //læg koden i array kode
        strip.SetPixelColor(kodeTal, red);
        strip.Show();
        delay(200);
        kodeTal++; 
        for (int c=0; c<6; c++){
          Serial.print(" ");
          Serial.print(kode[c]);
        }
        Serial.println();
         if (kodeTal>5){
           Serial.println("hey");
          strip.ClearTo(RgbColor(0,20,0));
          strip.Show();
          kodeTal=0;
          for (int a=0; a<6; a++){
            if (kode[a] == kodeTjek[a]){
               erKodeRigtig[a]=true;
               Serial.println(erKodeRigtig[a]);
            }
            else{
              erKodeRigtig[a]=false;
            }
          }
          for (int a=0; a<6; a++){
            if(erKodeRigtig[a]==true){
              tjekKoden++;
            }
            if(erKodeRigtig[a]==false){
              tjekKoden = 0;
            }
            if (tjekKoden==6){
              Serial.println("koden skulle være der");
              infrarodKlaret=true;
              delay(50);
              drej(90,0);
            }
           }
        for (int b=0; b<6; b++){
                 erKodeRigtig[b] = false;
                 kode[b] = 0;
              }
              tjekKoden=0;
            }
     }
    }


void setupRfid(){

  //Protocol Configuration  // Initialize serial communications with PC
  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware

  //If you set Antenna Gain to Max  it will increase reading distance
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  Serial.println(F("Access Control Example v0.1"));   // For debugging purposes
  ShowReaderDetails();  // Show details of PCD - MFRC522 Card Reader details

  //Wipe Code - If the Button (wipeB) Pressed while setup run (powered on) it wipes EEPROM
  if (digitalRead(wipeB) == LOW) {  // when button pressed pin should get low, button connected to ground
    strip.ClearTo(RgbColor(0,100,0));
    strip.Show();
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 10 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    bool buttonState = monitorWipeButton(10000); // Give user enough time to cancel operation
    if (buttonState == true && digitalRead(wipeB) == LOW) {    // If button still be pressed, wipe EEPROM
      Serial.println(F("Starting Wiping EEPROM"));
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
        if (EEPROM.read(x) == 0) {              //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else {
          EEPROM.write(x, 0);       // if not write 0 to clear, it takes 3.3mS
        }
      }
      Serial.println(F("EEPROM Successfully Wiped"));
    }
    else {
      Serial.println(F("Wiping Cancelled")); // Show some feedback that the wipe button did not pressed for 15 seconds
      strip.ClearTo(RgbColor(0,0,0));
      strip.Show();
    }
  }
  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    Serial.println(F("No Master Card Defined"));
    Serial.println(F("Scan A PICC to Define as Master Card"));
    do {
      successRead = getID(); 
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
    Serial.println(F("Master Card Defined"));
  }
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for ( uint8_t i = 0; i < 4; i++ ) {          // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything is ready"));
  Serial.println(F("Waiting PICCs to be scanned"));
  cycleLeds();    // Everything ready lets give user some feedback by cycling leds
}


void granted ( uint16_t setDelay) {
  strip.ClearTo(RgbColor(0,0,50));
  strip.Show();
  }

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  strip.ClearTo(RgbColor(50,0,0));
  strip.Show();
  delay(1000);
}


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    // Visualize system is halted
    while (true); // do not go further
  }
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  for (int i=0; i<PixelCount; i++){
    strip.SetPixelColor(i,RgbColor(0,0,10));
    strip.Show();
    delay(2);
    strip.SetPixelColor(i,RgbColor(0,0,0));
  }
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  for (int i=0; i<PixelCount; i++){
   strip.SetPixelColor(i,RgbColor(0,0,10));
   strip.Show();
  }
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite();      // If not
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    successDelete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
bool checkTwo ( byte a[], byte b[] ) {   
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] ) {     // IF a != b then false, because: one fails, all fail
       return false;
    }
  }
  return true;  
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
bool findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i < count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
    }
    else {    // If not, return false
    }
  }
  return false;
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  strip.ClearTo(RgbColor(0,0,0));
  strip.Show();
  delay(200);
  strip.ClearTo(RgbColor(0,50,0));
  strip.Show();   // Make sure green LED is on
  delay(200);
  strip.ClearTo(RgbColor(0,0,0));
  strip.Show();
  delay(200);
  strip.ClearTo(RgbColor(0,50,0));
  strip.Show();   // Make sure green LED is on
  delay(200);
  strip.ClearTo(RgbColor(0,0,0));
  strip.Show();  // Make sure green LED is off
  delay(200);
  strip.ClearTo(RgbColor(0,50,0));
  strip.Show();
  delay(200);
}

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
   for (int i=0; i<5; i++){
    strip.ClearTo(RgbColor(50,0,0));
    delay(500);
    strip.ClearTo(RgbColor(0,0,0));
    delay(500);
  }
}

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
  strip.ClearTo(RgbColor(50,0,0));
  strip.Show();
  delay(1000);
  strip.ClearTo(RgbColor(0,0,0));
  }

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
bool isMaster( byte test[] ) {
  return checkTwo(test, masterCard);
}

bool monitorWipeButton(uint32_t interval) {
  uint32_t now = (uint32_t)millis();
  while ((uint32_t)millis() - now < interval)  {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (digitalRead(wipeB) != LOW)
        return false;
    }
  }
  return true;
}

void drejEnOmgang(){
      digitalWrite(enPin, LOW);
      digitalWrite(dirPin, HIGH); 
      for (int i=0; i<6;i++){
        digitalWrite(stepPin, HIGH);
        delay(100);
        digitalWrite(stepPin, LOW);
      }
      digitalWrite(dirPin, LOW);
      Serial.println("nået gennem infrarød");
      digitalWrite(enPin, HIGH);
}

void drej(int vert, int hoz){
  myservo.attach(2);
  digitalWrite(enPin, LOW);
  myservo.write(vert);
  delay(500);
  if (hoz<gammelHoz){
    digitalWrite(dirPin, HIGH);
  }
  while (hoz<gammelHoz){
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(hastighed);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(hastighed);
    gammelHoz -= 1;
  }
  if (hoz>gammelHoz){
    digitalWrite(dirPin, LOW);
  }
  while (hoz>gammelHoz){
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(hastighed);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(hastighed);
    gammelHoz++;
  }
  digitalWrite(enPin, HIGH);
  //delay(500);
  myservo.detach();
}

void touchTjek(){
    // Get the currently touched pads
  currtouched = cap.touched();
  
  for (uint8_t i=0; i<12; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" touched"); tryk = i; numpad = true;
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {
      Serial.print(i); Serial.println(" released");
    }
  }

  // reset our state
  lasttouched = currtouched;
 }
