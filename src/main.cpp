#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#define SDA_PIN 22
#define SCL_PIN 23
#define WAKE_PIN 0  // Enter key

const uint8_t ROW_PINS[4] = {1, 2, 21, 16};
const uint8_t COL_PINS[4] = {18, 20, 19, 17};

/*
Physical Layout:

[ C ][ / ][ * ][ - ]
[ 7 ][ 8 ][ 9 ][ + ]
[ 4 ][ 5 ][ 6 ][ _ ]
[ 1 ][ 2 ][ 3 ][ = ]
[   0    ][ . ][ _ ]

*/

const char KEYMAP[4][4] = {
    {'C', '/', '*', '-'},
    {'7', '8', '9', '+'},
    {'4', '5', '6', '.'},
    {'1', '2', '3', '0'}
};

const char WAKE_KEY = '='; // Enter key

#define SLEEP_TIMEOUT 60000 // 1 min

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

void initMatrix();
char scanMatrix();
char scanWakeKey();
void handleKey(char key);
void drawTopBar();
void drawBottomBar();
void drawMainDisplay();
void updateDisplay();
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
    for (int row = 0; row < 4; row++) {
        digitalWrite(ROW_PINS[row], LOW);
        delayMicroseconds(10);
        
        for (int col = 0; col < 4; col++) {
            if (digitalRead(COL_PINS[col]) == LOW) {
                digitalWrite(ROW_PINS[row], HIGH);
                return KEYMAP[row][col];
            }
        }
        digitalWrite(ROW_PINS[row], HIGH);
    }
    return 0;
}


char scanWakeKey() {
    if (digitalRead(WAKE_PIN) == LOW) {
        return WAKE_KEY;
    }
    return 0;
}


void handleKey(char key) {
    lastActivity = millis();
    
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
            displayValue = String(result, 6);
            // trim trailing zeros
            while (displayValue.endsWith("0") && displayValue.indexOf('.') != -1) {
                displayValue.remove(displayValue.length() - 1);
            }
            if (displayValue.endsWith(".")) {
                displayValue.remove(displayValue.length() - 1);
            }
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
            displayValue = String(result, 6);
            // trim trailing zeros
            while (displayValue.endsWith("0") && displayValue.indexOf('.') != -1) {
                displayValue.remove(displayValue.length() - 1);
            }
            if (displayValue.endsWith(".")) {
                displayValue.remove(displayValue.length() - 1);
            }
            storedValue = "";
            pendingOp = 0;
            newEntry = true;
        }
    }
    else if (key == 'C') {
        displayValue = "0";
        storedValue = "";
        pendingOp = 0;
        newEntry = true;
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

void drawBottomBar() {
    u8g2.setFont(u8g2_font_5x7_tr);
    
    // left: function name or mode
    if (functionName.length() > 0) {
        u8g2.drawStr(0, 64, functionName.c_str());
    } else {
        u8g2.drawStr(0, 64, numpadMode ? "NUM" : "CALC");
    }
    
    // right: status icons
    int16_t iconX = 128;
    
    if (lowBattery) {
        iconX -= 18;
        u8g2.drawStr(iconX, 64, "LOW");
    }
    
    if (bleConnected) {
        iconX -= 12;
        u8g2.drawStr(iconX, 64, "BT");
    } else if (usbConnected) {
        iconX -= 18;
        u8g2.drawStr(iconX, 64, "USB");
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
    
    // debounce: only trigger on new press
    if (key && key != lastKey) {
        Serial.printf("Key: %c\n", key);
        handleKey(key);
    }
    lastKey = key;
    
    // sleep after timeout
    if (millis() - lastActivity > SLEEP_TIMEOUT) {
        goToSleep();
    }
    
    delay(20);  // scan rate ~50Hz
}
