#if defined(__AVR_ATtiny85__)
#define IR_RECEIVE_PIN  PB4
#else
#define IR_RECEIVE_PIN  2
#endif

#include <Stepper595.hpp>

#define DECODE_NEC
#include <IRremote.hpp>
#include "button_definitions.h"

bool ledState = 0;


void setup()
{
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  pinMode(0, OUTPUT);
}

void loop()
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