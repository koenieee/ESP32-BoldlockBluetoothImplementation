#include "BoldCommon.h"



static void notifyCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{

    if (length > 3)
    { // 0-3 is cmd, lengte (2xbytes)
        DEBUG_PRINT("Lengte is 3, pData: ");
        printHex(pData[0]);
        DEBUG_PRINT("LEngte is: ");
        DEBUG_PRINT((length - 3));

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
    // spClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)

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
