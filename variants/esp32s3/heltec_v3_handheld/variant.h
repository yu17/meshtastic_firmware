#define LED_POWER LED

#define USE_SSD1306 // Heltec_v3 has a SSD1306 display

#define RESET_OLED RST_OLED
#define I2C_SDA SDA_OLED // I2C pins for this board
#define I2C_SCL SCL_OLED

// Enable secondary bus for external periherals
#define I2C_SDA1 33 // handheld: BMP280 (0x76) and QMC5883L (0x0D)
#define I2C_SCL1 34

// VEXT_ENABLE is deliberately not defined: on this handheld the Vext rail (GPIO 36, active
// low) feeds only the GPS, so it is handed to the GPS code as its power switch below.
#define BUTTON_PIN 0

#define ADC_CTRL 37
#define ADC_CTRL_ENABLED LOW
#define BATTERY_PIN 1 // A battery voltage measurement pin, voltage divider connected here to measure battery voltage
#define ADC_CHANNEL ADC1_GPIO1_CHANNEL
#define ADC_ATTENUATION ADC_ATTEN_DB_2_5 // lower dB for high resistance voltage divider
#define ADC_MULTIPLIER 4.9 * 1.045

#define USE_SX1262

#define LORA_DIO0 -1 // a No connect on the SX1262 module
#define LORA_RESET 12
#define LORA_DIO1 14 // SX1262 IRQ
#define LORA_DIO2 13 // SX1262 BUSY
#define LORA_DIO3    // Not connected on PCB, but internally on the TTGO SX1262, if DIO3 is high the TXCO is enabled

#define LORA_SCK 9
#define LORA_MISO 11
#define LORA_MOSI 10
#define LORA_CS 8

#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_DIO2
#define SX126X_RESET LORA_RESET

#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8

#define HAS_32768HZ 1

// ---- Handheld additions ----
// External u-blox M10 GPS on UART1 (same pins as the old rocket-ctrl-handheld firmware)
#define GPS_RX_PIN 48
#define GPS_TX_PIN 47

// GPS power switch: the module's 3.3 V rail is fed from Vext and nothing else is on it.
// The firmware cuts it when the GPS is disabled and during long sleeps between fixes.
#define PIN_GPS_EN 36 // Vext
#define GPS_EN_ACTIVE 0

// Analog joystick (driver: AnalogJoystick.cpp in this folder). Same pins and thresholds
// as the old rocket-ctrl-handheld firmware.
#define JOYSTICK_PIN_UD 2  // ADC1_CH1; low reading = up
#define JOYSTICK_PIN_LR 4  // ADC1_CH3; low reading = left
#define JOYSTICK_PIN_BTN 3 // push button to ground
// #define JOYSTICK_DEBUG  // log raw readings twice a second, for calibration

// QMC5883L compass (driver: QMC5883LCompass.cpp in this folder), on the secondary I2C bus.
// #define QMC5883L_HEADING_OFFSET_DEG 90.0f // mounting offset; default matches the old firmware
// #define QMC5883L_HEADING_REVERSED 1       // if the heading turns the wrong way
// #define QMC5883L_DEBUG                    // log raw readings once a second
