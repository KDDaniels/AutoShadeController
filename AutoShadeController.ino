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
#define LIGHT_SENSE 3
#define LATCH 4
#endif

#include <Stepper595.hpp>

#define DECODE_NEC
#include <IRremote.hpp>
#include "button_definitions.h"

Stepper595 stepper(LATCH);
bool ledState = 0;

void setup()
{
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  pinMode(0, OUTPUT);
  pinMode(LIGHT_SENSE, INPUT);
}

void loop()
{
    checkIR();
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
          ledState = !ledState;
          digitalWrite(0, ledState);
            break;
          case FUNC:
            break;
          default:
            break;
      }
      IrReceiver.resume();
    }
  }
}