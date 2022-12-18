#include <jobs.h>
#include <parser.h>
#include <shell.h>

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static Process* lsh_create_process_from_command(Command command) {
    Process* process = NULL;
    Process* current_process = NULL;
    Process_Args* current = NULL;
    Process_Args* next = command.args;
    while(next != NULL) {
        current = next;
        next = next->next;
        if(process == NULL) {
            process = lsh_alloc_and_zero(sizeof(Process));
            current_process = process;
        } else {
            Process* new_process = lsh_alloc_and_zero(sizeof(Process));
            current_process->next = new_process;
            current_process = new_process;
        }

        current_process->args = current->values;
        current->values = NULL;

        if(current->redirect_in != NULL) {
            current_process->fd.in =
                open(current->redirect_in, O_RDONLY | O_CREAT);
        } else {
            current_process->fd.in = STDIN_FILENO;
        }

        if(current->redirect_out != NULL) {
            current_process->fd.out =
                open(current->redirect_out, O_WRONLY | O_CREAT);
        } else {
            current_process->fd.out = STDOUT_FILENO;
        }

        if(current->redirect_err != NULL) {
            current_process->fd.err =
                open(current->redirect_err, O_WRONLY | O_CREAT);
        } else {
            current_process->fd.err = STDERR_FILENO;
        }
    }
    return process;
}

static char const* const lsh_cwd_unknown = "<unknown>";
static char const* const lsh_lsh_color = "22;198;12";
static char const* const lsh_cwd_color = "56;114;242";

static void print_header(char const* const cwd) {
    if(cwd != NULL) {
        printf("\033[38;2;%smlsh \033[38;2;%sm%s\033[0m$ ", lsh_lsh_color,
               lsh_cwd_color, cwd);
    } else {
        printf("\033[38;2;%smlsh \033[38;2;%sm%s\033[0m$ ", lsh_lsh_color,
               lsh_cwd_color, lsh_cwd_unknown);
    }
}

int main(void) {
    Shell shell = lsh_shell_initialise();
    lsh_jobs_initialise();
    while(true) {
        char* const cwd = lsh_get_cwd();
        print_header(cwd);
        free(cwd);

        char* line = NULL;
        int const getline_result = lsh_getline(&line);
        if(getline_result == -1) {
            exit(EXIT_SUCCESS);
        }

        lsh_update_job_statuses();
        lsh_cleanup_jobs();

        if(getline_result == 0) {
            continue;
        }

        Parse_Result parse_result = lsh_parse(line);
        if(parse_result.kind == PARSE_ERROR) {
            fprintf(stderr, "lsh: %s\n", parse_result.error);
            free(parse_result.error);
            free(line);
            continue;
        }

        Command command = parse_result.value;
        Job* const job = lsh_create_job();
        job->command = line;
        job->first_process = lsh_create_process_from_command(command);
        lsh_start_job(&shell, job, command.foreground);
        lsh_free_command(command);
    }
    return 0;
}
