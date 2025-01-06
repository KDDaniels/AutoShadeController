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

// ======= EEPROM variables =======
#include <EEPROM.h>


// ======= Remote variabes ========
#define DECODE_NEC
#include <IRremote.hpp>
#include "button_definitions.h"


// ====== Motor variables ========
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

bool motorOneOpening = false;
bool motorTwoOpening = false;

bool motorOneClosing = false;
bool motorTwoClosing = false;

bool opening = false;
bool closing = false;

bool flashState = false;
unsigned long flashTarget = 0;
unsigned long flashDelay = 250;
unsigned long flashLongDelay = 10000;

bool stepperStopped = true;


// ===== Light variables ========
uint16_t averageVal = 0;
const uint16_t THRESHOLD = 40;
unsigned long readDelay = 5000; // 10 minutes in milliseconds (600,000ms)
unsigned long targetMillisSensor = 0;


void setup()
{
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  pinMode(LIGHT_SENSE, INPUT);

  // Load max steps
  EEPROM.get(0x00, motorOneMax);
  EEPROM.get(0x02, motorTwoMax);
  EEPROM.get(0x04, motorOneTarget);
  EEPROM.get(0x06, motorTwoTarget);

  stepper.setDelay(2);

  stepper.stop();
}

void loop()
{
  currentMillis = millis();
  checkIR();


  if (closing == true)
  {
    switch (activeMotor)
    {
      case 0:

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

        break;

      case 1:

        if (motorOneSteps < motorOneMax)
        {
          if (stepper.step(MOTOR_1, CW)) { motorOneSteps++; }
        }

        break;

      case 2:

        if (motorTwoSteps < motorTwoMax)
        {
          if (stepper.step(MOTOR_2, CW)) { motorTwoSteps++; }
        }

        break;
    }

    if (motorOneSteps >= motorOneMax)
    {
      motorOneOpen = false;
    }

    if (motorTwoSteps >= motorTwoMax)
    {
      motorTwoOpen = false;
    }

    if (motorOneOpen == false && motorTwoOpen == false)
    {
      closing = false;
      stepper.stop();
      stepperStopped = true;
    }
  }
  else if (opening == true)
  {
    switch (activeMotor)
    {
      case 0:
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
        break;

      case 1:
        if (motorOneSteps > motorOneTarget)
        {
          if (stepper.step(MOTOR_1, CCW)) { motorOneSteps--; }
        }
        break;

      case 2:
        if (stepper.step(MOTOR_2, CCW)) { motorTwoSteps--; }
        break;
    }

    if (motorOneSteps <= motorOneTarget)
    {
      motorOneOpen = true;
    }

    if (motorTwoSteps <= motorTwoTarget)
    {
      motorTwoOpen = true;
    }

    if (motorOneOpen == true && motorTwoOpen == true)
    {
      opening = false;
      stepper.stop();
      stepperStopped = true;
    }
  }

  // checks if light sensing should occur this loop
  if (isAuto == true)
  {

    if (currentMillis >= targetMillisSensor)
    {
      averageVal = 0;
      for (uint8_t x = 0; x < 10; x++)
      {
        averageVal += analogRead(LIGHT_SENSE); 
      }
      averageVal /= 10;
      targetMillisSensor += readDelay;
    }

    if (averageVal >= THRESHOLD + 5 && opening == false)
    {
      if ((motorOneSteps > motorOneTarget || motorTwoSteps > motorTwoTarget) || (motorOneOpen == false || motorTwoOpen == false))
      {
        opening = true;
        stepperStopped = false;
      }
    }
    else if (averageVal < THRESHOLD - 5 && closing == false)
    {
      if ((motorOneSteps < motorOneMax) || (motorTwoSteps < motorTwoMax) || (motorOneOpen == true || motorTwoOpen == true))
      {
        closing = true;
        stepperStopped = false;
      }
    }
    else
    {
      if (stepperStopped == false && ((motorOneOpen == true && motorTwoOpen == true) || (motorOneOpen == false && motorTwoOpen == false)))
      {
        stepper.stop();
        stepperStopped = true;
      }
    }
  }
  else if (opening == false && closing == false)
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
    switch (activeMotor) {
      
      case 1:
        if (stepper.step(MOTOR_1, CW)) { motorOneSteps++; }
        break;

      case 2:
        if (stepper.step(MOTOR_2, CW)) { motorTwoSteps++; }
        break;
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
            activeMotor = 0;
            stepper.stop();
            break;

          case FUNC: // Calibrate
            if (activeMotor != 0)
            {
              calibrating = !calibrating;
              isAuto = false;
              if (calibrating == false) {

                if (activeMotor == 1) 
                {
                  EEPROM.put(0x00, motorOneSteps);
                  motorOneMax = motorOneSteps;
                  motorOneOpen = false;
                }
                else if (activeMotor == 2) 
                {
                  EEPROM.put(0x02, motorTwoSteps);
                  motorTwoMax = motorTwoSteps;
                  motorTwoOpen = false;
                }
                stepper.stop();
              }
            }
            break;

          case PLAY: // Set target shade position
            if (activeMotor != 0)
            {
              if (activeMotor == 1)
              {
                EEPROM.put(0x04, motorOneSteps);
                motorOneTarget = motorOneSteps;
              }
              else if (activeMotor == 2)
              {
                EEPROM.put(0x06, motorTwoSteps);
                motorTwoTarget = motorTwoSteps;
              }
              opening = false;
              closing = false;
              stepper.stop();
            }
            break;

          case UP:
            isAuto = false;
            opening = !opening;
            closing = false;
            stepper.stop();
            break;

          case DOWN:
            isAuto = false;
            opening = false;
            closing = !closing;
            stepper.stop();
            break;

          case ZERO:
            isAuto = false;
            activeMotor = 0;
            opening = false;
            closing = false;
            stepper.stop();
            break;

          case ONE:
            isAuto = false;
            activeMotor = 1;
            opening = false;
            closing = false;
            stepper.stop();
            break;

          case TWO:
            isAuto = false;
            activeMotor = 2;
            opening = false;
            closing = false;
            stepper.stop();
            break;

          default:
            break;
      }
      IrReceiver.resume();
    }
  }
}