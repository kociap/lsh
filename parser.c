#include "common.h"
#include <parser.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void free_process_args(Process_Args* args) {
    if(args->values != NULL) {
        for(char** c = args->values; *c != NULL; ++c) {
            free(*c);
        }
        free(args->values);
    }
    free(args->redirect_in);
    free(args->redirect_out);
    free(args->redirect_err);
    free(args);
}

void lsh_free_command(Command command) {
    for(Process_Args* current = command.args; current != NULL;) {
        Process_Args* next = current->next;
        free_process_args(current);
        current = next;
    }
}

static bool lsh_is_whitespace(char c) {
    return c <= 32;
}

static bool lsh_is_string_character(char c) {
    return (c >= 48 && c <= 57) || (c >= 65 && c <= 90) ||
           (c >= 97 && c <= 122) || c == '"' || c == '\'' || c == '.' ||
           c == '/' || c == '%' || c == '-';
}

typedef enum Token_Kind {
    TOKEN_NONE,
    TOKEN_STRING,
    TOKEN_PIPE,
    TOKEN_AMP,
    TOKEN_REDIRECT_IN,
    TOKEN_REDIRECT_OUT,
    TOKEN_REDIRECT_ERR,
} Token_Kind;

typedef struct Token {
    Token_Kind kind;
    char const* begin;
    char const* end;
} Token;

static bool lsh_match(char const* begin, char const* pattern) {
    while(*begin != '\0' && *pattern != '\0' && *begin == *pattern) {
        ++begin;
        ++pattern;
    }

    return *pattern == '\0';
}

static Token lsh_tokenise(char const* begin) {
    // Ignore leading whitespace.
    while(*begin != '\0' && lsh_is_whitespace(*begin)) {
        ++begin;
    }

    Token token = {.kind = TOKEN_NONE, .begin = begin, .end = begin};
    if(*begin == '\0') {
        return token;
    }

    if(lsh_match(begin, "|")) {
        token.kind = TOKEN_PIPE;
        token.end = begin + 1;
    } else if(lsh_match(begin, "&")) {
        token.kind = TOKEN_AMP;
        token.end = begin + 1;
    } else if(lsh_match(begin, "2>")) {
        token.kind = TOKEN_REDIRECT_ERR;
        token.end = begin + 2;
    } else if(lsh_match(begin, ">")) {
        token.kind = TOKEN_REDIRECT_OUT;
        token.end = begin + 1;
    } else if(lsh_match(begin, "<")) {
        token.kind = TOKEN_REDIRECT_IN;
        token.end = begin + 1;
    } else if(lsh_is_string_character(*begin)) {
        token.kind = TOKEN_STRING;
        bool quoted = false;
        for(; *begin != '\0'; ++begin) {
            if(*begin == '"' || *begin == '\'') {
                quoted = !quoted;
                continue;
            }

            if(quoted) {
                continue;
            }

            if(!lsh_is_string_character(*begin)) {
                break;
            }
        }
        token.end = begin;
    }
    return token;
}

static char* lsh_normalise_token_string(Token const token) {
    int const size = token.end - token.begin;
    char* const buffer = lsh_alloc_and_zero(size + 1);
    char const* b = token.begin;
    char* i = buffer;
    for(; b != token.end; ++b) {
        if(*b != '"' && *b != '\'') {
            *i = *b;
            ++i;
        }
    }
    return buffer;
}

static bool lsh_parse_background_marker(char const** string) {
    Token const token = lsh_tokenise(*string);
    if(token.kind == TOKEN_AMP) {
        *string = token.end;
        return true;
    } else {
        return false;
    }
}

static bool lsh_parse_pipe(char const** string) {
    Token const token = lsh_tokenise(*string);
    if(token.kind == TOKEN_PIPE) {
        *string = token.end;
        return true;
    } else {
        return false;
    }
}

static bool lsh_parse_redirect(char const** string, Process_Args* const args) {
    char const* const backup = *string;
    while(true) {
        Token const token = lsh_tokenise(*string);
        if(token.kind == TOKEN_REDIRECT_IN) {
            Token const loc = lsh_tokenise(token.end);
            if(loc.kind != TOKEN_STRING) {
                *string = backup;
                return false;
            }

            if(args->redirect_in != NULL) {
                free(args->redirect_in);
            }
            args->redirect_in = lsh_normalise_token_string(loc);
        } else if(token.kind == TOKEN_REDIRECT_OUT) {
            Token const loc = lsh_tokenise(token.end);
            if(loc.kind != TOKEN_STRING) {
                *string = backup;
                return false;
            }

            if(args->redirect_out != NULL) {
                free(args->redirect_out);
            }
            args->redirect_out = lsh_normalise_token_string(loc);
        } else if(token.kind == TOKEN_REDIRECT_ERR) {
            Token const loc = lsh_tokenise(token.end);
            if(loc.kind != TOKEN_STRING) {
                *string = backup;
                return false;
            }

            if(args->redirect_err != NULL) {
                free(args->redirect_err);
            }
            args->redirect_err = lsh_normalise_token_string(loc);
        }

        return true;
    }
}

static bool lsh_parse_single_process(char const** string,
                                     Process_Args** const args) {
    int args_capacity = 0;
    int args_size = 0;
    while(true) {
        Token const token = lsh_tokenise(*string);
        if(token.kind == TOKEN_STRING) {
            if(*args == NULL) {
                *args = lsh_alloc_and_zero(sizeof(Process_Args));
            }

            if(args_size + 2 >= args_capacity) {
                args_capacity = (args_capacity == 0 ? 64 : args_capacity * 2);
                (*args)->values = realloc((*args)->values, args_capacity);
                if(!(*args)->values) {
                    fprintf(stderr,
                            "lsh_parse_single_process: allocation failure");
                    exit(EXIT_FAILURE);
                }
            }

            (*args)->values[args_size] = lsh_normalise_token_string(token);
            (*args)->values[args_size + 1] = NULL;
            args_size += 1;

            *string = token.end;
        } else {
            break;
        }
    }

    bool const redirect_result = lsh_parse_redirect(string, *args);
    if(!redirect_result) {
        free_process_args(*args);
        return false;
    }

    return true;
}

static bool lsh_parse_command(char const** string, Command* const command) {
    Process_Args* last_args = NULL;
    while(true) {
        Process_Args* out_args = NULL;
        bool const result = lsh_parse_single_process(string, &out_args);
        if(!result) {
            lsh_free_command(*command);
            return false;
        }

        if(last_args == NULL) {
            command->args = out_args;
            last_args = out_args;
        } else {
            last_args->next = out_args;
            last_args = out_args;
        }

        if(!lsh_parse_pipe(string)) {
            break;
        }
    }

    command->foreground = !lsh_parse_background_marker(string);
    return true;
}

Parse_Result lsh_parse(char const* command_string) {
    Command command;
    if(lsh_parse_command(&command_string, &command)) {
        return (Parse_Result){.kind = PARSE_VALUE, .value = command};
    } else {
        char const msg[] = "syntax error";
        return (Parse_Result){
            .kind = PARSE_ERROR,
            .error = lsh_allocate_from_slice(msg, msg + sizeof(msg))};
    }
}
