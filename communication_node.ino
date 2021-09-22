#include "globalconf.h"

// keyboard
#include "PS2Driver.h"

// screen
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// encryption
#include "CryptoTransport.h"

// pins for keyboard
const byte ps2ClockPin = 0; // interrupt here
const byte ps2ByteOffPin = 1; // interrupt here
const int ps2DataPin = 2;

PS2Driver keyboard=PS2Driver(ps2ClockPin,ps2ByteOffPin,ps2DataPin);

// pins for crypto
const int ranomSeedAnalogPin = 5; // keep this analog pin floating to seed random generator
const int receivedLedPin=12; // pin of the received message indicator LED

CryptoTransport gateway= CryptoTransport(ranomSeedAnalogPin,receivedLedPin); // the gateway to communicate: send and receive messages securely (aes256)


// display
GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> display(GxEPD2_290_T94_V2(/*CS=4*/ 4, /*DC=*/ 7, /*RST=*/ 6, /*BUSY=*/ 5)); // GDEM029T94, Waveshare 2.9" V2 variant
int cursorTopY; // cursors for top half of screen
int cursorBottomY; // cursors for bottom half of screen
// display constants
#define DISPLAY_TOP 0
#define DISPLAY_BOTTOM 1


// for general operation
const int writeModeLedPin = 13; // indicator led for MODE_TYPING
const int resetScreenPin = 11; // button to reset screen. button will pull low
// operation modes
#define MODE_READING 0
#define MODE_TYPING 1
int mode = MODE_READING;
String msgSendDraft = "";
bool resetPinDown = false;


void setup() {
  Serial.begin(115200);

  #ifdef DEBUG
  while (!Serial);
  Serial.print("Startup in debug mode\nVersion:");
  Serial.println(VERSION);
  #endif

  // ---display begin---
  display.init();
  display.setRotation(1); // landscape
  display.setTextColor(GxEPD_BLACK);   //black on white mode
  // setup cursor params
  cursorTopY = 0;
  cursorBottomY = display.height() / 2;
  helloScreen(); // first update should be full refresh, see library
  // ---display end---

  // MODE_TYPING indicator LED
  pinMode(writeModeLedPin,OUTPUT);
  digitalWrite(writeModeLedPin,LOW);
  pinMode(resetScreenPin,INPUT_PULLUP);

  gateway.begin();

  // clear screen. Now ready.
  clearFullScreen();
}


void loop() {
  // big mode switch
  switch (mode) {
    case MODE_READING:
      if (keyboard.flagTab) {
        #ifdef DEBUG
        Serial.println("[mode change] MODE_READING -> MODE_TYPING");
        #endif
        mode = MODE_TYPING;
        msgSendDraft = "";

        // clear flags & buffer
        keyboard.resetBuffer();
        keyboard.resetAllFlags();
        
        // led.  Note: the led is set after mode transfer has been completed.
        digitalWrite(writeModeLedPin,HIGH);
        break;
      }

      if (gateway.flagReceivedMsg && keyboard.flagEnter && gateway.messageAvailable()) {
        printOnScreen(gateway.popMessage(),/*doFullUpdate=*/false,/*location=*/DISPLAY_TOP);

        // update keyboard flag
        keyboard.flagEnter = 0;
      }
      break;

    case MODE_TYPING:
      if (keyboard.flagEsc) {
        #ifdef DEBUG
        Serial.println("[mode change] MODE_TYPING -> MODE_READING");
        #endif
        mode = MODE_READING;
        msgSendDraft = "";

        keyboard.resetBuffer();
        keyboard.resetAllFlags();

        // clear screen
        clearFullScreen();

        // led.  Note: the led is set after mode transfer has been completed.
        digitalWrite(writeModeLedPin,LOW);
        break;
      }

      if (keyboard.flagEnter) {
        if (msgSendDraft == "" && !keyboard.isBufferEmpty()) {
          // there is no draft. text is waiting in the buffer.
          while (!keyboard.isBufferEmpty()) {
            msgSendDraft += String(keyboard.popBuffer());
          }
          printOnScreen(msgSendDraft,/*doFullUpdate=*/false,/*location=*/DISPLAY_BOTTOM);
          printOnScreen("Press enter to confirm sending, delete else",/*doFullUpdate=*/false,/*location=*/DISPLAY_BOTTOM);
        } else if (msgSendDraft != "") {
          // there is a draft
          gateway.sendPacket(msgSendDraft);
          msgSendDraft = "";
          keyboard.resetBuffer(); // reset waiting text

          printOnScreen("sent. clearing now.",/*doFullUpdate=*/false,/*location=*/DISPLAY_BOTTOM);
          delay(700);
          // clear bottom screen
          clearPartialScreen(/*location=*/DISPLAY_BOTTOM);
        }
        keyboard.flagEnter = 0;
      } else if (keyboard.flagDel) {
        // clear buffer
        msgSendDraft = "";

        // clear bottom screen
        clearPartialScreen(/*location=*/DISPLAY_BOTTOM);
        #ifdef DEBUG
        Serial.println("message draft discarded");
        #endif

        // clear flags
        keyboard.flagDel = 0;
      }
      break;
  }

  // ---LoRa code START---
  gateway.receivePacket(); // maintain receiveBuffer, flag and led
  // ---LoRa code END---

  checkResetPin();
}

// check and maintain reset button functionality 
void checkResetPin(){
  if(resetPinDown){
    if(digitalRead(resetScreenPin)==HIGH){
      resetPinDown=false;
    }
  } else {
    if(digitalRead(resetScreenPin)==LOW){
      resetPinDown=true;
      #ifdef DEBUG
      Serial.println("resetting screen due to button press");
      #endif
      clearFullScreen();  
    }
  }
}


// --- ePaper code START---

// clear the screen partially, either top or bottom
void clearPartialScreen(int location) {
  int y = 0;
  if (location == DISPLAY_TOP) {
    y = 0;
    cursorTopY = 0;
  } else if (location == DISPLAY_BOTTOM) {
    y = display.height() / 2;
    cursorBottomY = display.height() / 2;
  }
  display.setPartialWindow(0, y, display.width(), display.height() / 2);
  display.firstPage();
  do
  {
    display.fillRect(0,  y, display.width(), display.height() / 2, GxEPD_WHITE);
  }
  while (display.nextPage());
}

void clearFullScreen() {
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
  }
  while (display.nextPage());
}

const char welcomeText[] = "Welcome. You are secure.";

void helloScreen()
{
  display.setRotation(1);
  //display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(welcomeText, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(welcomeText);
  }
  while (display.nextPage());
}


void printOnScreen(String txt, bool doFullUpdate, int location) {
  #ifdef DEBUG
  Serial.print("displaying msg ");
  Serial.print(txt);
  #endif

  if (doFullUpdate) {
    // reset partial parameters
    cursorTopY = 0;
    cursorBottomY = display.height() / 2;

    // display txt
    display.setFullWindow();
    display.firstPage();
    do
    {
      display.fillScreen(GxEPD_WHITE);
      display.setCursor(0, 0);
      display.print(txt);
    }
    while (display.nextPage());
  }
  else {
    if (location == DISPLAY_TOP) {
      // get the text bound. Mainly the height is important for future text
      int16_t txtbndX, txtbndY; uint16_t txtbndW, txtbndH;
      display.getTextBounds(txt, 0, 0, &txtbndX, &txtbndY, &txtbndW, &txtbndH);

      // if txt overflow, rewrite from beginning
      if (cursorTopY + txtbndH > display.height() / 2) {
        cursorTopY = 0;
      }

      // display message
      display.setPartialWindow(0, cursorTopY, display.width(), txtbndH);
      display.firstPage();
      do
      {
        display.fillRect(0,  cursorTopY, display.width(), txtbndH, GxEPD_WHITE);
        display.setCursor(0, cursorTopY);
        display.print(txt);
      }
      while (display.nextPage());
      cursorTopY += txtbndH;


    }
    else if (location == DISPLAY_BOTTOM) {
      // get the text bound. Mainly the height is important for future text
      int16_t txtbndX, txtbndY; uint16_t txtbndW, txtbndH;
      display.getTextBounds(txt, 0, 0, &txtbndX, &txtbndY, &txtbndW, &txtbndH);

      // if txt overflow, rewrite from beginning
      if (cursorBottomY + txtbndH > display.height()) {
        cursorBottomY = 0;
      }

      // display message
      display.setPartialWindow(0, cursorBottomY, display.width(), txtbndH);
      display.firstPage();
      do
      {
        display.fillRect(0,  cursorBottomY, display.width(), txtbndH, GxEPD_WHITE);
        display.setCursor(0, cursorBottomY);
        display.print(txt);
      }
      while (display.nextPage());
      cursorBottomY += txtbndH;
    }
  }
}
// --- ePaper code END---
