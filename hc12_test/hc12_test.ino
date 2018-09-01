/*
 * HC-12 test program
 * 
 * usage:
 * open console, any AT command switches the HC12 into SET mode and excutes the command 
 * any other console input will switch HC12 back into transparent mode.
 * 
 * circuit:
 * HC-12 VCC -> Nano 3V3
 * HC-12 GND -> Nano GND
 * HC-12 RXD -> Nano D3
 * HC-12 TXD -> Nano D2
 * HC-12 SET -> Nano D4
 * 
 * NOTE:
 * setting the ON-Air baudrate to anything other than 9600 causes problems 
 * with subsequent access to SET mode using 9600 BAUD.
 * The problem can be overcome by hard wiring the SET connection to a GND pin 
 * on the Nano before applying power to the HC-12.
 * Whilst in this scenario the hc12_setup() function will fail, the actual 
 * data rx/tx part works OK. 
 */

#include <SoftwareSerial.h>
//#include <SPI.h>
#include <U8x8lib.h>


#include "debug.h"

#define DEBUG_LEVEL_DEFAULT L_INFO;
debugLevels currentDebugLevel;
  
#define CONSOLE_BAUD 9600       // Console

#define HC12_BAUD 1200          // Baudrate over air
#define HC12_CHANNEL "001"      // 001 to 127, use spacing of 5 channels 
#define HC12_POWER 8            // TX power 1-8 = -1/2/5/8/11/14/17/20dBm
#define HC12_MODE 3             // 1-4 = FU1/FU2/FU3/FU4
#define HC12_SET_BAUD 9600      // Baudrate for HC12 set mode
#define HC12_SET_TIMEOUT 500    // Timeout for HC12 set mode
#define HC12_SET_RETRY 5        // Number of retries before we give up

#define BEACON_INTERVAL 2000

const byte HC12_rx_pin = 2;
const byte HC12_tx_pin = 3;
const byte HC12_set_pin = 4;

// Constructor for HC-12 serial interface
SoftwareSerial HC12( /* RX */ HC12_rx_pin, /* TX */ HC12_tx_pin);

// Constructor for OLED text mode
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(/* clock=*/ 21, /* data=*/ 20);

char HC12ByteIn, consoleByteIn;
String HC12ReadBuffer = "";  
String consoleReadBuffer = "";             
long nextBeacon;
int loopCount = 0;
bool setMode = false;

void setup() {
  int i;
  bool b = false;
  Serial.begin(CONSOLE_BAUD);
  currentDebugLevel = DEBUG_LEVEL_DEFAULT;

  oled_setup();
  
  HC12ReadBuffer.reserve(64);
  consoleReadBuffer.reserve(64);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);
  hc12_setup();
/*  
  // wait until we can see HC-12 
  while (!hc12_setup() ) {
    for (i=0; i<5000; i+=200) {
      delay(200);
      b = !b;
      digitalWrite(LED_BUILTIN, b);
    }
    digitalWrite(LED_BUILTIN, 0);
  }
*/
  nextBeacon = millis() + BEACON_INTERVAL;
  HC12ReadBuffer.remove(0);
}

void loop() {  
  delay(1);

  if (Serial.available()) {
    consoleByteIn = Serial.read();
    consoleReadBuffer += char(consoleByteIn);
    if (consoleByteIn == '\n') {
      consoleInProcess();
    }
  }

  if (millis() > nextBeacon) {
    if (!setMode) sendBeacon(); 
    nextBeacon = millis() + BEACON_INTERVAL;
  }

  if (HC12.available()) {
    //u8x8.drawString(0,1,"got one");
    HC12ByteIn = HC12.read();
    Serial.print(HC12ByteIn);  
    if (HC12ByteIn == '\n') {
      u8x8.setCursor(0,2);
      u8x8.print("                ");
      u8x8.setCursor(0,2);
      u8x8.print(HC12ReadBuffer);
      HC12ReadBuffer.remove(0);
    } else {
      HC12ReadBuffer += char(HC12ByteIn);
    }
  }
}

void oled_setup() {
  u8x8.begin(); /* u8x8.begin() is required and will sent the setup/init sequence to the display */

  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0,0," HC-12 test bed");
}

void consoleInProcess() {
  if ( (consoleReadBuffer[0] == 'A') && (consoleReadBuffer[1] == 'T') ) {
    hc12_set_mode(true);    
  } else {
    hc12_set_mode(false);   
  }

  HC12.print(consoleReadBuffer);

  consoleReadBuffer.remove(0);
}

void hc12_set_mode(bool newMode) {
  // change CH-12 between SET and transparent mode

  // do we need to change the mode?
  if (newMode == setMode) return;

  if (newMode) {
    // place HC12 into set mode
    HC12.begin(HC12_SET_BAUD);
    pinMode(HC12_set_pin, OUTPUT);
    digitalWrite(HC12_set_pin, LOW);
    debug(L_INFO, "SET mode\n");
    delay(500);
  } else {
    // return HC12 to transparent mode
    pinMode(HC12_set_pin, INPUT);
    debug(L_INFO, "Transparent mode\n");
    delay(100);
    HC12.begin(HC12_BAUD);
  }
  setMode = newMode;
}

void sendBeacon() {
  HC12.print(loopCount++);
  HC12.print(" - HC12 test\n"); 
}

// print debug output on console interface
void debug(debugLevels level, char *sFmt, ...)
{
  if (level > currentDebugLevel) return;  // bypass if level is not high enough
  char acTmp[128];       // place holder for sprintf output
  va_list args;          // args variable to hold the list of parameters
  va_start(args, sFmt);  // mandatory call to initilase args 

  vsprintf(acTmp, sFmt, args);
  Serial.print(acTmp);
  // mandatory tidy up
  va_end(args);
  return;
}

bool hc12_setup() {
  bool ret_stat = false;
  int retries = 0;

  hc12_set_mode(true);

retry:
  // check if HC12 is responding
  HC12.print("AT\n");
  if ( !hc12_rx_line( HC12_SET_TIMEOUT ) ) goto failed;
  if ( !hc12_check_set_response() ) goto failed;
  
  // retrieve HC12 version 
  HC12.print("AT+V\n");
  if ( !hc12_rx_line( HC12_SET_TIMEOUT ) ) goto failed;
  debug( L_INFO, HC12ReadBuffer.c_str() );
  
  // set channel number
  HC12.print("AT+C");
  HC12.print(HC12_CHANNEL);
  HC12.print("\n");
  if ( !hc12_rx_line( HC12_SET_TIMEOUT ) ) goto failed;

  // set baudrate for transparent mode
  HC12.print("AT+B");
  HC12.print(HC12_BAUD);
  HC12.print("\n");
  if ( !hc12_rx_line( HC12_SET_TIMEOUT ) ) goto failed;

  // set power level
  HC12.print("AT+P");
  HC12.print(HC12_POWER);
  HC12.print("\n");
  if ( !hc12_rx_line( HC12_SET_TIMEOUT ) ) goto failed;  

  // set mode
  HC12.print("AT+FU");
  HC12.print(HC12_MODE);
  HC12.print("\n");
  if ( !hc12_rx_line( HC12_SET_TIMEOUT ) ) goto failed;  
  
  // read all parameters
  HC12.print("AT+RX");
  if ( !hc12_rx( HC12_SET_TIMEOUT ) ) goto failed;
  debug( L_INFO, HC12ReadBuffer.c_str() );
  //Serial.print(HC12ReadBuffer);

  ret_stat = true;

failed:
  if (ret_stat == false) {
    if (++retries < HC12_SET_RETRY) {
      debug(L_INFO, "Retry %d\n", retries);
      goto retry;
    }
  }
  hc12_set_mode(false);
  
  if (!ret_stat)
    Serial.print("hc12_setup failed\n");
  return ret_stat;
}

bool hc12_rx(int timeout) {
  int rxCount = 0;
  HC12ReadBuffer.remove(0);   // clear string
  // wait for reply from hc12
  long finishtime = millis() + timeout;
  while (millis() < finishtime) {
    if (HC12.available()) {
      rxCount++;
      HC12ByteIn = HC12.read();
      HC12ReadBuffer += char(HC12ByteIn);
    }
  }
  if (rxCount < 1) return false;
  return true;
}

bool hc12_rx_line(int timeout) {
  // receive one line
  HC12ReadBuffer.remove(0);   // clear string
  long finishtime = millis() + timeout;
  while (millis() < finishtime) {
    if (HC12.available()) {
      HC12ByteIn = HC12.read();
      HC12ReadBuffer += char(HC12ByteIn);
      if (HC12ByteIn == '\n') return true;
    }
  }
  //debug(L_DEBUG, "hc12_rx_line() Timeout\n");
  return false;
}

bool hc12_check_set_response() {
  // check if we received "OK" from HC12
  if ( (HC12ReadBuffer[0] != 'O') || (HC12ReadBuffer[1] != 'K') ) return false;
  return true;
}

