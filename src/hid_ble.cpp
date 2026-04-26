#include "hid_ble.h"
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include "esp_gap_ble_api.h"

extern uint8_t zoomModifier;  // defined in main.cpp: 0 = Ctrl, 1 = Cmd/GUI

static const char* BLE_DEVICE_NAME = "Tactical Tenkey";

// Subclass to capture the connecting peer's MAC. T-vK BleKeyboard's connect
// callback only takes BLEServer*; the param overload (with remote_bda) is
// virtual on BLEServerCallbacks but unused upstream — we hook it here.
class PeerAwareBleKeyboard : public BleKeyboard {
public:
    using BleKeyboard::BleKeyboard;
    static uint8_t s_peerMac[6];
    static bool s_peerKnown;

    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override {
        if (param) {
            memcpy(s_peerMac, param->connect.remote_bda, 6);
            s_peerKnown = true;
        }
    }

    void onDisconnect(BLEServer* pServer) override {
        s_peerKnown = false;
        BleKeyboard::onDisconnect(pServer);
    }
};
uint8_t PeerAwareBleKeyboard::s_peerMac[6] = {0};
bool PeerAwareBleKeyboard::s_peerKnown = false;

static PeerAwareBleKeyboard* bleKb = nullptr;
static bool bleActive = false;


static void ensureKb() {
    if (bleKb == nullptr) {
        bleKb = new PeerAwareBleKeyboard(BLE_DEVICE_NAME, "Tactical Tenkey", 100);
    }
}


void hidBleInit(bool pairingMode) {
    if (bleActive) return;
    ensureKb();
    bleKb->begin();

    // Use the public BLE MAC instead of a rotating random one. Some hosts
    // (Windows BT stack notably) treat each rotated address as a new nameless
    // device, which is what causes "shows on iPhone, MAC-only on PC".
    esp_ble_gap_config_local_privacy(false);

    // T-vK BleKeyboard suppresses the scan response, which is normally
    // where the device name rides. Set the GAP name at the controller and
    // also embed the name in the primary advertising packet so hosts see
    // "Tactical Tenkey" instead of a raw MAC.
    esp_ble_gap_set_device_name(BLE_DEVICE_NAME);

    // Explicit SMP config: Secure Connections + bonding, no MITM, IO=None.
    // This produces clean "just-works" pairing that Linux BlueZ accepts for
    // HID. T-vK's defaults often trip BlueZ's "AuthenticationCanceled".
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key  = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,      &iocap,    sizeof(iocap));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,    &key_size, sizeof(key_size));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,    &init_key, sizeof(init_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,     &rsp_key,  sizeof(rsp_key));

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    if (adv) {
        adv->stop();

        BLEAdvertisementData advData;
        advData.setFlags(0x06);  // LE general discoverable + BR/EDR not supported
        advData.setName(BLE_DEVICE_NAME);
        advData.setAppearance(0x03C1);  // HID Keyboard
        adv->setAdvertisementData(advData);

        BLEAdvertisementData scanRsp;
        scanRsp.setCompleteServices(BLEUUID((uint16_t)0x1812));  // HID service
        adv->setScanResponseData(scanRsp);
        adv->setScanResponse(true);

        adv->start();
    }

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
        // + and - send Ctrl+Plus / Ctrl+Minus (zoom in/out)
        if (key == '+' || key == '-') {
            uint8_t k = (key == '+') ? KEY_NUM_PLUS : KEY_NUM_MINUS;
            uint8_t mod = (zoomModifier == 1) ? KEY_LEFT_GUI : KEY_LEFT_CTRL;
            bleKb->press(mod);
            bleKb->press(k);
            delay(10);
            bleKb->release(k);
            bleKb->releaseAll();
            return;
        }
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
            case '=': code = KEY_NUM_ENTER;  break;
            default: return;
        }
    }

    bleKb->press(code);
    delay(10);
    bleKb->release(code);
    bleKb->releaseAll();  // guard against stuck modifier bits
}


void hidBleSendString(const String& str) {
    if (!bleActive || !bleKb || !bleKb->isConnected()) return;
    for (size_t i = 0; i < str.length(); i++) {
        bleKb->write((uint8_t)str.charAt(i));
        bleKb->releaseAll();
        delay(10);
    }
}


void hidBleClearReport() {
    if (bleActive && bleKb && bleKb->isConnected()) {
        bleKb->releaseAll();
    }
}


const uint8_t* hidBleGetPeerMac() {
    if (!bleActive || !bleKb || !bleKb->isConnected()) return nullptr;
    if (!PeerAwareBleKeyboard::s_peerKnown) return nullptr;
    return PeerAwareBleKeyboard::s_peerMac;
}
