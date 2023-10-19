
#include "BoldCommon.h"

#define DEBUG true // -- > enable or disable

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.println(x);
#else
#define DEBUG_PRINT(x)
#endif

//wifi settings voor home assistant en api comm met bold server
const char *ssid = "<REPLACE_ME>";
const char *password = "<REPLACE_ME>";


//voor homeassistant gebruikt.
const char *mqttServer = "<REPLACE_ME>";
const int mqttPort = 1883;
const char *mqttUser = "<REPLACE_ME>";
const char *mqttPassword = "<REPLACE_ME>";

WiFiClient espClient;
PubSubClient client(espClient);

void connectToWifi()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void reconnectClient()
{
  connectToWifi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);

  while (!client.connected())
  {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP8266Client", mqttUser, mqttPassword))
    {
      Serial.println("connected to mqtt");
      client.subscribe("door_unlocking");
    }
    else
    {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void refreshEEPROMData()
{
  Serial.println("Refreshing sesamdata");
  UnlockData tempItem;
  memset(&tempItem, 0x0, sizeof(tempItem));

  if (WiFi.status() != WL_CONNECTED)
  {
    reconnectClient();
  }
  // authenticeer();
  bool result = (((getSesamHandshake(&tempItem) > 0) ? true : false) && ((getSesamCommand(&tempItem) > 0) ? true : false) && (authenticeer(&tempItem) > 0) ? true : false);
  if (result)
  {
    DEBUG_PRINT("Result success, writing to eeprom");
    DEBUG_PRINT(tempItem.handShakeKey);
    DEBUG_PRINT(tempItem.handShakePayload);
    DEBUG_PRINT(tempItem.commandPayload);
    DEBUG_PRINT(tempItem.authToken);
    writeBytesEEPROM((byte *)&tempItem, sizeof(tempItem));
    memcpy(&myLock, &tempItem, sizeof(tempItem));
    Serial.println("written data");
  }
  else
  {
    Serial.println("Refreshing data not happened");
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int lengte)
{

  DEBUG_PRINT("Message arrived in topic: ");
  DEBUG_PRINT(topic);

  DEBUG_PRINT("Message:");

  String content = String((const char *)payload);
  DEBUG_PRINT(content);

  if (content.indexOf("unlock_door") >= 0)
  {
     startUnlockingDoor();
  }
  else if (content.indexOf("nieuwe_key") >= 0) //10 karakter verder dus
  {
    size_t startPos = content.indexOf("nieuwe_key") + 11; //11 vanwege nieuwe_key_
    size_t sizeAuthToken = strlen(authToken.c_str());
    char tempKey[sizeAuthToken + 1] = { 0 };

    memcpy(tempKey, &payload[startPos], sizeAuthToken);
    Serial.println("Nieuwe key instellen, namelijk: ");
    Serial.println(tempKey);

    memcpy(myLock.authToken, tempKey, sizeof(myLock.authToken));
    refreshEEPROMData();

    client.publish("door_unlocking", "Nieuwe key instellen gelukt");


  }
}

void startUnlockingDoor()
{
  Serial.println("Starting door unlocking..");
  doWeHavePayloadData(); // check if data is right.

  notStart = 0;
  doConnect = true;
  connected = false;
  StartTimer();
}

void doWeHavePayloadData()
{

  if (myLock.handShakeKey[0] == 0 || myLock.handShakePayload[0] == 0 || !isAlphaNumeric(myLock.handShakeKey[0]) || !isAlphaNumeric(myLock.handShakePayload[0]) || !isAlphaNumeric(myLock.commandPayload[0]) || !isAlphaNumeric(myLock.authToken[0]))
  { // empty data, refresh
    refreshEEPROMData();
  }
  else
  {
    Serial.println("We have succesfull data");
  }
}

void setup()
{
  setupEEPROM();
  memset(&myLock, 0x0, sizeof(myLock));

  readBytesEEPROM((byte *)&myLock, sizeof(myLock));

  if (useHardcodedAuthToken)
  {
    memcpy(myLock.authToken, authToken.c_str(), sizeof(myLock.authToken));
  }

  Serial.begin(115200);

  DEBUG_PRINT("EEPROM PAYLOAD DATA:");
  DEBUG_PRINT(myLock.handShakeKey);
  DEBUG_PRINT(myLock.handShakePayload);
  DEBUG_PRINT(myLock.commandPayload);
  DEBUG_PRINT(myLock.authToken);

  doWeHavePayloadData();

  BLEDevice::init("");
  BLEDevice::setPower(ESP_PWR_LVL_P9);

  Serial.println("Connected to the WiFi network");

  reconnectClient();

  client.publish("door_unlocking", "Hello from ESP32");

  refreshEEPROMData();
}

void StartTimer()
{
  StartTime = millis();
  Serial.println("Start time is: ");
  Serial.println(StartTime);
}

void loop()
{
  if (!client.connected())
  {

    reconnectClient();
  }

  client.loop();

  if (StartTime && doConnect) // Since 0==false, this is the same as (StartTime != 0)
  {
    // The timer is running.  Check for stuff.
    unsigned long elapsedTime = millis() - StartTime;
    Serial.println("Starttime");
    Serial.println(elapsedTime);

    if (elapsedTime >= (long)10000)
    {
      // The timer has ended!  Do the end thing!
      StartTime = 0;
      if (numberOfRetries < MAX_RETRIES)
      {
        failedUnlocking = true;
        Serial.println("Unlocking failed in time, retrying");
      }
      else
      {
        Serial.println("Unlocking failed in time and MAX retries, stopping now");
      }
    }
  }

  if (failedUnlocking)
  {
    if (numberOfRetries < MAX_RETRIES)
    {
      Serial.println("Failed unlocking, NumberOfRetries: ");
      Serial.println(numberOfRetries);

      refreshEEPROMData();
      startUnlockingDoor();
      failedUnlocking = false;
      numberOfRetries++;
    }
    else
    {
      numberOfRetries = 0;
      doConnect = false;
      Serial.println("Unlocking failed, stopping retrying");
    }
  }

  if (doConnect == true)
  {
    failedUnlocking = false;
    if (connectToServer())
    {
      Serial.println("We are now connected to the BLE Server.");
    }
    else
    {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  if (connected)
  {

   // DEBUG_PRINT("notStart");
   // DEBUG_PRINT(notStart);

    if (notStart == 0)
    {
      notStart = 1;

      sendHandshake();
    }
    else if (notStart == 2)
    {
      DEBUG_PRINT("Sending bluetooth data");
      notStart = 3;

      pWriteCharacteristic->writeValue(buffer, bufferSending, true);
    }
  }
}
