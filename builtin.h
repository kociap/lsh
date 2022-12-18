#pragma once

#include <common.h>
#include <shell.h>

typedef int (*builtin_fn_t)(Shell*, char**);

typedef struct Builtin_Fn {
    char const* name;
    builtin_fn_t fn;
} Builtin_Fn;

Builtin_Fn const* lsh_find_builtin(char const* name);
