#include "BoldCommon.h"

void setupEEPROM()
{
    if (!EEPROM.begin(1000))
    {
        Serial.println("Failed to initialise EEPROM");
        Serial.println("Restarting...");
        delay(1000);
        ESP.restart();
    }
}

void writeBytesEEPROM(byte *inputData, int inputLength)
{
    for (int i = 0; i < inputLength; i++)
    {
        EEPROM.write(eeAddress + i, inputData[i]);
    }
    EEPROM.commit();
}

void readBytesEEPROM(byte *outputData, int outputLength)
{
    for (int i = 0; i < outputLength; i++)
    {
        outputData[i] = (EEPROM.read(eeAddress + i));
    }
}