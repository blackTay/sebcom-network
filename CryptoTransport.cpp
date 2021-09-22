#include "CryptoTransport.h"


CryptoTransport::CryptoTransport(int randomPin,int receivedLedPin){
    _ranomSeedAnalogPin = randomPin;
    _receivedLedPin = receivedLedPin;
}

// start everything up
void CryptoTransport::begin(){
  randomSeed(analogRead(_ranomSeedAnalogPin)); // seed the random generator
  pinMode(_receivedLedPin,OUTPUT);
  if (!LoRa.begin(868E6)) {
  #ifdef DEBUG
  Serial.println("Starting LoRa failed!");
  #endif
  while (1);
  }
}


void CryptoTransport::_encryptTxt(String txt, byte* ciphertxt, byte* iv, CTR<AES256>* strmcipher) {
  // first, convert string to bytes
  byte plaintxt[txt.length() + 1]; // +1 to account for null termination
  txt.getBytes(plaintxt, txt.length() + 1);

  strmcipher->setIV(iv, 16); // ctr needs 16 bytes
  strmcipher->setCounterSize(4);
  strmcipher->encrypt(ciphertxt, plaintxt, txt.length());
}


void CryptoTransport::_decryptTxt(byte* ciphertxt, byte* plaintxt, byte* iv, int len, CTR<AES256>* strmcipher) {
  strmcipher->setIV(iv, 16);
  strmcipher->decrypt(plaintxt, ciphertxt, len);
}


// generates a byte of random bits.
void CryptoTransport::_generateRandomByteSequence(byte* output, int len) {
  for (int i = 0; i < len; i++) {
    output[i] = random(256);
  }
}

void CryptoTransport::sendPacket(String txt) {
  byte encoded_txt[_calcEncodedTxtLength(txt.length())];
  _encodeMsg(txt, encoded_txt);
  
  // send packet
  LoRa.beginPacket();
  LoRa.write(encoded_txt,_calcEncodedTxtLength(txt.length()));
  LoRa.endPacket();

  #ifdef DEBUG
  Serial.print("msg sent: ");
  Serial.println(txt);
  #endif
}

// receive packets to maintain buffers, flags, etc. Need to call this regularly. 
// (The LoRa library does not support interrupts on message arrivel for MKR 1300 boards yet)
void CryptoTransport::receivePacket() {
  int physical_packet_size = LoRa.parsePacket();
  if (physical_packet_size) {
    // parse header
    char header[21];
    for (int i = 0; i < 21; i++) {
      if (!LoRa.available()) {
        #ifdef DEBUG
        Serial.println(physical_packet_size);
        Serial.println(i);
        Serial.println(LoRa.parsePacket());
        Serial.println("Error parsing packet header");
        return;
        #endif
      }
      header[i] = LoRa.read();
    }

    #ifdef DEBUG
    Serial.println("Inbound header:");
    for (int i = 0; i < 21; i++) {
      Serial.print(header[i], HEX);
      Serial.print('\t');
    }
    #endif

    // check for code and check that packet size is > 0 (i.e. data section is non-empty)
    if (header[0] != '#' || header[1] != '0' || header[2] == 0) {
      #ifdef DEBUG
      Serial.println("[ERROR] parsing packet header");
      return;
      #endif
    }

    int pkt_length = header[2]; // guranteed to be > 0.
    char pkt_checksum = header[3];
    char pkt_number = header[4];

    byte* iv = (byte*)header + 5; // store iv



    // check if the #received bytes matches the declared packet length
    if (pkt_length + 21 != physical_packet_size) {
      #ifdef DEBUG
      Serial.println(pkt_length);
      Serial.println(physical_packet_size);
      Serial.println("[ERROR] packet length wrong");
      return;
      #endif
    }


    // read data
    int sum = pkt_number;
    byte inp_str[pkt_length]; // store date in here. If checksum matches, push to buffer
    for (int i = 0; i < pkt_length; i++) {
      // should be available, except if corrupted
      if (!LoRa.available()) {
        #ifdef DEBUG
        Serial.println("Error parsing packet data");
        return;
        #endif
      }

      // read and push to buffer
      char inp_char =  LoRa.read();
      inp_str[i] = inp_char;
      sum += inp_char;
      #ifdef DEBUG
      Serial.print(inp_char);
      #endif
    }
    #ifdef DEBUG
    // print RSSI of packet
    Serial.print(" with RSSI ");
    Serial.println(LoRa.packetRssi());
    #endif

    // verify checksum
    for (int i = 0; i < 16; i++) {
      sum += iv[i];
    }
    if (lowByte(sum) != pkt_checksum) {
      #ifdef DEBUG
      Serial.println("Error parsing packet data: checksum wrong");
      return;
      #endif
    }

    // check pkt_number
    if (_lastRcvPktNumber == -1) { // this is the first packet received
      _lastRcvPktNumber = pkt_number;
    } else if (pkt_number <= _lastRcvPktNumber) { // received an old packet
      #ifdef DEBUG
      Serial.println("[ERROR] duplicate packet");
      return;
      #endif
    } else if (pkt_number > _lastRcvPktNumber + 1) { // missed some packet
      #ifdef DEBUG
      Serial.println("[WARNING] missed " + String(pkt_number - _lastRcvPktNumber - 1) + " packets");
      #endif
      String msg_to_display = "[WARNING] missed " + String(pkt_number - _lastRcvPktNumber - 1)  + " packets.";
      for (int i = 0; i < msg_to_display.length(); i++) {
        receiveBuffer.push(msg_to_display[i]);
      }
      receiveBuffer.push(DELIMITER_CHAR);
    }


    // update last received packet
    _lastRcvPktNumber = pkt_number;

    // decrypt
    byte decrypted_txt[pkt_length];
    _decryptTxt(inp_str, decrypted_txt, iv, pkt_length, &_ctr);

    // store in buffer
    for (int i = 0; i < pkt_length; i++) {
      // strip delimiter
      if (inp_str[i] != DELIMITER_CHAR) {
        receiveBuffer.push(decrypted_txt[i]);
      }
    }

    // handle end of input
    receiveBuffer.push(DELIMITER_CHAR);
    digitalWrite(_receivedLedPin, HIGH);
    flagReceivedMsg = 1;
  }
}


/*
    Packet format:  | #0 | data_length | checksum | pkt_number | iv | data_encrypted |
    Therefore, the physical packet size is 21 bytes bigger than data_length
    Note that checksum accounts for both pkt_number and data
*/

// calculated the length of the encoded message, accounting for the header
int CryptoTransport::_calcEncodedTxtLength(int strLen) {
  return strLen + 21;
}

void CryptoTransport::_encodeMsg(String txt, byte* encoded_msg) {
  int sum = _sendPacketNumber; // checksum also accounts for packet number
  char b = 0;
  char len = 0;

  // check for max length
  if (txt.length() > 255) {
    #ifdef DEBUG
    Serial.println("message to encode is too long");
    #endif
    return;
    //return "E [ERROR] message to encode is too long"; // TODO CAPTURE THIS WHEN CALLING METHOD
  }

  // generate iv
  byte iv[16];
  _generateRandomByteSequence(iv, 16);

  // encrypt
  byte cipher_txt[txt.length()];
  _encryptTxt(txt, cipher_txt, iv, &_ctr);


  // set checksum
  for (char i = 0; i < 16; i++)
  {
    sum += iv[i];
  }

  for (char i = 0; i < txt.length(); i++)
  {
    sum += cipher_txt[i];
    len++; // the length to send
  }

  // checksum bits
  char ls8b = lowByte(sum);

  encoded_msg[0] = '#';
  encoded_msg[1] = '0';
  encoded_msg[2] = len;
  encoded_msg[3] = ls8b;
  encoded_msg[4] = _sendPacketNumber++;
  for (int i = 0; i < 16; i++) {
    encoded_msg[5 + i] = iv[i];
  }
  for (int i = 0; i < txt.length(); i++) {
    encoded_msg[21 + i] = cipher_txt[i];
  }
  //return "#0" + String(len) + String(ls8b) + String(send_packet_number++) +String((char *)iv) + String((char *)cipher_txt); // note: it is by design ok for the packet number to reset after 256 messages
}

// has a message been received?
bool CryptoTransport::messageAvailable(){
  return !receiveBuffer.isEmpty();
}


// pop the oldest message, if available. Maintains the receive flag and led
String CryptoTransport::popMessage(){
  String ret = "";
  while (!receiveBuffer.isEmpty()) {
    char currentChar = receiveBuffer.shift();
    if (currentChar == DELIMITER_CHAR) {
      break;
    }
    ret += String(currentChar);
  }

  // maintain receive flag and led
  if(receiveBuffer.isEmpty()){
    flagReceivedMsg = 0;
    digitalWrite(_receivedLedPin,LOW);
  }
  return ret;
}
