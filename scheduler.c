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
    struct process *temp;
    printf("\nHistory:\n");
    double average_wait_time = 0, average_exe_time = 0, average_response_time = 0, average_turnaround_time = 0, average_latency = 0;
    int n = 0;
    while (curr != NULL){
        n++;
        printf("%d) %s\n", curr->index, curr->path);
        printf("\tPID: %d\n", curr->pid);
        printf("\tInitial priority: %d\n", curr->initial_pr);
        printf("\tFinal priority: %d\n", curr->pr);
        printf("\tTotal wait time: %.3f ms\n", curr->total_wait_time);
        printf("\tTotal execution time: %.3f ms\n", curr->total_exe_time);
        printf("\tResponse time: %.3f ms\n", curr->response_time);
        printf("\tTurnaround time: %.3f ms\n", curr->turnaround_time);
        double latency = (curr->arrival_time.tv_sec - curr->submission_time.tv_sec) * 1000.0 + (curr->arrival_time.tv_nsec - curr->submission_time.tv_nsec) / 1000000.0;
        printf("\tSubmission to arrival latency (ignored): %.3f ms\n", latency);
        printf("\n");
        average_wait_time += curr->total_wait_time;
        average_exe_time += curr->total_exe_time;
        average_response_time += curr->response_time;
        average_turnaround_time += curr->turnaround_time;
        average_latency += latency;
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    printf("Average wait time: %.3f ms\n", average_wait_time / n);
    printf("Average execution time: %.3f ms\n", average_exe_time / n);
    printf("Average response time: %.3f ms\n", average_response_time / n);
    printf("Average turnaround time: %.3f ms\n", average_turnaround_time / n);
    printf("Average latency: %.3f ms\n", average_latency / n);
    printf("\n");
}

void cleanup() {
    int ret = munmap(shm, sizeof(shm_t));
    if (ret == -1) {
        perror("Unable to unmap shared memory");
        exit(1);
    }
    ret = sem_destroy(&shm->mutex);
    if (ret == -1) {
        perror("Unable to destroy semaphore");
        exit(1);
    }
    ret = shm_unlink("/shell-scheduler");
    if (ret == -1) {
        perror("Unable to unlink shared memory");
        exit(1);
    }
    exit(0);
}

void terminate() {
    sort_history();
    display_history();
    cleanup();
    exit(0);
}

void insert_process(struct process *process){
    process->status = RUNNING;
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
    int ret = sem_wait(&shm->mutex);
    if (ret == -1) {
        perror("Unable to wait on mutex");
        exit(1);
    }
    int empty = shm->index == 0;
    ret = sem_post(&shm->mutex);
    if (ret == -1) {
        perror("Unable to post on mutex");
        exit(1);
    }
    return empty;
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
    int ret = clock_gettime(CLOCK_MONOTONIC, &now);
    if (ret == -1) {
        perror("Unable to get clock time");
        exit(1);
    }
    for (int i=0; i<NCPU; i++) {
        struct process *process = delete_process(queue);
        if (process == NULL ) {
            break;
        }
        current[i] = process;
        process->total_wait_time += (now.tv_sec - process->prev_wait_time.tv_sec) * 1000.0 + (now.tv_nsec - process->prev_wait_time.tv_nsec) / 1000000.0;
        // printf("process->total_wait_time = %.3f\n", process->total_wait_time);
        if (process->pid == 0) {
            process->response_time = (now.tv_sec - process->arrival_time.tv_sec) * 1000.0 + (now.tv_nsec - process->arrival_time.tv_nsec) / 1000000.0;
            process->status = RUNNING;
            int status = fork();
            process->pid = status;
            if (status < 0) {
                fprintf(stderr, "Unable to start child process %s\n", process->path);
                exit(1);
            } else if (status == 0) {
                char* argv[2] = {process->path, NULL};
                execvp(process->path, argv);
                fprintf(stderr, "Failed exec for path %s\n", process->path);
                perror("Error");
            }
        } else {
            int ret = kill(process->pid, SIGCONT);
            if (ret == -1) {
                fprintf(stderr, "Unable to continue child process %s. ", process->path);
                perror("Error");
                exit(1);
            }
        }
    }
}

shm_t *shm;
void enqueue_processes(){
    int ret = sem_wait(&shm->mutex);
    if (ret == -1) {
        perror("Unable to wait on mutex");
        exit(1);
    }
    for (int i = 0; i < shm->index; i++){
        struct process *process = malloc(sizeof(struct process));
        if (process == NULL) {
            perror("Unable to allocate memory for process");
            exit(1);
        }
        strcpy(process->path, shm->command[i]);
        process->pr = shm->priorities[i];
        process->initial_pr = process->pr;
        process->pid = 0;
        process->index = process_index++;
        process->init_pr = process->pr;
        clock_gettime(CLOCK_MONOTONIC, &process->arrival_time);
        process->times_executed = 0;
        process->prev_wait_time = shm->submission_time[i];
        process->submission_time = process->prev_wait_time;
        process->total_exe_time = 0;
        process->total_wait_time = 0;
        insert_process(process);
    }
    shm->index = 0;
    ret = sem_post(&shm->mutex);
    if (ret == -1) {
        perror("Unable to post on mutex");
        exit(1);
    }
}

void stop_current() {
    int ret = clock_gettime(CLOCK_MONOTONIC, &now);
    if (ret == -1) {
        perror("Unable to get clock time");
        exit(1);
    }
    int processes_interrupted = 0;
    for (int i=0; i<NCPU; i++) {
        if (current[i] != NULL && current[i]->status != TERMINATED) {
            processes_interrupted++;
            if (current[i]->pr < NUMBER_OF_QUEUES) ++current[i]->pr;
            int ret = kill(current[i]->pid, SIGSTOP);
            if (ret == -1) {
                fprintf(stderr, "Unable to stop child process %s\n", current[i]->path);
                exit(1);
            }
            insert_process(current[i]);
            current[i]->turnaround_time = (now.tv_sec - current[i]->arrival_time.tv_sec) * 1000.0 + (now.tv_nsec - current[i]->arrival_time.tv_nsec) / 1000000.0;
            ++current[i]->times_executed;
            current[i]->prev_wait_time = now;
        }
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
    enqueue_processes();
    stop_current();
    if (rounds_till_reset-- == 0) {
        reset_priorities();
        rounds_till_reset = RESET_INTERVAL;
    }
    wake();
}

struct itimerval timer_value;
void start() {
    int fd = shm_open("/shell-scheduler", O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1) {
        perror("Scheduler unable to open shared memory");
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
        perror("Scheduler unable to map shared memory");
        exit(1);
    }
    int ret = close(fd);
    if (ret == -1) {
        perror("Scheduler unable to close shared memory fd");
        exit(1);
    }
    timer_value.it_value.tv_sec = TSLICE/1000;
    timer_value.it_value.tv_usec = TSLICE%1000 * 1000;
    timer_value.it_interval.tv_sec = TSLICE/1000;
    timer_value.it_interval.tv_usec = TSLICE%1000 * 1000;
    start_round();
    ret = setitimer(ITIMER_REAL, &timer_value, NULL);
    if (ret == -1) {
        perror("Unable to set timer");
        exit(1);
    }
}

void signal_handler(int signum, siginfo_t *siginfo, void *trash) {
    if (signum == SIGALRM) {
        start_round();
    } else if (signum == SIGCHLD) {
        int ret = clock_gettime(CLOCK_MONOTONIC, &now);
        if (ret == -1) {
            perror("Unable to get clock time");
            exit(1);
        }
        int pid = siginfo->si_pid;
        int processes_alive = 0;
        for (int i=0; i<NCPU; i++) {
            if (current[i] == NULL) {
                break;
            }
            if (current[i]->pid == pid) {
                current[i]->status = TERMINATED;
                ++current[i]->times_executed;
                current[i]->total_exe_time = current[i]->times_executed * TSLICE;
                add_to_history(current[i]);
            } else if (current[i]->status != TERMINATED) {
                processes_alive++;
            }
        }
        if (BLOCKING_SIGINT && processes_alive == 0 && no_pending_processes()) terminate();
    } else if (signum == SIGINT){
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
    if (current == NULL) {
        perror("Unable to allocate memory for current processes");
        exit(1);
    }
    for (int i = 1; i <= NUMBER_OF_QUEUES; i++) {
        queue[i].pr = i;
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
    int ret = sigaction(SIGALRM, &sa, NULL);
    if (ret == -1) {
        perror("Unable to bind SIGALRM");
        exit(1);
    }
    ret = sigaction(SIGCHLD, &sa, NULL);
    if (ret == -1) {
        perror("Unable to bind SIGCHLD");
        exit(1);
    }
    ret = sigaction(SIGINT, &sa, NULL);
    if (ret == -1) {
        perror("Unable to bind SIGINT");
        exit(1);
    }

    start();
    while (1) {
        ret = usleep(TSLICE * 1000);
        if (ret == -1 && errno != EINTR) {
            perror("Unable to sleep:");
            exit(1);
        }
    }
}