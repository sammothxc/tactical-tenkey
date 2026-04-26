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

// Address of bonded peer at index (0..getBondCount()-1). Empty on out-of-range.
String hidBleGetBondAddress(uint8_t index);

// Delete a single bond by index. Returns true if removed.
bool hidBleDeleteBond(uint8_t index);

// Send a single numpad key (mirrors hidUsbSendNumpadKey contract).
void hidBleSendNumpadKey(char key, bool numLockOn);

// Type a string as ASCII.
void hidBleSendString(const String& str);

// Send an all-zeros HID report. Used to clear any stuck modifier/key bits
// after connection or pairing — some hosts (macOS notably) latch a phantom
// modifier from the initial post-connect report otherwise.
void hidBleClearReport();

#endif
