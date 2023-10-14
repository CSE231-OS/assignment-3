#include <semaphore.h>

#define MAX_COMMANDS 128
#define MAX_INPUT_LEN 256
#define MAX_INPUT_WORDS 128
#define MAX_PATH_LEN 128
#define MAX_PQUEUE_LEN 128
#define NUMBER_OF_QUEUES 4

struct process {
    int index;
    int init_pr;
    int times_executed;
    struct timespec prev_wait_time;
    struct timespec arrival_time;
    struct timespec submission_time;
    double response_time;
    double turnaround_time;
    double total_exe_time;
    double total_wait_time;
    int pr;
    int initial_pr;
    int pid;
    char path[MAX_PATH_LEN];
    int status;
    struct process *next;
    struct process *tail;
};

typedef struct {
    char command[MAX_COMMANDS][MAX_INPUT_LEN];
    int priorities[MAX_COMMANDS];
    struct timespec submission_time[MAX_COMMANDS];
    int index;
    sem_t mutex;
} shm_t;
shm_t *shm;

void insert_process(struct process *process);