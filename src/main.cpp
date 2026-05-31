#include <Arduino.h>
#include "version.h"

// changelog for this version, shown on major/minor bump
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

// guide pages, shown on first boot
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
    "Tap [-]+[5] together",
    "and release to open",
    "the macro menu.",
    "[4]/[6] page between",
    "Macros and Settings.",
    "In the menu:",
    "[8]/[2] scroll",
    "[5]/[Enter] select",
    "[NUM] cancel",
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
    "[=] Enter (both)",
    "[+/-] Zoom in/out",
    "(nav mode only)",
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
    "5 minutes idle.",
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
#include "hid.h"
#include "hid_ble.h"
#include "driver/rtc_io.h"

#define SDA_PIN 5
#define LED_PIN 21
#define SCL_PIN 6
#define WAKE_PIN 1 // Enter key
#define DEFAULT_SLEEP_TIMEOUT 300000

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

enum MenuPage {
    MENU_PAGE_MACROS = 0,
    MENU_PAGE_SETTINGS,
    MENU_PAGE_COUNT
};
uint8_t menuPage = MENU_PAGE_MACROS;

// persistent settings
uint32_t sleepTimeoutMs = DEFAULT_SLEEP_TIMEOUT;
uint8_t oledContrast = 255;
uint8_t ledBrightness = 255;
uint8_t zoomModifier = 0;  // 0 = Ctrl (Windows/Linux), 1 = Cmd/GUI (macOS)

// settings page sub-views
enum SettingsView {
    SETTINGS_VIEW_LIST = 0,
    SETTINGS_VIEW_TIMEOUT,
    SETTINGS_VIEW_CONTRAST,
    SETTINGS_VIEW_BRIGHTNESS,
    SETTINGS_VIEW_FW_INFO,
    SETTINGS_VIEW_QBIND_LIST,
    SETTINGS_VIEW_QBIND_PICK,
    SETTINGS_VIEW_RESET_CONFIRM,
    SETTINGS_VIEW_BT,
    SETTINGS_VIEW_BT_BONDS,
    SETTINGS_VIEW_BT_BOND,
    SETTINGS_VIEW_BT_BOND_OS,
    SETTINGS_VIEW_BT_FORGET,
    SETTINGS_VIEW_ZOOM_PICK
};
SettingsView settingsView = SETTINGS_VIEW_LIST;
uint8_t settingsIndex = 0;
String settingsInput = "";

// BLE state
enum BleMode {
    BLE_MODE_OFF = 0,
    BLE_MODE_ADVERTISING,
    BLE_MODE_PAIRING,
    BLE_MODE_CONNECTED
};
BleMode bleMode = BLE_MODE_OFF;
uint32_t bleModeUntil = 0;
const uint32_t BLE_PAIRING_WINDOW_MS = 180000;
const uint32_t BLE_ADVERTISE_WINDOW_MS = 180000;
uint8_t btMenuIdx = 0;
const char* BT_ITEM_NAMES[] = {
    "Pair New Device",
    "Reconnect",
    "Paired Devices",
    "Forget All Bonds"
};
const uint8_t BT_ITEM_COUNT = sizeof(BT_ITEM_NAMES) / sizeof(BT_ITEM_NAMES[0]);

// bonds list cache (filled on entering BT_BONDS view to avoid re-init churn)
#define BT_BOND_MAX 9
String btBondAddrs[BT_BOND_MAX];
uint8_t btBondCount = 0;
uint8_t btBondIdx = 0;

// per-bond metadata: maps a peer MAC to its OS (0 = Linux/Win → Ctrl, 1 = macOS → Cmd)
struct BondMeta {
    uint8_t mac[6];
    uint8_t os;
};
BondMeta bondMetaList[BT_BOND_MAX];
uint8_t bondMetaCount = 0;
uint8_t btBondActionIdx = 0;  // selection within BT_BOND view (0 = Set OS, 1 = Forget)

// macro quick bind: slot index holds a macro index, -1 = unbound
// (slot 5 unused: -+5 is the FN chord)
int8_t qbindSlots[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
const uint8_t QBIND_VALID_SLOTS[9] = {0, 1, 2, 3, 4, 6, 7, 8, 9};
uint8_t qbindListIdx = 0;     // selection within QBIND_LIST (0..8 -> QBIND_VALID_SLOTS[idx])
uint8_t qbindEditSlot = 0;    // slot being edited in QBIND_PICK
uint8_t qbindPickIdx = 0;     // selection in QBIND_PICK (0 = None, 1..MACRO_COUNT = macro)

const char* SETTINGS_NAMES[] = {
    "Sleep Timeout",
    "Contrast",
    "Brightness",
    "Quick Bind",
    "Bluetooth",
    "Host OS",
    "Show Guide",
    "FW Info",
    "Factory Reset"
};
const uint8_t SETTINGS_COUNT = sizeof(SETTINGS_NAMES) / sizeof(SETTINGS_NAMES[0]);

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
void drawMenu();
void drawMenuHeader(const char* title);
void drawMacroPage();
void drawSettingsPage();
void saveSettings();
void factoryReset();
void handleSettingsKey(char key);
void bleStartAdvertising();
void bleStartPairing();
void bleShutdown();
void blePoll();
String bleStatusLine();
static bool parseMacString(const String& s, uint8_t* out);
static int findBondMetaByMac(const uint8_t* mac);
static uint8_t getBondOSByMac(const uint8_t* mac);
static void setBondOSByMac(const uint8_t* mac, uint8_t os);
static void removeBondMetaByMac(const uint8_t* mac);
static void applyConnectedPeerOS();
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

    bool fnPressed = currentMinus && currentFive;

    // edge detection for minus/five release-fire
    static bool prevMinus = false;
    static bool prevFive = false;
    static bool minusFnUsed = false;
    static bool fiveFnUsed = false;
    bool minusReleased = prevMinus && !currentMinus;
    bool fiveReleased  = prevFive  && !currentFive;
    bool fiveJustPressed = currentFive && !prevFive;
    prevMinus = currentMinus;
    prevFive  = currentFive;

    // while FN chord active, mark both keys as "used for FN"
    if (fnPressed) {
        minusFnUsed = true;
        fiveFnUsed = true;
    }

    // track FN tap: true while FN is held and nothing else has been pressed
    static bool wasFn = false;
    static bool fnComboFired = false;
    if (fnPressed && !wasFn) {
        wasFn = true;
        fnComboFired = false;
    }
    // any non-FN key pressed during FN cancels the tap
    // (0 lives in `pressed`; 8/2 are tracked specially)
    if (fnPressed && (currentSlash || currentStar || currentClear
                      || currentEight || currentTwo
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
        // consume release events so - and 5 don't fire on release
        if (minusReleased) { minusReleased = false; minusFnUsed = false; }
        if (fiveReleased)  { fiveReleased  = false; fiveFnUsed  = false; }
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

    // FN + digit (0-9 except 5) = quick-bind macro trigger
    // (8/2 tracked separately; 0 lives in `pressed`; 5 is part of FN itself)
    if (fnPressed) {
        char qbDigit = 0;
        if (pressed >= '0' && pressed <= '9') qbDigit = pressed;
        else if (currentEight) qbDigit = '8';
        else if (currentTwo)   qbDigit = '2';

        if (qbDigit && qbDigit != '5') {
            return (char)(0x10 + (qbDigit - '0'));  // 0x10..0x19, skipping 0x15
        }
    }

    // minus/five fire on release, and only if they weren't part of an FN chord
    if (minusReleased) {
        bool wasSolo = !minusFnUsed;
        minusFnUsed = false;
        if (wasSolo) return '-';
    }
    // five fires on press when minus is not held — no FN chord is possible
    static bool fiveFiredOnPress = false;
    if (fiveJustPressed && !currentMinus) {
        fiveFiredOnPress = true;
        return '5';
    }
    if (fiveReleased) {
        bool suppressRelease = fiveFiredOnPress;
        fiveFiredOnPress = false;
        bool wasSolo = !fiveFnUsed;
        fiveFnUsed = false;
        if (wasSolo && !suppressRelease) return '5';
    }

    // other single keys fire on press (suppress / and * while - is held)
    if (currentEight) return '8';
    if (currentTwo) return '2';
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
        menuPage = MENU_PAGE_MACROS;
        settingsView = SETTINGS_VIEW_LIST;
        settingsInput = "";
        drawMenu();
        return;
    }

    // while menu is open, keys are modal: 4/6 page, 8/2 nav, 5 select, C cancel
    if (macro.state == MACRO_MENU) {
        // settings sub-views consume all input (C backs to settings list)
        if (menuPage == MENU_PAGE_SETTINGS && settingsView != SETTINGS_VIEW_LIST) {
            handleSettingsKey(key);
            return;
        }

        if (key == '4') {
            menuPage = (menuPage + MENU_PAGE_COUNT - 1) % MENU_PAGE_COUNT;
            drawMenu();
            return;
        }
        if (key == '6') {
            menuPage = (menuPage + 1) % MENU_PAGE_COUNT;
            drawMenu();
            return;
        }
        if (key == 'C') {
            macro.state = MACRO_IDLE;
            settingsView = SETTINGS_VIEW_LIST;
            settingsInput = "";
            updateDisplay();
            return;
        }

        if (menuPage == MENU_PAGE_MACROS) {
            if (key == '8') {
                macroMenuUp();
                drawMenu();
                return;
            }
            if (key == '2') {
                macroMenuDown();
                drawMenu();
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
        }
        else if (menuPage == MENU_PAGE_SETTINGS) {
            if (key == '8') {
                if (settingsIndex > 0) settingsIndex--;
                drawMenu();
                return;
            }
            if (key == '2') {
                if (settingsIndex < SETTINGS_COUNT - 1) settingsIndex++;
                drawMenu();
                return;
            }
            if (key == '5' || key == '=') {
                switch (settingsIndex) {
                    case 0: // Sleep Timeout
                        settingsView = SETTINGS_VIEW_TIMEOUT;
                        settingsInput = "";
                        break;
                    case 1: // Contrast
                        settingsView = SETTINGS_VIEW_CONTRAST;
                        break;
                    case 2: // Brightness
                        settingsView = SETTINGS_VIEW_BRIGHTNESS;
                        break;
                    case 3: // Quick Bind
                        settingsView = SETTINGS_VIEW_QBIND_LIST;
                        break;
                    case 4: // Bluetooth
                        settingsView = SETTINGS_VIEW_BT;
                        break;
                    case 5: // Host OS
                        settingsView = SETTINGS_VIEW_ZOOM_PICK;
                        break;
                    case 6: // Show Guide
                        showGuide();
                        break;
                    case 7: // FW Info
                        settingsView = SETTINGS_VIEW_FW_INFO;
                        break;
                    case 8: // Factory Reset
                        settingsView = SETTINGS_VIEW_RESET_CONFIRM;
                        break;
                }
                drawMenu();
                return;
            }
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

    // quick-bind macro trigger: FN+digit (0-9 except 5)
    if (key >= 0x10 && key <= 0x19) {
        if (numpadMode || macro.state != MACRO_IDLE) return;
        uint8_t slot = key - 0x10;
        if (slot == 5) return;
        int8_t macroIdx = qbindSlots[slot];
        if (macroIdx < 0 || macroIdx >= (int8_t)MACRO_COUNT) return;
        macroStart(MACRO_NAMES[macroIdx]);
        functionName = macro.functionName;
        displayValue = "0";
        storedValue = "";
        pendingOp = 0;
        newEntry = true;
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
            functionName = "";
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
        } else if (macro.state == MACRO_AWAITING_INPUT) {
            macroCancel();
            functionName = "";
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


void drawMenuHeader(const char* title) {
    u8g2.setFont(u8g2_font_6x10_tr);
    int16_t titleWidth = u8g2.getStrWidth(title);
    int16_t titleX = (128 - titleWidth) / 2;
    u8g2.drawStr(titleX, 10, title);
    u8g2.drawStr(0, 10, "<");
    u8g2.drawStr(122, 10, ">");
}


void drawMacroPage() {
    drawMenuHeader("MACROS");

    // show 3 items centered on current selection
    int startIdx = menuIndex - 1;
    if (startIdx < 0) startIdx = 0;
    if (startIdx > MACRO_COUNT - 3) startIdx = MACRO_COUNT - 3;
    if (MACRO_COUNT <= 3) startIdx = 0;

    u8g2.setFont(u8g2_font_6x10_tr);
    for (int i = 0; i < 3 && (startIdx + i) < MACRO_COUNT; i++) {
        int idx = startIdx + i;
        int y = 25 + (i * 14);

        if (idx == menuIndex) {
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(4, y, MACRO_NAMES[idx]);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(4, y, MACRO_NAMES[idx]);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[4][6]Pg [5]Sel [NUM]Bk");
}


static void drawSettingsList() {
    int startIdx = settingsIndex - 1;
    if (startIdx < 0) startIdx = 0;
    if (startIdx > SETTINGS_COUNT - 3) startIdx = SETTINGS_COUNT - 3;
    if (SETTINGS_COUNT <= 3) startIdx = 0;

    u8g2.setFont(u8g2_font_6x10_tr);
    for (int i = 0; i < 3 && (startIdx + i) < SETTINGS_COUNT; i++) {
        int idx = startIdx + i;
        int y = 25 + (i * 14);
        if (idx == settingsIndex) {
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(4, y, SETTINGS_NAMES[idx]);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(4, y, SETTINGS_NAMES[idx]);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[4][6]Pg [5]Sel [NUM]Bk");
}

static void drawTimeoutEntry() {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 28, "Sleep (min):");
    String shown = settingsInput.length() > 0 ? settingsInput : String(sleepTimeoutMs / 60000);
    shown += "_";
    u8g2.drawStr(0, 46, shown.c_str());
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[*]Bksp [=]Sv [NUM]Bk");
}

static void drawSlider(const char* label, uint8_t value) {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 28, label);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", value);
    u8g2.drawStr(0, 46, buf);
    // bar: 0..255 mapped to 0..120 px
    int16_t barW = (int16_t)((uint16_t)value * 120 / 255);
    u8g2.drawFrame(0, 50, 122, 6);
    u8g2.drawBox(1, 51, barW, 4);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[8/2]+-10 [4/6]+-1 [=]Sv");
}

static void drawQbindList() {
    // 9 valid slots, show 3 centered on current selection
    int total = 9;
    int startIdx = (int)qbindListIdx - 1;
    if (startIdx < 0) startIdx = 0;
    if (startIdx > total - 3) startIdx = total - 3;

    u8g2.setFont(u8g2_font_6x10_tr);
    char row[24];
    for (int i = 0; i < 3 && (startIdx + i) < total; i++) {
        int idx = startIdx + i;
        uint8_t slot = QBIND_VALID_SLOTS[idx];
        int8_t mi = qbindSlots[slot];
        const char* name = (mi >= 0 && mi < (int8_t)MACRO_COUNT) ? MACRO_NAMES[mi] : "(none)";
        snprintf(row, sizeof(row), "FN+%u: %s", slot, name);

        int y = 25 + (i * 14);
        if (idx == (int)qbindListIdx) {
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(4, y, row);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(4, y, row);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[8/2]Nav [5]Sel [NUM]Bk");
}

static void drawQbindPick() {
    // pick list: 0 = "(none)", 1..MACRO_COUNT = macros
    int total = MACRO_COUNT + 1;
    int startIdx = (int)qbindPickIdx - 1;
    if (startIdx < 0) startIdx = 0;
    if (startIdx > total - 3) startIdx = total - 3;
    if (total <= 3) startIdx = 0;

    u8g2.setFont(u8g2_font_6x10_tr);
    for (int i = 0; i < 3 && (startIdx + i) < total; i++) {
        int idx = startIdx + i;
        const char* name = (idx == 0) ? "(none)" : MACRO_NAMES[idx - 1];
        int y = 25 + (i * 14);
        if (idx == (int)qbindPickIdx) {
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(4, y, name);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(4, y, name);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[8/2]Nav [5]Save [NUM]Bk");
}

static void drawResetConfirm() {
    u8g2.setFont(u8g2_font_6x10_tr);
    const char* l1 = "FACTORY RESET";
    int16_t w1 = u8g2.getStrWidth(l1);
    u8g2.drawStr((128 - w1) / 2, 26, l1);

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 40, "Wipes settings, binds,");
    u8g2.drawStr(0, 50, "and shows guide on boot.");
    u8g2.drawStr(0, 64, "[5]Confirm [NUM]Cancel");
}

static void drawZoomPick() {
    const char* options[2] = { "Windows/Linux", "macOS" };
    u8g2.setFont(u8g2_font_6x10_tr);
    for (int i = 0; i < 2; i++) {
        int y = 28 + (i * 14);
        if (i == zoomModifier) {
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(4, y, options[i]);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(4, y, options[i]);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[8/2]Nav [5]Save [NUM]Bk");
}


static void drawBTBondsList() {
    if (btBondCount == 0) {
        u8g2.setFont(u8g2_font_6x10_tr);
        const char* msg = "(no paired devices)";
        int16_t w = u8g2.getStrWidth(msg);
        u8g2.drawStr((128 - w) / 2, 38, msg);
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(0, 64, "[NUM] Back");
        return;
    }

    int total = btBondCount;
    int startIdx = (int)btBondIdx - 1;
    if (startIdx < 0) startIdx = 0;
    if (startIdx > total - 3) startIdx = total - 3;
    if (total <= 3) startIdx = 0;

    u8g2.setFont(u8g2_font_6x10_tr);
    for (int i = 0; i < 3 && (startIdx + i) < total; i++) {
        int idx = startIdx + i;
        int y = 25 + (i * 14);
        const char* s = btBondAddrs[idx].c_str();
        if (idx == (int)btBondIdx) {
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(2, y, s);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(2, y, s);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[8/2]Nav [5]Forget [NUM]Bk");
}


static void drawBTBondActions() {
    // header is set in drawSettingsPage; render MAC + 2 actions
    if (btBondIdx < btBondCount) {
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(0, 22, btBondAddrs[btBondIdx].c_str());
    }

    // current OS for this bond
    uint8_t mac[6];
    uint8_t curOS = 0;
    if (btBondIdx < btBondCount && parseMacString(btBondAddrs[btBondIdx], mac)) {
        curOS = getBondOSByMac(mac);
    }
    char osLine[24];
    snprintf(osLine, sizeof(osLine), "OS: %s", curOS == 1 ? "macOS" : "Linux/Win");

    const char* items[2];
    items[0] = osLine;
    items[1] = "Forget";

    u8g2.setFont(u8g2_font_6x10_tr);
    for (int i = 0; i < 2; i++) {
        int y = 36 + (i * 14);
        if (i == btBondActionIdx) {
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(4, y, items[i]);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(4, y, items[i]);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[8/2]Nav [5]Sel [NUM]Bk");
}


static void drawBTBondOSPick() {
    uint8_t mac[6];
    uint8_t curOS = 0;
    if (btBondIdx < btBondCount && parseMacString(btBondAddrs[btBondIdx], mac)) {
        curOS = getBondOSByMac(mac);
    }
    const char* options[2] = { "Linux/Win", "macOS" };
    u8g2.setFont(u8g2_font_6x10_tr);
    for (int i = 0; i < 2; i++) {
        int y = 28 + (i * 14);
        if (i == curOS) {
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(4, y, options[i]);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(4, y, options[i]);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[8/2]Toggle [5]Save [NUM]Bk");
}


static void drawBTForgetConfirm() {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 24, "Forget bond:");
    if (btBondIdx < btBondCount) {
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(0, 40, btBondAddrs[btBondIdx].c_str());
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[5]Confirm [NUM]Cancel");
}


static void drawBTSettings() {
    // 3 items: Pair / Reconnect / Forget
    int total = BT_ITEM_COUNT;
    int startIdx = (int)btMenuIdx - 1;
    if (startIdx < 0) startIdx = 0;
    if (startIdx > total - 3) startIdx = total - 3;
    if (total <= 3) startIdx = 0;

    u8g2.setFont(u8g2_font_6x10_tr);
    for (int i = 0; i < 3 && (startIdx + i) < total; i++) {
        int idx = startIdx + i;
        int y = 25 + (i * 14);
        if (idx == (int)btMenuIdx) {
            u8g2.drawBox(0, y - 10, 128, 14);
            u8g2.setDrawColor(0);
            u8g2.drawStr(4, y, BT_ITEM_NAMES[idx]);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(4, y, BT_ITEM_NAMES[idx]);
        }
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 64, "[8/2]Nav [5]Sel [NUM]Bk");
}


static void drawFwInfo() {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 24, "Tactical Tenkey");
    u8g2.setFont(u8g2_font_5x7_tr);
    String v = String("Version: ") + FW_VERSION;
    u8g2.drawStr(0, 38, v.c_str());
    String d = String("Built:   ") + FW_DATE;
    u8g2.drawStr(0, 48, d.c_str());
    u8g2.drawStr(0, 64, "[NUM] Back");
}

void drawSettingsPage() {
    if (settingsView == SETTINGS_VIEW_LIST) {
        drawMenuHeader("SETTINGS");
        drawSettingsList();
        return;
    }
    // sub-view: plain header (no L/R arrows since page nav is locked out)
    u8g2.setFont(u8g2_font_6x10_tr);
    if (settingsView == SETTINGS_VIEW_QBIND_PICK) {
        char hdr[16];
        snprintf(hdr, sizeof(hdr), "BIND FN+%u", qbindEditSlot);
        u8g2.drawStr(0, 10, hdr);
    } else if (settingsView == SETTINGS_VIEW_BT) {
        String s = bleStatusLine();
        u8g2.drawStr(0, 10, s.c_str());
    } else if (settingsView == SETTINGS_VIEW_BT_BONDS) {
        u8g2.drawStr(0, 10, "PAIRED");
    } else if (settingsView == SETTINGS_VIEW_BT_BOND) {
        u8g2.drawStr(0, 10, "BOND");
    } else if (settingsView == SETTINGS_VIEW_BT_BOND_OS) {
        u8g2.drawStr(0, 10, "BOND OS");
    } else if (settingsView == SETTINGS_VIEW_BT_FORGET) {
        u8g2.drawStr(0, 10, "FORGET?");
    } else if (settingsView == SETTINGS_VIEW_ZOOM_PICK) {
        u8g2.drawStr(0, 10, "HOST OS");
    } else {
        u8g2.drawStr(0, 10, "SETTINGS");
    }
    u8g2.drawHLine(0, 12, 128);
    switch (settingsView) {
        case SETTINGS_VIEW_TIMEOUT:     drawTimeoutEntry(); break;
        case SETTINGS_VIEW_CONTRAST:    drawSlider("Contrast:",   oledContrast);  break;
        case SETTINGS_VIEW_BRIGHTNESS:  drawSlider("Brightness:", ledBrightness); break;
        case SETTINGS_VIEW_FW_INFO:     drawFwInfo(); break;
        case SETTINGS_VIEW_QBIND_LIST:  drawQbindList(); break;
        case SETTINGS_VIEW_QBIND_PICK:  drawQbindPick(); break;
        case SETTINGS_VIEW_RESET_CONFIRM: drawResetConfirm(); break;
        case SETTINGS_VIEW_BT:          drawBTSettings(); break;
        case SETTINGS_VIEW_BT_BONDS:    drawBTBondsList(); break;
        case SETTINGS_VIEW_BT_BOND:     drawBTBondActions(); break;
        case SETTINGS_VIEW_BT_BOND_OS:  drawBTBondOSPick(); break;
        case SETTINGS_VIEW_BT_FORGET:   drawBTForgetConfirm(); break;
        case SETTINGS_VIEW_ZOOM_PICK:   drawZoomPick(); break;
        default: break;
    }
}


void saveSettings() {
    Preferences p;
    p.begin("t2", false);
    p.putULong("sleepMs", sleepTimeoutMs);
    p.putUChar("contrast", oledContrast);
    p.putUChar("ledBri", ledBrightness);
    p.putUChar("zoomMod", zoomModifier);
    p.putBytes("qbind", qbindSlots, sizeof(qbindSlots));
    p.putUChar("bmCnt", bondMetaCount);
    p.putBytes("bmData", bondMetaList, bondMetaCount * sizeof(BondMeta));
    p.end();
}


void factoryReset() {
    Preferences p;
    p.begin("t2", false);
    p.clear();  // wipes everything including "guided" and "fwRel"
    p.end();

    sleepTimeoutMs = DEFAULT_SLEEP_TIMEOUT;
    oledContrast = 255;
    ledBrightness = 255;
    zoomModifier = 0;
    bondMetaCount = 0;
    for (int i = 0; i < 10; i++) qbindSlots[i] = -1;

    bleShutdown();
    hidBleClearAllBonds();

    u8g2.setContrast(oledContrast);
    analogWrite(LED_PIN, ledBrightness);
}


// --- bond metadata helpers ---

static bool parseMacString(const String& s, uint8_t* out) {
    if (s.length() < 17) return false;
    for (int i = 0; i < 6; i++) {
        char hex[3] = { s.charAt(i * 3), s.charAt(i * 3 + 1), 0 };
        out[i] = (uint8_t)strtol(hex, nullptr, 16);
    }
    return true;
}

static int findBondMetaByMac(const uint8_t* mac) {
    for (uint8_t i = 0; i < bondMetaCount; i++) {
        if (memcmp(bondMetaList[i].mac, mac, 6) == 0) return i;
    }
    return -1;
}

static uint8_t getBondOSByMac(const uint8_t* mac) {
    int idx = findBondMetaByMac(mac);
    return (idx < 0) ? 0 : bondMetaList[idx].os;
}

static void setBondOSByMac(const uint8_t* mac, uint8_t os) {
    int idx = findBondMetaByMac(mac);
    if (idx >= 0) {
        bondMetaList[idx].os = os;
    } else if (bondMetaCount < BT_BOND_MAX) {
        memcpy(bondMetaList[bondMetaCount].mac, mac, 6);
        bondMetaList[bondMetaCount].os = os;
        bondMetaCount++;
    }
    saveSettings();
}

static void removeBondMetaByMac(const uint8_t* mac) {
    int idx = findBondMetaByMac(mac);
    if (idx < 0) return;
    for (uint8_t i = idx; i < bondMetaCount - 1; i++) {
        bondMetaList[i] = bondMetaList[i + 1];
    }
    bondMetaCount--;
    saveSettings();
}

static void applyConnectedPeerOS() {
    const uint8_t* peer = hidBleGetPeerMac();
    if (!peer) return;
    zoomModifier = getBondOSByMac(peer);
}


void btRefreshBondsCache() {
    uint8_t total = hidBleGetBondCount();
    btBondCount = total > BT_BOND_MAX ? BT_BOND_MAX : total;
    for (uint8_t i = 0; i < btBondCount; i++) {
        btBondAddrs[i] = hidBleGetBondAddress(i);
    }
    if (btBondIdx >= btBondCount && btBondCount > 0) {
        btBondIdx = btBondCount - 1;
    }
}


void bleStartAdvertising() {
    hidBleInit(false);
    bleMode = BLE_MODE_ADVERTISING;
    bleModeUntil = millis() + BLE_ADVERTISE_WINDOW_MS;
}


void bleStartPairing() {
    hidBleInit(true);
    bleMode = BLE_MODE_PAIRING;
    bleModeUntil = millis() + BLE_PAIRING_WINDOW_MS;
}


void bleShutdown() {
    hidBleDeinit();
    bleMode = BLE_MODE_OFF;
    bleConnected = false;
    bleModeUntil = 0;
}


void blePoll() {
    if (bleMode == BLE_MODE_OFF) return;

    bool conn = hidBleIsConnected();

    if (conn) {
        if (bleMode != BLE_MODE_CONNECTED) {
            bleMode = BLE_MODE_CONNECTED;
            bleConnected = true;
            bleModeUntil = 0;
            // clear any stuck modifier bits from the initial post-pair report
            // (macOS will latch a phantom Ctrl otherwise → trackpad → right-click)
            hidBleClearReport();
            // auto-apply zoom modifier based on which bonded peer connected
            applyConnectedPeerOS();
            updateDisplay();
        }
        return;
    }

    // not connected
    if (bleMode == BLE_MODE_CONNECTED) {
        // peer dropped: re-enter advertising window
        bleMode = BLE_MODE_ADVERTISING;
        bleModeUntil = millis() + BLE_ADVERTISE_WINDOW_MS;
        bleConnected = false;
        updateDisplay();
        return;
    }

    // ADVERTISING or PAIRING: deinit when window expires
    if (bleModeUntil != 0 && (int32_t)(millis() - bleModeUntil) >= 0) {
        bleShutdown();
        updateDisplay();
    }
}


String bleStatusLine() {
    switch (bleMode) {
        case BLE_MODE_CONNECTED:
            return "CONNECTED";
        case BLE_MODE_PAIRING: {
            uint32_t left = (bleModeUntil > millis()) ? (bleModeUntil - millis()) / 1000 : 0;
            char buf[24];
            snprintf(buf, sizeof(buf), "PAIRING %lu:%02lu",
                     (unsigned long)(left / 60), (unsigned long)(left % 60));
            return String(buf);
        }
        case BLE_MODE_ADVERTISING: {
            uint32_t left = (bleModeUntil > millis()) ? (bleModeUntil - millis()) / 1000 : 0;
            char buf[24];
            snprintf(buf, sizeof(buf), "SEARCHING %lu:%02lu",
                     (unsigned long)(left / 60), (unsigned long)(left % 60));
            return String(buf);
        }
        default:
            return "BLUETOOTH";
    }
}


void handleSettingsKey(char key) {
    if (key == 'C') {
        if (settingsView == SETTINGS_VIEW_BT_FORGET || settingsView == SETTINGS_VIEW_BT_BOND_OS) {
            settingsView = SETTINGS_VIEW_BT_BOND;
        } else if (settingsView == SETTINGS_VIEW_BT_BOND) {
            settingsView = SETTINGS_VIEW_BT_BONDS;
        } else if (settingsView == SETTINGS_VIEW_BT_BONDS) {
            settingsView = SETTINGS_VIEW_BT;
        } else {
            settingsView = SETTINGS_VIEW_LIST;
            settingsInput = "";
        }
        drawMenu();
        return;
    }

    if (settingsView == SETTINGS_VIEW_TIMEOUT) {
        if (key >= '0' && key <= '9') {
            if (settingsInput.length() < 4) settingsInput += key;
            drawMenu();
            return;
        }
        if (key == '*') {
            if (settingsInput.length() > 0) {
                settingsInput.remove(settingsInput.length() - 1);
            }
            drawMenu();
            return;
        }
        if (key == '=') {
            if (settingsInput.length() > 0) {
                uint32_t mins = settingsInput.toInt();
                if (mins == 0) mins = 1; // floor at 1 minute
                sleepTimeoutMs = mins * 60000UL;
                saveSettings();
            }
            settingsView = SETTINGS_VIEW_LIST;
            settingsInput = "";
            drawMenu();
            return;
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_CONTRAST || settingsView == SETTINGS_VIEW_BRIGHTNESS) {
        uint8_t* val = (settingsView == SETTINGS_VIEW_CONTRAST) ? &oledContrast : &ledBrightness;
        int delta = 0;
        if (key == '8') delta = 10;
        else if (key == '2') delta = -10;
        else if (key == '6') delta = 1;
        else if (key == '4') delta = -1;
        else if (key == '=') {
            saveSettings();
            settingsView = SETTINGS_VIEW_LIST;
            drawMenu();
            return;
        }
        if (delta != 0) {
            int next = (int)*val + delta;
            if (next < 0) next = 0;
            if (next > 255) next = 255;
            *val = (uint8_t)next;
            if (settingsView == SETTINGS_VIEW_CONTRAST) {
                u8g2.setContrast(oledContrast);
            } else {
                analogWrite(LED_PIN, ledBrightness);
            }
            drawMenu();
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_QBIND_LIST) {
        if (key == '8') {
            if (qbindListIdx > 0) qbindListIdx--;
            drawMenu();
            return;
        }
        if (key == '2') {
            if (qbindListIdx < 8) qbindListIdx++;
            drawMenu();
            return;
        }
        if (key == '5' || key == '=') {
            qbindEditSlot = QBIND_VALID_SLOTS[qbindListIdx];
            int8_t cur = qbindSlots[qbindEditSlot];
            qbindPickIdx = (cur < 0) ? 0 : (uint8_t)(cur + 1);
            settingsView = SETTINGS_VIEW_QBIND_PICK;
            drawMenu();
            return;
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_QBIND_PICK) {
        int total = MACRO_COUNT + 1;
        if (key == '8') {
            if (qbindPickIdx > 0) qbindPickIdx--;
            drawMenu();
            return;
        }
        if (key == '2') {
            if (qbindPickIdx < total - 1) qbindPickIdx++;
            drawMenu();
            return;
        }
        if (key == '5' || key == '=') {
            qbindSlots[qbindEditSlot] = (qbindPickIdx == 0) ? -1 : (int8_t)(qbindPickIdx - 1);
            saveSettings();
            settingsView = SETTINGS_VIEW_QBIND_LIST;
            drawMenu();
            return;
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_BT) {
        if (key == '8') {
            if (btMenuIdx > 0) btMenuIdx--;
            drawMenu();
            return;
        }
        if (key == '2') {
            if (btMenuIdx < BT_ITEM_COUNT - 1) btMenuIdx++;
            drawMenu();
            return;
        }
        if (key == '5' || key == '=') {
            switch (btMenuIdx) {
                case 0: bleStartPairing();    break;
                case 1: bleStartAdvertising(); break;
                case 2:
                    btBondIdx = 0;
                    btRefreshBondsCache();
                    settingsView = SETTINGS_VIEW_BT_BONDS;
                    break;
                case 3:
                    bleShutdown();
                    hidBleClearAllBonds();
                    break;
            }
            drawMenu();
            return;
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_BT_BONDS) {
        if (btBondCount == 0) return;  // only NUM is meaningful (handled above)
        if (key == '8') {
            if (btBondIdx > 0) btBondIdx--;
            drawMenu();
            return;
        }
        if (key == '2') {
            if (btBondIdx < btBondCount - 1) btBondIdx++;
            drawMenu();
            return;
        }
        if (key == '5' || key == '=') {
            btBondActionIdx = 0;
            settingsView = SETTINGS_VIEW_BT_BOND;
            drawMenu();
            return;
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_BT_BOND) {
        if (key == '8') {
            if (btBondActionIdx > 0) btBondActionIdx--;
            drawMenu();
            return;
        }
        if (key == '2') {
            if (btBondActionIdx < 1) btBondActionIdx++;
            drawMenu();
            return;
        }
        if (key == '5' || key == '=') {
            if (btBondActionIdx == 0) {
                settingsView = SETTINGS_VIEW_BT_BOND_OS;
            } else {
                settingsView = SETTINGS_VIEW_BT_FORGET;
            }
            drawMenu();
            return;
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_BT_BOND_OS) {
        if (btBondIdx >= btBondCount) {
            settingsView = SETTINGS_VIEW_BT_BOND;
            drawMenu();
            return;
        }
        uint8_t mac[6];
        if (!parseMacString(btBondAddrs[btBondIdx], mac)) {
            settingsView = SETTINGS_VIEW_BT_BOND;
            drawMenu();
            return;
        }
        if (key == '8' || key == '2') {
            // toggle current OS
            uint8_t cur = getBondOSByMac(mac);
            setBondOSByMac(mac, cur == 0 ? 1 : 0);
            drawMenu();
            return;
        }
        if (key == '5' || key == '=') {
            // if this is the currently connected peer, apply immediately
            applyConnectedPeerOS();
            settingsView = SETTINGS_VIEW_BT_BOND;
            drawMenu();
            return;
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_BT_FORGET) {
        if (key == '5' || key == '=') {
            // if forgetting the currently-connected peer, drop the connection first
            bool wasConnected = (bleMode == BLE_MODE_CONNECTED);
            if (wasConnected) bleShutdown();
            uint8_t mac[6];
            bool macOk = parseMacString(btBondAddrs[btBondIdx], mac);
            hidBleDeleteBond(btBondIdx);
            if (macOk) removeBondMetaByMac(mac);
            btRefreshBondsCache();
            settingsView = SETTINGS_VIEW_BT_BONDS;
            drawMenu();
            return;
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_ZOOM_PICK) {
        if (key == '8') {
            if (zoomModifier > 0) zoomModifier--;
            drawMenu();
            return;
        }
        if (key == '2') {
            if (zoomModifier < 1) zoomModifier++;
            drawMenu();
            return;
        }
        if (key == '5' || key == '=') {
            saveSettings();
            settingsView = SETTINGS_VIEW_LIST;
            drawMenu();
            return;
        }
        return;
    }

    if (settingsView == SETTINGS_VIEW_RESET_CONFIRM) {
        if (key == '5' || key == '=') {
            factoryReset();
            settingsView = SETTINGS_VIEW_LIST;
            settingsIndex = 0;
            drawMenu();
            return;
        }
        return;
    }

    // SETTINGS_VIEW_FW_INFO: only C exits (handled above)
}


void drawMenu() {
    u8g2.clearBuffer();
    switch (menuPage) {
        case MENU_PAGE_MACROS:   drawMacroPage();    break;
        case MENU_PAGE_SETTINGS: drawSettingsPage(); break;
    }
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
    
    // OS group: Apple for macOS, Windows logo for Win/Linux (default)
    iconX -= 2;  // spacing
    iconX -= ICON_WIDTH;
    u8g2.drawXBM(iconX, iconY, ICON_WIDTH, ICON_HEIGHT,
                 zoomModifier == 1 ? ICON_APPLE : ICON_WINDOWS);

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
    bleShutdown();
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
    initMatrix();
    Wire.begin(SDA_PIN, SCL_PIN);
    u8g2.begin();
    u8g2.setFlipMode(1); // rotate display 180 degrees

    Preferences prefs;
    prefs.begin("t2", false);

    sleepTimeoutMs = prefs.getULong("sleepMs", DEFAULT_SLEEP_TIMEOUT);
    oledContrast   = prefs.getUChar("contrast", 255);
    ledBrightness  = prefs.getUChar("ledBri",   255);
    zoomModifier   = prefs.getUChar("zoomMod",  0);
    if (prefs.isKey("qbind")) {
        prefs.getBytes("qbind", qbindSlots, sizeof(qbindSlots));
    }
    bondMetaCount = prefs.getUChar("bmCnt", 0);
    if (bondMetaCount > BT_BOND_MAX) bondMetaCount = 0;
    if (bondMetaCount > 0 && prefs.isKey("bmData")) {
        prefs.getBytes("bmData", bondMetaList, bondMetaCount * sizeof(BondMeta));
    }
    u8g2.setContrast(oledContrast);
    analogWrite(LED_PIN, ledBrightness);

    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup == ESP_SLEEP_WAKEUP_UNDEFINED) {
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
    }
    prefs.end();

    if (hidBleGetBondCount() > 0) {
        bleStartAdvertising();
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

    blePoll();

    // refresh BT status countdown live while the BT page is open
    static uint32_t lastBtTick = 0;
    if (macro.state == MACRO_MENU && menuPage == MENU_PAGE_SETTINGS
        && settingsView == SETTINGS_VIEW_BT
        && millis() - lastBtTick > 1000) {
        drawMenu();
        lastBtTick = millis();
    }

    if (!numpadMode && millis() - lastActivity > sleepTimeoutMs) {
        goToSleep();
    }
    delay(20);
}