// Heltec Wireless Stick V3 (HTIT-WS_V3)
// Pin facts checked against HTIT-WS_V3_Schematic_Diagram.pdf.

#define LED_POWER LED // GPIO 35

// ---- Display: 0.49" SSD1306, 64x32, I2C 0x3C ----
#define USE_SSD1306
#define OLED_TINY                                // compact UI layouts for very small panels
#define OLED_GEOMETRY_OVERRIDE GEOMETRY_64_32
#define RESET_OLED RST_OLED // GPIO 21
#define I2C_SDA SDA_OLED    // GPIO 17
#define I2C_SCL SCL_OLED    // GPIO 18

// ---- Secondary I2C bus for external peripherals ----
// Same assignment as the 2.2.23 variant. Note the old rocket tracker firmware used the
// opposite order (SDA 47, SCL 48) for its compass; swap these if reusing that wiring.
#define I2C_SDA1 48
#define I2C_SCL1 47

#define VEXT_ENABLE Vext // GPIO 36, active low; switches the Vext rail that also feeds the OLED
#define BUTTON_PIN 0

// ---- Battery: GPIO 1 through a 390K/100K divider gated by GPIO 37 ----
#define ADC_CTRL 37
#define ADC_CTRL_ENABLED LOW
#define BATTERY_PIN 1
#define ADC_CHANNEL ADC1_GPIO1_CHANNEL
#define ADC_ATTENUATION ADC_ATTEN_DB_2_5 // lower dB for high resistance voltage divider
#define ADC_MULTIPLIER 4.9 * 1.045

// ---- LoRa: SX1262 ----
#define USE_SX1262

#define LORA_DIO0 -1 // a No connect on the SX1262 module
#define LORA_RESET 12
#define LORA_DIO1 14 // SX1262 IRQ
#define LORA_DIO2 13 // SX1262 BUSY
#define LORA_DIO3    // Not connected on PCB

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
