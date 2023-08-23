/*
 * AMT21_Multiturn_RS485_Sample_Code_Mega.ino
 * Company: CUI Devices
 * Author: Jason Kelly, Damon Tarry
 * Version: 1.0.0.0
 * Date: June 29, 2023
 *
 * This sample code can be used with the Arduino Mega to control the AMT21 encoder.
 * It uses one UART to control the encoder and the the another UART to report back to the PC
 * via the Arduino Serial Monitor.
 * For more information or assistance contact CUI Devices for support.
 *
 * After uploading code to Arduino Mega 2560 open the open the Serial Monitor under the Tools
 * menu and set the baud rate to 115200 to view the serial stream the position from the AMT21.
 *
 * This code is compatible with most Arduino boards, but it is important to verify the board
 * to be used has an available serial port for the RS485 connection.
 *
 * Arduino Pin Connections
 * TX:          Pin D19
 * RX:          Pin D18
 * REn:         Pin D8
 * DE:          Pin D9
 *
 *
 * AMT21 Pin Connections
 * Vdd (5V):    Pin  1
 * B:           Pin  2
 * A:           Pin  3
 * GND:         Pin  4
 * NC:          Pin  5
 * NC:          Pin  6
 *
 *
 * Required Equipment Note:
 * The AMT21 requires a high speed RS485 transceiver for operation. At the time this sample
 * code was written Arduino does not sell any compatible RS485 transceivers. For this demo
 * the MAX485 RS485 transceiver was used in a DIP format along with a breadboard. This code was
 * made in conjunction with a walkthrough demo project available online at www.cuidevices.com/resources/.
 *
 * This is free and unencumbered software released into the public domain.
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/* Serial rates for UART */
#define BAUDRATE             115200
#define RS485_BAUDRATE       2000000

/* We will use this define macro so we can write code once compatible with 12 or 14 bit encoders */
#define RESOLUTION            14

/* The AMT21 encoder is able to have a range of different values for its node address. This allows there
 * to be multiple encoders on on the RS485 bus. The encoder will listen for its node address, and that node
 * address doubles as the position request command. This simplifies the protocol so that all the host needs
 * to do is transmit the node address and the encoder will immediately respond with the position. The node address
 * can be any 8-bit number, where the bottom 2 bits are both 0. This means that there are 63 available addresses.
 * The bottom two bits are left as zero, because those bit slots are used to indicate extended commands. Taking
 * the node address and adding 0x01 changes the command to reading the turns counter (multi-turn only), and adding
 * 0x02 indicates that a second extended command will follow the first. We will define two encoder addresses below,
 * and then we will define the modifiers, but to reduce code complexity and the number of defines, we will not
 * define every single variation. 0x03 is unused and therefore reserved at this time.
 *
 */
#define RS485_RESET           0x75
#define RS485_ZERO            0x5E
#define RS485_ENC0            0x54
#define RS485_ENC1            0x58
#define RS485_POS             0x00 //this is unnecessary to use but it helps visualize the process for using the other modifiers
#define RS485_TURNS           0x01
#define RS485_EXT             0x02

/* The RS485 transceiver uses 4 pins to control the state of the differential RS485 lines A/B. To control
 * the transevier we will put those pins on our digital IO pins. We will define those pins here now. We will get
 * into more information about this later on, but it is important to understand that because of the high speed of the
 * AMT21 encoder, the toggling of these pins must occur very quickly. More information available in the walkthrough online.
 * Receive enable, drive enable, data in, received data out. Typically the RE and DE pins are connected together and controlled
 * with 1 IO pin but we'll connect them all up for full control.
 */
#define RS485_T_RE            8
#define RS485_T_DE            9
#define RS485_T_DI            18
#define RS485_T_RO            19

/* For ease of reading, we will create a helper function to set the mode of the transceiver. We will send that funciton
 * these arguments corresponding with the mode we want.
 */
#define RS485_T_TX            0 //transmit: receiver off, driver on
#define RS485_T_RX            1 //receiver: driver off, transmit on
#define RS485_T_2             2 //unused
#define RS485_T_3             3 //unused

void setup()
{
  //Set the modes for the RS485 transceiver
  pinMode(RS485_T_RE, OUTPUT);
  pinMode(RS485_T_DE, OUTPUT);
  //pinMode(RS485_T_DI, OUTPUT);
  //pinMode(RS485_T_RO, OUTPUT);

  //Initialize the UART serial connection to the PC
  Serial.begin(BAUDRATE);

  //Initialize the UART link to the RS485 transceiver
  Serial1.begin(RS485_BAUDRATE);
}

void loop()
{
  //create an array of encoder addresses so we can use them in a loop
  uint8_t addresses[2] = {RS485_ENC0, RS485_ENC1};

  for(int encoder = 0; encoder < sizeof(addresses); ++encoder)
  {
    //create a 16 bit variable to hold the encoders position and give it a default value of 0xFFFF which is our error value
    uint16_t encoderPosition = 0xFFFF;
    //create a 16 bit variable for the turns counter (multi-turn encoders only). This is a signed value
    int16_t encoderTurns;
   //let's also create a variable where we can count how many times we've tried to obtain the position in case there are errors
    uint8_t attempts = 0;
    //normally we use 0xFFFF as a failed value, but that is an acceptiblie value for a signed number so instead we will use a boolean for success
    bool turnsSuccess = false;

    //we will use a do-while loop for getting position because we want to start with calling the getposition command first
    //then repeat it if we get a failure, until we hit our limit number of attempts
    do
    {
      //this function gets the encoder position and returns it as a uint16_t
      encoderPosition = getPositionRS485(addresses[encoder], RESOLUTION);
    }
    while (encoderPosition == 0xFFFF && ++attempts < 3);


    if (encoderPosition == 0xFFFF) //position is bad, let the user know how many times we tried
    {
      Serial.print("Encoder ");
      Serial.print(encoder, DEC);
      Serial.print(" error. Attempts: ");
      Serial.print(attempts, DEC); //print out the number in decimal format.
      Serial.write('\n');
    }
    else //position was good, print to serial stream
    {
      //we'll use attempts again for the turns
      attempts = 0;

      Serial.print("Encoder ");
      Serial.print(encoder, DEC);
      Serial.print(": ");
      Serial.print(encoderPosition, DEC); //print the position in decimal format
      Serial.write('\n');

      //since the position was good lets also get the turns counter (multi-turn only)
      do
      {
       //this function gets the encoder turns and returns it as an int16_t
        encoderTurns = getTurnsRS485(addresses[encoder], turnsSuccess);
      }
      while (!turnsSuccess && ++attempts < 3);

      //and we'll do the if/else loop again. I know it's a bit messy now but we want to make sure we cover everything
      if (!turnsSuccess) //turns are bad, let the user know how many times we tried
      {
        Serial.print("Encoder ");
        Serial.print(encoder, DEC);
        Serial.print(" turns error. Attempts: ");
        Serial.print(attempts, DEC); //print out the number in decimal format.
        Serial.write('\n');
      }
      else
      {
        Serial.print("Encoder ");
        Serial.print(encoder, DEC);
        Serial.print(" turns: ");
        Serial.print(encoderTurns, DEC); //print the position in decimal format
        Serial.write('\n');
      }
    }

    Serial.write('\n');
  }

  //For the purpose of this demo we don't need the position returned that quickly so let's wait a half second between reads
  //delay() is in milliseconds
  delay(500);
}

bool verifyChecksumRS485(uint16_t message)
{
  //using the equation on the datasheet we can calculate the checksums and then make sure they match what the encoder sent
  //checksum is invert of XOR of bits, so start with 0b11, so things end up inverted
  uint16_t checksum = 0x3;
  for(int i = 0; i < 14; i += 2)
  {
    checksum ^= (message >> i) & 0x3;
  }
  return checksum == (message >> 14);
}

/*
 * This function gets the absolute position from the AMT21 encoder using the RS485 bus. The AMT21 position includes 2 checkbits to use
 * for position verification. Both 12-bit and 14-bit encoders transfer position via two bytes, giving 16-bits regardless of resolution.
 * For 12-bit encoders the position is left-shifted two bits, leaving the right two bits as zeros. This gives the impression that the encoder
 * is actually sending 14-bits, when it is actually sending 12-bit values, where every number is multiplied by 4.
 * This function takes the pin number of the desired device as an input
 * Error values are returned as 0xFFFF
 */
 uint16_t getPositionRS485(uint8_t address, uint8_t resolution)
{
  uint16_t currentPosition = 0xFFFF; // 16-bit response from the encoder
  bool error = false;                // we will use this instead of having a bunch of nested if statements, initialized to NO ERROR

  //clear whatever is in the read buffer in case there's nonsense in it
  while (Serial1.available()) Serial1.read();

  setStateRS485(RS485_T_TX); //call the function we use to put the transciver into transmit mode
  delayMicroseconds(10);   //IO operations take time, let's throw in an arbitrary 10 microsecond delay to make sure the transeiver is ready

  //here is where we send the command to get position. All we have to do is send the node address, but we can use the modifier for consistency
  Serial1.write(address + RS485_POS);

  //We expect a response from the encoder to begin within 3 microseconds. Each byte sent has a start and stop bit, so each 8-bit byte transmits
  //10 bits total. So for the AMT21 operating at 2 Mbps, transmitting the full 20 bit response will take about 10 uS. We expect the response
  //to start after 3 uS totalling 13 microseconds from the time we've finished sending data.
  //So we need to put the transceiver into receive mode within 3 uS, but we want to make sure the data has been fully transmitted before we
  //do that or we could cut it off mid transmission. This code has been tested and optimized for this; porting this code to another device must
  //take all this timing into account.

  //Here we will make sure the data has been transmitted and then toggle the pins for the transceiver
  //Here we are accessing the avr library to make sure this happens very fast. We could use Serial.flush() which waits for the output to complete
  //but it takes about 2 microseconds, which gets pretty close to our 3 microsecond window. Instead we want to wait until the serial transmit flag USCR1A completes.
  while (!(UCSR1A & _BV(TXC1)));

  setStateRS485(RS485_T_RX); //set the transceiver back into receive mode for the encoder response

  //We need to give the encoder enough time to respond, but not too long. In a tightly controlled application we would want to use a timeout counter
  //to make sure we don't have any issues, but for this demonstration we will just have an arbitrary delay before checking to see if we have data to read.
  delayMicroseconds(30);

  //Check the input buffer for 2 bytes
  if (Serial1.available() < 2) error = true;

  //if there wasn't an error
  if (!error)
  {
    currentPosition = Serial1.read();         //low byte comes first
    currentPosition |= Serial1.read() << 8;   //high byte next, OR it into our 16 bit holder but get the high bit into the proper placeholder

    if (verifyChecksumRS485(currentPosition))
    {
      //we got back a good position, so just mask away the checkbits
      currentPosition &= 0x3FFF;
    }
    else
    {
      currentPosition = 0xFFFF; //bad position response
    }

    //If the resolution is 12-bits, and wasn't 0xFFFF, then shift position, otherwise do nothing
    if ((resolution == 12) && (currentPosition != 0xFFFF)) currentPosition = currentPosition >> 2;
  }

  //flush the received serial buffer just in case anything extra got in there
  while (Serial1.available()) Serial1.read();

  return currentPosition;
}

/*
 * This function sets the state of the RS485 transceiver. We send it that state we want. Recall above I mentioned how we need to do this as quickly
 * as possible. To be fast, we are not using the digitalWrite functions but instead will access the avr io directly. I have shown the direct access
 * method and left commented the digitalWrite method.
 */
void setStateRS485(uint8_t state)
{
  //switch case to find the mode we want
  switch (state)
  {
    case RS485_T_TX:
      PORTH |= 0b01100000;
      //digitalWrite(RS485_RE, HIGH); //ph5
      //digitalWrite(RS485_DE, HIGH); //ph6
      break;
    case RS485_T_RX:
      PORTH &= 0b10011111;
      //digitalWrite(RS485_RE, LOW);
      //digitalWrite(RS485_DE, LOW);
      break;
    case RS485_T_2:
      PORTH &= 0b11011111;
      PORTH |= 0b01000000;
      //digitalWrite(RS485_RE, LOW);
      //digitalWrite(RS485_DE, HIGH);
      break;
    case RS485_T_3:
      PORTH &= 0b10111111;
      PORTH |= 0b00100000;
      //digitalWrite(RS485_RE, HIGH);
      //digitalWrite(RS485_DE, LOW);
      break;
    default:
      PORTH &= 0b10111111;
      PORTH |= 0b00100000;
      //digitalWrite(RS485_RE, HIGH);
      //digitalWrite(RS485_DE, LOW);
      break;
  }
}

/*
 * This function sends the command to the requested address and returns the turns counter as
 * a signed integer. This function requires a boolean to be passed to it that is used for verifying
 * success or failure. We will do our checksum and math on an unsigned int, then put it back into the signed
 * int once completed.
 */
int16_t getTurnsRS485(uint8_t address, bool &passTurns)
{
  uint16_t unsignedTurns = 0xFFFF;
  int16_t signedTurns = 0xFFFF;
  bool error = false;

  //clear whatever is in the read buffer in case there's nonsense in it
  while (Serial1.available()) Serial1.read();

  setStateRS485(RS485_T_TX); //call the function we use to put the transciver into transmit mode
  delayMicroseconds(10);   //IO operations take time, let's throw in an arbitrary 10 microsecond delay to make sure the transeiver is ready

  //here is where we send the command to get position. All we have to do is send the node address, but we can use the modifier for consistency
  Serial1.write(address + RS485_TURNS);

  //We expect a response from the encoder to begin within 3 microseconds. Each byte sent has a start and stop bit, so each 8-bit byte transmits
  //10 bits total. So for the AMT21 operating at 2 Mbps, transmitting the full 20 bit response will take about 10 uS. We expect the response
  //to start after 3 uS totalling 13 microseconds from the time we've finished sending data.
  //So we need to put the transceiver into receive mode within 3 uS, but we want to make sure the data has been fully transmitted before we
  //do that or we could cut it off mid transmission. This code has been tested and optimized for this; porting this code to another device must
  //take all this timing into account.

  //Here we will make sure the data has been transmitted and then toggle the pins for the transceiver
  //Here we are accessing the avr library to make sure this happens very fast. We could use Serial.flush() which waits for the output to complete
  //but it takes about 2 microseconds, which gets pretty close to our 3 microsecond window. Instead we want to wait until the serial transmit flag USCR1A completes.
  //This method takes about 1.1 micrseconds.
  while (!(UCSR1A & _BV(TXC1)));

  setStateRS485(RS485_T_RX); //set the transceiver back into receive mode for the encoder response

  //We need to give the encoder enough time to respond, but not too long. In a tightly controlled application we would want to use a timeout counter
  //to make sure we don't have any issues, but for this demonstration we will just have an arbitrary delay before checking to see if we have data to read.
  delayMicroseconds(30);

  if (!error)
  {
    unsignedTurns = Serial1.read();       //low byte comes first
    unsignedTurns |= Serial1.read() << 8; //high byte next, OR it into our 16 bit holder but get the high bit into the proper placeholder

    if (verifyChecksumRS485(unsignedTurns))
      {
        //the calculation was good
        passTurns = true;

        //at this point the turns counter still has the checkbits in it. This is where it gets tricky. The turns counter is a signed number
        //but it's held in a 16-bit data type. Because of that the first two bits, the check bits, affect the sign of the number. We can't just remove
        //them and leave as zero becuase then we'd be forcing a positive number, and if we left them as 1's we'd be forcing a negative number.
        //Now what we can do is let the arduino do some conversions for us, this works here but note that this is dangerous to do if you don't understand
        //how your compiler/processor manages these things, so directly copying this arduino code into a different device may not work. We could also just do
        //a check on what the leading bits are and as we shift the number make sure to apply them correctly but this is easier.

        //Here we're bitshifting the turns counter left twice to basically pad the right with 0's, and then dumping it into our variable that
        //has a signed data type.
        signedTurns = unsignedTurns << 2;

        //after we've asigned the left shifted unsignedTurns to signedTurns, signedTurns IS our signed turns counter, just multiplied or bitshifted a bit. To fix this, we'll now bitshift
        //it back to the right. If we bitshift an unsigned number the high bits would get padded with 0's, which is why we shifted everything to the left FIRST
        //before putting it into signedTurns. The arduino knows that signedTurns is a signed value and as such when it shifts to the right and does the padding,
        //it pads based on what was in the highest bit slot to begin with, knowing that to maintain a negative number they should be 1's, and to maintain a positive
        //number they should be 0's.
        signedTurns = (int16_t)(signedTurns >> 2);
      }
    else
    {
      //the calculation was bad. We use the pass variable as a notifier to the caller of this function to not accept what is in the output
      passTurns = false;
    }
  }

  //flush anything that could be in the receive buffer.
  while (Serial1.available()) Serial1.read();

  return signedTurns;
}
