#include <Wire.h>
#include <Adafruit_PN532.h>

#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_PN532 nfc(-1, -1);

void setup() {
  Serial.begin(115200);
  delay(1000);

  //Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("Starting PN532...");

  nfc.begin();
  nfc.SAMConfig();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not found");
    while (1);
  }

  Serial.println("Ready to scan tags...");
}
void loop() {
  uint8_t uid[7];
  uint8_t uidLength;

  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {

    Serial.println("\nTag detected:");
    nfc.PrintHex(uid, uidLength);

    uint8_t data[4];

    Serial.println("Reading NDEF pages...");

    for (uint8_t page = 4; page < 20; page++) {
      if (nfc.ntag2xx_ReadPage(page, data)) {
        Serial.print("Page ");
        Serial.print(page);
        Serial.print(": ");
        nfc.PrintHexChar(data, 4);
        Serial.println();
      }
    }

    delay(1500);
  }
}