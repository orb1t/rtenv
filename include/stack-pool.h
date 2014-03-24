#ifndef STACK_POOL_H
#define STACK_POOL_H

#include <stddef.h>
#include "kconfig.h"
#include "object-pool.h"

struct stack {
    char data [STACK_DEFAULT_SIZE];
};

struct stack_pool {
    struct object_pool *stacks;
    int sizes[STACK_LIMIT];
};

void stack_pool_init(struct stack_pool *pool, struct object_pool *stacks);
void *stack_pool_allocate(struct stack_pool *pool, size_t size);
void *stack_pool_relocate(struct stack_pool *pool, size_t *size, void *stack);
void stack_pool_free(struct stack_pool *pool, void *stack);

#endif
