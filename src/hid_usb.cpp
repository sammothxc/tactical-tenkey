#include "hid_usb.h"
#include "USB.h"
#include "USBHIDKeyboard.h"

static USBHIDKeyboard Keyboard;
static bool usbStarted = false;

// raw USB HID usage codes (page 0x07) for the keypad block
#define KEY_NUMPAD_DIV   0x54
#define KEY_NUMPAD_MULT  0x55
#define KEY_NUMPAD_MINUS 0x56
#define KEY_NUMPAD_PLUS  0x57
#define KEY_NUMPAD_ENTER 0x58
#define KEY_NUMPAD_1     0x59
#define KEY_NUMPAD_2     0x5A
#define KEY_NUMPAD_3     0x5B
#define KEY_NUMPAD_4     0x5C
#define KEY_NUMPAD_5     0x5D
#define KEY_NUMPAD_6     0x5E
#define KEY_NUMPAD_7     0x5F
#define KEY_NUMPAD_8     0x60
#define KEY_NUMPAD_9     0x61
#define KEY_NUMPAD_0     0x62
#define KEY_NUMPAD_DOT   0x63

// navigation cluster (when numlock is off)
#define KEY_HID_INSERT   0x49
#define KEY_HID_HOME     0x4A
#define KEY_HID_PAGEUP   0x4B
#define KEY_HID_DELETE   0x4C
#define KEY_HID_END      0x4D
#define KEY_HID_PAGEDOWN 0x4E
#define KEY_HID_RIGHT    0x4F
#define KEY_HID_LEFT     0x50
#define KEY_HID_DOWN     0x51
#define KEY_HID_UP       0x52
#define KEY_HID_TAB      0x2B
#define KEY_HID_BACKSPACE 0x2A


void hidUsbInit() {
    if (usbStarted) return;
    Keyboard.begin();
    USB.begin();
    usbStarted = true;
    delay(1500); // wait for host to re-enumerate with the HID interface
}


void hidUsbSendNumpadKey(char key, bool numLockOn) {
    if (!usbStarted) return;

    uint8_t keycode = 0;

    if (numLockOn) {
        switch (key) {
            case '0': keycode = KEY_NUMPAD_0; break;
            case '1': keycode = KEY_NUMPAD_1; break;
            case '2': keycode = KEY_NUMPAD_2; break;
            case '3': keycode = KEY_NUMPAD_3; break;
            case '4': keycode = KEY_NUMPAD_4; break;
            case '5': keycode = KEY_NUMPAD_5; break;
            case '6': keycode = KEY_NUMPAD_6; break;
            case '7': keycode = KEY_NUMPAD_7; break;
            case '8': keycode = KEY_NUMPAD_8; break;
            case '9': keycode = KEY_NUMPAD_9; break;
            case '.': keycode = KEY_NUMPAD_DOT; break;
            case '+': keycode = KEY_NUMPAD_PLUS; break;
            case '-': keycode = KEY_NUMPAD_MINUS; break;
            case '*': keycode = KEY_NUMPAD_MULT; break;
            case '/': keycode = KEY_NUMPAD_DIV; break;
            case '=': keycode = KEY_NUMPAD_ENTER; break;
            default: return;
        }
    } else {
        // numlock off: digits become nav cluster; / and * become Tab and Backspace
        switch (key) {
            case '0': keycode = KEY_HID_INSERT; break;
            case '1': keycode = KEY_HID_END; break;
            case '2': keycode = KEY_HID_DOWN; break;
            case '3': keycode = KEY_HID_PAGEDOWN; break;
            case '4': keycode = KEY_HID_LEFT; break;
            case '5': return; // center has no nav function
            case '6': keycode = KEY_HID_RIGHT; break;
            case '7': keycode = KEY_HID_HOME; break;
            case '8': keycode = KEY_HID_UP; break;
            case '9': keycode = KEY_HID_PAGEUP; break;
            case '.': keycode = KEY_HID_DELETE; break;
            case '/': keycode = KEY_HID_TAB; break;
            case '*': keycode = KEY_HID_BACKSPACE; break;
            case '+': keycode = KEY_NUMPAD_PLUS; break;
            case '-': keycode = KEY_NUMPAD_MINUS; break;
            case '=': keycode = KEY_NUMPAD_ENTER; break;
            default: return;
        }
    }

    Keyboard.pressRaw(keycode);
    delay(10);
    Keyboard.releaseRaw(keycode);
}


void hidUsbSendString(const String& str) {
    if (!usbStarted) return;
    for (int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        Keyboard.print(c);
        delay(10);
    }
}
