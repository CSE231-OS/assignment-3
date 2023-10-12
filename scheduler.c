#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#define NUMBER_OF_QUEUES 4

struct process queue[NUMBER_OF_QUEUES + 1];
struct process **current;
int NCPU, TSLICE;

// struct process *new_process(int pr){
//     struct process *temp;
//     temp = malloc(sizeof(struct process));
//     temp->pr = pr;
//     temp->next = NULL;
//     return temp;
// }

void insert_process(struct process *process){
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
    for (int i = 1; i <= NUMBER_OF_QUEUES; i++){
        if (queue[i].next == NULL) continue;
        struct process *temp = queue[i].next;
        queue[i].next = queue[i].next->next;
        if (queue[i].next == NULL) queue[i].tail = NULL;
        return temp;
    }
    return NULL;
}

void display_queue(){
    for (int i = 1; i <= NUMBER_OF_QUEUES; i++){
        struct process *temp = queue[i].next;
        printf("%d: ", i);
        while (temp != NULL){
            printf("%d ", temp->pid);
            temp = temp->next;
        }
        printf("\n");
    }
}

void wake() {
    // printf("\n-------------\n");
    // display_queue();
    // printf("\n-------------\n");
    for (int i=0; i<NCPU; i++) {
        struct process *process = delete_process(queue);
        if (process == NULL) {
            break;
        }
        current[i] = process;
        printf("\tWaking %s ", process->path);
        if (process->pid == -1) {
            printf("by execvp\n");
            int status = fork();
            process->pid = status;
            if (status < 0) {
                fprintf(stderr, "Unable to start child process %s\n", process->path);
                exit(1);
            } else if (status == 0) {
                char* argv[2] = {process->path, NULL};
                fprintf(stderr, "path: %s\n", process->path);
                execvp(process->path, argv); // TODO: Error checking
                fprintf(stderr, "Failed exec\n");
            } else {
                // perror("Error");
            }
        } else {
            printf("by SIGCONT\n");
            kill(process->pid, SIGCONT); // TODO: Error checking
        }
    }
}

void enqueue_processes(){
    for (int i = 0; i < shm->index; i++){
        struct process *process = malloc(sizeof(struct process));
        strcpy(process->path, shm->command[i]);
        process->pr = shm->priorities[i];
        process->pid = -1;
        insert_process(process);
    }
    shm->index = 0;
}

void start() {
    while (1) {
        enqueue_processes();
        for (int i=0; i<NCPU; i++) {
            if (current[i] != NULL && current[i]->pid > 0) {
                kill(current[i]->pid, SIGSTOP);
                insert_process(current[i]);
            }
        }
        wake();
        sleep(TSLICE);
    }
}

int main(int argc, char **argv)
{
    NCPU = atoi(argv[1]); TSLICE = atoi(argv[2]);
    current = malloc(sizeof(struct process *) * NCPU);
    for (int i = 1; i <= NUMBER_OF_QUEUES; i++) {
        queue[i].pr = i;
        queue[i].pid = 0;
        queue[i].next = NULL;
        queue[i].tail = NULL;
    }
    for (int j = 0; j < NCPU; j++) {
        current[j] = NULL;
    }
    // struct process *process = malloc(sizeof(struct process));
    // strcpy(process->path, "./fib");
    // process->pr = 1;
    // process->pid = -1;

    // struct process *process2 = malloc(sizeof(struct process));
    // strcpy(process2->path, "./fib2");
    // process2->pr = 1;
    // process2->pid = -1;
    // printf("Inserting\n");
    // insert_process(process);
    // insert_process(process2);
    start();
}