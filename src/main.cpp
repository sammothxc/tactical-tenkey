#define FIRMWARE_VERSION "0.1.0"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "icons.h"

#define SDA_PIN 22
#define SCL_PIN 23
#define WAKE_PIN 0  // Enter key

const uint8_t ROW_PINS[4] = {1, 2, 21, 16};
const uint8_t COL_PINS[4] = {18, 20, 19, 17};

const char KEYMAP[4][4] = {
    {'C', '/', '*', '-'},
    {'7', '8', '9', '+'},
    {'4', '5', '6', '.'},
    {'1', '2', '3', '0'}
};

const char WAKE_KEY = '=';

#define SLEEP_TIMEOUT 60000

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

void initMatrix();
char scanMatrix();
char scanWakeKey();
void handleKey(char key);
void drawTopBar();
void drawBottomBar();
void drawMainDisplay();
void updateDisplay();
String formatResult(double result);
double calculate(double a, double b, char op);
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
    
    for (int row = 0; row < 4; row++) {
        digitalWrite(ROW_PINS[row], LOW);
        delayMicroseconds(10);
        
        for (int col = 0; col < 4; col++) {
            if (digitalRead(COL_PINS[col]) == LOW) {
                char key = KEYMAP[row][col];
                if (key == '-') currentMinus = true;
                else if (key == '0') currentZero = true;
                else pressed = key;
            }
        }
        digitalWrite(ROW_PINS[row], HIGH);
    }
    
    // check for FN combo
    if (currentMinus && currentZero) {
        minusHeld = true;
        zeroHeld = true;
        return 'F';
    }
    
    // return individual key if no combo
    if (currentMinus && !zeroHeld) return '-';
    if (currentZero && !minusHeld) return '0';
    
    // reset hold states when released
    if (!currentMinus) minusHeld = false;
    if (!currentZero) zeroHeld = false;
    
    return pressed;
}


char scanWakeKey() {
    if (digitalRead(WAKE_PIN) == LOW) {
        return WAKE_KEY;
    }
    return 0;
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


void handleKey(char key) {
    lastActivity = millis();
    
    // FN combo: quick exit to normal calc mode
    if (key == 'F') {
        functionName = "";
        numpadMode = false;
        updateDisplay();
        return;
    }
    
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
        // progressive clear
        if (displayValue != "0" && !newEntry) {
            // first: clear current entry
            displayValue = "0";
            newEntry = true;
        } else if (storedValue.length() > 0 || pendingOp) {
            // second: clear operation
            storedValue = "";
            pendingOp = 0;
        } else if (functionName.length() > 0) {
            // third: clear function
            functionName = "";
        }
    }
    
    updateDisplay();
}


void drawTopBar() {
    u8g2.setFont(u8g2_font_6x10_tr);
    if (storedValue.length() > 0 && pendingOp) {
        String status = storedValue + " " + pendingOp;
        u8g2.drawStr(0, 10, status.c_str());
    }
}


vovoid drawBottomBar() {
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


void goToSleep() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 32, "Sleeping...");
    u8g2.sendBuffer();
    delay(500);
    
    u8g2.setPowerSave(1);
    
    esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKE_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
}


void setup() {
    Serial.begin(115200);
    
    pinMode(WAKE_PIN, INPUT_PULLUP);
    initMatrix();
    
    Wire.begin(SDA_PIN, SCL_PIN);
    u8g2.begin();
    u8g2.setContrast(255);
    
    lastActivity = millis();
    updateDisplay();
}


void loop() {
    char key = scanMatrix();
    if (!key) key = scanWakeKey();
    
    if (key && key != lastKey) {
        Serial.printf("Key: %c\n", key);
        handleKey(key);
    }
    lastKey = key;
    
    if (millis() - lastActivity > SLEEP_TIMEOUT) {
        goToSleep();
    }
    
    delay(20);
}