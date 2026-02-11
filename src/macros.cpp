#include "macros.h"

MacroContext macro = {MACRO_IDLE, "", {0}, 0, 0, 0};

// param prompts per macro
const char* PROMPTS_TAX_ADD[] = {"Amount?", "Tax %?"};
const char* PROMPTS_TAX_SUB[] = {"Total?", "Tax %?"};
const char* PROMPTS_PERCENT[] = {"Amount?", "Percent?"};
const char* PROMPTS_MARKUP[] = {"Cost?", "Markup %?"};
const char* PROMPTS_DISCOUNT[] = {"Price?", "Discount %?"};
const char* PROMPTS_COMPOUND[] = {"Principal?", "Rate %?", "Periods?"};

const char** currentPrompts = nullptr;

void macroStart(const char* name) {
    macro.functionName = name;
    macro.paramIndex = 0;
    macro.result = 0;
    macro.state = MACRO_AWAITING_INPUT;
    
    if (strcmp(name, "TAX+") == 0) {
        macro.paramCount = 2;
        currentPrompts = PROMPTS_TAX_ADD;
    }
    else if (strcmp(name, "TAX-") == 0) {
        macro.paramCount = 2;
        currentPrompts = PROMPTS_TAX_SUB;
    }
    else if (strcmp(name, "PCT") == 0) {
        macro.paramCount = 2;
        currentPrompts = PROMPTS_PERCENT;
    }
    else if (strcmp(name, "MRKUP") == 0) {
        macro.paramCount = 2;
        currentPrompts = PROMPTS_MARKUP;
    }
    else if (strcmp(name, "DISC") == 0) {
        macro.paramCount = 2;
        currentPrompts = PROMPTS_DISCOUNT;
    }
    else if (strcmp(name, "CMPND") == 0) {
        macro.paramCount = 3;
        currentPrompts = PROMPTS_COMPOUND;
    }
}

bool macroInput(double value) {
    if (macro.state != MACRO_AWAITING_INPUT) return false;
    
    macro.params[macro.paramIndex++] = value;
    
    if (macro.paramIndex >= macro.paramCount) {
        // all params collected, calculate
        if (macro.functionName == "TAX+") {
            macro.result = macro.params[0] * (1 + macro.params[1] / 100);
        }
        else if (macro.functionName == "TAX-") {
            macro.result = macro.params[0] / (1 + macro.params[1] / 100);
        }
        else if (macro.functionName == "PCT") {
            macro.result = macro.params[0] * (macro.params[1] / 100);
        }
        else if (macro.functionName == "MRKUP") {
            macro.result = macro.params[0] * (1 + macro.params[1] / 100);
        }
        else if (macro.functionName == "DISC") {
            macro.result = macro.params[0] * (1 - macro.params[1] / 100);
        }
        else if (macro.functionName == "CMPND") {
            macro.result = macro.params[0] * pow(1 + macro.params[1] / 100, macro.params[2]);
        }
        
        macro.state = MACRO_COMPLETE;
        return true;
    }
    
    return false;
}

void macroCancel() {
    macro.state = MACRO_IDLE;
    macro.functionName = "";
    macro.paramIndex = 0;
    currentPrompts = nullptr;
}

const char* macroGetPrompt() {
    if (macro.state != MACRO_AWAITING_INPUT || currentPrompts == nullptr) {
        return "";
    }
    return currentPrompts[macro.paramIndex];
}