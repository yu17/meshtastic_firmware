#pragma once

// QMC5883L magnetometer as a heading source for the on-screen compass.
// Upstream detects this chip but never reads it. This driver lives in the variant folder,
// is created from lateInitVariant(), and feeds headings through Screen::setHeading(), the
// same call the upstream magnetometer drivers use. No upstream source file changes.
// When moving faster than a threshold with fresh GPS fixes, it passes on the GPS course
// instead (upstream has no such switching: with a magnetometer it never uses GPS).

#include "configuration.h"

#include "concurrency/OSThread.h"
#include <Wire.h>

class QMC5883LCompass : public concurrency::OSThread
{
  public:
    explicit QMC5883LCompass(TwoWire &bus);

    // Forget the stored calibration and start learning it again
    void resetCalibration();

  protected:
    int32_t runOnce() override;

  private:
    bool begin();
    void setRunning(bool run);
    bool readRaw(int16_t &x, int16_t &y, int16_t &z);
    void loadCalibration();
    void saveCalibrationIfDue(uint32_t now);
    bool calibrated() const;
    bool gpsCourseUsable(uint32_t now, float &courseDeg);

    TwoWire &bus;
    bool present = false;
    bool running = false;
    bool usingGpsCourse = false; // heading currently comes from GPS course, not the magnetometer

    // Hard-iron calibration: running min/max of the two horizontal axes
    int16_t xMin = 0, xMax = 0, yMin = 0, yMax = 0;
    bool haveBounds = false;
    bool boundsDirty = false;
    uint32_t boundsChangedMs = 0;
};

extern QMC5883LCompass *qmc5883lCompass;
