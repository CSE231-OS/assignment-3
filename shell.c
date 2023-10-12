#include <errno.h>
#include <fcntl.h>           /* For O_* constants */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "common.h"
#define MAX_COMMANDS 128
#define MAX_INPUT_LEN 256
#define MAX_INPUT_WORDS 128
#define NUMBER_OF_QUEUES 4

typedef struct {
    char command[MAX_COMMANDS][MAX_INPUT_LEN];
    int priorities[MAX_COMMANDS];
    int index;
} shm_t;
shm_t *shm;

int create_process_and_run(char **command){
    int status = fork();
    if (status < 0){
        perror("Failed Fork\n");
        exit(0);
    } else if (status == 0){
        if (strcmp(command[0], "submit") == 0){
            int pr = -1;
            if (command[1] == NULL) {
                printf("Usage: %s <Executable>\n", command[0]);
                return;
            } else if (command[2] == NULL) {
                pr = 1;
            } else if (command[3] == NULL) {
                pr = atoi(command[2]);
                if (pr >= 5 || pr <= 0) {
                    fprintf(stderr, "Invalid priority\n");
                    exit(1);
                }
            }
            strcpy(shm->command[index], command[1]);
            shm->priorities[index++] = pr;
            // struct process *process = malloc(sizeof(struct process));
            // strcpy(process->path, command[1]);
            // process->pr = pr;
            // process->pid = -1;
            // insert_process(process);
        } else {
            execvp(command[0], command);
            perror("Exec failed");
            exit(1);
        }
        
    } else {
        int ret;
        int pid = wait(&ret);
        if (!WIFEXITED(ret)){
            printf("Abnormal Termination of %d\n", pid);
        }
    }
    return status;
}

int launch(char **command){
    int status;
    status = create_process_and_run(command);
    return status;
}

char **read_user_input(char *input, char **command){
    char *delim = " ";
    char *word;
    int i = 0;
    word = strtok(input, delim);
    while (word != NULL){
        command[i] = word;
        ++i;
        word = strtok(NULL, delim);
    }
    command[i-1][strlen(command[i-1])-1] = '\0';
    command[i] = NULL;
    return command;
}

void shell_loop()
{
    int status;
    char *input;
    char **command;
    char *cwd;
    do {
        cwd = getcwd(NULL, 0);
        input = malloc(256);
        command = malloc(256);
        printf("\033[1m\033[33mgroup-28@shell\033[0m:\033[1m\033[35m%s\033[0m$ ", cwd);
        fgets(input, sizeof(char)*MAX_INPUT_LEN, stdin);
        command = read_user_input(input, command);
        status = launch(command);
        free(command);
        free(input);
    } while (status);
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
    
    int fd = shm_open("shell-scheduler", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO); // TODO: Error checking
    ftruncate(fd, sizeof(shm_t));  // TODO: Error checking
    shm = mmap(
        NULL,                               /* void *__addr */
        sizeof(shm_t),                      /* size_t __len */
        PROT_READ | PROT_WRITE,             /* int __prot */
        MAP_SHARED,                         /* int __flags */
        fd,                                 /* int __fd */
        0                                   /* off_t __offset */
    );  // TODO: Error checking
    
    int status = fork();
    if (status == 0) {
        char **sched_argv = malloc(sizeof(char *)*4);  // TODO: Error checking
        char name[12] = "./scheduler";
        sched_argv[0] = name;
        sched_argv[1] = argv[1];
        sched_argv[2] = argv[2];
        sched_argv[3] = NULL;
        execvp(name, sched_argv);  // TODO: Error checking
    } else if (status > 0) {
        shell_loop();
        return 0;
    }
    fprintf(stderr, "Couldn't spawn scheduler\n");
    perror("Error: ");
    exit(1);
}