#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#define MAX_COMMANDS 128
#define MAX_INPUT_LEN 256
#define MAX_INPUT_WORDS 128


int command_count = 0;
int command_index = 0;
char *history[MAX_COMMANDS];


int launch(char **command, int n, int *offsets, int *background);
int read_user_input(char *input, char **command, int *n, int *offsets, int *background);

void add_command_to_history(char *input){
    if (command_count != 0) {
        if (strcmp(history[(command_index - 1)%MAX_COMMANDS], input) == 0) return;
    }
    if (command_index >= MAX_COMMANDS){
        command_index %= MAX_COMMANDS;
        free(history[command_index]);
    }
    history[command_index++] = strdup(input);
    ++command_count;
}

void print_history(){
    if (command_count == command_index){
        for (int ind = 0; ind < command_count; ind++){
            printf("%d %s\n", ind + 1, history[ind]);
        }
    } else {
        for (int ind = command_index + 1; ind <= command_index + MAX_COMMANDS; ind++){
            printf("%d %s\n", ind - command_index, history[(ind%MAX_COMMANDS)]);
        }
    }
}

struct Details{
    char *command;
    int pid_list[MAX_INPUT_WORDS];
    int pid_index;
    time_t start_time;
    double execution_time;
};

struct Details details[MAX_COMMANDS];
int details_index = -1;
int details_count = -1;

void add_details(char *input, time_t start){
    ++details_index;
    if (details_index >= MAX_COMMANDS) details_index %= MAX_COMMANDS;
    details[details_index].command = strdup(input);
    details[details_index].start_time = start;
    details[details_index].pid_index = 0;
    ++details_count;
}

void add_pid(int pid){
    details[details_index].pid_list[details[details_index].pid_index++] = pid;
}

void add_execution_time(double execution){
    details[details_index].execution_time = execution;
}

void display_details(){
    printf("\n");
    if (details_count == details_index){
        for (int i = 0; i <= details_count; i++){
            printf("Index: \t\t%d\n", i + 1);
            printf("Command: \t%s\n", details[i].command);
            printf("PID(s): \t");
            for (int j = 0; j < details[i].pid_index; j++){
                printf("%d ", details[i].pid_list[j]);
            }
            printf("\nStart Time: \t%s", ctime(&details[i].start_time));
            printf("Execution Time: %.3f ms\n\n", details[i].execution_time);
        }
    } else {
        for (int ind = details_index + 1; ind <= details_index + MAX_COMMANDS; ind++){
            int i = ind%MAX_COMMANDS;
            printf("Index: %d\n", ind - details_index);
            printf("Command: %s\n", details[i].command);
            printf("PID(s): ");
            for (int j = 0; j <= details[i].pid_index; j++){
                printf("%d ", details[i].pid_list[j]);
            }
            printf("\nStart Time: %s", ctime(&details[i].start_time));
            printf("Execution Time: %.3f ms\n\n", details[i].execution_time);
        }
    }
}

int create_process_and_run(char **command, int fds[2]){
    int shell_status = fork();
    if (shell_status < 0){
        perror("Failed fork");
        exit(0);
    } else if (shell_status == 0){
        if (fds != NULL) {
            close(fds[0]);
            dup2(fds[1], STDOUT_FILENO);
        }
        char *string;
        if (strcmp(command[0], "history") == 0) {
            if (command[1] != NULL) {
                fprintf(stderr, "Usage: history\n");
            } else {
                print_history();
            }
            exit(0);
        } else if ( (string = strrchr(command[0], '.')) != NULL ) {
            if (strcmp(string, ".sh") == 0) {
                if (command[1] != NULL) {
                    fprintf(stderr, "Usage: file.sh");
                    exit(0);
                }
                char *input = malloc(sizeof(char)*MAX_INPUT_LEN);
                char **subcommand = malloc(sizeof(char *)*MAX_INPUT_WORDS);
                char *cwd;
                int *offsets = malloc(sizeof(int *)*MAX_INPUT_WORDS);
                int *background = malloc(sizeof(int *)*MAX_INPUT_WORDS);
                if (input == NULL || subcommand == NULL || offsets == NULL || background == NULL){
                    fprintf(stderr, "Failed Memory Allocation\n");
                    exit(0);
                }
                int n;
                FILE *file = fopen(command[0], "r");
                if (file == NULL){
                    fprintf(stderr, "Failed File Open\n");
                    exit(0);
                }
                while ( fgets(input, MAX_INPUT_LEN, file) != NULL) {
                    if (input[strlen(input)-1] == '\n')
                        input[strlen(input)-1] = '\0';
                    
                    int valid = read_user_input(input, subcommand, &n, offsets, background);
                    if (valid)
                        shell_status = launch(subcommand, n, offsets, background);
                }
                free(input);
                free(subcommand);
                free(offsets);
                if (fclose(file) != 0) fprintf(stderr, "Error Closing File\n");
                exit(0);
            }
        }
        execvp(command[0], command);
        fprintf(stderr, "Failed execution of '%s'", command[0]);
        perror(" command");
        exit(0);
    } else {
        int ret;
        int pid = wait(&ret);
        add_pid(pid);
        if (!WIFEXITED(ret)){
            printf("Abnormal termination of %d\n", pid);
        }
    }

    return shell_status;
}

int launch(char **command, int n, int *offsets, int *background){
    int fds[2];
    int old_stdin = dup(STDIN_FILENO), old_stdout = dup(STDOUT_FILENO);
    int i = 0;
    int status;
    int background_handler = 0;
    while (i != n && (background_handler == 0 || background[i] == background_handler)) {
        // fprintf(stderr, "%d: %s\n", i, command[offsets[i]]);
        if ((i==0 && background[i] != 0) || (i !=0 && background[i-1] != background[i] && background[i] != 0)) {
            // Create a disconnected process for this background branch
            // fprintf(stderr, "Created %d handler starting for %s\n", i, command[offsets[i]]);
            int status = fork();
            if (status < 0) {
                perror("Background fork failed");
            } else if (status == 0) {
                int status2 = fork();
                if (status2 < 0) {
                    perror("Background fork failed");
                } else if (status2 == 0) {
                    // This fork can now wait without repurcussions on the terminal
                    background_handler = background[i];
                    fprintf(stderr, "[%d] %d\n", background_handler, getpid());
                } else {
                    _exit(0);
                }
            } else {
                // Add pid of this background handler in details
                add_pid(status);
                wait(NULL);
                while (background[i] == background[i+1]) {
                    // fprintf(stderr, "Skipping %s for background processor\n", command[i]);
                    i++;
                }
                i++;
                continue;
            }
        }
        // fprintf(stderr, "\tRunning %d's %s\n", background_handler, command[offsets[i]]);

        if (pipe(fds) == -1) {
            close(fds[0]);
            close(fds[1]);
            dup2(old_stdin, STDIN_FILENO);
            dup2(old_stdout, STDOUT_FILENO);
            perror("pipe failed");
            exit(0);
        }
        if (i != 0 && background[i-1] != background[i]) {
            // New batch of commands
            dup2(old_stdin, STDIN_FILENO);
        }
        if (i == n-1 || (i < n-1 && background[i] != background[i+1]) ) {
            // End of a batch of commands
            close(fds[1]);
            fds[1] = old_stdout;
        }
        create_process_and_run(command + offsets[i], fds);
        close(fds[1]);
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);

        i++;
    }
    dup2(old_stdin, STDIN_FILENO);
    dup2(old_stdout, STDOUT_FILENO);
    if (background_handler != 0) {
        // Only for the background fork
        printf("\n[%d]  Done\t\t", background_handler);
        for (int i=0; i<n; i++) {
            if (background[i] == background_handler) {
                for (int j=offsets[i]; command[j] != NULL; j++) {
                    printf("%s ", command[j]);
                }
                if (i < n-1 && background[i] == background[i+1]) {
                    printf("| ");
                }
            }
        }
        printf("\n");
        exit(0);
    }
}
 
int read_user_input(char *input, char **command, int *n, int *offsets, int *background){
    char *token, *subtoken, *ssubtoken;
    memset(background, 0, sizeof(int *)*MAX_INPUT_WORDS);
    *n = 1;
    int k = 0;  // Corresponds to each word
    int pipeOpen = 0;
    offsets[0] = 0;
    add_command_to_history(input);
    int bid = 1;
    for (int i = 0; (token = strsep(&input, " ")) && pipeOpen != -1; i++) {
        // fprintf(stderr, "Picking apart '%s'\n", token);
        for (int j = 0; (subtoken = strsep(&token, "|")) && pipeOpen != -1; j++) {
            if (j >= 1) {
                if (pipeOpen == 1) {
                    fprintf(stderr, "Invalid command: empty pipe\n");
                    pipeOpen = -1;
                    break;
                } else if (pipeOpen == 2) {
                    // Pipe has no command to pipe from
                    fprintf(stderr, "Invalid command: no command to pipe from\n");
                    pipeOpen = -1;
                    break;
                }
                command[k++] = NULL;
                offsets[*n] = k;
                *n += 1;
                pipeOpen = 1;
            }
            if (strlen(subtoken) > 0) {
                // Check for ampersands
                // printf("Looking for &s in '%s'\n", subtoken);
                for (int l = 0; (ssubtoken = strsep(&subtoken, "&")) && pipeOpen != -1; l++) {
                    // printf("\t'%s'\n", ssubtoken);
                    if (l >= 1) {
                        if (pipeOpen == 1) {
                            fprintf(stderr, "Invalid command: empty pipe\n");
                            pipeOpen = -1;
                            break;
                        } else if (pipeOpen == 2) {
                            fprintf(stderr, "Invalid command: no command to run in background\n");
                            pipeOpen = -1;
                            break;
                        }
                        // printf("\t\tMarking null\n");
                        command[k++] = NULL;
                        pipeOpen = 2;
                        offsets[*n] = k;
                        // Mark all offsets till now for background execution
                        // Since && is not implemented, no non-background command
                        // can occur before a background command
                        for (int m=*n-1; m>=0 && background[m]==0; m--) {
                            background[m] = bid;  // Mark background groups together
                        }
                        bid++;
                        *n += 1;
                    }
                    if (strlen(ssubtoken) > 0) {
                        pipeOpen = 0;
                        command[k++] = ssubtoken;
                        // printf("\t\tAdded command[%d] = %s\n", k-1, command[k-1]);
                    }
                }
            }
        }
    }
    if (pipeOpen == 1) {
        fprintf(stderr, "Invalid command: open pipe\n");
        pipeOpen = -1;
    }
    command[k] = NULL;
    if (command[k-1] == NULL) {
        // A terminating & has an extra command it thought would come after 
        *n -= 1;
    }
    // printf("command = ");
    // for (int i=0; i<10; i++) {
    //     printf("'%s', ", command[i]);
    // }
    // printf("\noffset = ");
    // for (int i=0; i<*n; i++) {
    //     printf("%d, ", offsets[i]);
    // }
    // printf("background = ");
    // for (int i=0; i<*n; i++) {
    //     printf("%d, ", background[i]);
    // }
    // printf("\n---------------------\n");
    if (pipeOpen == -1) {
        return 0;
    } else {
        return 1;
    }
}

int check_input(char *input){
    for (int i = 0; input[i] != '\0'; i++){
        if (input[i] == '\''){
            fprintf(stderr, "\' present in command\n");
            return 1;
        } else if (input[i] == '\"'){
            fprintf(stderr, "\" present in command\n");
            return 1;
        } else if (input[i] == '\\'){
            fprintf(stderr, "\\ present in command\n");
            return 1;
        }
    }
    return 0;
}

void terminator(int sig_num){
    display_details();
    exit(0);
}

void shell_loop()
{
    int shell_status;
    char *input = malloc(sizeof(char)*MAX_INPUT_LEN);
    char *input_copy = malloc(sizeof(char)*MAX_INPUT_LEN);
    char **command = malloc(sizeof(char *)*MAX_INPUT_WORDS);
    char *cwd;
    int *offsets = malloc(sizeof(int *)*MAX_INPUT_WORDS);
    int *background = malloc(sizeof(int *)*MAX_INPUT_WORDS);
    if (input == NULL || command == NULL || offsets == NULL || background == NULL || input_copy == NULL){
        fprintf(stderr, "Failed Memory Allocation\n");
        return;
    }
    int n;
    time_t now;
    struct timespec t1, t2;
    double elapsedTime;
    signal(SIGINT, terminator);
    do {
        cwd = getcwd(NULL, 0);
        printf("\033[1m\033[33mgroup-28@shell\033[0m:\033[1m\033[35m%s\033[0m$ ", cwd);
        fgets(input, sizeof(char)*MAX_INPUT_LEN, stdin);
        time(&now);
        input[strlen(input)-1] = '\0';
        input_copy = strdup(input);
        if (input[0] == '\0') continue;
        if (check_input(input)) continue;
        int valid = read_user_input(input, command, &n, offsets, background);
        add_details(input_copy, now);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        if (valid) {
            shell_status = launch(command, n, offsets, background);
        }
        clock_gettime(CLOCK_MONOTONIC, &t2);
        elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_nsec - t1.tv_nsec) / 1000000.0;
        add_execution_time(elapsedTime);
    } while (shell_status);
    free(input);
    free(command);
    free(offsets);
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        printf("Usage: %s <NCPU> <TSLICE (ms)> \n",argv[0]);
        exit(1);
    }
    int NCPU = (int) strtoul(argv[1], NULL, 10);
    if (NCPU == 0) {
        if (errno == EINVAL) {
            printf("Couldn't parse NCPU\n");
            exit(1);
        }
    } else if (NCPU <= 0) {
        printf("NCPU must be non-negative\n");
        exit(1);
    } else if (errno == ERANGE) {
        printf("NCPU is too large\n");
        exit(1);
    }
    int TSLICE = (int) strtoul(argv[2], NULL, 10);
    if (TSLICE == 0) {
        if (errno == EINVAL) {
            printf("Couldn't parse TSLICE\n");
            exit(1);
        }
    } else if (TSLICE <= 0) {
        printf("TSLICE must be non-negative\n");
        exit(1);
    } else if (errno == ERANGE) {
        printf("TSLICE is too large\n");
        exit(1);
    }
    shell_loop();
    return 0;
}