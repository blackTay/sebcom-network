#include "PS2Driver.h"


// for use by ISR glue routines
PS2Driver * PS2Driver::_instance; // important, this line is needed!

PS2Driver::PS2Driver(int clockInterruptPin,int byteInterruptPin,int dataPin)
{
    _instance = this;

    _clockPin = clockInterruptPin;
    _byteOffPin = byteInterruptPin;
    _dataPin = dataPin;

    pinMode(_clockPin, INPUT_PULLUP);
    pinMode(_dataPin, INPUT);
    attachInterrupt (digitalPinToInterrupt(_clockPin), _staticIsrClock, FALLING); 
    attachInterrupt (digitalPinToInterrupt(_byteOffPin), _staticIsrByte, FALLING); 

    _inputAssembleVar = 0;
    _shiftActive = false;
    _released = false;
    _capslock = false;
    _shiftDown = false;
    _bitCnt = 0;
    _parityTracker = 0;

    flagEnter = 0;
    flagEsc = 0;
    flagDel = 0;
    flagTab = 0;
}

void PS2Driver::_staticIsrClock ()
{
  _instance->_isrClock ();  
} 

void PS2Driver::_staticIsrByte ()
{
  _instance->_isrByte ();  
} 


// reset all flags
void PS2Driver::resetAllFlags(){
  flagEnter = 0;
  flagEsc = 0;
  flagDel = 0;
  flagTab = 0;
}


// reset the buffer
void PS2Driver::resetBuffer(){
  buffer.clear();
}

// true iff the buffer is empty
bool PS2Driver::isBufferEmpty(){
  return buffer.isEmpty();
}

// returns (and removes) the oldest char in the buffer
char PS2Driver::popBuffer(){
  return buffer.shift();
}




// look the key in the char map up
char PS2Driver::_keyToChar(int key) {
  return pgm_read_byte_near(keymap + key);
}


// receive a new bit
void PS2Driver::_isrClock() {
  int currentInp = digitalRead(_dataPin);
  _parityTracker ^= currentInp;
  currentInp <<= _bitCnt++;
  _inputAssembleVar |= currentInp;
}


// received on full 11-bit message (aka 1 byte as 3 bits don't carry data)
void PS2Driver::_isrByte() {

  // check sanity check bits
  if (_parityTracker != 0) goto cleanup;        // parity has to be 0, due to parity bit
  if (_bitCnt != 11) goto cleanup;              // one siginal consists of 11 bits
  if (_inputAssembleVar % 2 == 1) goto cleanup; // bit 0 aka first transmitted bit (least significant) has to be 0
  if (_inputAssembleVar < (1 << 10)) goto cleanup; // bit 11 aka last transmitted bit (most significant) has to be 1

  // strip sanity check bits
  _inputAssembleVar &= ~(11 << 9); // strip bits 11 and 10
  _inputAssembleVar = (unsigned int)_inputAssembleVar >> 1; // strip bit 0

  // check if there was a release
  if (_inputAssembleVar == KEY_RELEASE) {
    _released = true;
    goto cleanup;
  }
  if (!_released) goto cleanup; // process the key only when released

  // push to queue, if normal character key
  char inputChar;
  inputChar = _keyToChar(_inputAssembleVar);
  if (inputChar != 0) {
    buffer.push(inputChar);
  }

  _released = false;

  // secial flags
  if (_inputAssembleVar == KEY_ENTER) flagEnter = 1;
  if (_inputAssembleVar == KEY_ESC) flagEsc = 1;
  if (_inputAssembleVar == KEY_DEL) flagDel = 1;
  if (_inputAssembleVar == KEY_TAB) flagTab = 1;

cleanup:
  _bitCnt = 0; // reset bit cnt
  _parityTracker = 0; // reset parity tracker
  _inputAssembleVar = 0;
}
