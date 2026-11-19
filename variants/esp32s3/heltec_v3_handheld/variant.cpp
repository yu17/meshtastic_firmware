// Board-specific setup for the joystick handheld. lateInitVariant() is the upstream hook
// for this (see src/platform/extra_variants/README.md); it runs after the InputBroker
// has been initialised.

#include "AnalogJoystick.h"

void lateInitVariant()
{
    if (!analogJoystick)
        analogJoystick = new AnalogJoystick();
}
