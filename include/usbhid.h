#ifndef USBHID_H
#define USBHID_H

#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"

extern USBHIDKeyboard Keyboard;
extern bool hidInitialized;

void hidInit();
void hidSendKey(char key);
void hidSendString(const String& str);
void hidSendNumpadKey(char key);

#endif