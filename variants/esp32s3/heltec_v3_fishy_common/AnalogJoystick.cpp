#include "AnalogJoystick.h"

#ifdef JOYSTICK_PIN_UD

#include "main.h"
#if HAS_SCREEN
#include "graphics/Screen.h"
#endif

// ---- Tunables (override in variant.h) ----
// Raw 12-bit ADC readings. A direction engages past the outer threshold and releases only
// once the stick is back inside the inner one, so a stick resting near a threshold does
// not chatter.
#ifndef JOYSTICK_LOW_ENGAGE
#define JOYSTICK_LOW_ENGAGE 1000
#endif
#ifndef JOYSTICK_LOW_RELEASE
#define JOYSTICK_LOW_RELEASE 1300
#endif
#ifndef JOYSTICK_HIGH_ENGAGE
#define JOYSTICK_HIGH_ENGAGE 2600
#endif
#ifndef JOYSTICK_HIGH_RELEASE
#define JOYSTICK_HIGH_RELEASE 2300
#endif
#ifndef JOYSTICK_CENTER
#define JOYSTICK_CENTER 1816
#endif
// Set to 1 in variant.h if an axis reads the other way round
#ifndef JOYSTICK_INVERT_UD
#define JOYSTICK_INVERT_UD 0
#endif
#ifndef JOYSTICK_INVERT_LR
#define JOYSTICK_INVERT_LR 0
#endif

static constexpr uint32_t POLL_MS = 30;             // while the screen is on
static constexpr uint32_t POLL_IDLE_MS = 100;       // while the screen is off
static constexpr uint32_t REPEAT_DELAY_MS = 500;    // hold this long before up/down repeat
static constexpr uint32_t REPEAT_INTERVAL_MS = 180; // then repeat this often
static constexpr uint32_t DEBOUNCE_MS = 30;
static constexpr uint32_t LONG_PRESS_MS = 500; // same as the user button and trackball

static const char inputSourceName[] = "joystick";

AnalogJoystick *analogJoystick = nullptr;

AnalogJoystick::AnalogJoystick() : concurrency::OSThread(inputSourceName)
{
    pinMode(JOYSTICK_PIN_BTN, INPUT_PULLUP);
    pinMode(JOYSTICK_PIN_UD, INPUT);
    pinMode(JOYSTICK_PIN_LR, INPUT);
    if (inputBroker)
        inputBroker->registerSource(this);
    LOG_INFO("Analog joystick: UD=GPIO%d LR=GPIO%d BTN=GPIO%d", JOYSTICK_PIN_UD, JOYSTICK_PIN_LR, JOYSTICK_PIN_BTN);
}

void AnalogJoystick::send(input_broker_event event)
{
    InputEvent e = {};
    e.source = inputSourceName;
    e.inputEvent = event;
    notifyObservers(&e);
}

AnalogJoystick::Direction AnalogJoystick::readDirection()
{
    int ud = analogRead(JOYSTICK_PIN_UD);
    int lr = analogRead(JOYSTICK_PIN_LR);
#ifdef JOYSTICK_DEBUG
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 500) {
        lastLog = millis();
        LOG_DEBUG("Joystick raw UD=%d LR=%d BTN=%d", ud, lr, digitalRead(JOYSTICK_PIN_BTN));
    }
#endif

    // While a direction is held, use the (closer to centre) release thresholds
    const int low = heldDirection == NONE ? JOYSTICK_LOW_ENGAGE : JOYSTICK_LOW_RELEASE;
    const int high = heldDirection == NONE ? JOYSTICK_HIGH_ENGAGE : JOYSTICK_HIGH_RELEASE;

    const bool udActive = ud < low || ud > high;
    const bool lrActive = lr < low || lr > high;
    if (!udActive && !lrActive)
        return NONE;

    // On a diagonal, the axis pushed further from centre wins
    const bool useUd = udActive && (!lrActive || abs(ud - JOYSTICK_CENTER) >= abs(lr - JOYSTICK_CENTER));
    if (useUd)
        return ((ud < JOYSTICK_CENTER) != (JOYSTICK_INVERT_UD != 0)) ? UP : DOWN;
    return ((lr < JOYSTICK_CENTER) != (JOYSTICK_INVERT_LR != 0)) ? LEFT : RIGHT;
}

void AnalogJoystick::pollStick(uint32_t now)
{
    static const input_broker_event events[] = {INPUT_BROKER_NONE, INPUT_BROKER_UP, INPUT_BROKER_DOWN, INPUT_BROKER_LEFT,
                                                INPUT_BROKER_RIGHT};
    const Direction dir = readDirection();

    if (dir != heldDirection) {
        heldDirection = dir;
        if (dir != NONE) {
            send(events[dir]);
            nextRepeatMs = now + REPEAT_DELAY_MS;
        }
        return;
    }

    // Auto-repeat for scrolling only; repeating left/right would skip past tabs
    if ((dir == UP || dir == DOWN) && (int32_t)(now - nextRepeatMs) >= 0) {
        send(events[dir]);
        nextRepeatMs = now + REPEAT_INTERVAL_MS;
    }
}

void AnalogJoystick::pollButton(uint32_t now)
{
    const bool raw = digitalRead(JOYSTICK_PIN_BTN) == LOW; // active low, internal pull-up
    if (raw != buttonRaw) {
        buttonRaw = raw;
        buttonChangedMs = now;
    }

    if (raw != buttonDown && now - buttonChangedMs >= DEBOUNCE_MS) {
        buttonDown = raw;
        if (buttonDown) {
            buttonDownMs = now;
            longPressSent = false;
        } else if (!longPressSent) {
            send(INPUT_BROKER_SELECT); // short press, reported on release
        }
    }

    if (buttonDown && !longPressSent && now - buttonDownMs >= LONG_PRESS_MS) {
        longPressSent = true;
        send(INPUT_BROKER_SELECT_LONG); // reported once, while still held
    }
}

int32_t AnalogJoystick::runOnce()
{
    const uint32_t now = millis();
    pollStick(now);
    pollButton(now);

#if HAS_SCREEN
    // Any movement wakes the screen through the InputBroker, so a slow poll is enough then
    if (screen && !screen->isScreenOn() && heldDirection == NONE && !buttonDown)
        return POLL_IDLE_MS;
#endif
    return POLL_MS;
}

#endif // JOYSTICK_PIN_UD
