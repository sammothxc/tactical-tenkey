#ifndef MACROS_H
#define MACROS_H

#include <Arduino.h>

enum MacroState {
    MACRO_IDLE,
    MACRO_AWAITING_INPUT,
    MACRO_COMPLETE
};

struct MacroContext {
    MacroState state;
    String functionName;
    double params[4]; // up to 4 parameters (for now)
    uint8_t paramIndex;
    uint8_t paramCount; // how many params this macro needs
    double result;
};

extern MacroContext macro;

void macroStart(const char* name); // call this when user selects a macro
bool macroInput(double value); // call this when user hits enter with a number returns true if macro is complete and result is ready
void macroCancel();
const char* macroGetPrompt(); // prompt text for current param

// MACRO LIST
void macroTaxAdd();       // add tax: amount * (1 + rate)
void macroTaxSub();       // remove tax: amount / (1 + rate)
void macroPercent();      // percentage: amount * (percent / 100)
void macroMarkup();       // markup: cost * (1 + margin)
void macroDiscount();     // discount: price * (1 - discount)
void macroCompoundInt();  // compound interest: principal * (1 + rate)^periods

#endif