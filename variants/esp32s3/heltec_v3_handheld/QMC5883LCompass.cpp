#include "QMC5883LCompass.h"

#include "NodeDB.h"
#include "main.h"
#include <Preferences.h>
#if HAS_SCREEN
#include "graphics/Screen.h"
#endif

// ---- Tunables (override in variant.h) ----
// Heading = offset - atan2(y, x), in degrees clockwise from north. The default offset
// reproduces the old rocket-ctrl-handheld firmware for this module's mounting.
#ifndef QMC5883L_HEADING_OFFSET_DEG
#define QMC5883L_HEADING_OFFSET_DEG 90.0f
#endif
// Set to 1 if the heading turns the wrong way when the device is rotated
#ifndef QMC5883L_HEADING_REVERSED
#define QMC5883L_HEADING_REVERSED 0
#endif
// Each axis must have swung at least this many counts before a heading is trusted.
// At the 2 gauss range the earth's horizontal field spans roughly 4000-12000 counts
// over a full turn, so this is reached only by actually rotating the device.
#ifndef QMC5883L_MIN_SPAN
#define QMC5883L_MIN_SPAN 2000
#endif

static constexpr uint8_t REG_DATA = 0x00;      // X LSB, X MSB, Y LSB, Y MSB, Z LSB, Z MSB
static constexpr uint8_t REG_STATUS = 0x06;    // bit 0: data ready
static constexpr uint8_t REG_CONTROL1 = 0x09;  // OSR | range | rate | mode
static constexpr uint8_t REG_CONTROL2 = 0x0A;  // bit 7: soft reset
static constexpr uint8_t REG_SET_RESET = 0x0B; // datasheet: write 0x01
static constexpr uint8_t STATUS_DRDY = 0x01;
static constexpr uint8_t MODE_CONTINUOUS = 0x01; // OSR 512, 2 gauss, 10 Hz, continuous
static constexpr uint8_t MODE_STANDBY = 0x00;

static constexpr uint32_t POLL_MS = 100;       // matches the 10 Hz output rate
static constexpr uint32_t POLL_IDLE_MS = 1000; // while the screen is off
static constexpr uint32_t SAVE_DELAY_MS = 60 * 1000; // save calibration once it has settled

static const char *const PREFS_NAMESPACE = "qmc5883l";

QMC5883LCompass *qmc5883lCompass = nullptr;

QMC5883LCompass::QMC5883LCompass(TwoWire &bus) : concurrency::OSThread("QMC5883L"), bus(bus)
{
    present = begin();
    if (!present) {
        LOG_WARN("QMC5883L compass not responding at 0x%02x", QMC5883L_ADDR);
        disable();
        return;
    }
    loadCalibration();
    LOG_INFO("QMC5883L compass ready (%s)", calibrated() ? "calibration loaded" : "rotate the device once to calibrate");
}

static bool writeRegister(TwoWire &bus, uint8_t reg, uint8_t value)
{
    bus.beginTransmission(QMC5883L_ADDR);
    bus.write(reg);
    bus.write(value);
    return bus.endTransmission() == 0;
}

bool QMC5883LCompass::begin()
{
    if (!writeRegister(bus, REG_CONTROL2, 0x80)) // soft reset; also proves the chip answers
        return false;
    delay(10);
    writeRegister(bus, REG_SET_RESET, 0x01);
    running = false;
    setRunning(true);
    return true;
}

void QMC5883LCompass::setRunning(bool run)
{
    if (run == running)
        return;
    if (writeRegister(bus, REG_CONTROL1, run ? MODE_CONTINUOUS : MODE_STANDBY))
        running = run;
}

bool QMC5883LCompass::readRaw(int16_t &x, int16_t &y, int16_t &z)
{
    bus.beginTransmission(QMC5883L_ADDR);
    bus.write(REG_STATUS);
    if (bus.endTransmission() != 0 || bus.requestFrom((uint8_t)QMC5883L_ADDR, (uint8_t)1) != 1)
        return false;
    if (!(bus.read() & STATUS_DRDY))
        return false; // no new sample yet; never block waiting for one

    bus.beginTransmission(QMC5883L_ADDR);
    bus.write(REG_DATA);
    if (bus.endTransmission() != 0 || bus.requestFrom((uint8_t)QMC5883L_ADDR, (uint8_t)6) != 6)
        return false;
    uint8_t raw[6];
    for (uint8_t &b : raw)
        b = bus.read();
    x = (int16_t)(raw[0] | (raw[1] << 8));
    y = (int16_t)(raw[2] | (raw[3] << 8));
    z = (int16_t)(raw[4] | (raw[5] << 8));
    return true;
}

bool QMC5883LCompass::calibrated() const
{
    return haveBounds && (xMax - xMin) >= QMC5883L_MIN_SPAN && (yMax - yMin) >= QMC5883L_MIN_SPAN;
}

void QMC5883LCompass::loadCalibration()
{
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, true))
        return;
    if (prefs.isKey("xmin")) {
        xMin = prefs.getShort("xmin");
        xMax = prefs.getShort("xmax");
        yMin = prefs.getShort("ymin");
        yMax = prefs.getShort("ymax");
        haveBounds = true;
    }
    prefs.end();
}

void QMC5883LCompass::saveCalibrationIfDue(uint32_t now)
{
    // Write only once the bounds have stopped growing for a while, to spare the flash
    if (!boundsDirty || !calibrated() || now - boundsChangedMs < SAVE_DELAY_MS)
        return;
    Preferences prefs;
    if (prefs.begin(PREFS_NAMESPACE, false)) {
        prefs.putShort("xmin", xMin);
        prefs.putShort("xmax", xMax);
        prefs.putShort("ymin", yMin);
        prefs.putShort("ymax", yMax);
        prefs.end();
        LOG_INFO("QMC5883L calibration saved: x %d..%d, y %d..%d", xMin, xMax, yMin, yMax);
    }
    boundsDirty = false;
}

void QMC5883LCompass::resetCalibration()
{
    haveBounds = false;
    boundsDirty = false;
    Preferences prefs;
    if (prefs.begin(PREFS_NAMESPACE, false)) {
        prefs.clear();
        prefs.end();
    }
    LOG_INFO("QMC5883L calibration cleared");
}

int32_t QMC5883LCompass::runOnce()
{
#if HAS_SCREEN
    // Nothing shows a heading while the screen is off, so let the chip sleep too
    if (screen && !screen->isScreenOn()) {
        setRunning(false);
        return POLL_IDLE_MS;
    }
#endif
    setRunning(true);

    int16_t x, y, z;
    if (!readRaw(x, y, z))
        return POLL_MS;

    const uint32_t now = millis();
    if (!haveBounds) {
        xMin = xMax = x;
        yMin = yMax = y;
        haveBounds = true;
    }
    if (x < xMin || x > xMax || y < yMin || y > yMax) {
        xMin = min(xMin, x);
        xMax = max(xMax, x);
        yMin = min(yMin, y);
        yMax = max(yMax, y);
        boundsDirty = true;
        boundsChangedMs = now;
    }
#ifdef QMC5883L_DEBUG
    static uint32_t lastLog = 0;
    if (now - lastLog > 1000) {
        lastLog = now;
        LOG_DEBUG("QMC5883L raw x=%d y=%d z=%d bounds x %d..%d y %d..%d", x, y, z, xMin, xMax, yMin, yMax);
    }
#endif
    if (!calibrated())
        return POLL_MS; // no heading yet; the UI keeps using its GPS-based estimate

    // Remove the hard-iron offset and normalise each axis to its observed range
    const float fx = (x - (xMax + xMin) / 2.0f) / (float)(xMax - xMin);
    const float fy = (y - (yMax + yMin) / 2.0f) / (float)(yMax - yMin);
    float angle = atan2f(fy, fx) * RAD_TO_DEG;
#if QMC5883L_HEADING_REVERSED
    angle = -angle;
#endif
    float heading = QMC5883L_HEADING_OFFSET_DEG - angle;

    // Same user setting the upstream magnetometer drivers honour
    switch (config.display.compass_orientation) {
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_90:
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_90_INVERTED:
        heading += 90;
        break;
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_180:
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_180_INVERTED:
        heading += 180;
        break;
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_270:
    case meshtastic_Config_DisplayConfig_CompassOrientation_DEGREES_270_INVERTED:
        heading += 270;
        break;
    default:
        break;
    }

#if HAS_SCREEN
    if (screen)
        screen->setHeading(heading); // wraps to 0..360 and smooths
#endif
    saveCalibrationIfDue(now);
    return POLL_MS;
}
