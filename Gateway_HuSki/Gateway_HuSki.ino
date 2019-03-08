/* https://robotzero.one/heltec-wifi-lora-32/
   Sketch uses 146842 bytes (14%) of program storage space. Maximum is 1044464 bytes.
   Global variables use 11628 bytes (3%) of dynamic memory, leaving 283284 bytes for local variables. Maximum is 294912 bytes.
*/
// code de la gateway

#include <U8x8lib.h>
#include <LoRa.h>
#include "mbedtls/aes.h"
#include "BluetoothSerial.h" //Header File for Serial Bluetooth, will be added by default into Arduino

// WIFI_LoRa_32 ports
// GPIO5  -- SX1278's SCK
// GPIO19 -- SX1278's MISO
// GPIO27 -- SX1278's MOSI
// GPIO18 -- SX1278's CS
// GPIO14 -- SX1278's RESET
// GPIO26 -- SX1278's IRQ(Interrupt Request)

#define SS      18
#define RST     14
#define DI0     26
#define BAND    433E6

// the OLED used
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);
BluetoothSerial ESP_BT; //Object for Bluetooth

String receivedBluetooth;
String receivedLora;

union {
  char toChar[7];
  byte toByte[7];
} arrayByteBT;

int packetSize;
char chipKeyTable[15][12];
char chipKeyActive[12];
String chipIDdecoded;
unsigned long lastReceivedPacket[15];
bool activeMode;
bool ack = true;
int nbCarte = 0;
char chipKey[13];
int chipKeyInt;
char receivedBTArray[14];

int lengthMess;
unsigned char decryptedText[16];
String decryptedTextChar;
unsigned char cryptedText[16];

int receivedRssiInt;
String receivedRssi;
int batteryLevel;
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

void decodeMess() {
  // decodedMess -> 3 float et 1 int
  for (int i = 0 ; i < 4 ; i++) {
    latUnion.byteLat[i] = (byte)cryptedText[i];
    lonUnion.byteLon[i] = (byte)cryptedText[i + 4];
    altUnion.byteAlt[i] = (byte)cryptedText[i + 8];
  }
  batteryLevel = (int) cryptedText[12];
  Serial.printf("%f %f %f %d", latUnion.floatLat, lonUnion.floatLon, altUnion.floatAlt, batteryLevel);
  decryptedTextChar = String(latUnion.floatLat, 6) + " " + String(lonUnion.floatLon, 6) + " " + String(altUnion.floatAlt, 2) + " " + String(batteryLevel);
}

int inTable() {
  bool equal;
  for (int i = 0 ; i < nbCarte ; i++) {
    equal = true;
    for (int j = 0 ; j < 12 ; j++) {
      if (chipKey[j] != chipKeyTable[i][j]) {
        equal = false;
      }
    }
    if (equal) {
      return i;
    }
  }
  return -1;
}

void sendLora(char *str) {
  Serial.printf("Envoie du paquet LoRa : %s\n", str);
  LoRa.beginPacket() ;
  LoRa.print(str) ;
  LoRa.endPacket();
}

bool isChipActive() {
  activeMode = true;
  for (int i = 0; i < 12; i++) {
    if (chipKey[i] != chipKeyActive[i]) {
      activeMode = false;
    }
  }
  return (activeMode);
}

String receive_gps() {
  lengthMess = 16;
  receivedRssiInt = (int) LoRa.packetRssi();
  receivedRssi = String(-receivedRssiInt);

  // received a packet
  Serial.println("\n\nReceived packet ");
  u8x8.drawString(0, 4, "PacketID");

  // read packet
  receivedLora = "";
  while (LoRa.available()) {
    receivedLora += (char)LoRa.read();
  }
  Serial.println(receivedLora);


  String tmpChip;
  Serial.println("\n chipKey recupered : ");
  for (int i = 0 ; i < 6 ; i++) {
    tmpChip += String(receivedLora[i], HEX);
  }
  tmpChip.toUpperCase();
  tmpChip.toCharArray(chipKey, 13);
  Serial.println(chipKey);
  chipKeyInt = inTable();
  Serial.printf("chipkeyInt : %d", chipKeyInt);
  activeMode = isChipActive();

  if (chipKeyInt 
  == -1) {
    Serial.print("\nThis key is not in the table : ");
    Serial.println(chipKey);
    return "";
  }
  
  if (activeMode) {
    Serial.println("Paquet reçu de la carte en mode active");
    if (millis() - lastReceivedPacket[chipKeyInt] > 5000) {
      Serial.println("La carte n est pas en mode active");
      (String(chipKey) + String(1)).toCharArray(receivedBTArray, 14);
      delay(100);
      sendLora(arrayByteBT.toChar);
    }
    lastReceivedPacket[chipKeyInt] = millis();
  } else {
    Serial.println("Paquet reçu d une carte en mode normale");
    if (millis() - lastReceivedPacket[chipKeyInt] < 5000) {
      Serial.println("La carte n est pas en mode normale");
      (String(chipKey) + String(0)).toCharArray(receivedBTArray, 14);
      delay(100);
      sendLora(arrayByteBT.toChar);
    }
    lastReceivedPacket[chipKeyInt] = millis();
  }



  Serial.println("\n cryptedText : ");
  for (int i = 0; i < lengthMess; i++) {
    cryptedText[i] = (unsigned char) receivedLora[i + 6];
  }

  /*
    decrypt((unsigned char *) cryptedText, "abcdefghijklmnop", decryptedText);
    Serial.println("\nDeciphered text: ");
  */

  // convertir les 9 octets en 2 float de 4 octet et 1 int
  decodeMess();

  // print RSSI of packet
  Serial.print("' with RSSI ");
  Serial.println(receivedRssi);
  u8x8.drawString(0, 5, "PacketRS");
  char currentrs[64];
  receivedRssi.toCharArray(currentrs, 64);
  u8x8.drawString(9, 5, currentrs);

  return (String(chipKey) + " " + decryptedTextChar + " " + String(3) + " " + String(receivedRssi));
}

void receive_bluetooth() {
  receivedBluetooth = "";
  while (ESP_BT.available()) {
    receivedBluetooth += (char) ESP_BT.read();
  }
  if (receivedBluetooth != "") {
    receivedBluetooth.toCharArray(receivedBTArray, 14);
    for (int i = 0 ; i < 12 ; i++) {
      chipKey[i] = receivedBTArray[i];
    }
    for (int i = 0 ; i < 6 ; i++) {
      String tmp = String(receivedBTArray[2 * i]) + String(receivedBTArray[2 * i + 1]);
      arrayByteBT.toByte[i] = (byte) 
      strtol( tmp.c_str(), NULL, 16);
    }
    arrayByteBT.toByte[6] = (byte) receivedBTArray[12];
    switch (receivedBTArray[12]) {
      case '1':
        for (int i = 0 ; i < 12 ; i++) {
          chipKeyActive[i] = receivedBTArray[i];
        }
        chipKeyInt = inTable();
        lastReceivedPacket[chipKeyInt] = millis();
        sendLora(arrayByteBT.toChar);
        break;
      case '0':
        if (isChipActive) {
          for (int i = 0 ; i < 12 ; i++) {
            chipKeyActive[i] = (char) 0;
          }
        }
        sendLora(arrayByteBT.toChar);
        break;
      case '3':
        // ajout carte
        Serial.print("\nAjout carte : ");
        for (int i = 0 ; i < 12 ; i++) {
          chipKeyTable[nbCarte][i] = chipKey[i];
        }
        nbCarte++;
        break;

      case '4':
        // suppression carte
        break;

    }
    Serial.print("Received:"); Serial.println(receivedBTArray);
  }
}

void setup() {


  Serial.begin(115200);
  while (!Serial); //if just the the basic function, must connect to a computer
  delay(1000);

  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  LoRa.setPins(SS, RST, DI0);

  ESP_BT.begin("HuSki-1F05"); //Name of your Bluetooth Signal
  Serial.println("Bluetooth Device is Ready to Pair");

  Serial.println("Gateway launched");
  u8x8.drawString(0, 1, "Gateway launched");

  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    u8x8.drawString(0, 1, "Starting LoRa failed!");
    while (1);
  }
}

void loop() {
  if (ESP_BT.hasClient()) {
    Serial.println("Appareil connecté");
    while (ESP_BT.hasClient()) {
      packetSize = LoRa.parsePacket();
      // transite le LoRa à l'application
      if (packetSize) {
        receivedLora = receive_gps();
        Serial.println(receivedLora);
        ESP_BT.print(receivedLora);
        // ESP_BT.flush();
      }
      //transite le BT aux cartes
      if (ESP_BT.hasClient()) {
        receive_bluetooth();
      }
      delay(20);

    }
    ESP_BT.flush();
  }
}
