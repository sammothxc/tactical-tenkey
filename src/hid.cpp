#include "hid.h"
#include "hid_usb.h"
#include "hid_ble.h"

extern bool bleConnected;  // defined in main.cpp

bool hidInitialized = false;


void hidInit() {
    if (hidInitialized) return;
    hidUsbInit();
    hidInitialized = true;
}


void hidSendNumpadKey(char key, bool numLockOn) {
    if (!hidInitialized) return;
    if (bleConnected && hidBleIsConnected()) {
        hidBleSendNumpadKey(key, numLockOn);
    } else {
        hidUsbSendNumpadKey(key, numLockOn);
    }
}


void hidSendString(const String& str) {
    if (!hidInitialized) return;
    if (bleConnected && hidBleIsConnected()) {
        hidBleSendString(str);
    } else {
        hidUsbSendString(str);
    }
}


void hidSendKey(char key, bool numLockOn) {
    hidSendNumpadKey(key, numLockOn);
}
