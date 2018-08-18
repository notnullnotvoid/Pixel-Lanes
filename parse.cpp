#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "types.hpp"
#include "system.hpp"
#include "ArrayList.hpp"
#include "parse.hpp"

//TODO: improve error messages overall

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

enum TokenType {
    TOKEN_INVALID, TOKEN_COMMENT, TOKEN_COLON, TOKEN_CONST, TOKEN_IDENTIFIER,
};

struct Token {
    int start;
    int end;
    TokenType type;
};

struct Label {
    char * name;
    int len;
    int line;
    bool defined;
};

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

void set_error(TextArea * area, int line, int start, int end, const char * format, ...) {
    if (!area->foundError) {
        area->foundError = true;
        area->errorLine = line;
        area->errorStart = start;
        area->errorEnd = end;

        va_list argptr;
        va_start(argptr, format);
        vsnprintf(area->errorMessage, ERROR_MESSAGE_SIZE, format, argptr);
        va_end(argptr);
    }
}

template <typename ENUM>
ENUM match_enum_by_name(Pair<ENUM, const char *> * pairs, int pairCount,
                        const char * str, int len, ENUM defaultVal) {
    for (int n = 0; n < pairCount; ++n) {
        bool match = len == strlen(pairs[n].second);
        if (match) {
            for (int c = 0; c < len; ++c) {
                if (str[c] != pairs[n].second[c]) {
                    match = false;
                    break;
                }
            }
        }

        if (match) {
            return pairs[n].first;
        }
    }

    return defaultVal;
}

static int parse_const(TextArea * area, int lineNum, Token arg, int maxDigits, int argNum) {
    char * line = area->text[lineNum];

    //TODO: debug why this function is getting passed 16 for lane width when it should be
    //      gettign passed a smaller number
    printf("PARSING CONST WITH LANE WIDTH %d\n", maxDigits);

    //TODO: better handling of leading zeroes?
    if (arg.end - arg.start > maxDigits) {
        set_error(area, lineNum, arg.start, arg.end, "TOO MANY DIGITS IN ARGUMENT %d", argNum);
        return 0;
    }

    i64 value = 0;
    for (int c = arg.start; c < arg.end; ++c) {
        value *= 10;
        value += line[c] - '0';
    }
    return value;
}

static Location parse_loc(TextArea * area, int lineNum, Token arg, int argNum) {
    char * line = area->text[lineNum];

    Pair<Location, const char *> locationNames[] = {
        { LOC_LEFT, "LEFT" },
        { LOC_RIGHT, "RIGHT" },
        { LOC_IN, "IN" },
        { LOC_OUT, "OUT" },
        { LOC_REG0, "REG0" },
        { LOC_REG1, "REG1" },
        { LOC_REG2, "REG2" },
        { LOC_REG3, "REG3" },
        { LOC_REG0, "R0" },
        { LOC_REG1, "R1" },
        { LOC_REG2, "R2" },
        { LOC_REG3, "R3" },
    };

    //parse location name
    Location location = match_enum_by_name(locationNames, ARR_SIZE(locationNames),
        line + arg.start, arg.end - arg.start, LOC_NONE);

    if (location == LOC_NONE) {
        set_error(area, lineNum, arg.start, arg.end,
            "ARGUMENT %d DOES NOT NAME A VALID REGISTER/PORT/BUS", argNum);
    }

    return location;
}

static int parse_lane_width(TextArea * area, int lineNum, Token arg, int argNum) {
    char * line = area->text[lineNum];

    //parse lane width
    Pair<int, const char *> laneWidths[] = {
        { 1, "L1" },
        { 2, "L2" },
        { 4, "L4" },
        { 8, "L8" },
        { 16, "L16" },
    };

    int width = match_enum_by_name(laneWidths, ARR_SIZE(laneWidths),
        line + arg.start, arg.end - arg.start, 0);

    //TODO: better differentiation of error cases
    if (width == 0) {
        set_error(area, lineNum, arg.start, arg.end, "INVALID LANE WIDTH SUPPLIED (ARGUMENT 1)");
    }

    return width;
}

static int find_label_in_list(Label * labels, int labelCount, char * line, Token token) {
    for (int i = 0; i < labelCount; ++i) {
        bool match = token.end - token.start == labels[i].len;
        if (match) {
            for (int c = 0; c < labels[i].len; ++c) {
                if (line[token.start + c] != labels[i].name[c]) {
                    match = false;
                    break;
                }
            }
        }

        if (match) {
            return i;
        }
    }

    return -1;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

ArrayList<TextArea> load_save_from_file(const char * savePath) {
    char * save = read_entire_file(savePath);
    ArrayList<TextArea> areas = create_array_list<TextArea>(4);

    int areaLineCount = 0;
    char * line = save;
    do {
        //advance to next line and null-terminate
        char * c = line;
        for (c = line; *c != '\0'; ++c) {
            if (*c == '\n') {
                *c = '\0';
                c += 1;
                break;
            } else if (*c == '\r') {
                *c = '\0';
                if (c[1] == '\n') {
                    c += 2;
                } else {
                    c += 1;
                }
                break;
            }
        }

        if (line[0] == '@') {
            //the node number doesn't actually matter for now,
            //so we just do a routine append

            //DEBUG PRINT
            printf("ACTIVE AREA: %lu\n", areas.len);
            areas.add({});
            areaLineCount = 0;
        } else if (areas.len > 0) {
            TextArea * area = &areas[areas.len - 1];

            if (areaLineCount >= MAX_LINES) {
                if (strlen(line) > 0) {
                    //TODO: real error reporting
                    printf("ERROR: MALFORMED SAVE (TOO MANY LINES)\n");
                    exit(1);
                }
            } else {
                if (strlen(line) > MAX_LINE_LEN) {
                    //TODO: real error reporting
                    printf("ERROR: MALFORMED SAVE (LINE TOO LONG)\n");
                    exit(1);
                }

                //capitalize everything
                for (int i = 0; i < strlen(line); ++i) {
                    line[i] = toupper(line[i]);
                }

                strcpy(area->text[areaLineCount], line);
                areaLineCount += 1;
            }
        }

        line = c;
    } while (*line != '\0');

    areas.shrink_to_fit();
    return areas;

    //should not be used past this point (all further processing will be done
    //on the lines stored in the TextArea structs)
    free(save);

    //DEBUG PRINT
    for (int i = 0; i < areas.len; ++i) {
        for (int l = 0; l < MAX_LINES; ++l) {
            printf("AREA %2d LINE %2d: %s\n", i, l, areas[i].text[l]);
        }
    }
}

void compile_text_area(TextArea * area, int nodeIndex) {
    //clear out old info for a nice, fresh recompile
    area->foundError = false;

    int labelCount = 0;
    Label labels[MAX_LINES * (MAX_LINE_LEN/2)];

    for (int l = 0; l < MAX_LINES; ++l) {
        char * line = area->text[l];

        //the line length creates a convenient upper bound on the number of tokens
        //that can possibly be in one line (since all tokens are at least 1 char)
        int tokenCount = 0;
        Token tokens[MAX_LINE_LEN] = {};

        //find tokens until end of line
        int start = 0;
        while (line[start] != '\0') {
            //find next non-delimiter char
            const char * delims = "\t ,";
            bool foundDelim = false;
            do {
                foundDelim = false;
                for (int d = 0; d < strlen(delims); ++d) {
                    if (line[start] == delims[d]) {
                        foundDelim = true;
                        break;
                    }
                }
                if (foundDelim) {
                    ++start;
                }
            } while (foundDelim);

            if (line[start] == '\0') {
                //DEBUG PRINT
                printf("FOUND EOF PREMATURELY\n");
                break;
            }

            //based on what the delimiter is, find the end of the token (exclusive)
            int end = start;
            TokenType type = TOKEN_INVALID;
            if (line[start] == '#') {
                //this begins a comment, so we stomp the rest of the line
                type = TOKEN_COMMENT;
                end = strlen(line);
            } else if (line[start] == ':') {
                //this is a special token indicating that the previous identifier was a label
                type = TOKEN_COLON;
                end = start + 1;
            } else if (isdigit(line[start])) {
                //[0-9] begins a number literal
                type = TOKEN_CONST;
                while (isdigit(line[end])) {
                    ++end;
                }
            } else if (line[start] == '_' || isalpha(line[start])) {
                //[A-Z_] begins an identifier
                type = TOKEN_IDENTIFIER;
                while (line[end] == '_' || isalnum(line[end])) {
                    ++end;
                }
            } else {
                //any other character begins an invalid token
                type = TOKEN_INVALID;
                end = start + 1;
            }

#if 0
            //DEBUG PRINT
            printf("AREA %d LINE %2d: TYPE %d START %2d END %2d TOKEN  %.*s\n",
                nodeIndex, l, type, start, end, end - start, line + start);
#endif

            if (type == TOKEN_INVALID) {
                set_error(area, l, start, end, "INVALID TOKEN");
            }

            //apply text color
            Pair<TokenType, TextColor> colorMap[] = {
                { TOKEN_INVALID, TEXT_INVALID },
                { TOKEN_COMMENT, TEXT_COMMENT },
                { TOKEN_COLON, TEXT_COLON },
                { TOKEN_CONST, TEXT_CONST },
                { TOKEN_IDENTIFIER, TEXT_IDENTIFIER },
            };

            for (int i = start; i < end; ++i) {
                //get color
                TextColor color = TEXT_DEFAULT;
                for (int c = 0; c < ARR_SIZE(colorMap); ++c) {
                    if (colorMap[c].first == type) {
                        color = colorMap[c].second;
                        break;
                    }
                }

                area->colors[l][i] = color;
            }

            //push token onto list
            tokens[tokenCount] = { start, end, type };
            ++tokenCount;

            //move on to next token
            start = end;
        }



        //parser state machine guts
        enum ParserState {
            PS_BLANK, PS_IDENTIFIER, PS_INSTRUCTION,
        };

        int argCount = 0;
        Token argsArr[MAX_LINE_LEN] = {};
        Token * args = argsArr;
        Token instruction = {};
        InstructionType instructionType = INST_NONE;

        ParserState state = PS_BLANK;
        for (int i = 0; i < tokenCount; ++i) {
            if (state == PS_BLANK) {
                if (tokens[i].type == TOKEN_IDENTIFIER) {
                    state = PS_IDENTIFIER;
                } else if (tokens[i].type == TOKEN_COMMENT) {
                    //nothing to do
                    break;
                } else {
                    //TODO: attempt to differentiate errors here
                    set_error(area, l, tokens[i].start, tokens[i].end,
                        "THIS TOKEN DOES NOT BELONG HERE: %d  %.*s",
                        i, tokens[i].start - tokens[i].end, line + tokens[i].start);
                    break;
                }
            } else if (state == PS_IDENTIFIER) {
                if (tokens[i].type == TOKEN_COLON) {
                    Token prev = tokens[i - 1];
                    int idx = find_label_in_list(labels, labelCount, line, prev);
                    if (idx >= 0) {
                        if (labels[idx].defined) {
                            set_error(area, l, tokens[i].start, tokens[i].end,
                                "A LABEL WITH THIS NAME WAS ALREADY DEFINED");
                        } else {
                            //DEBUG PRINT
                            printf("REPLACING PREVIOUS LABEL: %.*s LINE %d WITH %.*s LINE %d\n",
                                labels[idx].len, labels[idx].name, labels[idx].line,
                                prev.end - prev.start, line + prev.start, l);

                            labels[idx] = { line + prev.start, prev.end - prev.start, l, true };
                        }
                    } else {
                        //DEBUG PRINT
                        printf("ADDING NEW LABEL: %.*s LINE %d\n",
                            prev.end - prev.start, line + prev.start, l);

                        //add previous token to label list
                        labels[labelCount] = { line + prev.start, prev.end - prev.start, l, true };
                        ++labelCount;
                    }

                    state = PS_BLANK;
                } else if (tokens[i].type == TOKEN_IDENTIFIER ||
                           tokens[i].type == TOKEN_CONST) {
                    Pair<InstructionType, const char *> instructionNames[] = {
                        { INST_MOV, "MOV" },
                        { INST_SHUF, "SHUF" },
                        { INST_SHIFT, "SHIFT" },
                        { INST_ADD, "ADD" },
                        { INST_SUB, "SUB" },
                        { INST_MUL, "MUL" },
                        { INST_DIV, "DIV" },
                        { INST_JMP, "JMP" },
                        { INST_JEQ, "JEQ" },
                        { INST_JGT, "JGT" },
                        { INST_WAIT, "WAIT" },
                    };

                    Token prev = tokens[i - 1];
                    instructionType = match_enum_by_name(
                        instructionNames, ARR_SIZE(instructionNames),
                        line + prev.start, prev.end - prev.start, INST_NONE);

                    if (instructionType == INST_NONE) {
                        set_error(area, l, prev.start, prev.end,
                            "UNKNOWN INSTRUCTION NAME: %d  %.*s\n",
                            i, prev.start - prev.end, line + prev.start);
                        break;
                    }

                    //begin storing instruction arguments
                    instruction = tokens[i - 1];
                    args[argCount] = tokens[i];
                    ++argCount;

                    state = PS_INSTRUCTION;
                } else {
                    //TODO: attempt to smartly differentiate errors here
                    set_error(area, l, tokens[i - 1].start, tokens[i - 1].end,
                        "EXPECTED ARGUMENTS AND/OR UNKNOWN INSTRUCTION: %d %.*s\n", i,
                        tokens[i - 1].start - tokens[i - 1].end, line + tokens[i - 1].start);
                    break;
                }
            } else if (state == PS_INSTRUCTION) {
                if (tokens[i].type == TOKEN_IDENTIFIER || tokens[i].type == TOKEN_CONST) {
                    args[argCount] = tokens[i];
                    ++argCount;
                } else if (tokens[i].type == TOKEN_COMMENT) {
                    //nothing to do
                    break;
                } else if (tokens[i].type == TOKEN_COLON) {
                    set_error(area, l, tokens[i - 1].start, tokens[i - 1].end,
                        "LABELS MUST COME BEFORE INSTRUCTION: %d %.*s\n", i,
                        tokens[i - 1].start - tokens[i - 1].end, line + tokens[i - 1].start);
                }
            }
        }

#if 0
        //DEBUG PRINT
        if (instructionType == INST_NONE) {
            printf("AREA %d LINE %2d: NO INSTRUCTION\n", nodeIndex, l);
        } else {
            printf("AREA %D LINE %2d: %.*s  ", nodeIndex, l,
                instruction.end - instruction.start, line + instruction.start);
            for (int i = 0; i < argCount; ++i) {
                printf(" %.*s", args[i].end - args[i].start, line + args[i].start);
            }
            printf("\n");
        }
#endif



        //parse instruction
        Instruction inst = { instructionType, 16, 0, LOC_NONE, LOC_NONE, 0 };
        //TODO: move the optional lane width consumption out to here
        //      so it works the same for all instructions

        if ((instructionType == INST_MOV && argCount == 3) ||
            (instructionType == INST_SHUF && argCount == 3) ||
            (instructionType == INST_SHIFT && argCount == 3) ||
            (instructionType == INST_ADD && argCount == 3) ||
            (instructionType == INST_SUB && argCount == 3) ||
            (instructionType == INST_MUL && argCount == 3) ||
            (instructionType == INST_DIV && argCount == 3) ||
            (instructionType == INST_JMP && argCount == 2) ||
            (instructionType == INST_JEQ && argCount == 4) ||
            (instructionType == INST_JGT && argCount == 4)) {
            //actually, this doesn't work for INST_WAIT. oops...
        }

        if (instructionType == INST_NONE) {
            //nothing to do (inst is already correctly initialized)
        } else if (instructionType == INST_MOV ||
                   instructionType == INST_SHUF ||
                   instructionType == INST_SHIFT ||
                   instructionType == INST_ADD ||
                   instructionType == INST_SUB ||
                   instructionType == INST_MUL ||
                   instructionType == INST_DIV) {
            //consume optional lane width argument
            int argShift = 0;
            if (argCount == 3) {
                if (args[0].type == TOKEN_CONST) {
                    set_error(area, l, args[0].start, args[0].end,
                        "INTEGER LITERAL SUPPLIED, LANE WIDTH EXPECTED (ARGUMENT 1)");
                } else {
                    inst.laneWidth = parse_lane_width(area, l, args[0], 1);
                }

                //consume first argument
                argShift = 1;
                args += 1;
                argCount -= 1;
            }

            if (argCount == 2) {
                //parse first arg
                if (args[0].type == TOKEN_CONST) {
                    inst.literal = parse_const(area, l, args[0], inst.laneWidth, 1 + argShift);
                } else {
                    inst.source = parse_loc(area, l, args[0], 1 + argShift);
                }

                //parse second arg
                if (args[1].type == TOKEN_CONST) {
                    set_error(area, l, args[1].start, args[1].end,
                        "ARGUMENT %d MUST BE A REGISTER, PORT, OR BUS", 2 + argShift);
                } else {
                    inst.destination = parse_loc(area, l, args[1], 2 + argShift);
                }
            } else {
                set_error(area, l, 0, strlen(line), "WRONG NUMBER OF ARGUMENTS SUPPLIED");
            }
        } else if (instructionType == INST_JMP) {
            if (argCount == 1) {
                if (args[0].type == TOKEN_CONST) {
                    set_error(area, l, args[0].start, args[0].end, "ARGUMENT 1 MUST NAME A LABEL");
                } else {
                    int idx = find_label_in_list(labels, labelCount, line, args[0]);
                    if (idx >= 0) {
                        inst.labelIndex = idx;
                    } else {
                        //DEBUG PRINT
                        printf("ADDING PLACEHOLDER LABEL: %.*s LINE %d\n",
                            args[0].end - args[0].start, line + args[0].start, l);

                        labels[labelCount] = { line + args[0].start,
                            args[0].end - args[0].start, l, false };
                        inst.labelIndex = labelCount;

                        ++labelCount;
                    }
                }
            } else {
                set_error(area, l, 0, strlen(line), "WRONG NUMBER OF ARGUMENTS SUPPLIED");
            }
        } else if (instructionType == INST_JEQ) {
            //consume optional lane width argument
            int argShift = 0;
            if (argCount == 4) {
                if (args[0].type == TOKEN_CONST) {
                    set_error(area, l, args[0].start, args[0].end,
                        "INTEGER LITERAL SUPPLIED, LANE WIDTH EXPECTED (ARGUMENT 1)");
                } else {
                    inst.laneWidth = parse_lane_width(area, l, args[0], 1);
                }

                //consume first argument
                argShift = 1;
                args += 1;
                argCount -= 1;
            }

            if (argCount == 3) {
                //parse first and second args
                if (args[0].type == TOKEN_CONST && args[1].type == TOKEN_CONST) {
                    int first = parse_const(area, l, args[0], 16, 1);
                    int second = parse_const(area, l, args[1], 16, 1);

                    //const==const comparisons will always branch the same way,
                    //so we replace them with jmp or nop, respectively
                    inst.type = first == second? INST_JMP : INST_NONE;
                } else if (args[0].type == TOKEN_CONST && args[1].type == TOKEN_IDENTIFIER) {
                    inst.literal = parse_const(area, l, args[0], 16, 1);
                    inst.destination = parse_loc(area, l, args[1], 2);
                } else if (args[0].type == TOKEN_IDENTIFIER && args[1].type == TOKEN_CONST) {
                    inst.literal = parse_const(area, l, args[1], 16, 2);
                    inst.destination = parse_loc(area, l, args[0], 1);
                } else { //both identifiers
                    inst.source = parse_loc(area, l, args[0], 1);
                    inst.destination = parse_loc(area, l, args[1], 2);
                }

                //parse third arg
                if (args[2].type == TOKEN_CONST) {
                    set_error(area, l, args[2].start, args[2].end, "ARGUMENT 3 MUST NAME A LABEL");
                } else {
                    int idx = find_label_in_list(labels, labelCount, line, args[2]);
                    if (idx >= 0) {
                        inst.labelIndex = idx;
                    } else {
#if 0
                        //DEBUG PRINT
                        printf("ADDING PLACEHOLDER LABEL: %.*s LINE %d\n",
                            args[2].end - args[2].start, line + args[2].start, l);
#endif

                        labels[labelCount] = { line + args[2].start,
                            args[2].end - args[2].start, l, false };
                        inst.labelIndex = labelCount;

                        ++labelCount;
                    }
                }
            } else {
                set_error(area, l, 0, strlen(line), "WRONG NUMBER OF ARGUMENTS SUPPLIED");
            }
        } else if (instructionType == INST_JGT) {
            //TODO: same as above (INST_JEQ) with a few minor changes
        } else if (instructionType == INST_WAIT) {
            //TODO: allow this instruction to specify a lane width
            //TODO: maybe try to collapse this down like we did for the other ones
            //      by parsing the first (BUS) arg and then shifting?
            if (argCount == 1) {
                if (args[0].type == TOKEN_CONST) {
                    set_error(area, l, args[0].start, args[0].end,
                        "ARGUMENT 1 MUST SPECIFY A DATA BUS");
                } else {
                    inst.source = parse_loc(area, l, args[0], 1);

                    //error if an invalid (non-bus) loc was provided
                    if (inst.source != LOC_LEFT && inst.source != LOC_RIGHT) {
                        set_error(area, l, args[0].start, args[0].end,
                            "ARGUMENT 1 MUST SPECIFY A DATA BUS");
                    }
                }

                inst.literal = -1;
            } else if (argCount == 2) {
                //parse first arg
                if (args[0].type == TOKEN_CONST) {
                    set_error(area, l, args[0].start, args[0].end,
                        "ARGUMENT 1 MUST SPECIFY A DATA BUS");
                } else {
                    inst.source = parse_loc(area, l, args[0], 1);

                    //error if an invalid (non-bus) loc was provided
                    if (inst.source != LOC_LEFT && inst.source != LOC_RIGHT) {
                        set_error(area, l, args[0].start, args[0].end,
                            "ARGUMENT 1 MUST SPECIFY A DATA BUS");
                    }
                }

                //parse second arg
                if (args[1].type == TOKEN_CONST) {
                    inst.literal = parse_const(area, l, args[1], 16, 2);
                } else {
                    set_error(area, l, args[0].start, args[0].end,
                        "ARGUMENT 1 MUST SPECIFY AN INTEGER CONSTANT");
                }
            } else {
                set_error(area, l, 0, strlen(line), "WRONG NUMBER OF ARGUMENTS SUPPLIED");
            }
        }

        area->instructions[l] = inst;
    }

    //convert label indices to line numbers (and error if there are mismatches)
    for (int i = 0; i < MAX_LINES; ++i) {
        Instruction * inst = area->instructions + i;
        if (inst->type == INST_JMP ||
            inst->type == INST_JEQ ||
            inst->type == INST_JGT) {
            Label * label = labels + inst->labelIndex;
            if (label->defined) {
                inst->labelIndex = label->line;
            } else {
                //TODO: properly set the extent of this error message to underline only the label
                set_error(area, i, 0, strlen(area->text[i]),
                    "LABEL IS NOT DEFINED: %.*s", label->len, label->name);
            }
        }
    }

#if 0
    //DEBUG PRINT
    for (int i = 0; i < labelCount; ++i) {
        printf("LABEL LINE %d: %.*s\n", labels[i].line, labels[i].len, labels[i].name);
    }

    //DEBUG PRINT
    for (int i = 0; i < MAX_LINES; ++i) {
        Instruction inst = area->instructions[i];
        printf("LINE %2d TYPE %2d WIDTH %2d LITERAL %2lld SOURCE %d DESTINATION %d LABEL %d\n",
            i, inst.type, inst.laneWidth, inst.literal, inst.source, inst.destination,
            inst.labelIndex);
    }
#endif
}
