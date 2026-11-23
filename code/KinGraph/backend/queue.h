#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Node in the linked-list based FIFO Queue.
 */
typedef struct QueueNode {
    void *data;
    struct QueueNode *next;
} QueueNode;

/**
 * FIFO Queue Structure.
 */
typedef struct Queue {
    QueueNode *front;
    QueueNode *rear;
    size_t size;
} Queue;

/**
 * Creates an empty Queue.
 */
Queue* queue_create(void);

/**
 * Enqueues an element to the rear of the Queue.
 * Returns true on success, false on allocation failure.
 */
bool queue_enqueue(Queue *q, void *data);

/**
 * Dequeues an element from the front of the Queue.
 * Returns data pointer, or NULL if queue is empty.
 */
void* queue_dequeue(Queue *q);

/**
 * Peeks at the front element without removing it.
 * Returns data pointer, or NULL if queue is empty.
 */
void* queue_peek(const Queue *q);

/**
 * Checks if the queue is empty.
 */
bool queue_is_empty(const Queue *q);

/**
 * Returns the number of elements in the queue.
 */
size_t queue_size(const Queue *q);

/**
 * Destroys the queue.
 * If free_data is true, free() will be invoked on remaining data pointers.
 */
void queue_destroy(Queue *q, bool free_data);

#endif /* QUEUE_H */
