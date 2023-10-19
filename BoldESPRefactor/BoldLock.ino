#include "BoldCommon.h"



struct AESData
{
  byte key[KEY_LENGTH];
  byte plaintext[MAX_PLAINTEXT_SIZE];
  byte ciphertext[MAX_CIPHERTEXT_SIZE];
  byte iv[KEY_LENGTH];
  size_t size;
};

static byte step2ByArray2[16];

static byte step2Object2[13];
static byte step2byArray[8];

CTR<AES128> ctraes128;

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
  byte decodedHandshakeKey[DECODED_LENGTH_BASE64]; // sizes?
  mbedtls_base64_decode(decodedHandshakeKey, DECODED_LENGTH_BASE64, &outputLength, (const unsigned char *)myLock.handShakeKey, strlen(myLock.handShakeKey));

  DEBUG_PRINT("Handshake vesturen: ");
  DEBUG_PRINT(outputLength);

  byte object2[13];
  memcpy(object2, byArray, 13);

  DEBUG_PRINT("Reclength: ");
  DEBUG_PRINT(recLength);
  byArray = &byArray[13]; // vanaf plek13
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

//  DEBUG_PRINT("Gelijk:");
//  DEBUG_PRINT(gelijk);

  if (gelijk == true && by == 0)
  {
    Serial.println("=== ENCRYPTIE ENABLED ==");
    lastSuccesCryptor = crypt;
    sendCommandPayload();
  }
}

void resultActivateCommand(byte *received, int recLength)
{
  byte * result = lastSuccesCryptor.process(received, recLength);

  DEBUG_PRINT("LAATSTE stap Notify callback for characteristic ");
  //DEBUG_PRINT(pBLERemoteCharacteristic->getUUID().toString().c_str());
  DEBUG_PRINT(" of data length ");
  DEBUG_PRINT(recLength);
  DEBUG_PRINT("data: ");
  // Serial.println((char*)pData);
  for (int i = 0; i < recLength; i++)
  {
    printHex(result[i]);
  }

DEBUG_PRINT("einde");

  if (result[0] == -16)
  {
    Serial.println("===ACCESS DENIED===");
    failedUnlocking = true;
  }
  DEBUG_PRINT("Stap5 return value:");
  DEBUG_PRINT(result[0]);

  if (result == 0)
  {
    Serial.println("!!ACCESS ALLOWED!!");
    client.publish("door_unlocking", "Door unlocking succes");
    failedUnlocking = false;
  }

  // STRAKS ONDERSTE WEER AANZETTEN.
      doConnect = false;
      connected = false;
      refreshEEPROMData();
}

void call(byte cmdType, byte inputData[], int inputDataLength)
{
  byte sendableArray[inputDataLength + 3];
  sendableArray[0] = cmdType;
  sendableArray[1] = (byte)(inputDataLength & 0xFF);
  sendableArray[2] = (byte)((inputDataLength >> 8) & 0xFF);
  memcpy(&sendableArray[3], inputData, inputDataLength);
  DEBUG_PRINT("Calling met lengte: ");
  DEBUG_PRINT(inputDataLength + 3);

//  DEBUG_PRINT("Data bijna vesturen:");
//  for (int i = 0; i < inputDataLength + 3; i++)
//  {
//    printHex(sendableArray[i]);
//  }

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
