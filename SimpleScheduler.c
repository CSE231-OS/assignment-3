#include <stdio.h>
#include <stdlib.h>
#define MAX_LEN 128

typedef struct {
    int pr;
} Process;

void swap(Process *a, Process *b){
    Process temp = *a;
    *a = *b;
    *b = temp;
}

void heapify(Process p_queue[], int *size, int i){
    if (*size == 1) return;
    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    if (left < *size && p_queue[left].pr > p_queue[largest].pr) largest = left;
    if (right < *size && p_queue[right].pr > p_queue[largest].pr) largest = right;
    if (largest != i){
        swap(&p_queue[i], &p_queue[largest]);
        heapify(p_queue, size, largest);
    }
}

void insert_process(Process p_queue[], int *size, int pr){
    if (*size == MAX_LEN) return;
    else if (*size == 0){
        p_queue[*size].pr = pr;
        *size += 1;
    }
    else {
        p_queue[*size].pr = pr;
        *size += 1;
        for (int i = *size / 2 - 1; i >= 0; i--){
            heapify(p_queue, size, i);
        }
    }
}

void delete_process(Process p_queue[], int *size){
    if (*size == 0) return;
    swap(&p_queue[0], &p_queue[*size - 1]);
    *size -= 1;
    for (int i = *size / 2; i >= 0; i--){
        heapify(p_queue, size, i);
    }
}

void display_processes(Process p_queue[], int *size){
    for (int i = 0; i < *size; i++) printf("%d ", p_queue[i].pr);
    printf("\n");
}

Process front(Process p_queue[], int *size){
    return p_queue[0];
}

int main()
{
    Process p_queue[MAX_LEN];
    int pq_size = 0;
    // insert_process(p_queue, &pq_size, 1);
    // insert_process(p_queue, &pq_size, 2);
    // insert_process(p_queue, &pq_size, 6);
    // insert_process(p_queue, &pq_size, 4);
    // insert_process(p_queue, &pq_size, 3);
    // insert_process(p_queue, &pq_size, 5);
    // insert_process(p_queue, &pq_size, 5);
    // display_processes(p_queue, &pq_size);
    // delete_process(p_queue, &pq_size);
    // delete_process(p_queue, &pq_size);
    // display_processes(p_queue, &pq_size);
    // Process curr = front(p_queue, &pq_size);
    // printf("%d\n", curr.pr);
}