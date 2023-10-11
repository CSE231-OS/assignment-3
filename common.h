#define MAX_PATH_LEN 128
#define MAX_PQUEUE_LEN 128
#define NUMBER_OF_QUEUES 4

struct process {
    int pr;
    int pid;
    char path[MAX_PATH_LEN];
    struct process *next;
    struct process *tail;
};

void insert_process(struct process *process);