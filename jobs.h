#pragma once

#include <common.h>
#include <shell.h>

#include <sys/types.h>
#include <termios.h>

typedef struct Job_List Job_List;
typedef struct Job_List_Entry Job_List_Entry;
typedef struct Job Job;

Job_List* lsh_get_primary_job_list(void);

Job_List_Entry* lsh_job_list_begin(Job_List* list);
Job_List_Entry* lsh_job_list_end(Job_List* list);
Job_List_Entry* lsh_job_list_next(Job_List_Entry* entry);
Job_List_Entry* lsh_job_list_prev(Job_List_Entry* entry);
Job* lsh_job_list_value(Job_List_Entry* entry);
void lsh_job_list_erase(Job_List_Entry* entry);
Job* lsh_find_job_with_id(Job_List* list, int id);

void lsh_jobs_initialise(void);

typedef enum Process_Status {
    PROCESS_RUNNING,
    PROCESS_STOPPED,
    PROCESS_COMPLETED,
    PROCESS_TERMINATED,
} Process_Status;

typedef struct Process {
    struct Process* next;
    char** args;
    pid_t pid;
    Process_Status status;
    Descriptors fd;
} Process;

Process* lsh_find_process_with_pid(pid_t pid);

typedef struct Job {
    int id;
    pid_t pgid;
    Process* first_process;
    char const* command;
    struct termios attributes;
} Job;

Job* lsh_get_current_job(void);

Job* lsh_create_job(void);

bool lsh_is_job_stopped(Job* job);
bool lsh_is_job_completed(Job* job);
bool lsh_is_job_terminated(Job* job);

void lsh_print_job_status(Job* job, int fd_out);
// lsh_update_job_statuses
//
void lsh_update_job_statuses(void);
void lsh_cleanup_jobs(void);

// lsh_start_job
//
// Parameters:
// foreground - whether to start the job in the foreground.
//
void lsh_start_job(Shell* shell, Job* job, bool foreground);

// lsh_set_job_in_foreground
// Move the job to the foreground.
//
// Parameters:
// send_continue - whether to send the continue signal to the job.
//
void lsh_set_job_in_foreground(Shell const* shell, Job* job,
                               bool send_continue);

// lsh_set_job_in_background
// Move the job to the background.
//
// Parameters:
// send_continue - whether to send the continue signal to the job.
//
void lsh_set_job_in_background(Shell const* shell, Job* job,
                               bool send_continue);
