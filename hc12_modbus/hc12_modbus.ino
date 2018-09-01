/*
 * Modbus_HC12
 * Demonstration of a Modbus Slave implementation using an 
 * Arduino Nano and a HC12 wireless board. This implementation 
 * uses SoftwareSerial to keep the main serial port available 
 * for other uses. 
 * 
 * Wiring details:
 * Nano - HC12
 *  5V - VCC
 * GND - GND
 *  D4 - RXD 
 *  D5 - TXD
 *  D6 - SET (not used)
 */

#include "Modbus.h"
#include "ModbusSerial.h"

const int HOLDING_REGISTER = 100;   // Holding Register address
const long MODBUS_BAUD = 1200;      // Baudrate for Modbus comms

const byte HC12RxdPin = 4;          // RX Pin on HC12
const byte HC12TxdPin = 5;          // TX Pin on HC12
const byte HC12SetPin = 6;          // SET Pin on HC12

SoftwareSerial HC12(HC12TxdPin,HC12RxdPin);
ModbusSerial mb;

int regValue = 25;

void setup() {
  mb.config (&HC12, 1200);
  mb.setSlaveId (10);
  mb.addHreg (HOLDING_REGISTER, regValue);
}

void loop() {
  mb.task ();
  delay(200);
  regValue = random(10,35);
  mb.Hreg(HOLDING_REGISTER, regValue);
}
