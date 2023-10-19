#include "BLEDevice.h"
//#include "BLEScan.h"
#include "mbedtls/base64.h"


#include <Crypto.h>
#include <AES.h>
#include <CTR.h>
#include <string.h>
#include "WiFi.h"
#include "HTTPClient.h"
#include <ArduinoJson.h>
#include "EEPROM.h"
#include "WiFiClient.h"
#include <PubSubClient.h>

#define DEBUG true -->enable or disable

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#endif

#define MAX_PLAINTEXT_SIZE 60
#define MAX_CIPHERTEXT_SIZE 60
#define KEY_LENGTH 16 //and nonce

#define DECODED_LENGTH_BASE64 70

#define HANDSHAKE_SEND ((byte)-96)
#define HANDSHAKE_RECV ((byte)-95)

#define CRYPTO_SEND ((byte)-94)
#define CRYPTO_RECV ((byte)-93)

#define ACTIVATECOMMAND_SEND ((byte)-92)
#define ACTIVATECOMMAND_RECV ((byte)-91)

//mac adres van bluetooth device
#define BLUETOOTH_DEVICE "<REPLACE_ME>" 
#define MAX_RETRIES 3

//YOUR WIFI ID
const char *ssid = "<REPLACE_ME>";
const char *password = "<REPLACE_ME>";


//MQTT SERVER settings
const char* mqttServer = "<REPLACE_ME>";
const int mqttPort = 1883;
const char* mqttUser = "<REPLACE_ME>";
const char* mqttPassword = "<REPLACE_ME>";

int numberOfRetries = 0;
unsigned long StartTime = 0;


WiFiClient espClient;
PubSubClient client(espClient);




struct AESData
{
  byte key[KEY_LENGTH];
  byte plaintext[MAX_PLAINTEXT_SIZE];
  byte ciphertext[MAX_CIPHERTEXT_SIZE];
  byte iv[KEY_LENGTH];
  size_t size;
};

static BLEUUID serviceUUID("0000fd30-0000-1000-8000-00805F9B34FB");

static BLEUUID notifyUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID writeUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");


struct UnlockData {
  char handShakePayload[78] = {0}; //; 77 + 1 (for 0)
  char handShakeKey[49] = {0};     //48 + 1 (for 0)
  char commandPayload[66] = {0};   // 65 + 1 (for 0)
  char authToken[38] = {0}; //
};



static UnlockData myLock;
int eeAddress = 0;

bool useHardcodedAuthToken = false; //set on true and change authToken into the sniffed one.


//klopt niet meer
const String apiUrl = "https://api.sesamtechnology.com/";



//temp item, alleen te gebruiken in authteniceer als key weer eens is verlopen
const String authToken = "<REPLACE_ME>"; 

const String deviceID = "<REPLACE_ME>";

static byte step2ByArray2[16];

static byte step2Object2[13];
static byte step2byArray[8];

static int notStart = 0;

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic *pReadCharacteristic;
static BLERemoteCharacteristic *pWriteCharacteristic;

static BLEAdvertisedDevice *detectedBluetoothDevice;

CTR<AES128> ctraes128;

byte buffer[128];
static int bufferSending = 0;
DynamicJsonDocument doc(2024);

static bool failedUnlocking = false;

void printHex(uint8_t num)
{
  char hexCar[2];

  sprintf(hexCar, "%02X ", num);
  DEBUG_PRINT(hexCar);
}
class Cryptor
{
  public:
    int communicatorCounter = 0;
    byte key[KEY_LENGTH];
    byte nonce[KEY_LENGTH];
    byte randomData[8];

    int nonceArrayLength = 0;
    int keyArrayLength = 0;

    AESData aesCryptorData;

    Cryptor()
    {
    }

    Cryptor(byte *key, int keyArrayLength, byte *nonceArray, int nonceArrayLength)
    {
      memcpy(this->key, key, keyArrayLength);

      DEBUG_PRINT("key1 : ");
      for (int i = 0; i < keyArrayLength; i++)
      {
        printHex(this->key[i]);
      }

      memcpy(this->nonce, nonceArray, nonceArrayLength);
      DEBUG_PRINT("created");
      this->keyArrayLength = keyArrayLength;
      this->nonceArrayLength = nonceArrayLength;
    }

    byte *random(int length)
    {
      size_t i;

      for (i = 0; i < length; i++)
      {
        this->randomData[i] = rand();
      }

      return randomData;
    }

    bool encrypt(Cipher *cipher, struct AESData *test)
    {
      size_t inc = test->size;
      byte output[inc] = {0};
      size_t posn, len;

      cipher->clear();
      if (!cipher->setKey(test->key, cipher->keySize()))
      {
        Serial.println("setKey ");
        return false;
      }
      if (!cipher->setIV(test->iv, cipher->ivSize()))
      {
        Serial.println("setIV ");
        return false;
      }

      for (posn = 0; posn < test->size; posn += inc)
      {
        len = test->size - posn;
        if (len > inc)
          len = inc;
        DEBUG_PRINT("posn");
        DEBUG_PRINT(posn);
        cipher->encrypt(output + posn, test->plaintext + posn, len);
      }

      memcpy(test->ciphertext, output, inc);
      return true;
    }

    byte *process(byte *inputData, int inputDataLength)
    {
      byte byCounter = (byte)this->communicatorCounter;

      byte ivExtra[] = {0, 0, byCounter};
      byte tempNonce[nonceArrayLength + 3] = {0};
      memcpy(tempNonce, this->nonce, nonceArrayLength);
      memcpy(&tempNonce[nonceArrayLength], ivExtra, 3);
      DEBUG_PRINT("Keyarraylength");
      DEBUG_PRINT(keyArrayLength);
      memcpy(aesCryptorData.key, this->key, keyArrayLength);
      DEBUG_PRINT("key: ");
      for (int i = 0; i < keyArrayLength; i++)
      {
        printHex(this->key[i]);
      }

      memcpy(aesCryptorData.iv, tempNonce, nonceArrayLength + 3);

      DEBUG_PRINT("NonceArrayLength: ");
      DEBUG_PRINT(nonceArrayLength + 3);
      memcpy(aesCryptorData.plaintext, inputData, inputDataLength);
      aesCryptorData.size = inputDataLength;
      encrypt(&ctraes128, &aesCryptorData);

      this->communicatorCounter = (int)((double)this->communicatorCounter + ceil((double)inputDataLength / 16.0));
      return aesCryptorData.ciphertext;
    }
};

static Cryptor lastSuccesCryptor;

void handShakeReceive(byte *byArray, int recLength)
{
  size_t outputLength;
  byte decodedHandshakeKey[DECODED_LENGTH_BASE64]; //sizes?
  mbedtls_base64_decode(decodedHandshakeKey, DECODED_LENGTH_BASE64, &outputLength, (const unsigned char *)myLock.handShakeKey, strlen(myLock.handShakeKey));

  DEBUG_PRINT("Handshake vesturen: ");
  DEBUG_PRINT(outputLength);

  byte object2[13];
  memcpy(object2, byArray, 13);

  DEBUG_PRINT("Reclength: ");
  DEBUG_PRINT(recLength);
  byArray = &byArray[13]; //vanaf plek13
  Cryptor temp = Cryptor(decodedHandshakeKey, outputLength, object2, 13);
  DEBUG_PRINT("next");
  byte *byArray2 = temp.process(byArray, 8);
  DEBUG_PRINT("next2");

  byte byArrayRandom[8] = {0};
  memcpy(byArrayRandom, temp.random(8), 8);
  DEBUG_PRINT("next3");
  DEBUG_PRINT("byArray data");

  for (int i = 0; i < 8; i++)
  {
    printHex(byArrayRandom[i]);
  }

  byte concatArray2[16];
  memcpy(concatArray2, byArray2, 8);
  memcpy(&concatArray2[8], byArrayRandom, 8);
  DEBUG_PRINT("next4");

  memcpy(step2ByArray2, concatArray2, 16);
  memcpy(step2Object2, object2, 13);
  memcpy(step2byArray, byArrayRandom, 8);
  call(CRYPTO_SEND, temp.process(concatArray2, 16), 16); //(byte)-93, new v1(byArray2, (byte[])object2, byArray, result, connection)

}

void checkCryptorData(byte *byArray, int recLength)
{
  DEBUG_PRINT("checkCryptorData");
  Cryptor crypt = Cryptor(step2ByArray2, 16, step2Object2, 13);
  byArray = crypt.process(byArray, recLength);
  byte by = byArray[0];

  bool gelijk = true;
  for (int i = 0; i < 8; i++)
  {
    byte storedByte = step2byArray[i];
    byte recvByte = byArray[i + 1];
    if (storedByte != recvByte)
    {
      gelijk = false;
      Serial.println("Gelijk was niet waar");
      break;
    }
  }

  DEBUG_PRINT("Gelijk:");
  DEBUG_PRINT(gelijk);

  if (gelijk == true && by == 0)
  {
    Serial.println("=== ENCRYPTIE ENABLED ==");
    lastSuccesCryptor = crypt;
    sendCommandPayload();
  }
}

void resultActivateCommand(byte *received, int recLength)
{
  byte result = lastSuccesCryptor.process(received, recLength)[0];
  if (result == -16)
  {
    Serial.println("===ACCESS DENIED===");
    failedUnlocking = true;
  }
  DEBUG_PRINT("Stap5 return value:");
  DEBUG_PRINT(result);

  if (result == 0)
  {
    Serial.println("!!ACCESS ALLOWED!!");
    client.publish("door_unlocking", "Door unlocking succes");
    failedUnlocking = false;
  }
  doConnect = false;
  connected = false;
  refreshEEPROMData();
}

static void notifyCallback(
  BLERemoteCharacteristic *pBLERemoteCharacteristic,
  uint8_t *pData,
  size_t length,
  bool isNotify)
{
  DEBUG_PRINT("Notify callback for characteristic ");
  DEBUG_PRINT(pBLERemoteCharacteristic->getUUID().toString().c_str());
  DEBUG_PRINT(" of data length ");
  DEBUG_PRINT(length);
  DEBUG_PRINT("data: ");
  //Serial.println((char*)pData);
  for (int i = 0; i < length; i++)
  {
    printHex(pData[i]);
  }

  if (length > 3)
  { //0-3 is cmd, lengte (2xbytes)
    DEBUG_PRINT("Lengte is 3, pData: ");
    printHex(pData[0]);

    switch (pData[0])
    {
      case HANDSHAKE_RECV:
        handShakeReceive(&pData[3], (length - 3));
        break;

      case CRYPTO_RECV:
        checkCryptorData(&pData[3], (length - 3));
        break;

      case ACTIVATECOMMAND_RECV:
        resultActivateCommand(&pData[3], (length - 3));
        break;
    }
  }
  else
  {
    failedUnlocking = true; // data failed.
    Serial.println("Lengte is NIET 3");
  }
}

class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
      Serial.println("Connected");
    }

    void onDisconnect(BLEClient *pclient)
    {
      connected = false;
      Serial.println("onDisconnect");
    }
};

void call(byte cmdType, byte inputData[], int inputDataLength)
{
  byte sendableArray[inputDataLength + 3];
  sendableArray[0] = cmdType;
  sendableArray[1] = (byte)(inputDataLength & 0xFF);
  sendableArray[2] = (byte)((inputDataLength >> 8) & 0xFF);
  memcpy(&sendableArray[3], inputData, inputDataLength);
  DEBUG_PRINT("Calling met lengte: ");
  DEBUG_PRINT(inputDataLength + 3);

  DEBUG_PRINT("Data bijna vesturen:");
  for (int i = 0; i < inputDataLength + 3; i++)
  {
    printHex(sendableArray[i]);
  }

  notStart = 2;
  memcpy(buffer, sendableArray, inputDataLength + 3);
  bufferSending = inputDataLength + 3;
}

void sendCommandPayload()
{
  DEBUG_PRINT("sendCommandPayload");
  size_t outputLength;
  byte decodedcommandPayload[DECODED_LENGTH_BASE64];
  mbedtls_base64_decode(decodedcommandPayload, DECODED_LENGTH_BASE64, &outputLength, (const unsigned char *)myLock.commandPayload, strlen(myLock.commandPayload));

  Serial.println(outputLength);

  call(ACTIVATECOMMAND_SEND, lastSuccesCryptor.process(decodedcommandPayload, outputLength), 46);

}

void sendHandshake()
{
  size_t outputLength;
  byte decodedHandshake[DECODED_LENGTH_BASE64];
  mbedtls_base64_decode(decodedHandshake, DECODED_LENGTH_BASE64, &outputLength, (const unsigned char *)myLock.handShakePayload, strlen(myLock.handShakePayload));

  DEBUG_PRINT("Handshake vesturen: " + outputLength);

  call(HANDSHAKE_SEND, decodedHandshake, outputLength);

}

bool connectToServer()
{
  DEBUG_PRINT("Forming a connection to ");
  // DEBUG_PRINT(detectedBluetoothDevice->getAddress().toString().c_str());

  BLEClient *pClient = BLEDevice::createClient();
  DEBUG_PRINT(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remote BLE Server.
  pClient->connect(BLEAddress(BLUETOOTH_DEVICE), BLE_ADDR_TYPE_RANDOM); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println("Connecting and sleeping");
  delay(200);
  DEBUG_PRINT(" - Connected to server");
  if (!pClient->isConnected())
    return false;
  //spClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr)
  {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  DEBUG_PRINT(" - Found our service");

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pReadCharacteristic = pRemoteService->getCharacteristic(notifyUUID);
  pWriteCharacteristic = pRemoteService->getCharacteristic(writeUUID);

  if (pReadCharacteristic == nullptr || pWriteCharacteristic == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(notifyUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Read the value of the characteristic.
  if (pWriteCharacteristic->canWrite())
  {
    DEBUG_PRINT("Writing possible");
  }

  if (pReadCharacteristic->canNotify())
  {
    DEBUG_PRINT("Notifying possible");
    pReadCharacteristic->registerForNotify(notifyCallback);
  }

  connected = true;
  return true;
}


int authenticeer(UnlockData * theLock)
{
  if (WiFi.status() == WL_CONNECTED)
  {

    

    HTTPClient http;
    String url = apiUrl + "v1/authentications/" + myLock.authToken;
    http.begin(url);
    http.addHeader("X-Auth-Token", myLock.authToken);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept-Encoding", "gzip");
    http.addHeader("User-Agent", "okhttp/3.14.9");

    DEBUG_PRINT("Authenticeer");

    int httpResponseCode = http.PUT("{\"appDescription\":\"Bold Smart Lock - Android application\",\"appId\":\"sesam.technology.bold\",\"appName\":\"Bold Smart Lock\",\"appVersion\":\"2.2.0a (453)\",\"clientDescription\":\"<REPLACE_ME>\",\"clientId\":<REPLACE_ME>,\"clientLocale\":\"en_US\",\"clientType\":\"Android\",\"clientVersion\":\"29\",\"language\":\"en\",\"notificationToken\":\"<REPLACE_ME>\"}");

    if (httpResponseCode > 0)
    {

      String response = http.getString();
      //char outputData[38];

      int res = getInterestingData(response, "token", theLock->authToken);


      DEBUG_PRINT(httpResponseCode);
      DEBUG_PRINT(response);
      DEBUG_PRINT("Outputdata: ");
      DEBUG_PRINT(theLock->authToken);
          http.end();

      return res;


    }
    else
    {

      Serial.print("Error on sending PUT Request: ");
      Serial.println(httpResponseCode);
      return 0;
    }

    http.end();
  }
  else
  {
    Serial.println("Error in WiFi connection");
    return 0;
  }
}

int getInterestingData(String response, String wanted, char *outputBuffer)
{
  //String response = http.getString();
//Serial.println("asdfasdfd");
 // Serial.println(response);


  deserializeJson(doc, response); 

  JsonObject object = doc.as<JsonObject>();

  String cmdPayload = object[wanted];
  if (cmdPayload.length() > 1) {
    DEBUG_PRINT("response was a succes for string: " + wanted);
    DEBUG_PRINT(cmdPayload.length());
    memcpy(outputBuffer, cmdPayload.c_str(), cmdPayload.length());
    DEBUG_PRINT(cmdPayload);
    return cmdPayload.length();

  } else {
    Serial.println("requested data not found");
    return 0;
  }
}

int makeRequest(String apiUrl, int sort, String interestingData[], int lengthInterestingData, String httpData, char *outputData[])
{
  if (WiFi.status() == WL_CONNECTED)
  {

    HTTPClient http;

    http.begin(apiUrl);
    http.addHeader("X-Auth-Token", myLock.authToken);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept-Encoding", "gzip");
    http.addHeader("User-Agent", "okhttp/3.14.9");
    int httpResponseCode = 0;

    switch (sort)
    {
      case 1:
        httpResponseCode = http.GET();
        break;
      case 2:
        httpResponseCode = http.POST(httpData);
        break;
      case 3:
        httpResponseCode = http.PUT(httpData);
        break;
    }

    if (httpResponseCode > 0)
    {

      String response = http.getString();
      response.remove(0, 1); //verwijder [ en ]
      response.remove(-1, 1);
      int result = 0;
      for (int i = 0; i < lengthInterestingData; i++)
      {
        result = getInterestingData(response, interestingData[i], outputData[i]);
        if (result == 0) { //when getting an error.
          break;
        }
      }
      return result;
    }
    else
    {

      Serial.print("Error on sending Request: ");
      Serial.println(httpResponseCode);
      Serial.println(apiUrl);
      http.end();

      return -1;

    }

    http.end();
  }
  else
  {
    Serial.println("Error in WiFi connection");
    return -1;
  }
  return -1;
}

int getSesamHandshake(UnlockData * theLock)
{

  //https://api.sesamtechnology.com/v1/controller/v0/handshakes?deviceId=<REPLACE_ME>

  String url = apiUrl + "v1/controller/v0/handshakes?deviceId=" + deviceID;

  String wantedData[] = {"handshakeKey", "payload"};
  char *outputData[] = {theLock->handShakeKey, theLock->handShakePayload};
  DEBUG_PRINT("GetSesamHandShake");
  return makeRequest(url, 1, wantedData, 2, "", outputData);



}

int getSesamCommand(UnlockData * theLock)
{
  String url = apiUrl + "v1/controller/v0/commands/activate-device?deviceId=" + deviceID;
  String wantedData[] = {"payload"};

  char *outputData[] = {theLock->commandPayload};
  DEBUG_PRINT("GetSesamCommand");
  return makeRequest(url, 1, wantedData, 1, "", outputData);

}



void connectToWifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void reconnectClient() {
  connectToWifi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP8266Client", mqttUser, mqttPassword )) {
      Serial.println("connected to mqtt");
      client.subscribe("door_unlocking");

    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}



void setupEEPROM() {
  if (!EEPROM.begin(1000)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }
}

void writeBytesEEPROM(byte * inputData, int inputLength) {
  for (int i = 0; i < inputLength; i++) {
    EEPROM.write(eeAddress + i, inputData[i]);
  }
  EEPROM.commit();
}


void readBytesEEPROM(byte * outputData, int outputLength) {
  for (int i = 0; i < outputLength; i++) {
    outputData[i] = (EEPROM.read(eeAddress + i));
  }
}

void refreshEEPROMData() {
  Serial.println("Refreshing sesamdata");
  UnlockData tempItem;
  memset(&tempItem, 0x0, sizeof(tempItem));

  if (WiFi.status() != WL_CONNECTED) {
    reconnectClient();
  }
  // authenticeer();
  bool result = (((getSesamHandshake(&tempItem) > 0) ? true : false) && ((getSesamCommand(&tempItem) > 0) ? true : false) && (authenticeer(&tempItem) > 0) ? true : false);
  if (result) {
    DEBUG_PRINT("Result success, writing to eeprom");
    DEBUG_PRINT(tempItem.handShakeKey);
    DEBUG_PRINT(tempItem.handShakePayload);
    DEBUG_PRINT(tempItem.commandPayload);
    DEBUG_PRINT(tempItem.authToken);
    writeBytesEEPROM((byte*)&tempItem, sizeof(tempItem));
    memcpy(&myLock, &tempItem, sizeof(tempItem));
    Serial.println("written data");
  } else {
    Serial.println("Refreshing data not happened");
  }

}


void mqttCallback(char* topic, byte* payload, unsigned int length) {

  DEBUG_PRINT("Message arrived in topic: ");
  DEBUG_PRINT(topic);

  DEBUG_PRINT("Message:");


  String content = String((const char*)payload);
  DEBUG_PRINT(content);

  if (content.indexOf("unlock_door") >= 0) {
    startUnlockingDoor();
  }
}


void startUnlockingDoor() {
  Serial.println("Starting door unlocking..");
  doWeHavePayloadData(); //check if data is right.

  notStart = 0;
  doConnect = true;
  connected = false;
  StartTimer();
}

void doWeHavePayloadData() {

  if (myLock.handShakeKey[0] == 0 || myLock.handShakePayload[0] == 0 || !isAlphaNumeric(myLock.handShakeKey[0]) || !isAlphaNumeric(myLock.handShakePayload[0]) || !isAlphaNumeric(myLock.commandPayload[0]) || !isAlphaNumeric(myLock.authToken[0])) { //empty data, refresh
    refreshEEPROMData();
  } else {
    Serial.println("We have succesfull data");
  }
}

void setup()
{
  setupEEPROM();
  memset(&myLock, 0x0, sizeof(myLock));



  readBytesEEPROM((byte*)&myLock, sizeof(myLock));

  if(useHardcodedAuthToken){
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



void StartTimer() {
  StartTime = millis();
  Serial.println("Start time is: ");
  Serial.println(StartTime);
}




void loop()
{
  if (!client.connected()) {

    reconnectClient();
  }

  client.loop();

  if (StartTime && doConnect)   // Since 0==false, this is the same as (StartTime != 0)
  {
    // The timer is running.  Check for stuff.
    unsigned long elapsedTime = millis() - StartTime;
    Serial.println("Starttime");
    Serial.println(elapsedTime);

    if (elapsedTime >= (long)10000)
    {
      // The timer has ended!  Do the end thing!
      StartTime = 0;
      if (numberOfRetries < MAX_RETRIES) {
        failedUnlocking = true;
        Serial.println("Unlocking failed in time, retrying");
      } else {
        Serial.println("Unlocking failed in time and MAX retries, stopping now");
      }

    }
  }



  if (failedUnlocking) {
    if (numberOfRetries < MAX_RETRIES) {
      Serial.println("Failed unlocking, NumberOfRetries: ");
      Serial.println(numberOfRetries);

      refreshEEPROMData();
      startUnlockingDoor();
      failedUnlocking = false;
      numberOfRetries++;
    } else {
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

    DEBUG_PRINT("notStart");
    DEBUG_PRINT(notStart);

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
