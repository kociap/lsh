#pragma once

#include <common.h>

typedef struct Process_Args {
    struct Process_Args* next;
    char** values;
    char* redirect_in;
    char* redirect_out;
    char* redirect_err;
} Process_Args;

typedef struct Command {
    Process_Args* args;
    bool foreground;
} Command;

typedef enum Parse_Result_Kind {
    PARSE_VALUE,
    PARSE_ERROR,
} Parse_Result_Kind;

typedef struct Parse_Result {
    Parse_Result_Kind kind;
    union {
        Command value;
        char* error;
    };
} Parse_Result;

Parse_Result lsh_parse(char const* command_string);
void lsh_free_command(Command command);
