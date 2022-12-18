#pragma once

#include <common.h>

#include <sys/types.h>
#include <termios.h>

typedef struct Shell {
    int terminal;
    pid_t pgid;
    pid_t pid;
    bool is_interactive;
    struct termios attributes;
} Shell;

// shell_initialise
// Initialise the shell.
//
Shell lsh_shell_initialise(void);

// lsh_get_cwd
// Obtain the current working directory of the calling process as an absolute
// path. Caller must free the buffer.
//
char* lsh_get_cwd(void);
