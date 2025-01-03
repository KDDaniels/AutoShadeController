/*
* This Arduino sketch is to allow an ATtiny85 to control two window shades via
* IR remote using the NEC protocol
* Copyright (C) 2024 Kendall Daniels
* 
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
thoughts

POWER will toggle automatic opening on or off for the selected shade, I think 0 will select both, 1 will select
motor 1, and 2 will select motor 2

UP and DOWN are self explanatory (up makes it go up, down makes it go down)

FUNC will act as a calibrate, shade starts all the way open and steps = 0, so 0 is all the way open
shade moves down and steps go up, pressing FUNC at the bottom will let the controller know
it's at the bottom and can save max steps for that shade
this'll need to be split between two shades of course, but that shouldn't be too hard
save the max values in EEPROM, probably the first two addresses that are available

PLAY/PAUSE will save target steps, so you can set where you want the shade to open to instead
of fully opening
gotta figure out circle buffer or whatever so it can save the current steps when it reaches target or max
will make it pretty much last forever hopefully
*/

#if defined(__AVR_ATtiny85__)
#define IR_RECEIVE_PIN  PB4
#define LIGHT_SENSE PB3
#define LATCH PB0
#else
#define IR_RECEIVE_PIN  2
#define LIGHT_SENSE A7
#define LATCH 10
#endif

unsigned long currentMillis = 0;

// ======= EEPROM stuff =======
#include <EEPROM.h>


// ======= Remote stuff ========
#define DECODE_NEC
#include <IRremote.hpp>
#include "button_definitions.h"


// ====== Motor stuff ========
#include <Stepper595.hpp>
Stepper595 stepper(LATCH);
uint16_t motorOneSteps = 0;
uint16_t motorTwoSteps = 0;

uint16_t motorOneMax = 0;
uint16_t motorTwoMax = 0;

uint16_t motorOneTarget = 0;
uint16_t motorTwoTarget = 0;

uint8_t activeMotor = 0b00; // motor 2 | motor 1
uint8_t isAuto = 0b00; // same as above

bool calibrating = false;

bool opening = false;
bool closing = false;


// ===== Light stuff ========
const uint8_t NUM_OF_READINGS = 10;
uint16_t readings[NUM_OF_READINGS]; // using an array of readings to get the average light level (more accurate)

uint16_t totalVal = 0;
uint8_t averageVal = 0;
uint8_t senseLoopNum = 0;
uint8_t readIndex = 0;

uint8_t avgDelay = 10; // delay between readings
uint16_t readDelay = 6000; // 10 minutes in milliseconds (600,000ms)
unsigned long targetMillisSensor = 0;
unsigned long targetMillisAvg = 0;

bool sensing = false;


void setup()
{
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  pinMode(LIGHT_SENSE, INPUT);

  // Initializes the readings array with 0
  for (uint8_t x = 0; x < NUM_OF_READINGS; x++) { readings[x] = 0; }

  // Load max steps
  if (EEPROM.read(0x00) != 0xFF) {
    EEPROM.get(0x00, motorOneMax);
    EEPROM.get(0x02, motorTwoMax);
    EEPROM.get(0x04, motorOneTarget);
    EEPROM.get(0x06, motorTwoTarget);
  }

  stepper.stop();
}

void loop()
{
  currentMillis = millis();
  checkIR();

  // needs to be between target and max, not 0 and target

  if (closing == true)
  {
    if (activeMotor == 0b01) {
      if (motorOneSteps < motorOneMax) {
        if (stepper.step(MOTOR_1, CW)) { motorOneSteps++; }
      } else {
        closing == false;
        stepper.stop();
      }
    } else if (activeMotor == 0b10) {
      if (motorTwoSteps < motorTwoMax) {
        if (stepper.step(MOTOR_2, CW)) { motorTwoSteps++; }
      } else {
        closing == false;
        stepper.stop();
      }
    }
  }


  if (opening == true) {
    if (activeMotor == 0b01) {
      if (motorOneSteps > motorOneTarget) {
        if (stepper.step(MOTOR_1, CCW)) { motorOneSteps--; }
      } else {
        opening = false;
        stepper.stop();
      }
    } else if (activeMotor == 0b10) {
      if (motorTwoSteps > motorOneTarget) {
        if (stepper.step(MOTOR_2, CCW)) { motorTwoSteps--; }
      } else {
        opening = false;
        stepper.stop();
      }
    }
  }


  // checks if light sensing should occur this loop
  if (currentMillis - targetMillisSensor > 0)
  {
    sensing = true;
    targetMillisSensor = currentMillis + readDelay;
  }

  if (sensing == true)
  {
    checkLightLevels();
  }

  // only calibrate one motor at a time
  if (calibrating == true)
  {
    if (activeMotor == 0b01) {
      if (stepper.step(MOTOR_1, CW)) { motorOneSteps++; }
    } else if (activeMotor == 0b10) {
      if (stepper.step(MOTOR_2, CW)) { motorTwoSteps++; }
    }
  }
}


void checkLightLevels()
{
  if (currentMillis - targetMillisAvg > 0)
  {
    totalVal = totalVal - readings[readIndex];
    readings[readIndex] = analogRead(LIGHT_SENSE);
    totalVal = totalVal + readings[readIndex];
    readIndex++;

    if (readIndex >= NUM_OF_READINGS)
    {
      readIndex = 0;
      sensing = false;
    }

    averageVal = totalVal / NUM_OF_READINGS;
    targetMillisAvg = currentMillis + avgDelay;
  }
}


void checkIR()
{
  if (IrReceiver.decode()) {
      if ((IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) || IrReceiver.decodedIRData.protocol == UNKNOWN) {
        IrReceiver.resume();
        return;
      } else {
        switch (IrReceiver.decodedIRData.command) {
          case POWER:
            
            break;

          case FUNC: // Calibrate
            calibrating = !calibrating;
            if (calibrating == false) {
              // save max steps into EEPROM when calibrating switches back to false
              if (activeMotor == 0b01) 
              { EEPROM.put(0x00, motorOneSteps); motorOneMax = motorOneSteps; }
              else if (activeMotor == 0b10) 
              { EEPROM.put(0x02, motorTwoSteps); motorTwoMax = motorTwoSteps; }
              stepper.stop();
            }
            break;

          case PLAY: // Set target shade position
            if (activeMotor == 0b01)
            { EEPROM.put(0x04, motorOneSteps); motorOneTarget = motorOneSteps; }
            else if (activeMotor == 0b10)
            { EEPROM.put(0x06, motorTwoSteps); motorTwoTarget = motorOneSteps; }
            stepper.stop();
            break;

          case UP:
            opening = true;
            closing = false;
            break;

          case DOWN:
            opening = false;
            closing = true;
            break;

          case ONE:
            if (opening == false && closing == false) { activeMotor = 0b01; }
            
            break;

          case TWO:
            if (opening == false && closing == false) { activeMotor = 0b10; }
            break;

          default:
            break;
      }
      IrReceiver.resume();
    }
  }
}