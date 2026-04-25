#include "hid.h"
#include "hid_usb.h"

extern bool bleConnected;  // defined in main.cpp

bool hidInitialized = false;


void hidInit() {
    if (hidInitialized) return;
    hidUsbInit();
    // BLE backend will be initialized lazily in phase 2 (bond-aware)
    hidInitialized = true;
}


void hidSendNumpadKey(char key, bool numLockOn) {
    if (!hidInitialized) return;
    // future: if (bleConnected) hidBleSendNumpadKey(key, numLockOn); else
    hidUsbSendNumpadKey(key, numLockOn);
}


void hidSendString(const String& str) {
    if (!hidInitialized) return;
    // future: if (bleConnected) hidBleSendString(str); else
    hidUsbSendString(str);
}


void hidSendKey(char key, bool numLockOn) {
    hidSendNumpadKey(key, numLockOn);
}
