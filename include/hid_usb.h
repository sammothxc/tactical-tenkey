#ifndef HID_USB_H
#define HID_USB_H

#include <Arduino.h>

void hidUsbInit();
void hidUsbSendNumpadKey(char key, bool numLockOn);
void hidUsbSendString(const String& str);

#endif
