#include "hid_ble.h"
#include <BleKeyboard.h>
#include <NimBLEDevice.h>

static BleKeyboard* bleKb = nullptr;
static bool bleActive = false;


static void ensureKb() {
    if (bleKb == nullptr) {
        bleKb = new BleKeyboard("Tactical Tenkey", "Tactical Tenkey", 100);
    }
}


void hidBleInit(bool pairingMode) {
    if (bleActive) return;
    ensureKb();
    bleKb->begin();
    bleActive = true;

    // pairingMode currently informational only; T-vK BleKeyboard always
    // advertises as bondable. A future revision can switch to whitelist-only
    // when pairingMode == false (requires direct NimBLE advertising config).
    (void)pairingMode;
}


void hidBleDeinit() {
    if (!bleActive) return;
    if (bleKb) bleKb->end();
    bleActive = false;
    // Free the controller for lowest-power state.
    NimBLEDevice::deinit(true);
}


bool hidBleIsActive() {
    return bleActive;
}


bool hidBleIsConnected() {
    if (!bleActive || !bleKb) return false;
    return bleKb->isConnected();
}


uint8_t hidBleGetBondCount() {
    // NimBLEDevice::getNumBonds() requires the stack to be initialized.
    // Init briefly if not active so we can query bonds at boot.
    bool wasActive = bleActive;
    if (!NimBLEDevice::getInitialized()) {
        NimBLEDevice::init("Tactical Tenkey");
    }
    int n = NimBLEDevice::getNumBonds();
    if (!wasActive) {
        NimBLEDevice::deinit(true);
    }
    return (uint8_t)n;
}


void hidBleClearAllBonds() {
    bool wasActive = bleActive;
    if (!NimBLEDevice::getInitialized()) {
        NimBLEDevice::init("Tactical Tenkey");
    }
    NimBLEDevice::deleteAllBonds();
    if (!wasActive) {
        NimBLEDevice::deinit(true);
    }
}


void hidBleSendNumpadKey(char key, bool numLockOn) {
    if (!bleActive || !bleKb || !bleKb->isConnected()) return;

    uint8_t code = 0;
    if (numLockOn) {
        switch (key) {
            case '0': code = KEY_NUM_0;        break;
            case '1': code = KEY_NUM_1;        break;
            case '2': code = KEY_NUM_2;        break;
            case '3': code = KEY_NUM_3;        break;
            case '4': code = KEY_NUM_4;        break;
            case '5': code = KEY_NUM_5;        break;
            case '6': code = KEY_NUM_6;        break;
            case '7': code = KEY_NUM_7;        break;
            case '8': code = KEY_NUM_8;        break;
            case '9': code = KEY_NUM_9;        break;
            case '.': code = KEY_NUM_PERIOD;   break;
            case '+': code = KEY_NUM_PLUS;     break;
            case '-': code = KEY_NUM_MINUS;    break;
            case '*': code = KEY_NUM_ASTERISK; break;
            case '/': code = KEY_NUM_SLASH;    break;
            case '=': code = KEY_NUM_ENTER;    break;
            default: return;
        }
    } else {
        switch (key) {
            case '0': code = KEY_INSERT;     break;
            case '1': code = KEY_END;        break;
            case '2': code = KEY_DOWN_ARROW; break;
            case '3': code = KEY_PAGE_DOWN;  break;
            case '4': code = KEY_LEFT_ARROW; break;
            case '5': return;
            case '6': code = KEY_RIGHT_ARROW; break;
            case '7': code = KEY_HOME;       break;
            case '8': code = KEY_UP_ARROW;   break;
            case '9': code = KEY_PAGE_UP;    break;
            case '.': code = KEY_DELETE;     break;
            case '/': code = KEY_TAB;        break;
            case '*': code = KEY_BACKSPACE;  break;
            case '+': code = KEY_NUM_PLUS;   break;
            case '-': code = KEY_NUM_MINUS;  break;
            case '=': code = KEY_NUM_ENTER;  break;
            default: return;
        }
    }

    bleKb->press(code);
    delay(10);
    bleKb->release(code);
}


void hidBleSendString(const String& str) {
    if (!bleActive || !bleKb || !bleKb->isConnected()) return;
    for (size_t i = 0; i < str.length(); i++) {
        bleKb->write((uint8_t)str.charAt(i));
        delay(10);
    }
}
