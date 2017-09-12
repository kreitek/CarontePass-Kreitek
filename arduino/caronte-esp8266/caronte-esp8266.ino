#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "MFRC522.h"

/*
 * PINOUT
 * +--------------------------------+-----------------------+
 * | WEMOS D1 ESP8266 BOARD         |    CONECT TO PIN      |
 * +--------------------------------+---------------+-------+
 * | PIN  | FUCTION  | ESP-8266 PIN | RC522 | RELAY | BUZZER|
 * +------+----------+--------------+-------+-------+-------+
 * | 3.3V | POWER    | 3.3V         | 3.3V  |       |       |
 * +------+----------+--------------+-------+-------+-------+
 * | 5V   | POWER    | 5V           |       | VCC   |       |
 * +------+----------+--------------+-------+-------+-------+
 * | GND  | GND      | GND          | GND   | GND   |       |
 * +------+----------+--------------+-------+-------+-------+
 * | D13  | SCK      | GPIO-14      | SCK   |       |       |
 * +------+----------+--------------+-------+       +-------+
 * | D12  | MISO     | GPIO-12      | MISO  |       |       |
 * +------+----------+--------------+-------+       +-------+
 * | D11  | MOSI     | GPIO-13      | MOSI  |       |       |
 * +------+----------+--------------+-------+       +-------+
 * | D10  | SS (SDA) | GPIO-15      | SDA   |       |       |
 * +------+----------+--------------+-------+       +-------+
 * | D8   | IO       | GPIO-0       | RESET |       |       |
 * +------+----------+--------------+-------+-------+-------+
 * | D0   | IO       | GPIO-16      |       | IN1   |       |
 * +------+----------+--------------+-------+-------+-------+
 * | D1   | PWM      | GPIO-5       |       |       | Red   |
 * +------+----------+--------------+-------+-------+-------+
 * | D2   | GROUND   | GPIO-4       |       |       | Black |
 * +------+----------+--------------+-------+-------+-------+
 */

#define RST_PIN   0 // RST-PIN for RC522 - RFID - SPI - Module GPIO-0
#define SS_PIN    15  // SDA-PIN for RC522 - RFID - SPI - Module GPIO-15
#define RELAY_PIN 16 // RELAY-PIN in GPI0-16
#define BUZZER    5 // D1
#define BUZZER_GND  4 // D2, just to ground all the time

const char* ssid     = "your-ssid";
const char* password = "your-password";

const char* host = "server-ip";
const int httpPort = 80;

/*
 * For User Auhentication:
 * https://en.wikipedia.org/wiki/Basic_access_authentication
 * Use user:password in base64
 * Ej: https://webnet77.net/cgi-bin/helpers/base-64.pl
*/

const char* userpass = "xxxx";

int tag[4];
boolean lasttagaccepted;
unsigned long lasttime;

WiFiClient client;
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance

void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? "" : ".");
    Serial.print(buffer[i], DEC);
    tag[i] = buffer[i];
  }
}

void playbuzzer(){
  analogWrite(BUZZER, 180);
  delay(750);
  analogWrite(BUZZER, 0);
}

void wificonnect(){
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void opendoor(){
  digitalWrite(RELAY_PIN, HIGH); //Relay ON
  Serial.println("Relay Activated");
  delay(1500);
  digitalWrite(RELAY_PIN, LOW); //Relay OFF
}

void setup() {
  // IO configuration
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUZZER_GND, OUTPUT);
  analogWrite(BUZZER, 0);
  digitalWrite(BUZZER_GND, LOW);
  // Initialize serial communications
  Serial.begin(115200);
  delay(10);
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  wificonnect();
}

void loop() {
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  playbuzzer();
  // Show some details of the PICC (that is: the tag/card)
  Serial.println("RFID Tag Detected...");
  Serial.print(F("Card UID:"));
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(host);

  // Use WiFiClient class to create TCP connections
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  // We now create a URI for the request
  String url = "/api/1/device/";
  url += tag[0];
  url += ".";
  url += tag[1];
  url += ".";
  url += tag[2];
  url += ".";
  url += tag[3];

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  client.println(
    String("GET ") + url + " HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" + "Authorization: Basic " + userpass + "\r\n" +
    "Connection: close\r\n\r\n");

  delay(10);

  /*
  // Read all the lines of the reply from server and print them to Serial
  while (client.connected()) {
    String response = client.readStringUntil('\n');
    if (response == "\r") {
      Serial.print("Headers Received: ");
      break;
    }
  }
  String response = client.readStringUntil('\n');
  Serial.println(response);
  */

  while(client.connected()){

    String line = client.readStringUntil('\r');
    //Serial.println(line);
    String result = line.substring(1,2);

    if (result=="[") //detects the beginning of the string json
    {
      Serial.print("Response: ");
      Serial.println(line);

      if (line.indexOf("true") >= 0 )
      {
        Serial.println("Access Granted");
        opendoor();
       }
       else if (line.indexOf("null") >= 0 )
       {
        Serial.println("Access Unidentified");
        // TODO use the buzzer in other tone to notify this
       }
       else{
          Serial.println("False");
        // TODO use the buzzer in other tone to notify this
        }
    }
  }
  delay(3000);
}
