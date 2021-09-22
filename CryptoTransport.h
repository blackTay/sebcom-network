/*
* Implements an encrypted transport layer on top of lora
* It can be thought of as a better version of UDP with encryption and packet numbering, but without ports
* Features: AES256 encryption, checksum-checked, packet numbering
*/
#ifndef CRYPTOTRANSPORTFILE
#define CRYPTOTRANSPORTFILE

#include "globalconf.h"

#include <Crypto.h>
#include <AES.h>
#include <CTR.h>
#include <LoRa.h>
#include "Arduino.h"
#include "key.h"

// the buffer
#define CIRCULAR_BUFFER_INT_SAFE // because we use interrupts, declear buffer int safe
#include <CircularBuffer.h>

#define DELIMITER_CHAR '#'

// the buffer
static CircularBuffer<char, 500> receiveBuffer;

class CryptoTransport{
    public:
        CryptoTransport(int randomPin,int receivedLedPin);

        // functions
        void sendPacket(String txt);
        void receivePacket();
        bool messageAvailable();
        String popMessage();
        void begin();

        // flags
        byte flagReceivedMsg = 0; // if a message is waiting in the receive_buffer

    private:
        // crypto
        AES256 _aes256;
        CTR<AES256> _ctr;
        byte _aes256Key[32] = SECRET_KEY;

        // service functions
        void _encryptTxt(String txt, byte* ciphertxt, byte* iv, CTR<AES256>* strmcipher);
        void _decryptTxt(byte* ciphertxt, byte* plaintxt, byte* iv, int len, CTR<AES256>* strmcipher);
        void _generateRandomByteSequence(byte* output, int len);
        int _calcEncodedTxtLength(int strLen);
        void _encodeMsg(String txt, byte* encodedMsg);

        // bookkeeping
        int _lastRcvPktNumber = -1; // the packet number of the last received packet. -1: not received any yet
        char _sendPacketNumber = 0; // the packet number of the next packet to send

        // pin configuration
        int _ranomSeedAnalogPin; // keep this analog pin floating to seed random generator
        int _receivedLedPin=11; // pin of the received message indicator LED
};
#endif
