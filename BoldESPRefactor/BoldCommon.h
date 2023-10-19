#ifndef GRANDPARENT_H
#define GRANDPARENT_H

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

struct UnlockData
{
    char handShakePayload[78] = {0}; //= "<REPLACE_ME>"; 77 + 1 (for 0) //zie boldhttp.ino van bold api server
    char handShakeKey[49] = {0};     //= "<REPLACE_ME>"; 48 + 1 (for 0)  //zie boldhttp.ino  van bold api server
    char commandPayload[66] = {0};   //= "<REPLACE_ME>"; 65 + 1 (for 0)  // zie boldhttp.ino van bold api server
    char authToken[38] = {0};        // <REPLACE_ME>  //key voor toegang tot bold api server, dit is nu vervangen met versie 2 api. 
}; 


const String apiUrl = "https://api.boldsmartlock.com/";
// temp item, alleen te gebruiken in authteniceer als key weer eens is verlopen
const String authToken = "<REPLACE_ME>"; 
//id van boldlock, kort getalletje
const String deviceID = "<REPLACE_ME>";
bool useHardcodedAuthToken = true; // set on true and change authToken into the sniffed one.
static UnlockData myLock;
int eeAddress = 0;
static int notStart = 0;
byte buffer[128];
static int bufferSending = 0;
DynamicJsonDocument doc(2024);
static bool failedUnlocking = false;

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;

int numberOfRetries = 0;
unsigned long StartTime = 0;

static BLEUUID serviceUUID("0000fd30-0000-1000-8000-00805F9B34FB"); //voor alle sloten hetzelfde
static BLEUUID notifyUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e"); //voor alle sloten hetzelfde
static BLEUUID writeUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e"); //voor alle sloten hetzelfde

static BLERemoteCharacteristic *pReadCharacteristic;
static BLERemoteCharacteristic *pWriteCharacteristic;

static BLEAdvertisedDevice *detectedBluetoothDevice;


#define MAX_PLAINTEXT_SIZE 60
#define MAX_CIPHERTEXT_SIZE 60
#define KEY_LENGTH 16 // and nonce

#define DECODED_LENGTH_BASE64 70

#define HANDSHAKE_SEND ((byte)-96)
#define HANDSHAKE_RECV ((byte)-95)

#define CRYPTO_SEND ((byte)-94)
#define CRYPTO_RECV ((byte)-93)

#define ACTIVATECOMMAND_SEND ((byte)-92)
#define ACTIVATECOMMAND_RECV ((byte)-91)


//id van boldlock in bold
#define BLUETOOTH_DEVICE "<REPLACE_ME>" 
#define MAX_RETRIES 3

#endif /* GRANDPARENT_H */
