#include <jobs.h>

#include <builtin.h>

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct Job_List_Entry {
    struct Job_List_Entry* prev;
    struct Job_List_Entry* next;
    Job job;
};

typedef struct {
    Job_List_Entry* prev;
    Job_List_Entry* next;
} Fake_Job_List_Entry;

struct Job_List {
    Fake_Job_List_Entry _node;
};

static Job_List job_list;
static Job* current_job = NULL;

static void lsh_job_list_initialise(Job_List* const list) {
    list->_node.prev = (Job_List_Entry*)&list->_node;
    list->_node.next = (Job_List_Entry*)&list->_node;
}

Job_List* lsh_get_primary_job_list(void) {
    return &job_list;
}

Job_List_Entry* lsh_job_list_begin(Job_List* const list) {
    return list->_node.next;
}

Job_List_Entry* lsh_job_list_end(Job_List* const list) {
    return (Job_List_Entry*)&list->_node;
}

Job_List_Entry* lsh_job_list_next(Job_List_Entry* const entry) {
    return entry->next;
}

Job_List_Entry* lsh_job_list_prev(Job_List_Entry* const entry) {
    return entry->prev;
}

Job* lsh_job_list_value(Job_List_Entry* const entry) {
    return &entry->job;
}

static Job* lsh_job_list_push_back(Job_List* list) {
    Job_List_Entry* const entry = lsh_alloc_and_zero(sizeof(Job_List_Entry));
    Job_List_Entry* const prev = (Job_List_Entry*)list->_node.prev;
    Job_List_Entry* const next = (Job_List_Entry*)prev->next;
    prev->next = entry;
    entry->prev = prev;
    next->prev = entry;
    entry->next = next;
    return &entry->job;
}

void lsh_job_list_erase(Job_List_Entry* const entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;

    Job* const job = &entry->job;
    for(Process* process = job->first_process; process != NULL;) {
        for(char** b = process->args; *b != NULL; ++b) {
            free(*b);
        }
        free(process->args);
        Process* current = process;
        process = process->next;
        free(current);
    }
    free(entry);
}

Job* lsh_find_job_with_id(Job_List* const list, int const id) {
    for(Job_List_Entry *b = lsh_job_list_begin(list),
                       *e = lsh_job_list_end(list);
        b != e; b = lsh_job_list_next(b)) {
        Job* job = lsh_job_list_value(b);
        if(job->id == id) {
            return job;
        }
    }
    return NULL;
}

void lsh_jobs_initialise(void) {
    lsh_job_list_initialise(&job_list);
}

Job* lsh_get_current_job(void) {
    return current_job;
}

Process* lsh_find_process_with_pid(pid_t const pid) {
    for(Job_List_Entry *b = lsh_job_list_begin(&job_list),
                       *e = lsh_job_list_end(&job_list);
        b != e; b = lsh_job_list_next(b)) {
        Job* job = lsh_job_list_value(b);
        for(Process* process = job->first_process; process != NULL;
            process = process->next) {
            if(process->pid == pid) {
                return process;
            }
        }
    }
    return NULL;
}

Job* lsh_create_job(void) {
    Job_List_Entry* const end = lsh_job_list_end(&job_list);
    Job_List_Entry* const prev = lsh_job_list_prev(end);
    int id = 1;
    if(prev != end) {
        Job* job = lsh_job_list_value(prev);
        id = job->id + 1;
    }

    Job* const job = lsh_job_list_push_back(&job_list);
    job->id = id;
    return job;
}

bool lsh_is_job_stopped(Job* job) {
    for(Process* process = job->first_process; process != NULL;
        process = process->next) {
        if(process->status == PROCESS_RUNNING) {
            return false;
        }
    }
    return true;
}

bool lsh_is_job_completed(Job* job) {
    for(Process* process = job->first_process; process != NULL;
        process = process->next) {
        if(process->status != PROCESS_COMPLETED &&
           process->status != PROCESS_TERMINATED) {
            return false;
        }
    }
    return true;
}

bool lsh_is_job_terminated(Job* job) {
    for(Process* process = job->first_process; process != NULL;
        process = process->next) {
        if(process->status != PROCESS_TERMINATED) {
            return false;
        }
    }
    return true;
}

static void lsh_update_process_status(pid_t const pid, int const code) {
    Process* const process = lsh_find_process_with_pid(pid);
    if(process == NULL) {
        return;
    }

    switch(code) {
    case CLD_EXITED:
        process->status = PROCESS_COMPLETED;
        break;
    case CLD_KILLED:
    case CLD_DUMPED:
        process->status = PROCESS_TERMINATED;
        break;
    case CLD_STOPPED:
        process->status = PROCESS_STOPPED;
        break;
    case CLD_CONTINUED:
        process->status = PROCESS_RUNNING;
        break;
    default:
        break;
    }
}

void lsh_print_job_status(Job* const job, int const fd_out) {
    bool const stopped = lsh_is_job_stopped(job);
    bool const completed = lsh_is_job_completed(job);
    bool const terminated = lsh_is_job_terminated(job);
    if(terminated) {
        dprintf(fd_out, "[%d] Terminated %s\n", job->id, job->command);
    } else if(completed) {
        dprintf(fd_out, "[%d] Completed %s\n", job->id, job->command);
    } else if(stopped) {
        dprintf(fd_out, "[%d] Stopped %s\n", job->id, job->command);
    } else {
        dprintf(fd_out, "[%d] Running %s\n", job->id, job->command);
    }
}

void lsh_update_job_statuses(void) {
    // We poll the statuses of all our child processes.
    int status = 0;
    while(true) {
        siginfo_t info = {0};
        status =
            waitid(P_ALL, 0, &info, WNOHANG | WEXITED | WSTOPPED | WCONTINUED);
        if(info.si_pid == 0 || status != 0) {
            break;
        }

        lsh_update_process_status(info.si_pid, info.si_code);
    }

    if(status != 0 && errno == ECHILD) {
        // There are no child processes, therefore all jobs have been terminated
        // and we may mark them as such.
        for(Job_List_Entry *b = lsh_job_list_begin(&job_list),
                           *e = lsh_job_list_end(&job_list);
            b != e; b = lsh_job_list_next(b)) {
            Job* job = lsh_job_list_value(b);
            for(Process* process = job->first_process; process != NULL;
                process = process->next) {
                if(process->status != PROCESS_TERMINATED) {
                    process->status = PROCESS_COMPLETED;
                }
            }
        }
    }
}

void lsh_cleanup_jobs(void) {
    Job_List_Entry* const end = lsh_job_list_end(&job_list);
    for(Job_List_Entry* b = lsh_job_list_begin(&job_list); b != end;) {
        Job_List_Entry* const next = lsh_job_list_next(b);
        Job* job = lsh_job_list_value(b);
        if(lsh_is_job_completed(job)) {
            if(job == current_job) {
                current_job = NULL;
            }
            lsh_print_job_status(job, STDOUT_FILENO);
            lsh_job_list_erase(b);
        }
        b = next;
    }

    if(current_job == NULL && end->prev != end) {
        current_job = &end->prev->job;
    }
}

// lsh_run_process
// Start a child process.
//
// Parameters:
// file - the name of the file that is to be executed.
// args - null-terminated array of arguments to the program. The first argument
//        ought to be the name of the file being executed.
//
// Returns:
// The PID of the child process or -1 if an error occured.
//
static pid_t lsh_run_process(Shell* const shell, char* const* args,
                             pid_t const pgid, Descriptors const fd,
                             bool const foreground) {
    pid_t const pid = fork();
    if(pid != 0) { // Parent
        if(pgid == 0) {
            setpgid(pid, pid);
        } else {
            setpgid(pid, pgid);
        }
        return pid;
    } else { // Child
        pid_t const child_pid = getpid();
        pid_t const child_pgid = (pgid == 0 ? child_pid : pgid);
        setpgid(child_pid, child_pgid);

        if(foreground) {
            tcsetpgrp(shell->terminal, child_pgid);
        }

        // Shell set its signals to SIG_IGN. We inherited those, therefore we
        // have to reset them to SIG_DFL.
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        if(fd.in != STDIN_FILENO) {
            dup2(fd.in, STDIN_FILENO);
            close(fd.in);
        }

        if(fd.out != STDOUT_FILENO) {
            dup2(fd.out, STDOUT_FILENO);
            close(fd.out);
        }

        if(fd.err != STDERR_FILENO) {
            dup2(fd.err, STDERR_FILENO);
            close(fd.err);
        }

        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}

static void lsh_close(int const fd) {
    if(fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        close(fd);
    }
}

void lsh_start_job(Shell* const shell, Job* const job, bool const foreground) {
    if(foreground) {
        current_job = job;
    }

    int fd_pipe[2];
    Descriptors fd = {
        .in = STDIN_FILENO,
        .out = STDOUT_FILENO,
        .err = STDERR_FILENO,
    };
    for(Process* process = job->first_process; process != NULL;
        process = process->next) {
        // Set up pipe.
        if(process->next) {
            if(pipe(fd_pipe) < 0) {
                perror("lsh_start_job: pipe failed");
                exit(EXIT_FAILURE);
            }
            fd.out = fd_pipe[1];
        }

        // Redirects take priority over pipes, therefore we overwrite.
        if(process->fd.in != STDIN_FILENO) {
            // Close previous pipe.
            lsh_close(fd.in);
            fd.in = process->fd.in;
        }

        if(process->fd.out != STDOUT_FILENO) {
            fd.out = process->fd.out;
        }

        if(process->fd.err != STDERR_FILENO) {
            fd.err = process->fd.err;
        }

        Builtin_Fn const* const builtin = lsh_find_builtin(process->args[0]);
        if(builtin != NULL) {
            // TODO: Ignore status.
            builtin->fn(shell, process->args, fd);
            process->status = PROCESS_COMPLETED;
        } else {
            pid_t const pid = lsh_run_process(shell, process->args, job->pgid,
                                              fd, foreground);
            process->pid = pid;
            if(job->pgid == 0) {
                job->pgid = pid;
            }
        }

        lsh_close(fd.in);
        lsh_close(fd_pipe[1]);
        fd.in = fd_pipe[0];
        fd.out = STDOUT_FILENO;
        fd.err = STDERR_FILENO;
    }

    if(job->pgid == 0) {
        // Job consisted only of builtin commands, which execute immediately,
        // therefore the job is complete and there is nothing else to be done.
        return;
    }

    if(foreground) {
        lsh_set_job_in_foreground(shell, job, false);
    } else {
        lsh_set_job_in_background(shell, job, false);
    }
}

static void lsh_wait_for(Job* const job) {
    while(true) {
        siginfo_t info = {0};
        int const status = waitid(P_ALL, 0, &info, WEXITED | WSTOPPED);
        if(status != 0 || info.si_pid == 0) {
            break;
        }

        lsh_update_process_status(info.si_pid, info.si_code);
        if(lsh_is_job_completed(job) || lsh_is_job_stopped(job)) {
            break;
        }
    }
}

void lsh_set_job_in_foreground(Shell const* const shell, Job* const job,
                               bool const send_continue) {
    tcsetpgrp(shell->terminal, job->pgid);

    if(send_continue) {
        tcsetattr(shell->terminal, TCSADRAIN, &job->attributes);
        int const status = kill(job->pgid, SIGCONT);
        if(status < 0) {
            perror("lsh_set_job_in_foreground: failed to send SIGCONT");
        }
    }

    lsh_wait_for(job);

    // Restore control to the shell.
    tcsetpgrp(shell->terminal, shell->pgid);
    tcgetattr(shell->terminal, &job->attributes);
    tcsetattr(shell->terminal, TCSADRAIN, &shell->attributes);
}

void lsh_set_job_in_background(Shell const* const shell, Job* const job,
                               bool const send_continue) {
    UNUSED(shell);
    if(send_continue) {
        int const status = kill(job->pgid, SIGCONT);
        if(status < 0) {
            perror("lsh_set_job_in_background: failed to send SIGCONT");
        }
    }
}
