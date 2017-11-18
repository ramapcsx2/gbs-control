#include "I2CBitBanger.h"

//#include <util/delay.h>

// Initialize statics
uint8_t I2CBitBanger::I2CBB_Buffer[I2CBB_BUF_SIZE];             // Buffer to hold I2C data
uint8_t I2CBitBanger::I2CBB_BufferIndex = 0;                    // The current index value into the buffer


I2CBitBanger::I2CBitBanger(uint8_t sevenBitAddressArg) {
  setSlaveAddress(sevenBitAddressArg);
  initializePins();
}


// Public interface functions

void I2CBitBanger::initializePins() {
  // initialize SDA and SCL lines (release them by setting DDR to 0 (input), and PORT to 0 (no pullup))

  //DDR_I2CBB &= ~(1<<SDA_BIT); // Set SDA as an input
  //DDR_I2CBB &= ~(1<<SCL_BIT); // Set SCL as an input
  pinMode(SDA_BIT, INPUT);
  pinMode(SCL_BIT, INPUT);
}

void I2CBitBanger::setSlaveAddress(uint8_t sevenBitAddressArg) {
  // Setup address. Store in buffer
  I2CBB_BufferIndex = 0;
  I2CBB_Buffer[I2CBB_BufferIndex] = (sevenBitAddressArg << 1);
}

void I2CBitBanger::addByteForTransmission(uint8_t data) {
  // stores data to send later
  if (I2CBB_BufferIndex >= I2CBB_BUF_SIZE) {
    return; // return to avoid exceeding buffer size
  }

  I2CBB_BufferIndex++; // increment for the next byte in buffer
  I2CBB_Buffer[I2CBB_BufferIndex] = data;
}

void I2CBitBanger::addBytesForTransmission(uint8_t* buffer, uint8_t bufferSize) {
  for (int i = 0; i < bufferSize; i++)
  {
    addByteForTransmission(buffer[i]);
  }
}



bool I2CBitBanger::transmitData() {
  // make sure the RW bit is a write (set the RW bit to 0)
  I2CBB_Buffer[0] &= ~(I2CBB_RW_BIT_POSITION);

  // actually sends the buffer
  bool success = sendDataOverI2c(I2CBB_Buffer, I2CBB_BufferIndex + 1);
  I2CBB_BufferIndex = 0;
  return success;
}


int I2CBitBanger::recvData(int numBytesToRead, uint8_t* outputBuffer) {
  // make sure the RW bit is a read (set the RW bit to 1)
  I2CBB_Buffer[0] |= I2CBB_RW_BIT_POSITION;

  sendI2cStartSignal();

  // Start the read by sending the slave address + RW bit set to read
  if (!sendI2cByte(I2CBB_Buffer[0])) {
    return 0;
  }


  int i = 0;
  while (i < numBytesToRead) {

    if (i == (numBytesToRead - 1)) {
      // on the last byte, we send a NAK
      receiveI2cByte(false, outputBuffer + i);

    } else {
      // read a byte, sending an ACK
      receiveI2cByte(true, outputBuffer + i);
    }

    i++;
  }

  sendI2cStopSignal();

  return i;
}


// Private functions

bool I2CBitBanger::sendDataOverI2c(uint8_t* buffer, uint8_t bufferSize) {

  sendI2cStartSignal();

  for (uint8_t i = 0; i < bufferSize; i++) {
    if (!sendI2cByte(buffer[i])) {
      return false;
    }
  }

  sendI2cStopSignal();

  return true;
}


void I2CBitBanger::sendI2cStartSignal() {
  /*
    Set SDA low
    Wait 10.5us
    Set SCL low
    Wait 10.5us
    return;
  */

  //DDR_I2CBB |= (1<<SDA_BIT); // set SDA low
  pinMode(SDA_BIT, OUTPUT);
  digitalWrite(SDA_BIT, 0);
  delayMicroseconds(10);
  //DDR_I2CBB |= (1<<SCL_BIT); // set SCL low
  pinMode(SCL_BIT, OUTPUT);
  digitalWrite(SCL_BIT, 0);
  delayMicroseconds(10);

  return;
}


bool I2CBitBanger::sendI2cByte(uint8_t dataByte) {

  /*
    Loop 8 times to send 8 bits
      Set SDA to bit
      Wait 1us
      Release SCL
      Wait 2.5us
      Set SCL low
      Wait 6us

    Relase SDA
    Wait 1us
    Make SDA an input
    Release SCL
    Read the value of SDA
    Wait 3.5us
    if(SDA was NACK) return false;
    Set SCL low
    Wait 1us
    Make SDA an output (released)
    Wait 15us
    return true;
  */

  // send each bit, MSB to LSB
  uint8_t mask = 0x80;
  for (int i = 0; i < 8; i++) {
    if (dataByte & mask) {
      // 1
      //DDR_I2CBB &= ~(1<<SDA_BIT); // release SDA
      pinMode(SDA_BIT, INPUT);
    } else {
      // 0
      //DDR_I2CBB |= (1<<SDA_BIT); // set SDA low
      pinMode(SDA_BIT, OUTPUT);
      digitalWrite(SDA_BIT, 0);
    }
    delayMicroseconds(1);
    //DDR_I2CBB &= ~(1<<SCL_BIT); // release SCL
    pinMode(SCL_BIT, INPUT);
    delayMicroseconds(3);
    //DDR_I2CBB |= (1<<SCL_BIT); // set SCL low
    pinMode(SCL_BIT, OUTPUT);
    digitalWrite(SCL_BIT, 0);
    delayMicroseconds(6);

    mask = mask >> 1;
  }

  //DDR_I2CBB &= ~(1<<SDA_BIT); // release SDA
  pinMode(SDA_BIT, INPUT);
  delayMicroseconds(1);
  // make SDA an input (it already is)
  //DDR_I2CBB &= ~(1<<SCL_BIT); // release SCL
  pinMode(SCL_BIT, INPUT);
  // read the value of SDA
  delayMicroseconds(3);

  //if( PIN_I2CBB & (1<<SDA_BIT) ) { // received a NACK
  if (digitalRead(SDA_BIT) == 1) {
    // SDA and SCL remain released
    Serial.println("nak'd");
    return false;
  }

  // received an ACK
  //DDR_I2CBB |= (1 << SCL_BIT); // set SCL low
  pinMode(SCL_BIT, OUTPUT);
  digitalWrite(SCL_BIT, 0);
  delayMicroseconds(1);
  // SDA remains released
  delayMicroseconds(15);

  return true;
}


void I2CBitBanger::sendI2cStopSignal() {
  /*
    Set SDA low
    Wait 10.5us
    Release SCL
    Wait 11.5us
    Relase SDA
    Wait 50us
    return;
  */

  //DDR_I2CBB |= (1 << SDA_BIT); // set SDA low
  pinMode(SDA_BIT, OUTPUT);
  digitalWrite(SDA_BIT, 0);
  delayMicroseconds(10);
  //DDR_I2CBB &= ~(1 << SCL_BIT); // release SCL
  pinMode(SCL_BIT, INPUT);
  delayMicroseconds(11);
  //DDR_I2CBB &= ~(1 << SDA_BIT); // release SDA
  pinMode(SDA_BIT, INPUT);
  delayMicroseconds(50);

  return;
}



void I2CBitBanger::receiveI2cByte(bool sendAcknowledge, uint8_t* output) {
  /*
    Loop 8 times:
      Wait 15us (with SCL low)
      Release SCL (ensure release)
      Wait 15.5us
      Set SCL low

    Wait 25us
    Release SCL (ensure release)
    Wait 13us
    Set SCL low
    Wait 25us
  */

  uint8_t mask = 0x80;
  for (int i = 0; i < 8; i++) {
    delayMicroseconds(15);
    //DDR_I2CBB &= ~(1 << SCL_BIT); // release SCL
    pinMode(SCL_BIT, INPUT);
    ///////
    // rama: my nodeMCU crashes when polling an input pin for value
    // Not doing this still works fine on Arduino and fixes the problem on nodeMCU
    ///////
    //while ( (PIN_I2CBB & (1 << SCL_BIT)) == 0x00 ); // ensure SCL is actually high now (accounts for clock stretching)
    //while ( digitalRead(SCL_BIT) == 0 );

    // read the bit sent to us from the slave device
    //if (PIN_I2CBB & (1 << SDA_BIT)) {
    if (digitalRead(SDA_BIT) == 1) {
      // we received a 1
      (*output) = (*output) | mask;
    } else {
      // we received a 0
      (*output) = (*output) & ~mask;
    }
    mask = mask >> 1;

    delayMicroseconds(15);
    //DDR_I2CBB |= (1 << SCL_BIT); // set SCL low
    pinMode(SCL_BIT, OUTPUT);
    digitalWrite(SCL_BIT, 0);
  }

  delayMicroseconds(23);
  if (sendAcknowledge) {
    // pull SDL low to send an ACK to the slave device
    //DDR_I2CBB |= (1 << SDA_BIT); // set SDA low
    pinMode(SDA_BIT, OUTPUT);
    digitalWrite(SDA_BIT, 0);
  }

  delayMicroseconds(2);
  //DDR_I2CBB &= ~(1 << SCL_BIT); // release SCL
  pinMode(SCL_BIT, INPUT);

  // my nodeMCU crashes when polling an input pin for value
  //while ( (PIN_I2CBB & (1 << SCL_BIT)) == 0x00 ); // ensure SCL is actually high now (accounts for clock stretching)
  // while ( digitalRead(SCL_BIT) == 0);

  delayMicroseconds(13);
  //DDR_I2CBB |= (1 << SCL_BIT); // set SCL low
  pinMode(SCL_BIT, OUTPUT);
  digitalWrite(SCL_BIT, 0);

  delayMicroseconds(23);
  //DDR_I2CBB &= ~(1 << SDA_BIT); // release SDA
  pinMode(SDA_BIT, INPUT);
  delayMicroseconds(2);

  return;
}






