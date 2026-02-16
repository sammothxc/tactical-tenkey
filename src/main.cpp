#define FW_VERSION "0.1.0"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "icons.h"
#include "macros.h"
#include "usbhid.h"
#include "driver/rtc_io.h"

#define SDA_PIN 5
#define SCL_PIN 6
#define WAKE_PIN 1  // Enter key
#define SLEEP_TIMEOUT 60000

const uint8_t ROW_PINS[4] = {2, 3, 4, 43};
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
String functionName = "";

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
                else if (key == 'C') currentClear = true;
                else pressed = key;
            }
        }
        digitalWrite(ROW_PINS[row], HIGH);
    }
    
    bool fnPressed = currentMinus && currentZero;
    
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
    
    // FN + 5 = open menu
    static bool fnFivePressed = false;
    if (fnPressed && currentFive && !fnFivePressed) {
        fnFivePressed = true;
        fnHeld = true;
        fnWasUsed = false;
        macroMenuOpen();
        return 'M';
    }
    if (!currentFive) fnFivePressed = false;
    
    // FN held (after menu opened) - check for navigation
    if (fnPressed && fnHeld) {
        if (currentEight) {
            fnWasUsed = true;
            return 'U';
        }
        if (currentTwo) {
            fnWasUsed = true;
            return 'D';
        }
        return 0;
    }
    
    // FN released while menu open
    if (!fnPressed && fnHeld) {
        fnHeld = false;
        if (macro.state == MACRO_MENU) {
            macroMenuSelect();
            return 'S';
        }
        return 0;
    }
    
    // reset combo tracking
    if (!currentMinus) minusHeld = false;
    if (!currentZero) zeroHeld = false;
    
    // normal single keys (only if FN not held)
    if (!fnHeld) {
        if (currentMinus && !zeroHeld) return '-';
        if (currentZero && !minusHeld) return '0';
        if (currentEight) return '8';
        if (currentTwo) return '2';
        if (currentFive) return '5';
        if (currentSlash) return '/';
        if (currentClear) return 'C';
    }
    
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
        // brief visual feedback
        String temp = displayValue;
        displayValue = "SENT";
        updateDisplay();
        delay(300);
        displayValue = temp;
        updateDisplay();
        return;
    }
    
    // cancel menu
    if (key == 'X') {
        macro.state = MACRO_IDLE;
        updateDisplay();
        return;
    }
    
    // menu controls
    if (key == 'M') {
        drawMacroMenu();
        return;
    }
    if (key == 'U' && macro.state == MACRO_MENU) {
        macroMenuUp();
        drawMacroMenu();
        return;
    }
    if (key == 'D' && macro.state == MACRO_MENU) {
        macroMenuDown();
        drawMacroMenu();
        return;
    }
    if (key == 'S') {
        functionName = macro.functionName;
        displayValue = "0";
        newEntry = true;
        updateDisplay();
        return;
    }
    
    // FN combo when not in menu: quick exit
    if (key == 'F') {
        macroCancel();
        functionName = "";
        numpadMode = false;
        updateDisplay();
        return;
    }
    
    // NUMPAD MODE - send keys to computer
    if (numpadMode) {
        hidSendKey(key);
        // still show on display for feedback
        if (key >= '0' && key <= '9') {
            if (newEntry) {
                displayValue = key;
                newEntry = false;
            } else if (displayValue.length() < 10) {
                displayValue += key;
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
        else if (key == '=' || key == 'C') {
            displayValue = "0";
            newEntry = true;
        }
        else if (key == '+' || key == '-' || key == '*' || key == '/') {
            displayValue = "0";
            newEntry = true;
        }
        updateDisplay();
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
            if (displayValue.length() < 10) {
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
        if (storedValue.length() > 0 && pendingOp && !newEntry) {
            double a = storedValue.toDouble();
            double b = displayValue.toDouble();
            double result = calculate(a, b, pendingOp);
            displayValue = formatResult(result);
        }
        storedValue = displayValue;
        pendingOp = key;
        newEntry = true;
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
        if (displayValue != "0" && !newEntry) {
            displayValue = "0";
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
    u8g2.setFont(u8g2_font_logisoso16_tr);
    u8g2.drawStr(10, 25, "Tactical");
    u8g2.drawStr(10, 45, "Tenkey");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(100, 64, "v " FW_VERSION);
    u8g2.sendBuffer();
    delay(1000);
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
    u8g2.drawStr(0, 64, "[8] Up  [2] Down [Release FN] Select");
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
    
    // leftmost: function name
    if (functionName.length() > 0) {
        u8g2.drawStr(0, 64, functionName.c_str());
    }
}


void drawMainDisplay() {
    u8g2.setFont(u8g2_font_logisoso32_tn);
    int16_t width = u8g2.getStrWidth(displayValue.c_str());
    int16_t x = 128 - width - 2;
    if (x < 0) x = 0;
    u8g2.drawStr(x, 50, displayValue.c_str());
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



void setup() {
    pinMode(WAKE_PIN, INPUT_PULLUP);
    initMatrix();
    Wire.begin(SDA_PIN, SCL_PIN);
    u8g2.begin();
    u8g2.setContrast(255);
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup == ESP_SLEEP_WAKEUP_UNDEFINED) {
        showBootScreen();
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
    
    if (millis() - lastActivity > SLEEP_TIMEOUT) {
        goToSleep();
    }
    delay(20);
}