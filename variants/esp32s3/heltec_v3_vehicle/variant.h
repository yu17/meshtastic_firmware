#define LED_POWER LED

#define USE_SSD1306 // Heltec_v3 has a SSD1306 display

#define RESET_OLED RST_OLED
#define I2C_SDA SDA_OLED // I2C pins for this board
#define I2C_SCL SCL_OLED

// Enable secondary bus for external periherals
#define I2C_SDA1 SDA
#define I2C_SCL1 SCL

// VEXT_ENABLE is deliberately not defined: on this node the Vext rail (GPIO 36, active low)
// feeds only the GPS module, so it is handed to the GPS code as its power switch below.
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

// ---- Vehicle node additions ----
// MicoAir MG-902 (u-blox M9) on UART1
#define GPS_RX_PIN 48
#define GPS_TX_PIN 47

// GPS power switch: the module is fed from Vext and nothing else is on that rail.
// The firmware cuts it when the GPS is disabled and during long sleeps between fixes.
#define PIN_GPS_EN 36 // Vext
#define GPS_EN_ACTIVE 0

// The MG-902 also contains a QMC5883L compass, wired to GPIO 33 (SDA) / 34 (SCL). It is
// not used: on this module it cannot measure a steady field (tested 2026-11-19), so this
// build has no compass driver and the secondary I2C bus keeps its stock pins.
