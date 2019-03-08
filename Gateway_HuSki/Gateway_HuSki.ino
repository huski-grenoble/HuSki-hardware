/* 
HuSki - HuConnect

Author : FOMBARON Quentin - FERREIRA Joffrey

*/

#include <U8x8lib.h>
#include <LoRa.h>
#include "mbedtls/aes.h"
#include "BluetoothSerial.h" //Header File for Serial Bluetooth, will be added by default into Arduino
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
  Serial.printf("\nValeur recup : %f %f %f %d", latUnion.floatLat, lonUnion.floatLon, altUnion.floatAlt, batteryLevel);
  decryptedTextChar = String(latUnion.floatLat, 6) + " " + String(lonUnion.floatLon, 6) + " " + String(altUnion.floatAlt, 2) + " " + String(batteryLevel);
}

// retourne la place dans la table de la variable chipKey ou -1 sinon
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

// Envoie de packet LoRa
void sendLora(char *str) {
  Serial.printf("\nEnvoie du paquet LoRa : %s", str);
  LoRa.beginPacket() ;
  LoRa.print(str) ;
  LoRa.endPacket();
}

// Retourne true si la variable chipKey est bien la carte en mode active, false sinon
bool isChipActive() {
  activeMode = true;
  for (int i = 0; i < 12; i++) {
    if (chipKey[i] != chipKeyActive[i]) {
      activeMode = false;
    }
  }
  return (activeMode);
}

//Retourne le messsage recu en Lora dechiffre ou null si la chipID recu n est pas dans la table
String receive_gps() {
  lengthMess = 16;
  receivedRssiInt = (int) LoRa.packetRssi();
  receivedRssi = String(-receivedRssiInt);

  Serial.println("\nReceived packet ");
  u8x8.drawString(0, 4, "PacketID");

  // read packet
  receivedLora = "";
  while (LoRa.available()) {
    receivedLora += (char)LoRa.read();
  }
  Serial.println(receivedLora);


  String tmpChip;
  
  for (int i = 0 ; i < 6 ; i++) {
    tmpChip += String(receivedLora[i], HEX);
  }
  tmpChip.toUpperCase();
  tmpChip.toCharArray(chipKey, 13);
  
  chipKeyInt = inTable();
  activeMode = isChipActive();
  Serial.println("\nChipKey recupered : ");
  Serial.print(chipKey);
  Serial.printf("\nPlace dans la table: %d", chipKeyInt);
  
  if (chipKeyInt == -1) {
    Serial.print("\nCarte absente de la table : ");
    Serial.println(chipKey);
    return "";
  }

  // verifie et corrige si la carte est bien dans le bon mode
  if (activeMode) {
    //Serial.println("\nPaquet reçu de la carte en mode active");
    if (millis() - lastReceivedPacket[chipKeyInt] > 5000) {
      Serial.println("\nWARNING : La carte n'est pas en mode active comme prevu -> Envoie mode active a la carte");
      (String(chipKey) + String(1)).toCharArray(receivedBTArray, 14);
      delay(100);
      sendLora(arrayByteBT.toChar);
    }
    lastReceivedPacket[chipKeyInt] = millis();
  } else {
    // Serial.println("\nPaquet reçu d une carte en mode normale");
    if (millis() - lastReceivedPacket[chipKeyInt] < 5000) {
      Serial.println("\nWARNING : La carte n est pas en mode normale -> Envoie mode normale a la carte");
      (String(chipKey) + String(0)).toCharArray(receivedBTArray, 14);
      delay(100);
      sendLora(arrayByteBT.toChar);
    }
    lastReceivedPacket[chipKeyInt] = millis();
  }

  for (int i = 0; i < lengthMess; i++) {
    cryptedText[i] = (unsigned char) receivedLora[i + 6];
  }
  
  // Dechiffrage du paquet recu
  decodeMess();

  Serial.print("\nRSSI : ");
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
    
    int place = inTable();
    receivedBluetooth.toCharArray(receivedBTArray, 14);
    Serial.print("Received:"); Serial.println(receivedBTArray);
    
    // Recuperation de la chipID en hexa
    for (int i = 0 ; i < 12 ; i++) {
      chipKey[i] = receivedBTArray[i];
    }

    // Creation du chipID sur 6 byte
    for (int i = 0 ; i < 6 ; i++) {
      String tmp = String(receivedBTArray[2*i]) + String(receivedBTArray[2*i+1]);
      arrayByteBT.toByte[i] = (byte) 
      strtol( tmp.c_str(), NULL, 16);
    }
    arrayByteBT.toByte[6] = (byte) receivedBTArray[12];
    
    // switch sur le dernier octet pour savoir quoi faire
    switch (receivedBTArray[12]) {

      // Mise en mode normal de la carte
      case '0':
        Serial.println("\nMise en mode normale de la carte : ");Serial.println(chipKey);
        if (isChipActive) {
          for (int i = 0 ; i < 12 ; i++) {
            chipKeyActive[i] = (char) 0;
          }
        }
        sendLora(arrayByteBT.toChar);
        break;
        
      // Mise en mode actif de la carte
      case '1':
        Serial.println("\nMise en mode actif de la carte : ");Serial.println(chipKey);
        for (int i = 0 ; i < 12 ; i++) {
          chipKeyActive[i] = receivedBTArray[i];
        }
        lastReceivedPacket[place] = millis();
        sendLora(arrayByteBT.toChar);
        break;
        
      // Ajout de carte a la table des chip ID
      case '3':
        Serial.println("\nAjout de la carte : ");Serial.println(chipKey);
        if(place!=-1){
          Serial.println("WARNING : Tentative d'ajout d'une carte deja presente dans la table de chip ID");
          break;
        }
        for (int i = 0 ; i < 12 ; i++) {
          chipKeyTable[nbCarte][i] = chipKey[i];
        }
        nbCarte++;
        break;

      // Suppression de carte à la table des chip ID
      case '4':
        Serial.println("\Suppression de la carte : ");Serial.println(chipKey);
        if(place==-1){
          Serial.println("WARNING : Tentative de suppression d'une carte absente dans la table de chip ID");
          break;
        }
        for(int i = place ; i<nbCarte-2 ; i++){
          for(int j = 0 ; j<12 ; j++){
            chipKeyTable[i][j] = chipKeyTable[i+1][j]; 
          }
        }
        nbCarte--;
        break;
    }
  }
}

void setup() {

  Serial.begin(115200);
  while (!Serial);
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
        ESP_BT.print(receivedLora);
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
