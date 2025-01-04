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

uint8_t activeMotor = 0; // motor 2 | motor 1 | both 0
bool isAuto = true;

bool calibrating = false;

bool motorOneOpen = true;
bool motorTwoOpen = true;

bool opening = false;
bool closing = false;


bool flashState = false;
unsigned long flashTarget = 0;
unsigned long flashDelay = 250;
unsigned long flashLongDelay = 10000;


// ===== Light stuff ========
const uint8_t NUM_OF_READINGS = 3;
uint16_t readings[NUM_OF_READINGS]; // using an array of readings to get the average light level (more accurate)

uint16_t totalVal = 0;
uint16_t averageVal = 0;
uint8_t senseLoopNum = 0;
uint8_t readIndex = 0;

unsigned long avgDelay = 10; // delay between readings
unsigned long readDelay = 1000; // 10 minutes in milliseconds (600,000ms)
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
  if (EEPROM.read(0x01) != 0xFF) {
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

  if (closing == true)
  {
    if (activeMotor == 0)
    {
      if ((motorOneSteps < motorOneMax) && (motorTwoSteps < motorTwoMax))
      {
        if (stepper.step(CW)) { motorOneSteps++; motorTwoSteps++; }
      }
      else if (motorOneSteps < motorOneMax) 
      {
        if (stepper.step(MOTOR_1, CW)) { motorOneSteps++; }
      }
      else if (motorTwoSteps < motorTwoMax) 
      {
        if (stepper.step(MOTOR_2, CW)) { motorTwoSteps++; }
      }
    }
    else
    {
      if ((activeMotor == 1) && (motorOneSteps < motorOneMax))
      {
        if (stepper.step(MOTOR_1, CW)) { motorOneSteps++; }
      }
      else if ((activeMotor == 2) && (motorTwoSteps < motorTwoMax))
      {
        if (stepper.step(MOTOR_2, CW)) { motorTwoSteps++; }
      }
    }

    if ((motorOneSteps == motorOneMax) && (motorTwoSteps == motorTwoMax))
    {
      motorOneOpen = false;
      motorTwoOpen = false;
      closing = false;
      stepper.stop();
    }
  }
  else if (opening == true)
  {
    if (activeMotor == 0)
    {
      if ((motorOneSteps > motorOneTarget) && (motorTwoSteps > motorTwoTarget))
      {
        if (stepper.step(CCW)) { motorOneSteps--; motorTwoSteps--; }
      }
      else if (motorOneSteps > motorOneTarget) 
      {
        if (stepper.step(MOTOR_1, CCW)) { motorOneSteps--; }
      }
      else if (motorTwoSteps > motorTwoTarget) 
      {
        if (stepper.step(MOTOR_2, CCW)) { motorTwoSteps--; }
      }
    }
    else
    {
      if ((activeMotor == 1) && (motorOneSteps > motorOneTarget))
      {
        if (stepper.step(MOTOR_1, CCW)) { motorOneSteps--; }
      }
      else if ((activeMotor == 2) && (motorTwoSteps > motorTwoTarget))
      {
        if (stepper.step(MOTOR_2, CCW)) { motorTwoSteps--; }
      }
    }

    if ((motorOneSteps == motorOneTarget) && (motorTwoSteps == motorTwoTarget))
    {
      motorOneOpen = true;
      motorTwoOpen = true;
      opening = false;
      stepper.stop();
    }
  }

  // checks if light sensing should occur this loop
  if (currentMillis >= targetMillisSensor)
  {
    averageVal = (analogRead(LIGHT_SENSE) + analogRead(LIGHT_SENSE)) / 2;
    targetMillisSensor += readDelay;
  }

  // move all motor checks into an if isAuto statement
  if ((isAuto == true) && (averageVal >= 20))
  {
    if (motorOneOpen == true && motorTwoOpen == true)
    {
      opening = false;
    }
    else
    {
      opening = true;
    }
  } 
  else if ((isAuto == true) && (averageVal < 20)) 
  {
    if (motorOneOpen == false && motorTwoOpen == false)
    {
      closing = false;
    }
    else
    {
      closing = true;
    }
  }

  if ((isAuto == false) && (opening == false) && (closing == false))
  {
    if (currentMillis >= flashTarget)
    {
      flashState = !flashState;
      flash(flashState);
      flashTarget += flashState ? flashDelay : flashLongDelay;
    }
  }


  // only calibrate one motor at a time
  if (calibrating == true)
  {
    if (activeMotor == 1) {
      if (stepper.step(MOTOR_1, CW)) { motorOneSteps++; }
    } else if (activeMotor == 2) {
      if (stepper.step(MOTOR_2, CW)) { motorTwoSteps++; }
    }
  }
}


void flash(bool enabled)
{
  if (enabled == true)
  {
    stepper.step(CW);
  }
  else
  {
    stepper.step(CCW);
    stepper.stop();
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
            isAuto = !isAuto;
            stepper.stop();
            break;

          case FUNC: // Calibrate
            calibrating = !calibrating;
            if (calibrating == false) {
              // save max steps into EEPROM when calibrating switches back to false
              if (activeMotor == 1) 
              { EEPROM.put(0x00, motorOneSteps); motorOneMax = motorOneSteps; }
              else if (activeMotor == 2) 
              { EEPROM.put(0x02, motorTwoSteps); motorTwoMax = motorTwoSteps; }
              stepper.stop();
            }
            break;

          case PLAY: // Set target shade position
            if (activeMotor == 1)
            { EEPROM.put(0x04, motorOneSteps); motorOneTarget = motorOneSteps; }
            else if (activeMotor == 2)
            { EEPROM.put(0x06, motorTwoSteps); motorTwoTarget = motorOneSteps; }
            stepper.stop();
            break;

          case UP:
            isAuto = false;
            opening = true;
            closing = false;
            break;

          case DOWN:
            isAuto = false;
            opening = false;
            closing = true;
            break;

          case ZERO:
            activeMotor = 0;
            opening = false;
            closing = false;
            stepper.stop();
            break;

          case ONE:
            opening = false;
            closing = false;
            activeMotor = 1;
            break;

          case TWO:
            opening = false;
            closing = false;
            activeMotor = 2;
            break;

          default:
            break;
      }
      IrReceiver.resume();
    }
  }
}