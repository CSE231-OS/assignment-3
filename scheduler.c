#include <errno.h>
#include <fcntl.h>           /* For O_* constants */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "common.h"
#define RUNNING 1
#define TERMINATED 2
#define RESET_INTERVAL 3

struct process queue[NUMBER_OF_QUEUES + 1];
struct process **current;
int NCPU, TSLICE;
int rounds_till_reset = RESET_INTERVAL; 
int process_index = 1;
struct timespec now;
struct process history;
// struct process *new_process(int pr){
//     struct process *temp;
//     temp = malloc(sizeof(struct process));
//     temp->pr = pr;
//     temp->next = NULL;
//     return temp;
// }
int debugging = 0;
int BLOCKING_SIGINT = 0;
int ALLOW_TERMINATION = 0;

void add_to_history(struct process *process){
    struct process *temp = history.next;
    history.next = process;
    process->next = temp;
}

void sort_history(){
    struct process *arr[process_index - 1];
    struct process *temp = history.next;
    int i = 0;
    while (temp != NULL){
        arr[i++] = temp;
        temp = temp->next;
    }
    if (i == 0) return;
    int flag;
    for (int j = 0; j < i - 1; j++){
        flag = 1;
        for (int k = 0; k < i - j - 1; k++){
            if (arr[k]->index > arr[k + 1]->index){
                temp = arr[k];
                arr[k] = arr[k + 1];
                arr[k + 1] = temp;
                flag = 0;
            }
        }
        if (flag) break;
    }
    struct process *temp_head = arr[0];
    temp = temp_head;
    for (int j = 1; j < i; j++){
        temp->next = arr[j];
        temp = temp->next;
    }
    temp->next = NULL;
    history.next = temp_head;
}

void display_history(){
    struct process *curr = history.next;
    printf("\nHistory:\n");
    while (curr != NULL){
        printf("%d) %s\n", curr->index, curr->path);
        printf("\tPID: %d\n", curr->pid);
        printf("\tTotal Wait Time: %.3f ms\n", curr->total_wait_time);
        printf("\tTotal Execution Time: %.3f ms\n", curr->total_exe_time);
        printf("\n");
        curr = curr->next;
    }
}

void terminate() {
    sort_history();
    display_history();
    exit(0);
}

void insert_process(struct process *process){
    process->status = RUNNING;
    // printf(">> Inserting %s with next = %p\n", process->path, process->next);
    if (process->next != NULL) {
        // printf(">>\t\tNext=%s (%d)\n", process->next->path, process->next->pid);
    }
    int pr = process->pr;
    if (queue[pr].tail == NULL){
        queue[pr].next = process;
        queue[pr].tail = process;
    } else {
        queue[pr].tail->next = process;
        queue[pr].tail = queue[pr].tail->next;
    }
}

struct process *delete_process(){
    // printf(">> Deleting\n");
    for (int i = 1; i <= NUMBER_OF_QUEUES; i++){
        if (queue[i].next == NULL) continue;
        struct process *temp = queue[i].next;
        queue[i].next = queue[i].next->next;
        if (queue[i].next == NULL) queue[i].tail = NULL;
        temp->next = NULL;
        return temp;
    }
    return NULL;
}

int no_pending_processes() {
    for (int i=1; i<=NUMBER_OF_QUEUES; i++) {
        if (queue[i].next != NULL) {
            return 0;
        }
    }
    return shm->index == 0;
}

void display_queue(){
    for (int i = 1; i <= NUMBER_OF_QUEUES; i++){
        struct process *temp = queue[i].next;
        printf("%d: ", i);
        while (temp != NULL){
            printf("%d (status=%d)", temp->pid, temp->status);
            temp = temp->next;
        }
        printf("\n");
    }
}

void wake() {
    // printf("\n-------------\n");
    // display_queue();
    // printf("\n-------------\n");
    // printf("Before wake\n");
    // display_queue();
    clock_gettime(CLOCK_MONOTONIC, &now);
    for (int i=0; i<NCPU; i++) {
        struct process *process = delete_process(queue);
        if (process == NULL ) {
            break;
        }
        // if (process->status == TERMINATED) {
        //     printf("------ Terminated process in queue ----------\n");
        //     display_queue();
        //     continue;
        // }
        current[i] = process;
        process->total_wait_time += (now.tv_sec - process->prev_wait_time.tv_sec) * 1000.0 + (now.tv_nsec - process->prev_wait_time.tv_nsec) / 1000000.0;
        process->prev_exe_time = now;
        // printf("\tWaking %s ", process->path);
        if (process->pid == 0) {
            process->status = RUNNING;
            // printf("by execvp\n");
            int status = fork();
            process->pid = status;
            if (status < 0) {
                fprintf(stderr, "Unable to start child process %s\n", process->path);
                exit(1);
            } else if (status == 0) {
                char* argv[2] = {process->path, NULL};
                execvp(process->path, argv); // TODO: Error checking
                fprintf(stderr, "Failed exec for path %s\n", process->path);
                perror("Error");
            } else {
                // perror("Error");
            }
        } else {
            // printf("by SIGCONT\n");
            kill(process->pid, SIGCONT); // TODO: Error checking
        }
    }
    // printf("After wake\n");
    // display_queue();
}

shm_t *shm;
void enqueue_processes(){
    // printf("Enqueue called\n");
    for (int i = 0; i < shm->index; i++){
        struct process *process = malloc(sizeof(struct process));
        strcpy(process->path, shm->command[i]);
        process->pr = shm->priorities[i];
        process->pid = 0;
        process->index = process_index++;
        process->init_pr = process->pr;
        process->prev_wait_time = shm->submission_time[i];
        process->total_exe_time = 0;
        process->total_wait_time = 0;
        insert_process(process);
    }
    shm->index = 0;
}

void stop_current() {
    clock_gettime(CLOCK_MONOTONIC, &now);
    int processes_interrupted = 0;
    for (int i=0; i<NCPU; i++) {
        if (current[i] != NULL && current[i]->status != TERMINATED) {
            processes_interrupted++;
            // printf("Stopping %s\n", current[i]->path);
            if (current[i]->pr < NUMBER_OF_QUEUES) ++current[i]->pr;
            kill(current[i]->pid, SIGSTOP); // TODO: Error checking
            insert_process(current[i]);
            current[i]->total_exe_time += (now.tv_sec - current[i]->prev_exe_time.tv_sec) * 1000.0 + (now.tv_nsec - current[i]->prev_exe_time.tv_nsec) / 1000000.0;
            current[i]->prev_wait_time = now;
            // printf("Inserted %s (%d)\n", current[i]->path, current[i]->pid);
        }
        // if (current[i] != NULL && current[i]->status == TERMINATED) {
        //     display_queue();
        // }
        current[i] = NULL;
    }
    if (BLOCKING_SIGINT && processes_interrupted == 0 && no_pending_processes()) terminate();
}

void reset_priorities() {
    for (int i=2; i<=NUMBER_OF_QUEUES; i++) {
        if (queue[i].next == NULL) continue;
        if (queue[1].tail != NULL) {
            queue[1].tail->next = queue[i].next;
        } else {
            queue[1].next = queue[i].next;
        }
        queue[1].tail = queue[i].tail;
        struct process *cursor = queue[i].next;
        while (cursor != NULL) {
            cursor->pr = 1;
            cursor = cursor->next;
        }
        queue[i].next = NULL;
        queue[i].tail = NULL;
    }
}

void start_round() {
    // printf("Starting round\n");
    enqueue_processes();
    stop_current();
    if (rounds_till_reset-- == 0) {
        reset_priorities();
        rounds_till_reset = RESET_INTERVAL;
    }
    // display_queue();
    // printf("\n");
    wake();
    // printf("Round ended\n");
}

struct itimerval timer_value;
void start() {
    // int fd = shm_open("/shell-scheduler", O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);  // Actual line
    int fd = shm_open("/shell-scheduler", O_RDWR | O_CREAT | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO); // TODO: Error checking
    debugging = fd != -1;
    if (!debugging) {
        fd = shm_open("/shell-scheduler", O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO); // TODO: Error checking
    } else {
        ftruncate(fd, sizeof(shm_t));
    }
    shm = mmap(
        NULL,                               /* void *__addr */
        sizeof(shm_t),                      /* size_t __len */
        PROT_READ | PROT_WRITE,             /* int __prot */
        MAP_SHARED,                         /* int __flags */
        fd,                                 /* int __fd */
        0                                   /* off_t __offset */
    );  // TODO: Error checking
    if (debugging) {
        int priorities[2] = {1, 1};
        char commands[2][MAX_INPUT_LEN] = {"./fib", "./fib2"};
        memcpy(shm->command, commands, sizeof(commands));
        memcpy(shm->priorities, priorities, sizeof(priorities));
        shm->index = 2;
    }
    timer_value.it_value.tv_sec = TSLICE/1000;
    timer_value.it_value.tv_usec = TSLICE%1000 * 1000;
    timer_value.it_interval.tv_sec = TSLICE/1000;
    timer_value.it_interval.tv_usec = TSLICE%1000 * 1000;
    start_round();
    setitimer(ITIMER_REAL, &timer_value, NULL);
}

void signal_handler(int signum, siginfo_t *siginfo, void *trash) {
    if (signum == SIGALRM) {
        start_round();
    } else if (signum == SIGCHLD) {
        // display_queue();
        clock_gettime(CLOCK_MONOTONIC, &now);
        int pid = siginfo->si_pid;
        int processes_alive = 0;
        for (int i=0; i<NCPU; i++) {
            if (current[i] == NULL) {
                break;
            }
            if (current[i]->pid == pid) {
                // printf("%s terminated\n", current[i]->path);
                current[i]->status = TERMINATED;
                current[i]->total_exe_time += (now.tv_sec - current[i]->prev_exe_time.tv_sec) * 1000.0 + (now.tv_nsec - current[i]->prev_exe_time.tv_nsec) / 1000000.0;
                add_to_history(current[i]);
            } else if (current[i]->status != TERMINATED) {
                processes_alive++;
            }
        }
        if (BLOCKING_SIGINT && processes_alive == 0 && no_pending_processes()) terminate();
    } else if (signum == SIGINT){
        if (debugging) {
            shm_unlink("/shell-scheduler");
        }
        BLOCKING_SIGINT = 1;
        int processes_alive = 0;
        for (int i=0; i<NCPU; i++) {
            if (current[i] == NULL) continue;
            if (current[i]->status != TERMINATED) processes_alive++;
        }
        if (processes_alive == 0 && no_pending_processes()) terminate();
    }
}

int main(int argc, char **argv)
{
    NCPU = atoi(argv[1]); TSLICE = atoi(argv[2]);
    current = malloc(sizeof(struct process *) * NCPU);
    for (int i = 1; i <= NUMBER_OF_QUEUES; i++) {
        queue[i].pr = i;
        // queue[i].pid = 0;
        queue[i].next = NULL;
        queue[i].tail = NULL;
    }
    for (int j = 0; j < NCPU; j++) {
        current[j] = NULL;
    }
    history.next = NULL;
    history.index = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
    sa.sa_sigaction = &signal_handler;
    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    start();
    while (1) {
        usleep(TSLICE * 1000);
    }
}