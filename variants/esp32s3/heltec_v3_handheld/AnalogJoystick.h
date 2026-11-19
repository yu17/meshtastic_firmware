#pragma once

// Two-axis analog joystick with a push button, as an InputBroker source.
// Self-contained: lives in the variant folder and is created from lateInitVariant(),
// so no upstream source file needs to change. Pins and thresholds come from variant.h.

#include "configuration.h"

#include "concurrency/OSThread.h"
#include "input/InputBroker.h"

class AnalogJoystick : public Observable<const InputEvent *>, public concurrency::OSThread
{
  public:
    AnalogJoystick();

  protected:
    int32_t runOnce() override;

  private:
    enum Direction : uint8_t { NONE, UP, DOWN, LEFT, RIGHT };

    Direction readDirection();
    void pollStick(uint32_t now);
    void pollButton(uint32_t now);
    void send(input_broker_event event);

    // Stick state
    Direction heldDirection = NONE;
    uint32_t nextRepeatMs = 0;

    // Button state
    bool buttonDown = false;     // debounced state
    bool buttonRaw = false;      // last raw reading
    bool longPressSent = false;  // long press already reported for this hold
    uint32_t buttonChangedMs = 0; // when the raw reading last changed
    uint32_t buttonDownMs = 0;    // when the debounced press started
};

extern AnalogJoystick *analogJoystick;
