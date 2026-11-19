// Board-specific setup for the joystick handheld. lateInitVariant() is the upstream hook
// for this (see src/platform/extra_variants/README.md); it runs after the InputBroker
// has been initialised.

#include "AnalogJoystick.h"
#include "QMC5883LCompass.h"
#include "input/ButtonThread.h"

extern ButtonThread *UserButtonThread;

// With a joystick for navigation, a short press of the user (PRG) button is more useful
// as "cancel" than as "next screen". Upstream hard-codes the button's events, so instead
// of patching InputBroker.cpp we replace the click callback on the button object itself.
// Long press (select) and the very long press (shutdown) keep their upstream behaviour.
static void userButtonClick(void *)
{
    static const InputEvent event = {"UserButton", INPUT_BROKER_CANCEL, 0, 0, 0};
    // May run from the button's pin interrupt, so hand over through the broker's queue;
    // the main loop delivers it.
    inputBroker->queueInputEvent(&event);
}

void lateInitVariant()
{
    if (!analogJoystick)
        analogJoystick = new AnalogJoystick();

    // The compass shares the secondary I2C bus (GPIO 33/34) with the BMP280
    if (!qmc5883lCompass)
        qmc5883lCompass = new QMC5883LCompass(Wire1);

    if (UserButtonThread && inputBroker)
        UserButtonThread->userButton.attachClick(userButtonClick, nullptr);
}
