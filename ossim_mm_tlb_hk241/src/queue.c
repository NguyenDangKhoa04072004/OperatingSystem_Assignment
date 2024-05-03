#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        q->rear = (q->rear + 1)%MAX_QUEUE_SIZE;
        q->proc[q->rear] = proc;
        q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        struct pcb_t * proc = q->proc[q->front];
        q->front = (q->front + 1) % MAX_QUEUE_SIZE;
        q->size--;
        return proc;
}

