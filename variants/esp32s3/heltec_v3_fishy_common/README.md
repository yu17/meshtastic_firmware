# Shared drivers for the custom Heltec V3 builds

Sources here are compiled only by the variants that list this folder in their
`build_src_filter` (`heltec-v3-handheld`, `heltec-v3-vehicle`). Each variant decides in its
own `variant.cpp` (`lateInitVariant()`) which drivers to create, and sets pins and tunables
in its `variant.h`. Nothing here modifies upstream sources.

- `AnalogJoystick` — two-axis analog joystick + button as an InputBroker source.
- `QMC5883LHeading` — QMC5883L heading for the on-screen compass, with GPS-course
  switching when moving fast.
