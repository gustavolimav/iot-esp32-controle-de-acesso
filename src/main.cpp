/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-rfid-nfc
 */

#include <SPI.h>
#include <MFRC522.h>
#include "WiFi.h"
#include <Keypad.h>
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#define ROW_NUM 4          // four rows
#define COLUMN_NUM 3       // four columns
#define DOOR_DELAY 3000    // 3k miliseconds to close the door
#define SS_PIN 5           // ESP32 pin GPIO5
#define RST_PIN 22         // ESP32 pin GPIO22
#define RELAY_PIN 15       // pin state door yellow
#define RELAY_NEGADO 4     // pin rejected red
#define RELAY_PERMITIDO 21 // pin authorized green
#define BUTTON_PIN 34      // pin button authorized open door
#define MAGNET_PIN 32      // pin input magnet door is open

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_PINPAD "esp32/pinPad"
#define AWS_IOT_PUBLISH_TOPIC "esp32/pub_dgp"
#define AWS_IOT_PUBLISH_DOOR "esp32/door"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/open_door"
#define AWS_DOOR_SUBSCRIBE_TOPIC "esp32/test_access"

// AWS IoT certificate
WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

unsigned long closeDoorMillis = 0;

// Elasticsearch
const char *serverName = "http://172.26.119.197:9202/iot/_doc/";
unsigned long previousPingMillis = 0; // Stores the last time data was sent to Elasticsearch
const long pingInterval = 60000;

// Button

int oldButton = 0;
int currentButtonPin = 0;

// Magnet

int oldSensor = 1;
int currentSensorPin = 0;

// Keypad

const String password = "7890"; // change your password here
String input_password;

char keys[ROW_NUM][COLUMN_NUM] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};

byte pin_rows[ROW_NUM] = {27, 12, 14, 13};  //  connect to the row pins
byte pin_column[COLUMN_NUM] = {33, 25, 26}; // connect to the column pins
// byte keyTagUID[4] = {0x0D, 0x95, 0xBE, 0x00}; //code to open dor

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

// Rfid

MFRC522 rfid(SS_PIN, RST_PIN);

// Code

void OpenDoor()
{
  Serial.println("DOOR IS OPENING!");
  digitalWrite(RELAY_PERMITIDO, HIGH);
}

void CloseDoor()
{
  Serial.println("DOOR IS CLOSING!");
  digitalWrite(RELAY_PERMITIDO, LOW);
  closeDoorMillis = 0;
}

void AlarmBuzz()
{
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(RELAY_NEGADO, HIGH);
    delay(200);
    digitalWrite(RELAY_NEGADO, LOW);
    delay(200);
  }
}

void messageHandler(String &topic, String &payload)
{
  Serial.println("Incoming: " + topic + " - " + payload);

  if (payload[19] == '1')
  {
    OpenDoor();
    unsigned long currentMillis = millis();
    closeDoorMillis = currentMillis + DOOR_DELAY;
  }
  else
  {
    AlarmBuzz();
  }
}

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  Serial.print("Connecting to AWS IOT - ");

  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }

  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  client.subscribe(AWS_DOOR_SUBSCRIBE_TOPIC);

  Serial.println(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
}

void publishMessagePINPAD(String &letter)
{
  StaticJsonDocument<200> doc;

  doc["sensor_a0"] = letter; // analogRead(0);
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_PINPAD, jsonBuffer);
}

void publishMessage(String &letter)
{
  StaticJsonDocument<200> doc;

  doc["sensor_a0"] = letter; // analogRead(0);
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void publishMessageDoor(int state)
{
  String letter = String(state);
  StaticJsonDocument<200> doc;
  String milisString;
  milisString = String(millis());
  doc["time"] = milisString;
  doc["sensor_porta"] = letter; // analogRead(0);
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_DOOR, jsonBuffer);
}

void read_sensor()
{
  currentSensorPin = !digitalRead(MAGNET_PIN);

  if (currentSensorPin != oldSensor)
  {
    if (currentSensorPin == 1)
    {
      Serial.println("Door is Closed");
      digitalWrite(RELAY_PIN, LOW);
    }
    else
    {
      Serial.println("Door is Open");
      digitalWrite(RELAY_PIN, HIGH);
    }

    Serial.println("-----------------");

    publishMessageDoor(currentSensorPin);
    oldSensor = currentSensorPin;
  }
}

void read_button()
{
  currentButtonPin = !digitalRead(BUTTON_PIN);

  if (currentButtonPin != oldButton)
  {
    if (currentButtonPin == 1)
    {
      Serial.println("Button Released");
      digitalWrite(RELAY_PERMITIDO, LOW);
    }
    else
    {
      Serial.println("Button Pressed");
      digitalWrite(RELAY_PERMITIDO, HIGH);
    }

    Serial.println("-----------------");

    publishMessageDoor(currentButtonPin);
    oldButton = currentButtonPin;
  }
}

void read_keypad()
{
  unsigned long currentMillis = millis();
  static unsigned long lastkeypadMillis = 0;

  if ((currentMillis >= closeDoorMillis) and (closeDoorMillis != 0))
  {
    CloseDoor();
  }

  if (currentMillis >= lastkeypadMillis + 50)
  {
    char key = keypad.getKey();
    if (key)
    {
      Serial.println(key);
      if (key == '*')
      {
        input_password = ""; // clear input password
      }
      else if (key == '#')
      {
        if (password == input_password)
        {
          Serial.print("Input password is: ");
          Serial.println(input_password);
          publishMessagePINPAD(input_password);
          Serial.println("The password is correct, ACCESS GRANTED!");
          OpenDoor();
          closeDoorMillis = millis() + DOOR_DELAY;
        }
        else
        {
          Serial.print("Input password is: ");
          Serial.println(input_password);
          publishMessagePINPAD(input_password);
          Serial.println("\nThe password is incorrect, ACCESS DENIED!");
          for (int i = 0; i < 5; i++)
          {
            digitalWrite(RELAY_NEGADO, HIGH);
            delay(200);
            digitalWrite(RELAY_NEGADO, LOW);
            delay(200);
          }
        }

        input_password = ""; // clear input password
      }
      else
      {
        input_password += key; // append new character to input password string
      }
    }
    lastkeypadMillis = currentMillis;
  }
}

void read_rfid()
{
  if (rfid.PICC_IsNewCardPresent())
  { // new tag is available
    if (rfid.PICC_ReadCardSerial())
    { // NUID has been readed
      Serial.print("RFID/NFC Tag Type: ");
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      Serial.println(rfid.PICC_GetTypeName(piccType));

      Serial.print("UID: ");
      String uidString = "";

      for (int i = 0; i < rfid.uid.size; i++)
      {
        uidString += (rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        uidString += String(rfid.uid.uidByte[i], HEX);
      }

      Serial.println(uidString);
      publishMessage(uidString);
      Serial.println();

      rfid.PICC_HaltA();      // halt PICC
      rfid.PCD_StopCrypto1(); // stop encryption on PCD
    }
  }
}

void monitor_elasticsearch()
{
  unsigned long currentPingMillis = millis();

  if (currentPingMillis - previousPingMillis >= pingInterval)
  {
    previousPingMillis = currentPingMillis;
    HTTPClient http;

    Serial.println("Connecting to Elasticsearch...");
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String timestamp = String(currentPingMillis / 1000);

    // Construir JSON com dados de telemetria
    // Utilizar Métricas definidas e mocalas
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["device"] = "ESP32";
    jsonDoc["status"] = "active";
    jsonDoc["rfid"] = "12345";
    jsonDoc["access"] = "granted";
    jsonDoc["timestamp"] = timestamp;

    String jsonData;
    serializeJson(jsonDoc, jsonData);

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0)
    {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    }
    else
    {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
}

void setup()
{
  Serial.begin(115200);
  input_password.reserve(32);
  SPI.begin();     // init SPI bus
  rfid.PCD_Init(); // init MFRC522
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(MAGNET_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY_NEGADO, OUTPUT);
  pinMode(RELAY_PERMITIDO, OUTPUT);
  Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");
  connectAWS();
}

void loop()
{
  client.loop();

  client.onMessage(messageHandler);
  monitor_elasticsearch();

  read_button();

  read_sensor();

  read_keypad();

  read_rfid();
}