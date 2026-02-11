#include "usbhid.h"

USBHIDKeyboard Keyboard;
bool hidInitialized = false;

#define KEY_NUMPAD_0 0xEA
#define KEY_NUMPAD_1 0xE1
#define KEY_NUMPAD_2 0xE2
#define KEY_NUMPAD_3 0xE3
#define KEY_NUMPAD_4 0xE4
#define KEY_NUMPAD_5 0xE5
#define KEY_NUMPAD_6 0xE6
#define KEY_NUMPAD_7 0xE7
#define KEY_NUMPAD_8 0xE8
#define KEY_NUMPAD_9 0xE9
#define KEY_NUMPAD_DOT 0xEB
#define KEY_NUMPAD_ENTER 0xE0
#define KEY_NUMPAD_PLUS 0xDF
#define KEY_NUMPAD_MINUS 0xDE
#define KEY_NUMPAD_MULT 0xDD
#define KEY_NUMPAD_DIV 0xDC


void hidInit() {
    if (!hidInitialized) {
        Keyboard.begin();
        USB.begin();
        hidInitialized = true;
    }
}


void hidSendNumpadKey(char key) {
    if (!hidInitialized) return;
    
    uint8_t keycode = 0;
    
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
        case 'C': keycode = KEY_BACKSPACE; break;
        default: return;
    }
    
    Keyboard.pressRaw(keycode);
    delay(10);
    Keyboard.releaseRaw(keycode);
}


void hidSendString(const String& str) {
    if (!hidInitialized) return;
    
    for (int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        Keyboard.print(c);
        delay(10);
    }
}


void hidSendKey(char key) {
    hidSendNumpadKey(key);
}