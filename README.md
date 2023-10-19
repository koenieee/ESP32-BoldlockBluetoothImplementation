# ESP32-BoldlockBluetoothImplementation
I made my own bluetooth esp32 implementation for the smart Boldlock, this repo is to share the basic code. It's not working at this moment, it did almost one year ago.
Bold changed their server API calls, so I wasn't able to hold up with them.

I wasn't able to refactor it in a nice way due to time shortage,  so this is it, I had to read some decompiled java code for the android app to see how it worked. That's why the code is not really readable.

The implementation worked though they changed their server API and eventually I just bought the BoldConnect.

I also had bluetooth issues with the ESP32, they have a bug in the firmware so sometimes it hangs. This topic: https://github.com/espressif/esp-idf/issues/5105#issuecomment-1573233398

`

For commands and payload, also see this implementation: https://github.com/robbertkl/homebridge-bold-ble/blob/main/src/bold/api/api.ts#L234C23-L234C23

For getting the auth keys, you can follow:  https://auth.boldsmartlock.com/ 

Watch for the last step chrome developer tools network to get the refresh token and final token. See: https://github.com/robbertkl/homebridge-bold-ble/blob/main/src/bold/api/api.ts#L198



For bluetooth implementation also see this file: https://github.com/robbertkl/homebridge-bold-ble/blob/main/src/bold/ble/ble.ts

To get some inspiration.

BoldESPTotaal is my old version, maybe it compiles...
BoldESPRefactor was my new version, but I think it doesn't compile. 

```
Make sure to install all these libraries in Arduino for ESP32:
#include "BLEDevice.h"
//#include "BLEScan.h"
#include "mbedtls/base64.h" //to decode 


#include <Crypto.h> //AES
#include <AES.h>
#include <CTR.h>
#include <string.h>
#include "WiFi.h"
#include "HTTPClient.h"
#include <ArduinoJson.h>
#include "EEPROM.h"
#include "WiFiClient.h"
#include <PubSubClient.h>
```

Any questions ask in a new bug ticket in this repo.

Important files are BoldLock.ino in BoldRefactor, this is the bluetooth callback file. 

This project is using the old API from bold connect server, I think that actually quite easy to build for yourself. For access tokens see:   https://auth.boldsmartlock.com/  and chrome developer network inspection.

You can sniff the android app network traffic if you have phone with root and system fiddler certificate installed. 
Or just ask for the API documentation somewhere else (homebridge bold repo can also help out with that I think)

But you only need to following data from bold: 

```

  https://api.sesamtechnology.com/v1/controller/v0/handshakes?deviceId=<REPLACE_ME>
handshake key and payload

https://api.sesamtechnology.com/v1/controller/v0/commands/activate-device?deviceId=<REPLACE_ME>

command payload.

```

Feed that into the Bluetooth part and you should be able to open the lock.


The handshake/command payload info expires also in a few hours/days so be aware that you can't use them all the time (only a few times I think).

The auth token can also be used (refresh token) to get a new final token, I think this token also expires after a while.

Would be nice if someones makes a raspberry pi python bluetooth implementation or just another ESP32 bluetooth implementation. 
Although the bluetooth is not stable for the ESP :(

For my home assisant I made another implementation for the BoldConnect and whenever I press the button on the bold lock, this is captured by the BoldConnect and logged to the servers of Bold. So I retrieve the event logging from the servers of Bold and see whenever I clicked the button on the BoldLock to leave the house, you can do all kinds of cool automation stuff. 
I wasn't able to retrieve the bluetooth callback signal at that time with my ESP32, I think it's a bluetooth 5.x thing to be able to retrieve events from the lock. Don't know how this works actually... So BoldConnect was still nice to buy because of some extra options (such as event logging from the button press). 

I am also very curieus how they did the clicker Bold, because there has to be a fixed key in there. 
This one: https://boldsmartlock.com/nl/producten/bold-clicker/

Maybe someone can find out, or if someones uses his phone to learn the bold clicker into the lock with their phone and sniff network traffic at the same time the key that is retrieved.... :)

Well have fun exploring. 