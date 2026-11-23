#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Node in the linked-list based LIFO Stack.
 */
typedef struct StackNode {
    void *data;
    struct StackNode *next;
} StackNode;

/**
 * LIFO Stack Structure.
 */
typedef struct Stack {
    StackNode *top;
    size_t size;
} Stack;

/**
 * Creates an empty Stack.
 */
Stack* stack_create(void);

/**
 * Pushes an element onto the top of the Stack.
 * Returns true on success, false on allocation failure.
 */
bool stack_push(Stack *s, void *data);

/**
 * Pops an element from the top of the Stack.
 * Returns data pointer, or NULL if stack is empty.
 */
void* stack_pop(Stack *s);

/**
 * Peeks at the top element without removing it.
 * Returns data pointer, or NULL if stack is empty.
 */
void* stack_peek(const Stack *s);

/**
 * Checks if the stack is empty.
 */
bool stack_is_empty(const Stack *s);

/**
 * Returns the number of elements in the stack.
 */
size_t stack_size(const Stack *s);

/**
 * Destroys the stack.
 * If free_data is true, free() will be invoked on remaining data pointers.
 */
void stack_destroy(Stack *s, bool free_data);

#endif /* STACK_H */
