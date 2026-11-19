// Board-specific setup for the vehicle node. lateInitVariant() is the upstream hook for
// this (see src/platform/extra_variants/README.md).

#include "QMC5883LHeading.h"

void lateInitVariant()
{
    // The compass inside the GPS module sits on the secondary I2C bus (GPIO 33/34)
    if (!qmc5883lHeading)
        qmc5883lHeading = new QMC5883LHeading(Wire1);
}
