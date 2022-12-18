#include <shell.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

Shell lsh_shell_initialise(void) {
    Shell info = {.terminal = STDIN_FILENO};
    info.is_interactive = isatty(info.terminal);
    if(!info.is_interactive) {
        // We do not support the non-interactive mode.
        fprintf(stderr,
                "shell_initialise: shell not running in interactive mode");
        exit(EXIT_FAILURE);
    }

    // Loop until in the foreground.
    while(true) {
        info.pgid = getpgrp();
        pid_t const tcpgid = tcgetpgrp(info.terminal);
        if(tcpgid == info.pgid) {
            break;
        } else {
            kill(info.pgid, SIGTTIN);
        }
    }

    // Ignore interactive signals.
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    info.pid = getpid();
    if(setpgid(info.pid, info.pid)) {
        perror("shell_initialise: could not create own process group");
        exit(-1);
    }

    info.pgid = info.pid;
    // Set ourselves in the foreground.
    tcsetpgrp(info.terminal, info.pgid);
    tcgetattr(info.terminal, &info.attributes);
    return info;
}

char* lsh_get_cwd(void) {
    // glibc's getcwd allocated the buffer for us if we pass NULL and 0 as the
    // parameters.
    return getcwd(NULL, 0);
}
