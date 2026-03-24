// WRONG (what I wrote):

#define OTA_BOOT_PIN   8   // ❌ This is not the BOOT pin on C3

#define OTA_RESET_PIN  10

// CORRECT:

#define OTA_BOOT_PIN   9   // ✅ GPIO9 = BOOT pin on ESP32-C3

#define OTA_RESET_PIN  10  // ✅ EN = reset



Physical wireFromToNormal useDuring OTAWire 1C3 GPIO2S3 GPIO14CSV RX @ 9600Bootloader RX @ 115200Wire 2C3 GPIO3S3 GPIO44$CFG TX @ 9600Bootloader TX @ 115200Wire 3C3 GPIO8S3 GPIO0HIGH (idle)LOW = enter download modeWire 4C3 GPIO10S3 ENHIGH (idle)Pulse LOW = resetWire 5C3 GPIO20SIM800L TXDUART0 RXUntouchedWire 6C3 GPIO21SIM800L RXDUART0 TXUntouched
