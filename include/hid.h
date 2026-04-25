#ifndef HID_H
#define HID_H

#include <Arduino.h>

extern bool hidInitialized;

void hidInit();
void hidSendKey(char key, bool numLockOn = true);
void hidSendString(const String& str);
void hidSendNumpadKey(char key, bool numLockOn = true);

#endif
