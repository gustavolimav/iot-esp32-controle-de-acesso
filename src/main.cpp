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

#define ROW_NUM 4          // four rows
#define COLUMN_NUM 3       // four columns
#define DOOR_DELAY 3000    // 3k miliseconds to close the door
#define SS_PIN 5           // ESP32 pin GPIO5
#define RST_PIN 22         // ESP32 pin GPIO27
#define RELAY_PIN 15       // pin state door yellow
#define RELAY_NEGADO 4     // pin rejected red
#define RELAY_PERMITIDO 21 // pin authorized green
#define BUTTON_PIN 34      // pin button authorized open door
#define MAGNET_PIN 32      // pin input magnet door is open

MFRC522 rfid(SS_PIN, RST_PIN);

const String password = "7890"; // change your password here
String input_password;

#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_PINPAD "esp32/pinPad"
#define AWS_IOT_PUBLISH_TOPIC "esp32/pub_dgp"
#define AWS_IOT_PUBLISH_DOOR "esp32/door"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/open_door"
#define AWS_DOOR_SUBSCRIBE_TOPIC "esp32/test_access"

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

unsigned long previousMillis = 0;
unsigned long openDoorMillis = 0;
unsigned long closeDoorMillis = 0;
unsigned long greenLedMillis = 0;
unsigned long redLedMillis = 0;
bool DoorFlag = false;

void OpenDoor()
{
  Serial.println("DOOR IS OPENING!");
  digitalWrite(RELAY_PERMITIDO, HIGH);
  openDoorMillis = 0;
}

void CloseDoor()
{
  Serial.println("DOOR IS CLOSING");
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

void publishMessageDoor(String &letter)
{
  StaticJsonDocument<200> doc;
  String milisString;
  milisString = String(millis());
  doc["time"] = milisString;
  doc["sensor_porta"] = letter; // analogRead(0);
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_DOOR, jsonBuffer);
}

void setup()
{
  Serial.begin(115200);
  input_password.reserve(32);
  SPI.begin();     // init SPI bus
  rfid.PCD_Init(); // init MFRC522
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MAGNET_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY_NEGADO, OUTPUT);
  pinMode(RELAY_PERMITIDO, OUTPUT);
  Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");
  connectAWS();
}

char keys[ROW_NUM][COLUMN_NUM] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};

byte pin_rows[ROW_NUM] = {27, 12, 14, 13};  //  connect to the row pins
byte pin_column[COLUMN_NUM] = {33, 25, 26}; // connect to the column pins
// byte keyTagUID[4] = {0x0D, 0x95, 0xBE, 0x00}; //code to open dor

int read_sensor(int oldSensor, int currentSensorPin)
{

  String sensorState;

  currentSensorPin = !digitalRead(MAGNET_PIN);

  if (currentSensorPin != oldSensor)
  {
    Serial.print("Door is: ");
    Serial.println(currentSensorPin);

    if (currentSensorPin == 1)
    {
      digitalWrite(RELAY_PIN, LOW);
    }
    else
    {
      digitalWrite(RELAY_PIN, HIGH);
    }

    sensorState = String(currentSensorPin);
    publishMessageDoor(sensorState);
    oldSensor = currentSensorPin;
  }

  return oldSensor;
}

int read_button(int oldButton, int currentButtonPin)
{
  String buttonState;

  currentButtonPin = !digitalRead(BUTTON_PIN);

  if (currentButtonPin != oldButton)
  {
    if (currentButtonPin == 1)
    {
      digitalWrite(RELAY_PERMITIDO, HIGH);
    }
    else
    {
      digitalWrite(RELAY_PERMITIDO, LOW);
    }

    buttonState = String(currentButtonPin);
    publishMessageDoor(buttonState);
    oldButton = currentButtonPin;
  }

  return oldButton;
}

void read_keypad(Keypad keypad, long currentMillis)
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
        closeDoorMillis = currentMillis + DOOR_DELAY;
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

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);
int oldButton = 0;
int currentButtonPin = 0;

int oldSensor = 1;
int currentSensorPin = 0;

void loop()
{

  oldButton = read_button(oldButton, currentButtonPin);

  oldSensor = read_sensor(oldSensor, currentSensorPin);

  unsigned long currentMillis = millis();
  static unsigned long lastkeypadMillis = 0;

  client.loop();

  client.onMessage(messageHandler);

  if ((currentMillis >= closeDoorMillis) and (closeDoorMillis != 0))
  {
    CloseDoor();
  }

  if (currentMillis >= lastkeypadMillis + 500)
  {
    read_keypad(keypad, currentMillis);
    lastkeypadMillis = currentMillis;
  }

  read_rfid();
}