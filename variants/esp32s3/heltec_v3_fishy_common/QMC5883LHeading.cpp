#include "QMC5883LHeading.h"

#include "Default.h"
#include "GPS.h"
#include "GPSStatus.h"
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

// Above this ground speed the GPS course replaces the magnetometer: a moving vehicle
// disturbs the magnetic field, and its direction of travel is what matters there. Two
// thresholds so the source does not flap around one value. Below walking/jogging pace GPS
// course is too noisy to use, so the lower limit should stay above that.
// SET/RESET period register; the datasheet says to write 0x01
#ifndef QMC5883L_SET_RESET_VALUE
#define QMC5883L_SET_RESET_VALUE 0x01
#endif
#ifndef COMPASS_GPS_ENTER_KMH
#define COMPASS_GPS_ENTER_KMH 11 // about 3 m/s
#endif
#ifndef COMPASS_GPS_EXIT_KMH
#define COMPASS_GPS_EXIT_KMH 7 // about 2 m/s
#endif
// GPS course is only used when the receiver is kept always on, which the firmware does
// for gps_update_interval <= 10 s; it then publishes one fix per interval. With longer
// intervals the receiver sleeps between fixes and the magnetometer is the better source.
// A fix counts as fresh for one interval plus this margin, so the source does not flip
// back to the magnetometer between two consecutive fixes.
// Set to 1 in variant.h to show GPS course even when the magnetometer gives no heading
#ifndef COMPASS_GPS_COURSE_WITHOUT_MAG
#define COMPASS_GPS_COURSE_WITHOUT_MAG 0
#endif
#ifndef COMPASS_GPS_AGE_MARGIN_MS
#define COMPASS_GPS_AGE_MARGIN_MS 4000
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
static constexpr uint32_t RETRY_MS = 5000;     // between attempts to find a missing chip
static constexpr uint32_t SAVE_DELAY_MS = 60 * 1000; // save calibration once it has settled

static const char *const PREFS_NAMESPACE = "qmc5883l";

QMC5883LHeading *qmc5883lHeading = nullptr;

QMC5883LHeading::QMC5883LHeading(TwoWire &bus) : concurrency::OSThread("QMC5883L"), bus(bus)
{
    loadCalibration();
    present = begin();
    if (!present) {
        LOG_WARN("QMC5883L compass not responding at 0x%02x, will keep trying", QMC5883L_ADDR);
        return;
    }
    LOG_INFO("QMC5883L compass ready (%s)", calibrated() ? "calibration loaded" : "rotate the device once to calibrate");
}

static bool writeRegister(TwoWire &bus, uint8_t reg, uint8_t value)
{
    bus.beginTransmission(QMC5883L_ADDR);
    bus.write(reg);
    bus.write(value);
    return bus.endTransmission() == 0;
}

bool QMC5883LHeading::begin()
{
    if (!writeRegister(bus, REG_CONTROL2, 0x80)) // soft reset; also proves the chip answers
        return false;
    delay(10);
    writeRegister(bus, REG_SET_RESET, QMC5883L_SET_RESET_VALUE);
    running = false;
    setRunning(true);
#ifdef QMC5883L_DEBUG
    // Register dump, to tell chip variants apart (a genuine QMC5883L reads 0xFF at 0x0D)
    delay(150);
    bus.beginTransmission(QMC5883L_ADDR);
    bus.write((uint8_t)0x00);
    if (bus.endTransmission() == 0 && bus.requestFrom((uint8_t)QMC5883L_ADDR, (uint8_t)14) == 14) {
        uint8_t r[14];
        for (uint8_t &b : r)
            b = bus.read();
        LOG_DEBUG("QMC5883L regs 00-0D: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", r[0], r[1], r[2],
                  r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11], r[12], r[13]);
    }
#endif
    return true;
}

void QMC5883LHeading::setRunning(bool run)
{
    if (run == running)
        return;
    if (writeRegister(bus, REG_CONTROL1, run ? MODE_CONTINUOUS : MODE_STANDBY))
        running = run;
}

bool QMC5883LHeading::readRaw(int16_t &x, int16_t &y, int16_t &z)
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

bool QMC5883LHeading::calibrated() const
{
    return haveBounds && (xMax - xMin) >= QMC5883L_MIN_SPAN && (yMax - yMin) >= QMC5883L_MIN_SPAN;
}

void QMC5883LHeading::loadCalibration()
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

void QMC5883LHeading::saveCalibrationIfDue(uint32_t now)
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

void QMC5883LHeading::resetCalibration()
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

// True when the GPS course should be shown instead of the magnetometer heading
bool QMC5883LHeading::gpsCourseUsable(uint32_t now, float &courseDeg)
{
#if HAS_GPS && !MESHTASTIC_EXCLUDE_GPS
    if (!gpsStatus || !gpsStatus->getHasLock())
        return false;
    const uint32_t intervalMs = Default::getConfiguredOrDefaultMs(config.position.gps_update_interval);
    if (intervalMs > GPS_UPDATE_ALWAYS_ON_THRESHOLD_MS)
        return false; // receiver sleeps between fixes; course would be stale most of the time
    const uint32_t lastFix = gpsStatus->getLastFixMillis();
    if (lastFix == 0 || now - lastFix > intervalMs + COMPASS_GPS_AGE_MARGIN_MS)
        return false;
    const uint32_t speedKmh = localPosition.ground_speed; // km/h, as filled in by the GPS driver
    if (speedKmh < (usingGpsCourse ? COMPASS_GPS_EXIT_KMH : COMPASS_GPS_ENTER_KMH))
        return false;
    courseDeg = gpsStatus->getHeading() * 1e-5f; // ground track, degrees clockwise from north
    return true;
#else
    return false;
#endif
}

// For variants that set COMPASS_GPS_COURSE_WITHOUT_MAG (e.g. a vehicle node): show the GPS
// course even though the magnetometer has no usable heading. Once fed, the UI keeps the
// last value while slow or stopped, i.e. the last direction of travel.
void QMC5883LHeading::feedGpsCourseOnly(uint32_t now)
{
#if COMPASS_GPS_COURSE_WITHOUT_MAG && HAS_SCREEN
    float course;
    const bool useGps = gpsCourseUsable(now, course);
    if (useGps != usingGpsCourse) {
        usingGpsCourse = useGps;
        LOG_INFO("Compass heading source: %s", useGps ? "GPS course" : "none (holding last GPS course)");
    }
    if (useGps && screen)
        screen->setHeading(course);
#endif
}

int32_t QMC5883LHeading::runOnce()
{
#if HAS_SCREEN
    // Nothing shows a heading while the screen is off, so let the chip sleep too
    if (screen && !screen->isScreenOn()) {
        setRunning(false);
        return POLL_IDLE_MS;
    }
#endif
    const uint32_t now = millis();
    if (!present) {
        // e.g. the chip shares a switched supply that was off at boot
        present = begin();
        if (!present) {
            feedGpsCourseOnly(now);
            return RETRY_MS;
        }
        LOG_INFO("QMC5883L compass now responding");
    }
    setRunning(true);

    int16_t x, y, z;
    if (!readRaw(x, y, z))
        return POLL_MS;

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
    if (now - lastLog > 250) {
        lastLog = now;
        LOG_DEBUG("QMC5883L raw x=%d y=%d z=%d bounds x %d..%d y %d..%d", x, y, z, xMin, xMax, yMin, yMax);
    }
#endif
    if (!calibrated()) {
        // No magnetometer heading yet. Unless the variant opts in to GPS course on its own,
        // stay silent so the UI keeps using its GPS position-based estimate.
        feedGpsCourseOnly(now);
        return POLL_MS;
    }

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

    // Moving fast with fresh fixes: show the direction of travel instead
    float course;
    const bool useGps = gpsCourseUsable(now, course);
    if (useGps != usingGpsCourse) {
        usingGpsCourse = useGps;
        LOG_INFO("Compass heading source: %s", useGps ? "GPS course" : "magnetometer");
    }
    if (useGps)
        heading = course;

#if HAS_SCREEN
    if (screen)
        screen->setHeading(heading); // wraps to 0..360 and smooths
#endif
    saveCalibrationIfDue(now);
    return POLL_MS;
}
