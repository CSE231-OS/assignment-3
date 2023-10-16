#include <errno.h>
#include <fcntl.h>           /* For O_* constants */
#include <semaphore.h>
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

shm_t *shm;
struct timespec now;
int create_process_and_run(char **command){
    int status = fork();
    if (status < 0){
        perror("Failed fork");
        exit(0);
    } else if (status == 0){
        if (strcmp(command[0], "submit") == 0){
            int pr = -1;
            if (command[1] == NULL) {
                printf("Usage: %s <Executable>\n", command[0]);
                return -1;
            } else if (command[2] == NULL) {
                pr = 1;
            } else if (command[3] == NULL) {
                pr = atoi(command[2]);
                if (pr >= 5 || pr <= 0) {
                    fprintf(stderr, "Invalid priority\n");
                    exit(1);
                }
            } else {
                exit(1);
            }
            printf("Waiting on shell\n");
            int ret = sem_wait(&shm->mutex);
            printf("Starting on shell\n");
            if (ret == -1) {
                perror("Unable to wait for semaphore");
                exit(1);
            }
            strcpy(shm->command[shm->index], command[1]);
            shm->priorities[shm->index] = pr;
            shm->submission_time[shm->index++] = now;
            ret = sem_post(&shm->mutex);
            printf("Posted\n");
            if (ret == -1) {
                perror("Unable to post to semaphore");
                exit(1);
            }
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
    command[i] = NULL;
    return command;
}

void shell_loop()
{
    int status;
    char *input = malloc(sizeof(char)*MAX_INPUT_LEN);
    char **command = malloc(sizeof(char *)*MAX_INPUT_WORDS);
    char *cwd;
    do {
        cwd = getcwd(NULL, 0);
        if (cwd == NULL) {
            perror("Unable to get current working directory");
            exit(1);
        }
        printf("\033[1m\033[33mgroup-28@shell\033[0m:\033[1m\033[35m%s\033[0m$ ", cwd);
        fgets(input, sizeof(char)*MAX_INPUT_LEN, stdin);
        int ret = clock_gettime(CLOCK_MONOTONIC, &now);
        if (ret == -1) {
            perror("Unable to get time");
            exit(1);
        }
        input[strlen(input) - 1] = '\0';
        if (input[0] == '\0') continue;
        command = read_user_input(input, command);
        status = launch(command);
    } while (status);
    free(command);
    free(input);
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        int ret = munmap(shm, sizeof(shm_t));
        if (ret == -1) {
            perror("Unable to unmap shared memory");
            exit(1);
        }
        exit(0);
    }
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
    int fd = shm_open("/shell-scheduler", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1) {
        perror("Unable to open shared memory");
        exit(1);
    }
    int ret = ftruncate(fd, sizeof(shm_t));
    if (ret == -1) {
        perror("Unable to truncate shared memory");
        exit(1);
    }
    shm = mmap(
        NULL,                               /* void *__addr */
        sizeof(shm_t),                      /* size_t __len */
        PROT_READ | PROT_WRITE,             /* int __prot */
        MAP_SHARED,                         /* int __flags */
        fd,                                 /* int __fd */
        0                                   /* off_t __offset */
    );
    if (shm == MAP_FAILED) {
        perror("Unable to map shared memory");
        exit(1);
    }
    ret = close(fd);
    if (ret == -1) {
        perror("Unable to close shared memory fd");
        exit(1);
    }
    shm->index = 0;
    ret = sem_init(&shm->mutex, 1, 1);
    if (ret == -1) {
        perror("Unable to initialize semaphore");
        exit(1);
    }
    int status = fork();
    if (status == 0) {
        status = fork();
        if (status == 0) {
            char **sched_argv = malloc(sizeof(char *)*4);
            if (sched_argv == NULL) {
                perror("Unable to allocate memory");
                exit(1);
            }
            char name[12] = "./scheduler";
            sched_argv[0] = name;
            sched_argv[1] = argv[1];
            sched_argv[2] = argv[2];
            sched_argv[3] = NULL;
            execvp(name, sched_argv);
            perror("Scheduler exec failed");
            exit(1);
        } else {
            _exit(0);
        }
    } else if (status > 0) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = &signal_handler;
        sigaction(SIGINT, &sa, NULL);
        shell_loop();
        return 0;
    }
    fprintf(stderr, "Couldn't spawn scheduler\n");
    perror("Error: ");
    exit(1);
}