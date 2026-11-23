#include "stack.h"
#include <stdlib.h>

Stack* stack_create(void) {
    Stack *s = (Stack*)malloc(sizeof(Stack));
    if (!s) return NULL;
    s->top = NULL;
    s->size = 0;
    return s;
}

bool stack_push(Stack *s, void *data) {
    if (!s) return false;

    StackNode *node = (StackNode*)malloc(sizeof(StackNode));
    if (!node) return false;

    node->data = data;
    node->next = s->top;
    s->top = node;
    s->size++;
    return true;
}

void* stack_pop(Stack *s) {
    if (!s || s->top == NULL) return NULL;

    StackNode *temp = s->top;
    void *data = temp->data;

    s->top = s->top->next;
    free(temp);
    s->size--;

    return data;
}

void* stack_peek(const Stack *s) {
    if (!s || s->top == NULL) return NULL;
    return s->top->data;
}

bool stack_is_empty(const Stack *s) {
    return !s || s->top == NULL;
}

size_t stack_size(const Stack *s) {
    return s ? s->size : 0;
}

void stack_destroy(Stack *s, bool free_data) {
    if (!s) return;

    StackNode *curr = s->top;
    while (curr) {
        StackNode *next = curr->next;
        if (free_data && curr->data) {
            free(curr->data);
        }
        free(curr);
        curr = next;
    }
    free(s);
}
