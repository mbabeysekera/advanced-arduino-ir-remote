volatile uint16_t inputCaptureData[32]; //To store received time periods
volatile uint8_t isFirstTriggerOccured = 0; //First Trigger Flag
volatile uint8_t receiveCounter = 0;  //Receiver Counter
volatile uint8_t receiveComplete = 0; //Receive Complete Flag

void relayControllerConfig();
void relayDev01();
void relayDev02();
void timerOneConfigForCapture();
uint32_t getCommand();

ISR (TIMER1_CAPT_vect) {  //Timer 01 has been configured to work with Input capture mode
  if (isFirstTriggerOccured) {  //Capturing will start after first falling edge detected by ICP1 Pin
    inputCaptureData[receiveCounter] = ICR1;  //Read the INPUT CAPTURE REGISTER VALUE
    if (inputCaptureData[receiveCounter] >  625) {  // if the value is greater than 625 (~2.5ms), then
      receiveCounter = 0; //reset "receiveCounter"
      receiveComplete = 0;  //reset "receiveComplete"
    } else {
      receiveCounter++;
      if (receiveCounter == 32) { //if all the bits are detected,
        receiveComplete = 1;  //then set the "receiveComplete" flag to "1"
      }
    }
  } else {
    isFirstTriggerOccured = 1;  //First falling edge occured! Start capturing from the second falling edge.
  }
  TCNT1 = 0;  //Reset Timer 01 counter after every capture
}

void setup() {
  relayControllerConfig();  //Configure Digital pins for Controlling Relays
  timerOneConfigForCapture(); //Configure Timer 01 to run in Input Capture Mode
  Serial.begin(115200); //Serial Interface for Debugging
  Serial.println("Decoder Starting!!");
}

void loop() {
  switch (getCommand()) { //get Remote Command
    case 0x20DF8877: relayDev01(); break; //If button 1 pressed -> Turn ON/OFF Relay 01
    case 0x20DF48B7: relayDev02(); break; //If button 2 pressed -> Turn ON/OFF Relay 02
  }
}

void relayControllerConfig() {
  DDRD |= (1 << DDD4) | (1 << DDD5);  //Configure PORTD PIN4 and PIN5 as outputs
  PORTD |= (1 << PORTD4) | (1 << PORTD5); //Set them to Logic HIGH (Both relays are off)
}

void relayDev01() {
  PORTD ^= (1 << PORTD4); //Toggle digital pin 4 on pro mini -> Relay 1
}

void relayDev02() {
  PORTD ^= (1 << PORTD5); //Toggle digital pin 5 on pro mini -> Relay 2
}

void timerOneConfigForCapture() {
  DDRB &= ~(1 << DDB0); //Set digital pin 8 as a input
  PORTB |= (1 << PORTB0); //Internal Pull-up enabled
  cli();  //Stop all interrupte until timer 01 configs are done
  TCCR1A = 0x00;  //Set to 0
  TCCR1B &= ~(1 << ICES1);  //Falling edge trigger enabled
  TCCR1B |= (1 << CS11) | (1 << CS10);  //Prescaler to 64 -> will increment Timer01 every 4us
  TCCR1C = 0x00;  //Set to 0
  TIMSK1 |= (1 << ICIE1); //Enable input capture interrupt
  sei();  //Enable all global interrupts
}

/*
   the time period t is calculated by:
   ( inputCaptureData[<INDEX>] * 4us ) / 1000 -> will give the result in milliseconds
   Ex:
      t = (325 * 4) / 1000
      t = 1.3ms
*/
uint32_t getCommand() {
  if (receiveComplete) {  //If receive complete, start decoding process
    uint32_t receiveStream = 0; //To store decoded value
    for (int i = 0; i < 32; i++) {  //Decode all 32 bits receive as time periods
      if (inputCaptureData[i] < 325 && inputCaptureData[i] > 250 && i != 31) {  //if the time period t* -> 1.0ms < t < 1.3ms
        receiveStream = (receiveStream << 1); //Only bit shift the current value
      } else if (inputCaptureData[i] < 625 && inputCaptureData[i] > 500) {  //if the time period t* -> 2.0ms < t < 2.5ms
        receiveStream |= 0x0001;  //increment value by 1 using Logic OR operation
        if (i != 31) {  //Only shift the bit unless it is the last bit of the captured stream
          receiveStream = (receiveStream << 1); //Only bit shift the current value
        }
        receiveComplete = 0;  //Set the receive complete to 0 for next data to be captured
      }
    }
    Serial.println(receiveStream, HEX); //Print the value in serial monitor for debugging
    return receiveStream; //Return the received data stream
  }
  return 0; //default return value is 0
}
