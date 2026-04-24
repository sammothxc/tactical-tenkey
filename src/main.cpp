#include <Arduino.h>
#include "version.h"

// changelog for this version (shown on major/minor bump)
const char* CHANGELOG[] = {
    "v0.1.0 - First Release",
    "",
    "* Calculator + numpad mode",
    "* 6 accounting macros",
    "* FN key combinations",
    "* USB HID support",
    "* Auto sleep/wake",
};
const uint8_t CHANGELOG_LINES = 7;

// guide pages (shown on first boot)
struct GuidePage {
    const char* const* lines;
    uint8_t lineCount;
};

static const char* GUIDE_NAVIGATION[] = {
    "NAVIGATION",
    "[4] Prev  [6] Next",
    "[8] Up    [2] Down",
    "[5]/[Enter] Select",
    "[NUM] Back",
    "Universal across",
    "all menus on this",
    "device.",
};
static const char* GUIDE_BASIC[] = {
    "BASIC KEYS",
    "Numbers and + - * /",
    "work like a normal",
    "calculator. [Enter]",
    "computes the result.",
    "[NUM] clears in",
    "steps: current entry,",
    "then pending op,",
    "then macro name.",
};
static const char* GUIDE_MACRO_MENU[] = {
    "MACRO MENU",
    "Tap [-]+[0] together",
    "and release to open",
    "the macro menu.",
    "In the menu:",
    "[8]/[2] scroll",
    "[5]/[Enter] select",
    "[NUM] cancel",
    "Macros then prompt",
    "for each input.",
};
static const char* GUIDE_NUMPAD[] = {
    "NUMPAD MODE",
    "Hold [-]+[/] to",
    "toggle USB numpad",
    "mode.",
    "[NUM] toggles NUM/NAV",
    "state, shown at the",
    "bottom-left of the",
    "display.",
    "NUM: digits + [.]",
    "NAV:",
    "[8/2] Up/Down",
    "[4/6] Left/Right",
    "[7/1] Home/End",
    "[9/3] PgUp/PgDn",
    "[0] Insert",
    "[.] Delete",
    "[/] Tab",
    "[*] Backspace",
    "[+ - = Enter] work",
    "in both modes.",
};
static const char* GUIDE_SEND[] = {
    "SEND ANSWER",
    "Hold [-]+[*] in",
    "calculator mode to",
    "type the current",
    "display to your",
    "computer via USB.",
    "Handy for pasting",
    "results into forms.",
};
static const char* GUIDE_SLEEP[] = {
    "AUTO SLEEP",
    "Device sleeps after",
    "60 seconds idle.",
    "Press [Enter] to",
    "wake.",
};
static const char* GUIDE_DONE[] = {
    "ALL SET",
    "Happy calculating!",
    "[Enter] to start.",
};

const GuidePage GUIDE_PAGES[] = {
    {GUIDE_NAVIGATION, sizeof(GUIDE_NAVIGATION) / sizeof(GUIDE_NAVIGATION[0])},
    {GUIDE_BASIC,      sizeof(GUIDE_BASIC)      / sizeof(GUIDE_BASIC[0])},
    {GUIDE_MACRO_MENU, sizeof(GUIDE_MACRO_MENU) / sizeof(GUIDE_MACRO_MENU[0])},
    {GUIDE_NUMPAD,     sizeof(GUIDE_NUMPAD)     / sizeof(GUIDE_NUMPAD[0])},
    {GUIDE_SEND,       sizeof(GUIDE_SEND)       / sizeof(GUIDE_SEND[0])},
    {GUIDE_SLEEP,      sizeof(GUIDE_SLEEP)      / sizeof(GUIDE_SLEEP[0])},
    {GUIDE_DONE,       sizeof(GUIDE_DONE)       / sizeof(GUIDE_DONE[0])},
};
const uint8_t GUIDE_PAGE_COUNT = sizeof(GUIDE_PAGES) / sizeof(GUIDE_PAGES[0]);

enum GuideAction {
    GUIDE_NEXT,
    GUIDE_PREV,
    GUIDE_SCROLL_UP,
    GUIDE_SCROLL_DOWN,
    GUIDE_SELECT,
    GUIDE_BACK,
};

#include <U8g2lib.h>
#include <Wire.h>
#include <Preferences.h>
#include "icons.h"
#include "macros.h"
#include "usbhid.h"
#include "driver/rtc_io.h"

#define SDA_PIN 5
#define LED_PIN 21
#define SCL_PIN 6
#define WAKE_PIN 1 // Enter key
#define SLEEP_TIMEOUT 60000

const uint8_t ROW_PINS[4] = {42, 2, 4, 43};
const uint8_t COL_PINS[4] = {9, 8, 7, 44};

const char WAKE_KEY = '=';
const char KEYMAP[4][4] = {
    {'C', '/', '*', '-'},
    {'7', '8', '9', '+'},
    {'4', '5', '6', '.'},
    {'1', '2', '3', '0'}
};

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

String displayValue = "0";
String storedValue = "";
char pendingOp = 0;
bool newEntry = true;
uint32_t lastActivity = 0;
char lastKey = 0;
bool bleConnected = false;
bool usbConnected = true;
bool lowBattery = false;
bool numpadMode = false;
bool numLockOn = true;
String functionName = "";
uint32_t messageUntil = 0;

// combo tracking
bool minusHeld = false;
bool zeroHeld = false;
bool fnHeld = false;
bool fnWasUsed = false; // track if we navigated while holding
bool fnSlashPressed = false; // for mode toggle
bool fnClearPressed = false; // for send answer

void initMatrix();
char scanMatrix();
char scanWakeKey();
void handleKey(char key);
void showBootScreen();
void introMode();
void drawMacroMenu();
void drawTopBar();
void drawBottomBar();
void drawMainDisplay();
void updateDisplay();
void setFunction(const char* name);
void clearFunction();
double calculate(double a, double b, char op);
String formatResult(double result);
void goToSleep();
void showGuide();
void showChangelog();
bool waitForEnter();
bool isKeyPressed(char target);
GuideAction waitForGuideNav(bool canScrollUp, bool canScrollDown);


void initMatrix() {
    for (int i = 0; i < 4; i++) {
        pinMode(ROW_PINS[i], OUTPUT);
        digitalWrite(ROW_PINS[i], HIGH);
    }
    for (int i = 0; i < 4; i++) {
        pinMode(COL_PINS[i], INPUT_PULLUP);
    }
}


char scanMatrix() {
    char pressed = 0;
    bool currentMinus = false;
    bool currentZero = false;
    bool currentEight = false;
    bool currentTwo = false;
    bool currentFive = false;
    bool currentSlash = false;
    bool currentStar = false;
    bool currentClear = false;

    for (int row = 0; row < 4; row++) {
        digitalWrite(ROW_PINS[row], LOW);
        delayMicroseconds(10);

        for (int col = 0; col < 4; col++) {
            if (digitalRead(COL_PINS[col]) == LOW) {
                char key = KEYMAP[row][col];
                if (key == '-') currentMinus = true;
                else if (key == '0') currentZero = true;
                else if (key == '8') currentEight = true;
                else if (key == '2') currentTwo = true;
                else if (key == '5') currentFive = true;
                else if (key == '/') currentSlash = true;
                else if (key == '*') currentStar = true;
                else if (key == 'C') currentClear = true;
                else pressed = key;
            }
        }
        digitalWrite(ROW_PINS[row], HIGH);
    }
    
    bool fnPressed = currentMinus && currentZero;

    // edge detection for minus/zero release-fire
    static bool prevMinus = false;
    static bool prevZero = false;
    static bool minusFnUsed = false;
    static bool zeroFnUsed = false;
    bool minusReleased = prevMinus && !currentMinus;
    bool zeroReleased  = prevZero  && !currentZero;
    prevMinus = currentMinus;
    prevZero  = currentZero;

    // while FN chord active, mark both keys as "used for FN"
    if (fnPressed) {
        minusFnUsed = true;
        zeroFnUsed = true;
    }

    // track FN tap: true while FN is held and nothing else has been pressed
    static bool wasFn = false;
    static bool fnComboFired = false;
    if (fnPressed && !wasFn) {
        wasFn = true;
        fnComboFired = false;
    }
    // any non-FN key pressed during FN cancels the tap
    if (fnPressed && (currentSlash || currentStar || currentClear
                      || currentFive || currentEight || currentTwo
                      || pressed != 0)) {
        fnComboFired = true;
    }

    // FN + slash = toggle numpad mode
    if (fnPressed && currentSlash && !fnSlashPressed) {
        fnSlashPressed = true;
        return 'T';
    }
    if (!currentSlash) fnSlashPressed = false;

    // FN + clear = send answer
    if (fnPressed && currentClear && !fnClearPressed) {
        fnClearPressed = true;
        return 'A';
    }
    if (!currentClear) fnClearPressed = false;

    // FN released: if it was a tap (no combos), open macro menu
    if (!fnPressed && wasFn) {
        wasFn = false;
        bool wasTap = !fnComboFired;
        fnComboFired = false;
        // consume release events so - and 0 don't fire on release
        if (minusReleased) { minusReleased = false; minusFnUsed = false; }
        if (zeroReleased)  { zeroReleased  = false; zeroFnUsed  = false; }
        if (wasTap && macro.state == MACRO_IDLE) {
            macroMenuOpen();
            return 'M';
        }
        return 0;
    }

    // - + / = toggle numpad/calc mode
    static bool minusSlashChord = false;
    if (currentMinus && currentSlash && !minusSlashChord) {
        minusSlashChord = true;
        minusFnUsed = true;
        return 'T';
    }
    if (!currentMinus || !currentSlash) minusSlashChord = false;

    // - + * = send answer (calc mode only)
    static bool minusStarChord = false;
    if (!numpadMode && currentMinus && currentStar && !minusStarChord) {
        minusStarChord = true;
        minusFnUsed = true;
        return 'A';
    }
    if (!currentMinus || !currentStar) minusStarChord = false;

    // minus/zero fire on release, and only if they weren't part of an FN chord
    if (minusReleased) {
        bool wasSolo = !minusFnUsed;
        minusFnUsed = false;
        if (wasSolo) return '-';
    }
    if (zeroReleased) {
        bool wasSolo = !zeroFnUsed;
        zeroFnUsed = false;
        if (wasSolo) return '0';
    }

    // other single keys fire on press (suppress / and * while - is held)
    if (currentEight) return '8';
    if (currentTwo) return '2';
    if (currentFive) return '5';
    if (currentSlash && !currentMinus) return '/';
    if (currentStar && !currentMinus) return '*';
    if (currentClear) return 'C';

    return pressed;
}


char scanWakeKey() {
    if (digitalRead(WAKE_PIN) == LOW) {
        return WAKE_KEY;
    }
    return 0;
}


void handleKey(char key) {
    lastActivity = millis();
    messageUntil = 0; // any keypress dismisses the RESULT SENT message

    // menu just opened
    if (key == 'M') {
        drawMacroMenu();
        return;
    }

    // while menu is open, keys are modal: 8/2 nav, 5 select, C cancel
    if (macro.state == MACRO_MENU) {
        if (key == '8') {
            macroMenuUp();
            drawMacroMenu();
            return;
        }
        if (key == '2') {
            macroMenuDown();
            drawMacroMenu();
            return;
        }
        if (key == '5' || key == '=') {
            macroMenuSelect();
            functionName = macro.functionName;
            displayValue = "0";
            newEntry = true;
            updateDisplay();
            return;
        }
        if (key == 'C') {
            macro.state = MACRO_IDLE;
            updateDisplay();
            return;
        }
        return; // ignore other keys while menu is open
    }

    // toggle numpad/calc mode
    if (key == 'T') {
        numpadMode = !numpadMode;
        if (numpadMode) {
            hidInit();
            functionName = "";
            displayValue = "0";
            storedValue = "";
            pendingOp = 0;
            newEntry = true;
        }
        updateDisplay();
        return;
    }

    // send answer to computer
    if (key == 'A') {
        hidInit();
        hidSendString(displayValue);
        messageUntil = millis() + 5000;
        updateDisplay();
        return;
    }
    
    // NUMPAD MODE - send keys to computer, don't touch display
    if (numpadMode) {
        if (key == 'C') {
            numLockOn = !numLockOn;
            updateDisplay();
            return;
        }
        hidSendKey(key, numLockOn);
        return;
    }
    
    // macro input
    if (macro.state == MACRO_AWAITING_INPUT && key == '=') {
        double value = displayValue.toDouble();
        
        if (macroInput(value)) {
            displayValue = formatResult(macro.result);
            macro.state = MACRO_IDLE;
        } else {
            displayValue = "0";
        }
        newEntry = true;
        updateDisplay();
        return;
    }
    
    // normal calculator keys
    if (key >= '0' && key <= '9') {
        if (newEntry) {
            displayValue = key;
            newEntry = false;
        } else {
            if (displayValue.length() < 13) {
                displayValue += key;
            }
        }
    }
    else if (key == '.') {
        if (newEntry) {
            displayValue = "0.";
            newEntry = false;
        } else if (displayValue.indexOf('.') == -1) {
            displayValue += '.';
        }
    }
    else if (key == '+' || key == '-' || key == '*' || key == '/') {
        if (newEntry && storedValue.length() > 0 && pendingOp) {
            // no new operand typed yet — just swap the operator
            pendingOp = key;
        } else {
            if (storedValue.length() > 0 && pendingOp && !newEntry) {
                double a = storedValue.toDouble();
                double b = displayValue.toDouble();
                double result = calculate(a, b, pendingOp);
                displayValue = formatResult(result);
            }
            storedValue = displayValue;
            pendingOp = key;
            displayValue = "";
            newEntry = true;
        }
    }
    else if (key == '=') {
        if (storedValue.length() > 0 && pendingOp) {
            double a = storedValue.toDouble();
            double b = displayValue.toDouble();
            double result = calculate(a, b, pendingOp);
            displayValue = formatResult(result);
            storedValue = "";
            pendingOp = 0;
            newEntry = true;
        }
    }
    else if (key == 'C') {
        if (displayValue.length() > 0) {
            displayValue = "";
            newEntry = true;
        } else if (storedValue.length() > 0 || pendingOp) {
            storedValue = "";
            pendingOp = 0;
        } else if (functionName.length() > 0) {
            functionName = "";
        }
    }
    
    updateDisplay();
}


void showBootScreen() {
    u8g2.clearBuffer();
    u8g2.drawXBM(1, 1, LOGO_WIDTH, LOGO_HEIGHT, LOGO);
    u8g2.setFont(u8g2_font_logisoso16_tr);
    u8g2.drawStr(55, 17, "Tactical");
    u8g2.drawStr(59, 37, "Tenkey");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(68, 50, "v " FW_VERSION);
    u8g2.sendBuffer();
    delay(2000);
}


void welcomeText() {
    u8g2.drawStr(10, 64, "Press [Enter] to start");
    u8g2.sendBuffer();
    waitForEnter();
}


void drawMacroMenu() {
    u8g2.clearBuffer();
    
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "SELECT MACRO");
    
    // show 3 items centered on current selection
    int startIdx = menuIndex - 1;
    if (startIdx < 0) startIdx = 0;
    if (startIdx > MACRO_COUNT - 3) startIdx = MACRO_COUNT - 3;
    if (MACRO_COUNT <= 3) startIdx = 0;
    
    for (int i = 0; i < 3 && (startIdx + i) < MACRO_COUNT; i++) {
        int idx = startIdx + i;
        int y = 28 + (i * 14);
        
        if (idx == menuIndex) {
            // highlight selected
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(4, y, MACRO_NAMES[idx]);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(4, y, MACRO_NAMES[idx]);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[8][2]Nav [5]Sel [NUM]Bk");
    u8g2.sendBuffer();
}


void drawTopBar() {
    u8g2.setFont(u8g2_font_6x10_tr);
    
    if (macro.state == MACRO_AWAITING_INPUT) {
        u8g2.drawStr(0, 10, macroGetPrompt());
    }
    else if (storedValue.length() > 0 && pendingOp) {
        String status = storedValue + " " + pendingOp;
        u8g2.drawStr(0, 10, status.c_str());
    }
}


void drawBottomBar() {
    u8g2.setFont(u8g2_font_5x7_tr);
    
    int16_t iconY = 53;  // 64 - 11 = 53
    int16_t iconX = 128;
    
    // rightmost: BLE or USB or blank
    if (bleConnected) {
        iconX -= ICON_WIDTH;
        u8g2.drawXBM(iconX, iconY, ICON_WIDTH, ICON_HEIGHT, ICON_BLE);
    } else if (usbConnected) {
        iconX -= ICON_WIDTH;
        u8g2.drawXBM(iconX, iconY, ICON_WIDTH, ICON_HEIGHT, ICON_USB);
    } else {
        iconX -= ICON_WIDTH;  // blank space
    }
    
    // numpad mode or blank
    iconX -= 2;  // spacing
    if (numpadMode) {
        iconX -= ICON_WIDTH;
        u8g2.drawXBM(iconX, iconY, ICON_WIDTH, ICON_HEIGHT, ICON_NUMPAD);
    } else {
        iconX -= ICON_WIDTH;  // blank
    }
    
    // battery low or blank
    iconX -= 2;
    if (lowBattery) {
        iconX -= ICON_WIDTH;
        u8g2.drawXBM(iconX, iconY, ICON_WIDTH, ICON_HEIGHT, ICON_LOWBATT);
    } else {
        iconX -= ICON_WIDTH;  // blank
    }
    
    // leftmost: RESULT SENT flash, NUM/NAV in numpad mode, else function name
    if (messageUntil > 0 && millis() < messageUntil) {
        u8g2.drawStr(0, 64, "RESULT SENT");
    } else if (numpadMode) {
        u8g2.drawStr(0, 64, numLockOn ? "NUM" : "NAV");
    } else if (functionName.length() > 0) {
        u8g2.drawStr(0, 64, functionName.c_str());
    }
}


void drawMainDisplay() {
    int len = displayValue.length();
    int16_t y;
    if (len <= 6) {
        u8g2.setFont(u8g2_font_logisoso32_tn);
        y = 50;
    } else if (len <= 8) {
        u8g2.setFont(u8g2_font_logisoso24_tn);
        y = 47;
    } else if (len <= 10) {
        u8g2.setFont(u8g2_font_logisoso20_tn);
        y = 45;
    } else {
        u8g2.setFont(u8g2_font_logisoso16_tn);
        y = 43;
    }
    int16_t width = u8g2.getStrWidth(displayValue.c_str());
    int16_t x = 128 - width - 2;
    if (x < 0) x = 0;
    u8g2.drawStr(x, y, displayValue.c_str());
}


void updateDisplay() {
    u8g2.clearBuffer();
    drawTopBar();
    drawMainDisplay();
    drawBottomBar();
    u8g2.sendBuffer();
}


void setFunction(const char* name) {
    functionName = name;
    updateDisplay();
}


void clearFunction() {
    functionName = "";
    updateDisplay();
}


double calculate(double a, double b, char op) {
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return b != 0 ? a / b : 0;
        default: return b;
    }
}


String formatResult(double result) {
    String out = String(result, 6);
    while (out.endsWith("0") && out.indexOf('.') != -1) {
        out.remove(out.length() - 1);
    }
    if (out.endsWith(".")) {
        out.remove(out.length() - 1);
    }
    return out;
}


void goToSleep() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 32, "Sleeping...");
    u8g2.sendBuffer();
    delay(500);
    u8g2.setPowerSave(1);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, 0); // wake on LOW
    esp_deep_sleep_start();
}


bool waitForEnter() {
    // wait for enter key press (with debounce)
    while (true) {
        if (digitalRead(WAKE_PIN) == LOW) {
            delay(200);  // debounce
            while (digitalRead(WAKE_PIN) == LOW) delay(10);  // wait release
            return true;
        }
        delay(20);
    }
}


bool isKeyPressed(char target) {
    bool found = false;
    for (int row = 0; row < 4; row++) {
        digitalWrite(ROW_PINS[row], LOW);
        delayMicroseconds(10);
        for (int col = 0; col < 4; col++) {
            if (digitalRead(COL_PINS[col]) == LOW && KEYMAP[row][col] == target) {
                found = true;
            }
        }
        digitalWrite(ROW_PINS[row], HIGH);
    }
    return found;
}


GuideAction waitForGuideNav(bool canScrollUp, bool canScrollDown) {
    while (true) {
        if (digitalRead(WAKE_PIN) == LOW) {
            delay(50);
            while (digitalRead(WAKE_PIN) == LOW) delay(10);
            return GUIDE_SELECT;
        }
        if (isKeyPressed('6')) {
            delay(50);
            while (isKeyPressed('6')) delay(10);
            return GUIDE_NEXT;
        }
        if (isKeyPressed('4')) {
            delay(50);
            while (isKeyPressed('4')) delay(10);
            return GUIDE_PREV;
        }
        if (canScrollUp && isKeyPressed('8')) {
            delay(50);
            while (isKeyPressed('8')) delay(10);
            return GUIDE_SCROLL_UP;
        }
        if (canScrollDown && isKeyPressed('2')) {
            delay(50);
            while (isKeyPressed('2')) delay(10);
            return GUIDE_SCROLL_DOWN;
        }
        if (isKeyPressed('5')) {
            delay(50);
            while (isKeyPressed('5')) delay(10);
            return GUIDE_SELECT;
        }
        if (isKeyPressed('C')) {
            delay(50);
            while (isKeyPressed('C')) delay(10);
            return GUIDE_BACK;
        }
        delay(20);
    }
}


void showGuide() {
    const int visibleLines = 4;
    int page = 0;
    int scroll = 0;
    while (page < GUIDE_PAGE_COUNT) {
        int lineCount = GUIDE_PAGES[page].lineCount;
        int maxScroll = lineCount > visibleLines ? lineCount - visibleLines : 0;
        if (scroll > maxScroll) scroll = maxScroll;
        if (scroll < 0) scroll = 0;

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        for (int line = 0; line < visibleLines && (scroll + line) < lineCount; line++) {
            const char* text = GUIDE_PAGES[page].lines[scroll + line];
            if (strlen(text) > 0) {
                u8g2.drawStr(0, 12 + (line * 12), text);
            }
        }

        // scroll arrows (top-right / above progress dots)
        if (scroll > 0) {
            u8g2.drawTriangle(123, 0, 119, 4, 127, 4);
        }
        if (scroll < maxScroll) {
            u8g2.drawTriangle(123, 57, 119, 53, 127, 53);
        }

        // progress dots at bottom
        u8g2.setFont(u8g2_font_5x7_tr);
        int dotsWidth = GUIDE_PAGE_COUNT * 6;
        int dotsX = (128 - dotsWidth) / 2;
        for (int i = 0; i < GUIDE_PAGE_COUNT; i++) {
            if (i == page) {
                u8g2.drawDisc(dotsX + (i * 6) + 2, 62, 2);
            } else {
                u8g2.drawCircle(dotsX + (i * 6) + 2, 62, 2);
            }
        }
        u8g2.sendBuffer();
        lastActivity = millis();

        GuideAction action = waitForGuideNav(scroll > 0, scroll < maxScroll);
        switch (action) {
            case GUIDE_NEXT:
            case GUIDE_SELECT:
                page++;
                scroll = 0;
                break;
            case GUIDE_PREV:
                if (page > 0) { page--; scroll = 0; }
                break;
            case GUIDE_BACK:
                if (page > 0) { page--; scroll = 0; }
                else return;
                break;
            case GUIDE_SCROLL_UP:
                if (scroll > 0) scroll--;
                break;
            case GUIDE_SCROLL_DOWN:
                if (scroll < maxScroll) scroll++;
                break;
        }
    }
}


void showChangelog() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "What's New:");
    u8g2.drawHLine(0, 12, 128);
    
    u8g2.setFont(u8g2_font_5x7_tr);
    for (int i = 0; i < CHANGELOG_LINES && i < 6; i++) {
        u8g2.drawStr(0, 24 + (i * 8), CHANGELOG[i]);
    }
    
    u8g2.drawStr(30, 64, "[=] Continue");
    u8g2.sendBuffer();
    lastActivity = millis();
    waitForEnter();
}


void setup() {
    pinMode(WAKE_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    initMatrix();
    Wire.begin(SDA_PIN, SCL_PIN);
    u8g2.begin();
    u8g2.setContrast(255);
    u8g2.setFlipMode(1); // rotate display 180 degrees
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup == ESP_SLEEP_WAKEUP_UNDEFINED) {
        Preferences prefs;
        prefs.begin("t2", false);
        
        bool firstBoot = prefs.getBool("guided", false) == false;
        String lastRelease = prefs.getString("fwRel", "");
#ifdef FORCE_FIRST_BOOT
        firstBoot = true;
#endif
        
        if (firstBoot) {
            showBootScreen();
            welcomeText();
            showGuide();
            prefs.putBool("guided", true);
            prefs.putString("fwRel", FW_RELEASE);
        } else if (lastRelease != FW_RELEASE) {
            showBootScreen();
            showChangelog();
            prefs.putString("fwRel", FW_RELEASE);
        } else {
            showBootScreen();
        }
        
        prefs.end();
    }
    lastActivity = millis();
    updateDisplay();
}


void loop() {
    char key = scanMatrix();
    if (!key) key = scanWakeKey();
    
    if (key && key != lastKey) {
        handleKey(key);
    }
    lastKey = key;

    // clear the RESULT SENT message once its window expires
    if (messageUntil > 0 && millis() >= messageUntil) {
        messageUntil = 0;
        updateDisplay();
    }

    if (!numpadMode && millis() - lastActivity > SLEEP_TIMEOUT) {
        goToSleep();
    }
    delay(20);
}