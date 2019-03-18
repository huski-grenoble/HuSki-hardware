#include <TinyGPS++.h>
#include <SPI.h>
#include <LoRa.h>
#include <SSD1306.h>
#include <Arduino.h>

// Choose two Arduino pins to use for software serial
#define RXD2 22
#define TXD2 23

#define SS 18
#define RST 14
#define DI0 26
#define BAND 433E6
#define OLED_BTN 0

HardwareSerial gpsSerial(1);

SSD1306 display(0x3c, 4, 15);

int analogInput = 39;           // Battery voltage pin
float resistance1 = 100000.0;   // First resistance for voltage divider (100 kOhms)
float resistance2 = 100000.0;   // Second resistance for voltage divider (100 kOhms)


byte chipByte[6];
String chipId;                  // ChipID's card

int batteryLevel;
float gpsLat;
float gpsLon;
float gpsAlt;
bool gpsReady;

String receivedTextBinary;
String receivedRssi;
long lastSendTime = 0;          // Last send time
int interval = 10000;           // Interval between sends

char encodedMess[13];

bool displayOn = true;

// Create a TinyGPS++ object
TinyGPSPlus gps;
bool paired;

union {
  float floatLat;
  byte byteLat[4];
} latUnion;

union {
  float floatLon;
  byte byteLon[4];
} lonUnion;

union {
  float floatAlt;
  byte byteAlt[4];
} altUnion;

/*---------------------------------------------------------- Encoding Message ------------------------------------------------------------ */
void encodeMess() {
  // Encode les 2 float et batterie level
  latUnion.floatLat = gpsLat;
  lonUnion.floatLon = gpsLon;
  altUnion.floatAlt = gpsAlt;
  byte batByte = (byte) batteryLevel;
  for (int i = 0 ; i < 4 ; i++) {
    encodedMess[i] = (char) latUnion.byteLat[i];
    encodedMess[i + 4] = (char) lonUnion.byteLon[i];
    encodedMess[i + 8] = (char) altUnion.byteAlt[i];
  }
  encodedMess[12] = (char) batByte;
}

/*--------------------------------------------------------- Send a LoRa Message ---------------------------------------------------------- */
void sendLoRaMessage() {
  Serial.printf("Sending packet: ");

  //Serial.printf("%s %.6f %.6f %d", chipId.c_str(), gpsLat, gpsLon, batteryLevel);
  Serial.printf("%s%s", chipByte, encodedMess);
  display.drawString(0, 50, "Sending " + String(chipId) + String(encodedMess));
  Serial.printf("\n\n");

  // send packet
  LoRa.beginPacket();
  //LoRa.printf("%s %.6f %.6f %d", chipId.c_str(), gpsLat, gpsLon, batteryLevel);
  LoRa.printf("%s%s", chipByte, encodedMess);
  LoRa.endPacket();

  digitalWrite(25, HIGH); // Turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(25, LOW); // Turn the LED off by making the voltage LOW
}

/*-------------------------------------------------------- Receive a LoRa Message -------------------------------------------------------- */
void receiveLoRaMessage() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.printf("Received packet '");
    receivedTextBinary = "";

    while (LoRa.available()) {
      receivedTextBinary += (char)LoRa.read();
    }

    Serial.printf("%s", receivedTextBinary.c_str());

    Serial.printf("' with RSSI ");
    Serial.printf("%d\n\n", LoRa.packetRssi());

    bool messageForMe = true;
    for (int i = 0; i < 6; i++) {

      if (receivedTextBinary[i] != chipByte[i]) {
        messageForMe = false;
        break;
      }
    }
    if (messageForMe) {
      Serial.println("\nRecu un message pour moi\n");
      paired = true;
      switch (receivedTextBinary[6]) {
        case '1':
          Serial.printf("--> Mode ACTIVE <--\n\n");
          interval = 1000;
          break;
        case '0' :
          Serial.printf("--> Mode NORMAL <--\n\n");
          interval = 10000;
          break;
        case '3' :
          Serial.println("--> Appareillage <--\n\n");
          paired = true;
          break;
        case '4' :
          Serial.println("--> Désappareillage <--\n\n");
          paired = false;
          break;
      }
    } else {
      Serial.printf("Incorrect ChipID\n\n");
    }
  }
}

/*------------------------------------------------------- Get ChipID (Mac Address) ------------------------------------------------------- */
void getChipId() {
  chipId = uint64_t_to_String(ESP.getEfuseMac()); //The chip ID is essentially its MAC address(length: 6 bytes).
}

/*---------------------------------------------------------- Get Battery Voltage --------------------------------------------------------- */
void getBatteryVoltage() {
  float voltageInput = analogRead(analogInput) / 1024.0; // Get the voltage
  voltageInput = voltageInput * 0.9; // Apply a 90% ratio to get more precision
  float voltageBatterry = (voltageInput * (resistance1 + resistance2)) / resistance2; // Calculate the real tension of the battery
  if (voltageInput  < 0.09) {
    voltageBatterry = 0;
  }
  /**/
  Serial.printf("AnalogInput : ");
  Serial.printf("%.2f", voltageInput);
  Serial.printf(" V\n");
  Serial.printf("Battery : ");
  Serial.printf("%.2f", voltageBatterry);
  Serial.printf(" V\n");
  /**/
  float percentage = (voltageBatterry * 100.0) / 4.0;
  Serial.printf("Percentage : ");
  Serial.printf("%.2f", percentage);
  Serial.printf(" %%\n");
  if (percentage < 20.0) {
    batteryLevel = 0;
  } else if (percentage >= 20.0 && percentage < 40.0) {
    batteryLevel = 1;
  } else if (percentage >= 40.0 && percentage < 60.0) {
    batteryLevel = 2;
  } else if (percentage >= 60.0 && percentage < 80.0) {
    batteryLevel = 3;
  } else if (percentage >= 80.0) {
    batteryLevel = 4;
  }
  Serial.printf("Level : %d /4\n", batteryLevel);
  display.drawString(0, 40, "Level : " + String(batteryLevel) + "/4 (" + percentage + "%)");
}

/*------------------------------------------------------------- Get GPS Info ------------------------------------------------------------ */
void gpsInfo()
{
  if (gps.location.isValid()) {
    gpsReady = true;
    Serial.printf("Latitude : ");
    gpsLat = gps.location.lat();
    Serial.printf("%.6f\n", gpsLat);
    Serial.printf("Longitude: ");
    gpsLon = gps.location.lng();
    Serial.printf("%.6f\n", gpsLon);
    Serial.printf("Altitude: ");
    gpsAlt = gps.altitude.meters();
    Serial.printf("%.2f\n", gpsAlt);

    display.drawString(0, 10, "GPS : " + String(gpsLat, 6) + "; " + String(gpsLon, 6) + "; " + String(gpsAlt, 2));
  } else {
    gpsReady = true;
    gpsLat = 45.181375;
    gpsLon = 5.743377;
    gpsAlt = 200.12;
    Serial.printf("Location : Not Available\n");
    display.drawString(0, 10, "GPS : Not Available");
  }
  Serial.printf("Date : ");
  if (gps.date.isValid()) {
    Serial.printf("%d", gps.date.month());
    Serial.printf("/");
    Serial.printf("%d", gps.date.day());
    Serial.printf("/");
    Serial.printf("%d\n", gps.date.year());
  } else {
    Serial.printf("Not Available\n");
  }

  Serial.printf("Time : ");
  if (gps.time.isValid()) {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.printf("%d", gps.time.hour());
    Serial.printf(":");
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.printf("%d", gps.time.minute());
    Serial.printf(":");
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.printf("%d", gps.time.second());
    Serial.printf(".");
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.printf("%d\n", gps.time.centisecond());
  } else {
    Serial.printf("Not Available\n");
  }
}

String uint64_t_to_String(uint64_t number) {
  unsigned long long1 = (unsigned long)((number >> 32 ));
  unsigned long long2 = (unsigned long)((number));

  chipByte[0] = (byte) (long1 >> 8);
  chipByte[1] = (byte) (long1);
  for (int i = 0 ; i < 4 ; i++) {
    chipByte[5 - i] = (byte) (long2 >> 8 * i);
  }
  String hex = String(long1, HEX) + String(long2, HEX); // six octets
  hex.toUpperCase();
  return hex;
}


void setup() {
  Serial.begin(115200);
  pinMode(analogInput, INPUT);

  pinMode(25, OUTPUT); // Send success, LED will bright 1 second
  pinMode(16, OUTPUT);

  digitalWrite(16, LOW); // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH);

  //while (!Serial); // If just the the basic function, must connect to a computer

  gpsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);

  display.init();
  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(45, 10, "HuSki");
  display.drawString(30, 25, "is loading...");
  display.display();
  display.setFont(ArialMT_Plain_10);

  SPI.begin(5, 19, 27, SS);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.printf("Starting LoRa failed!\n");
    while (1);
  }
  Serial.printf("LoRa Initial OK!\n");

  getChipId();
  paired = false;
  delay(2000);
}

void loop() {
  if (paired) {

    /* Switch Off/On the OLED Display */
    if (digitalRead(OLED_BTN) && displayOn) {
      display.displayOff();
      displayOn = false;
    } else if (!digitalRead(OLED_BTN) && !displayOn) {
      display.displayOn();
      displayOn = true;
    }

    if (millis() - lastSendTime > interval) {
      display.clear();

      while (gpsSerial.available()) {
        if (gps.encode(gpsSerial.read())) {
          gpsInfo();
          break;
        }
      }

      /*if (millis() > 5000 && gps.charsProcessed() < 10) {
        Serial.printf("No GPS detected\n");
        while (true);
        }*/

      Serial.printf("Time %d\n", millis());

      display.drawString(0, 20, "Time : " + String(millis()));

      Serial.printf("ChipID : %s\n", chipId.c_str());

      display.drawString(0, 30, "ChipID : " + String(chipId));

      getBatteryVoltage();

      // Recuperation en char * de la concat de la valeur
      encodeMess();

      if (gpsReady) {
        sendLoRaMessage();
      } else {
        Serial.printf("GPS not ready...\n\n");
      }

      display.display();

      lastSendTime = millis();
    }
  }
  receiveLoRaMessage();
}
