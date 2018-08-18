#ifndef PARSE_HPP
#define PARSE_HPP

#include "ArrayList.hpp"

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

enum Location {
    LOC_NONE, LOC_LEFT, LOC_RIGHT, LOC_IN, LOC_OUT, LOC_REG0, LOC_REG1, LOC_REG2, LOC_REG3,
};

enum InstructionType {
    INST_NONE, INST_MOV, INST_SHUF, INST_SHIFT, INST_ADD, INST_SUB, INST_MUL, INST_DIV,
    INST_JMP, INST_JEQ, INST_JGT, INST_WAIT,
};

struct Instruction {
    InstructionType type;
    int laneWidth;
    i64 literal;
    Location source;
    Location destination;
    int labelIndex; //used to store index into labels array during compilation,
                    //then line number of label during execution
};

//REASONABLE VALUES: 20 lines, 30 chars per line
const int MAX_LINE_LEN = 30;
const int MAX_LINES = 20;
const int ERROR_MESSAGE_SIZE = 100;

struct TextArea {
    // int lineCount; //TODO: use this in some more productive way?
    char text[MAX_LINES][MAX_LINE_LEN + 1];
    u8 colors[MAX_LINES][MAX_LINE_LEN];
    Instruction instructions[MAX_LINES];
    //error info
    bool foundError;
    int errorLine;
    int errorStart;
    int errorEnd;
    char errorMessage[ERROR_MESSAGE_SIZE + 1];
    //registers
    u8 registers[4][16];

    //we set a flag here instead of doing it in-place to ensure we only recompile once per frame
    //even if multiple text changes happen during the frame (compiling multiple times is wasteful)
    bool shouldRecompile;
};

enum TextColor {
    TEXT_DEFAULT, TEXT_INVALID, TEXT_COMMENT, TEXT_COLON, TEXT_CONST, TEXT_IDENTIFIER,
    // TEXT_LABEl, TEXT_INST, TEXT_LANE, TEXT_REGISTER, TEXT_BUS, TEXT_PORT
};

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

ArrayList<TextArea> load_save_from_file(const char * savePath);
void compile_text_area(TextArea * area, int nodeIndex);

#endif
