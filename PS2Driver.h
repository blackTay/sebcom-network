#ifndef PS2DRIVER
#define PS2DRIVER

#include "globalconf.h"
#include "Arduino.h"

// the buffer
#define CIRCULAR_BUFFER_INT_SAFE // because we use interrupts, declear buffer int safe
#include <CircularBuffer.h>

// some special key codes
#define KEY_RELEASE 0xf0
#define KEY_ENTER 0x5a
#define KEY_ESC 0x76
#define KEY_DEL 0x66
#define KEY_TAB 0x0D    
 
// the key buffer
static CircularBuffer<char, 100> buffer;


class PS2Driver{
    
    public:
        PS2Driver();

        
        PS2Driver(int clockInterruptPin,int byteInterruptPin,int dataPin);
        
        void resetAllFlags();
        void resetBuffer();
        bool isBufferEmpty();
        char popBuffer();

         // flags
        volatile byte flagEnter; // flag if enter has been pressed
        volatile byte flagEsc; // flag if esc has been pressed
        volatile byte flagDel; // delete has been pressed (backshift)
        volatile byte flagTab; // tab has been pressed
    private:
        
        // service functions
        static char _keyToChar(int key);

        // interrupts
        static PS2Driver* _instance; // needed for ISR as they work only for static functions
        static void _staticIsrClock();
        static void _staticIsrByte();
        void _isrClock(); // 1 bit
        void _isrByte(); // a byte has been received (including service bits, 11 bits -- but 1 byte of data)

        // bookkeeping
        volatile int _inputAssembleVar; 
        volatile bool _shiftActive; // is shift active?
        volatile bool _released; //  was the last char a release?
        volatile bool _capslock; // is capslock active
        volatile bool _shiftDown; // is the shift key pressed down
        volatile int _bitCnt ; // the number of bits received so far. needed to assemble the byte
        volatile byte _parityTracker; // the parity of the bits received so favolatile static 
        // pins
        volatile int _clockPin; // interrupt here
        volatile int _byteOffPin; // interrupt here
        volatile int _dataPin;
};

const PROGMEM char keymap[] = {0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               '`',
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               'Q',
                               '1',
                               0,
                               0,
                               0,
                               'Z',
                               'S',
                               'A',
                               'W',
                               '2',
                               0,
                               0,
                               'C',
                               'X',
                               'D',
                               'E',
                               '4',
                               '3',
                               0,
                               0,
                               ' ',
                               'V',
                               'F',
                               'T',
                               'R',
                               '5',
                               0,
                               0,
                               'N',
                               'B',
                               'H',
                               'G',
                               'Y',
                               '6',
                               0,
                               0,
                               0,
                               'M',
                               'J',
                               'U',
                               '7',
                               '8',
                               0,
                               0,
                               ',',
                               'K',
                               'I',
                               'O',
                               '0',
                               '9',
                               0,
                               0,
                               '.',
                               '/',
                               'L',
                               ';',
                               'P',
                               '-',
                               0,
                               0,
                               0,
                               '\'',
                               0,
                               '[',
                               '=',
                               0,
                               0,
                               0,
                               0,
                               0,
                               ']',
                               0,
                               '\\',
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               0,
                               '1',
                               0,
                               '4',
                               '7',
                               0,
                               0,
                               0,
                               '0',
                               '.',
                               '2',
                               '5',
                               '6',
                               '8',
                               0,
                               0,
                               0,
                               '+',
                               '3',
                               '-',
                               '*',
                               '9',
                               0,
                               0,
                               0,
                               0,
                               0,
                               0
                              };

#endif
