/*
  mybook4's I2C bit banging library for the Digispark Pro
  Only supports writing at the moment
  Developed specifically to write register values to the GBS 8220
  Timing is based on the timing observed with the GBS 8220's onboard microcontroller

  This is (somewhat) based off of the TinyWireM and USI_TWI_Master interface
*/
/*
  Further changes for portability and generalization by rama:
  - uses standard Arduiono pin manipulation and delay functions
  - workaround for a crash on nodeMCU boards
  Notes:
  The delay timing is not very important. Correct order of operations is though!
  This is probably the reason the standard Wire library wouldn't work for mybook4.
*/

#ifndef I2CBitBanger_h
#define I2CBitBanger_h

#include <inttypes.h>
#include <Arduino.h>

#define I2CBB_RW_BIT_POSITION 0x01
#define I2CBB_BUF_SIZE    33             // bytes in message buffer (holds a slave address and 32 bytes

//#define DDR_I2CBB             DDRD
//#define PORT_I2CBB            PORTD
//#define PIN_I2CBB             PIND

#define SDA_BIT        5
#define SCL_BIT        4

#define I2C_ACK 0
#define I2C_NACK 1


/*
      Normal interface usage pseudo code is as follows:

      To Write:

      I2CBitBanger test(<7-bit address>);
      test.addByteForTransmission(<byte to send>);
      if(!test.transmitData())
        failed to send data
      else
        success

      To Read:

      uint8_t recevedData[4];
      I2CBitBanger test(<7-bit address>);
      int bytesReceived = test.recvData(4, receivedData);
      if(bytesReceived != 4) {
        failed to read data
      }

      To Change The Slave Address:

      test.setSlaveAddress(<7-bit address>);

*/

class I2CBitBanger
{
  public:
    I2CBitBanger(uint8_t sevenBitAddressArg);

    void setSlaveAddress(uint8_t sevenBitAddressArg); // 7-bit address

    // Adds a byte to an internally managed transmission buffer (preping for a write)
    void addByteForTransmission(uint8_t data);

    // Adds a buffer to an internally managed transmission buffer (preping for a write)
    void addBytesForTransmission(uint8_t* buffer, uint8_t bufferSize);

    bool transmitData();  // returns true if transmission was successful (everything ACKed by the slave device)
    int recvData(int numBytesToRead, uint8_t* outputBuffer);  // returns the number of bytes that were read


  private:
    static uint8_t I2CBB_Buffer[];           // holds I2C send data
    static uint8_t I2CBB_BufferIndex;        // current number of bytes in the send buff

    void initializePins();

    bool sendDataOverI2c(uint8_t* buffer, uint8_t bufferSize);
    void sendI2cStartSignal();
    bool sendI2cByte(uint8_t dataByte);
    void sendI2cStopSignal();

    void receiveI2cByte(bool sendAcknowledge, uint8_t* output);
};

#endif



