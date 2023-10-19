# ESP32-BoldlockBluetoothImplementation
I made my own bluetooth esp32 implementation for the smart Boldlock, this repo is to share the basic code. 

I wasn't able to refactor it in a nice way due to time shortage,  so this is it, I had to read some decompiled java code for the android app to see how it worked. That's why the code is not really readable.

The implementation worked though they changed their server API and eventually I just bought the BoldConnect.

I also had bluetooth issues with the ESP32, they have a bug in the firmware so sometimes it hangs. This topic: https://github.com/espressif/esp-idf/issues/5105#issuecomment-1573233398
