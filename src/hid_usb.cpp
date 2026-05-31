#include "hid_usb.h"
#include "USB.h"
#include "USBHIDKeyboard.h"

extern uint8_t zoomModifier;  // defined in main.cpp: 0 = Ctrl, 1 = Cmd/GUI

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

// main-row digits (page 0x07). Unlike the keypad digit usages (0x59-0x62),
// these always produce digits regardless of the *host's* NumLock state, which
// the device can neither see nor control. Using them keeps NUM mode reliable
// even when the host has NumLock off (otherwise the host reads keypad digits
// as navigation -> "NUM mode but no numbers, only nav").
#define KEY_ROW_1        0x1E
#define KEY_ROW_2        0x1F
#define KEY_ROW_3        0x20
#define KEY_ROW_4        0x21
#define KEY_ROW_5        0x22
#define KEY_ROW_6        0x23
#define KEY_ROW_7        0x24
#define KEY_ROW_8        0x25
#define KEY_ROW_9        0x26
#define KEY_ROW_0        0x27
#define KEY_ROW_PERIOD   0x37

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
        // digits + '.' use main-row usages so they're immune to host NumLock;
        // operators and Enter use the keypad usages (already NumLock-independent)
        switch (key) {
            case '0': keycode = KEY_ROW_0; break;
            case '1': keycode = KEY_ROW_1; break;
            case '2': keycode = KEY_ROW_2; break;
            case '3': keycode = KEY_ROW_3; break;
            case '4': keycode = KEY_ROW_4; break;
            case '5': keycode = KEY_ROW_5; break;
            case '6': keycode = KEY_ROW_6; break;
            case '7': keycode = KEY_ROW_7; break;
            case '8': keycode = KEY_ROW_8; break;
            case '9': keycode = KEY_ROW_9; break;
            case '.': keycode = KEY_ROW_PERIOD; break;
            case '+': keycode = KEY_NUMPAD_PLUS; break;
            case '-': keycode = KEY_NUMPAD_MINUS; break;
            case '*': keycode = KEY_NUMPAD_MULT; break;
            case '/': keycode = KEY_NUMPAD_DIV; break;
            case '=': keycode = KEY_NUMPAD_ENTER; break;
            default: return;
        }
    } else {
        // numlock off: digits become nav cluster; / and * become Tab and Backspace;
        // + and - send Ctrl+Plus / Ctrl+Minus (zoom in/out)
        if (key == '+' || key == '-') {
            uint8_t k = (key == '+') ? KEY_NUMPAD_PLUS : KEY_NUMPAD_MINUS;
            uint8_t mod = (zoomModifier == 1) ? KEY_LEFT_GUI : KEY_LEFT_CTRL;
            Keyboard.press(mod);
            Keyboard.pressRaw(k);
            delay(10);
            Keyboard.releaseRaw(k);
            Keyboard.releaseAll();
            return;
        }
        // Home/End/PageUp/PageDown -> cursor-moving combos so they always move
        // the caret visibly on both OSes. The bare nav usages only scroll on
        // macOS (and need scrollable content elsewhere), so map them to:
        // Home/End -> line start/end, PageUp/PageDown -> document top/bottom,
        // using the host OS (zoomModifier: 0 = Win/Linux default, 1 = macOS).
        if (key == '7' || key == '1' || key == '9' || key == '3') {
            bool mac = (zoomModifier == 1);
            uint8_t mod = 0, k = 0;
            switch (key) {
                case '7': mod = mac ? KEY_LEFT_GUI : 0;             k = mac ? KEY_HID_LEFT  : KEY_HID_HOME; break; // line start
                case '1': mod = mac ? KEY_LEFT_GUI : 0;             k = mac ? KEY_HID_RIGHT : KEY_HID_END;  break; // line end
                case '9': mod = mac ? KEY_LEFT_GUI : KEY_LEFT_CTRL; k = mac ? KEY_HID_UP    : KEY_HID_HOME; break; // document top
                case '3': mod = mac ? KEY_LEFT_GUI : KEY_LEFT_CTRL; k = mac ? KEY_HID_DOWN  : KEY_HID_END;  break; // document bottom
            }
            if (mod) Keyboard.press(mod);
            Keyboard.pressRaw(k);
            delay(10);
            Keyboard.releaseRaw(k);
            Keyboard.releaseAll();
            return;
        }
        switch (key) {
            case '0': keycode = KEY_HID_INSERT; break;
            case '2': keycode = KEY_HID_DOWN; break;
            case '4': keycode = KEY_HID_LEFT; break;
            case '5': return; // center has no nav function
            case '6': keycode = KEY_HID_RIGHT; break;
            case '8': keycode = KEY_HID_UP; break;
            case '.': keycode = KEY_HID_DELETE; break;
            case '/': keycode = KEY_HID_TAB; break;
            case '*': keycode = KEY_HID_BACKSPACE; break;
            case '=': keycode = KEY_NUMPAD_ENTER; break;
            default: return;
        }
    }

    Keyboard.pressRaw(keycode);
    delay(10);
    Keyboard.releaseRaw(keycode);
    Keyboard.releaseAll();  // guard against stuck modifier/key bits in the report
}


void hidUsbSendString(const String& str) {
    if (!usbStarted) return;
    for (int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        Keyboard.print(c);
        Keyboard.releaseAll();
        delay(10);
    }
}
