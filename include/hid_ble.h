#ifndef HID_BLE_H
#define HID_BLE_H

#include <Arduino.h>

// Initialize the BLE HID stack and start advertising.
// pairingMode = true: open advertising, accept new pairings (60s typical)
// pairingMode = false: standard advertising; bonded hosts can reconnect
void hidBleInit(bool pairingMode);

// Stop advertising and free the BLE stack (lowest power state).
void hidBleDeinit();

// True if the stack has been initialized this boot session.
bool hidBleIsActive();

// True if a host is currently connected.
bool hidBleIsConnected();

// Number of bonded peers persisted in NVS.
uint8_t hidBleGetBondCount();

// Wipe all bonded peers.
void hidBleClearAllBonds();

// Send a single numpad key (mirrors hidUsbSendNumpadKey contract).
void hidBleSendNumpadKey(char key, bool numLockOn);

// Type a string as ASCII.
void hidBleSendString(const String& str);

#endif
