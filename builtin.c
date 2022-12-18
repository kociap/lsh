#include <builtin.h>

#include <jobs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int lsh_builtin_exit(Shell* const shell, char** const args,
                            Descriptors const fd) {
    UNUSED(args);
    UNUSED(shell);
    UNUSED(fd);
    exit(EXIT_SUCCESS);
}

static int lsh_builtin_cd(Shell* const shell, char** const args,
                          Descriptors const fd) {
    UNUSED(shell);
    if(args[1] == NULL) {
        dprintf(fd.err, "cd: expected argument");
        return 1;
    } else {
        if(chdir(args[1]) != 0) {
            perror("cd");
            return 1;
        }
    }
    return 0;
}

static int lsh_builtin_jobs(Shell* const shell, char** const args,
                            Descriptors const fd) {
    UNUSED(args);
    UNUSED(shell);
    Job* const current_job = lsh_get_current_job();
    Job_List* const job_list = lsh_get_primary_job_list();
    for(Job_List_Entry *b = lsh_job_list_begin(job_list),
                       *e = lsh_job_list_end(job_list);
        b != e; b = lsh_job_list_next(b)) {
        Job* job = lsh_job_list_value(b);
        if(job == current_job) {
            continue;
        }

        lsh_print_job_status(job, fd.out);
    }
    return 0;
}

static int lsh_builtin_fg(Shell* const shell, char** const args,
                          Descriptors const fd) {
    // TODO: We do not handle the status properly.
    if(args[1] == NULL) {
        Job* const job = lsh_get_current_job();
        if(job == NULL) {
            dprintf(fd.err, "fg: no current job\n");
        } else {
            lsh_set_job_in_foreground(shell, job, true);
        }
    } else {
        int const id = atoi(args[1]);
        Job_List* const job_list = lsh_get_primary_job_list();
        Job* const job = lsh_find_job_with_id(job_list, id);
        if(job == NULL) {
            dprintf(fd.err, "fg: job with id %d not found", id);
        } else {
            lsh_set_job_in_foreground(shell, job, true);
        }
    }
    return 0;
}

static int lsh_builtin_bg(Shell* const shell, char** const args,
                          Descriptors const fd) {
    // TODO: We do not handle the status properly.
    if(args[1] == NULL) {
        Job* const job = lsh_get_current_job();
        if(job == NULL) {
            dprintf(fd.err, "fg: no current job\n");
        } else {
            lsh_set_job_in_background(shell, job, true);
        }
    } else {
        int const id = atoi(args[1]);
        Job_List* const job_list = lsh_get_primary_job_list();
        Job* const job = lsh_find_job_with_id(job_list, id);
        if(job == NULL) {
            dprintf(fd.err, "fg: job with id %d not found", id);
        } else {
            lsh_set_job_in_background(shell, job, true);
        }
    }
    return 0;
}

static Builtin_Fn const builtin_fns[] = {{"exit", lsh_builtin_exit},
                                         {"cd", lsh_builtin_cd},
                                         {"jobs", lsh_builtin_jobs},
                                         {"fg", lsh_builtin_fg},
                                         {"bg", lsh_builtin_bg}};

Builtin_Fn const* lsh_find_builtin(char const* const name) {
    for(Builtin_Fn const *
            b = builtin_fns,
           *e = builtin_fns + sizeof(builtin_fns) / sizeof(Builtin_Fn);
        b != e; ++b) {
        if(strcmp(b->name, name) == 0) {
            return b;
        }
    }

    return NULL;
}
