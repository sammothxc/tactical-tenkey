#include "hid_ble.h"
#include <BleKeyboard.h>
#include "esp_gap_ble_api.h"

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

    // pairingMode is informational for now; T-vK BleKeyboard always
    // advertises as bondable and accepts new pairings.
    (void)pairingMode;
}


void hidBleDeinit() {
    if (!bleActive) return;
    if (bleKb) bleKb->end();
    bleActive = false;
}


bool hidBleIsActive() {
    return bleActive;
}


bool hidBleIsConnected() {
    if (!bleActive || !bleKb) return false;
    return bleKb->isConnected();
}


uint8_t hidBleGetBondCount() {
    int n = esp_ble_get_bond_device_num();
    return n < 0 ? 0 : (uint8_t)n;
}


void hidBleClearAllBonds() {
    int num = esp_ble_get_bond_device_num();
    if (num <= 0) return;
    esp_ble_bond_dev_t* list =
        (esp_ble_bond_dev_t*)malloc(num * sizeof(esp_ble_bond_dev_t));
    if (!list) return;
    if (esp_ble_get_bond_device_list(&num, list) == ESP_OK) {
        for (int i = 0; i < num; i++) {
            esp_ble_remove_bond_device(list[i].bd_addr);
        }
    }
    free(list);
}


String hidBleGetBondAddress(uint8_t index) {
    int num = esp_ble_get_bond_device_num();
    if ((int)index >= num || num <= 0) return String("");
    esp_ble_bond_dev_t* list =
        (esp_ble_bond_dev_t*)malloc(num * sizeof(esp_ble_bond_dev_t));
    if (!list) return String("");
    String result = "";
    if (esp_ble_get_bond_device_list(&num, list) == ESP_OK) {
        char buf[18];
        snprintf(buf, sizeof(buf),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 list[index].bd_addr[0], list[index].bd_addr[1],
                 list[index].bd_addr[2], list[index].bd_addr[3],
                 list[index].bd_addr[4], list[index].bd_addr[5]);
        result = String(buf);
    }
    free(list);
    return result;
}


bool hidBleDeleteBond(uint8_t index) {
    int num = esp_ble_get_bond_device_num();
    if ((int)index >= num || num <= 0) return false;
    esp_ble_bond_dev_t* list =
        (esp_ble_bond_dev_t*)malloc(num * sizeof(esp_ble_bond_dev_t));
    if (!list) return false;
    bool ok = false;
    if (esp_ble_get_bond_device_list(&num, list) == ESP_OK) {
        ok = (esp_ble_remove_bond_device(list[index].bd_addr) == ESP_OK);
    }
    free(list);
    return ok;
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
