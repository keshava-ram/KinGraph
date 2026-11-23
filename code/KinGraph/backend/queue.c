#include "queue.h"
#include <stdlib.h>

Queue* queue_create(void) {
    Queue *q = (Queue*)malloc(sizeof(Queue));
    if (!q) return NULL;
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
    return q;
}

bool queue_enqueue(Queue *q, void *data) {
    if (!q) return false;

    QueueNode *node = (QueueNode*)malloc(sizeof(QueueNode));
    if (!node) return false;

    node->data = data;
    node->next = NULL;

    if (q->rear == NULL) {
        q->front = node;
        q->rear = node;
    } else {
        q->rear->next = node;
        q->rear = node;
    }
    q->size++;
    return true;
}

void* queue_dequeue(Queue *q) {
    if (!q || q->front == NULL) return NULL;

    QueueNode *temp = q->front;
    void *data = temp->data;

    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }

    free(temp);
    q->size--;
    return data;
}

void* queue_peek(const Queue *q) {
    if (!q || q->front == NULL) return NULL;
    return q->front->data;
}

bool queue_is_empty(const Queue *q) {
    return !q || q->front == NULL;
}

size_t queue_size(const Queue *q) {
    return q ? q->size : 0;
}

void queue_destroy(Queue *q, bool free_data) {
    if (!q) return;

    QueueNode *curr = q->front;
    while (curr) {
        QueueNode *next = curr->next;
        if (free_data && curr->data) {
            free(curr->data);
        }
        free(curr);
        curr = next;
    }
    free(q);
}
