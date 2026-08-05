#include <Wire.h>

// I2C scanner for Arduino IDE
// - Apri il Monitor Seriale a 115200 baud
// - Collega SDA/SCL al bus I2C del dispositivo che vuoi testare
// - Carica lo sketch e osserva gli indirizzi trovati

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("I2C Scanner - Arduino IDE");
  Wire.begin();
  Serial.println("Scanning for I2C devices...");
}

void loop() {
  byte error, address;
  int count = 0;

  Serial.println("\nScanning...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Found I2C device at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");
      count++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (count == 0) {
    Serial.println("No I2C devices found.");
  } else {
    Serial.print("Done. ");
    Serial.print(count);
    Serial.println(" device(s) found.");
  }

  delay(5000);
}
