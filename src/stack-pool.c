#include "stack-pool.h"



void stack_pool_init(struct stack_pool *pool, struct object_pool *stacks)
{
    int i;

    pool->stacks = stacks;

    for (i = 0; i < stacks->num; i++) {
        pool->sizes[i] = 0;
    }
}

void* stack_pool_allocate(struct stack_pool *pool, size_t size)
{
    /* Align */
    struct object_pool *stacks = pool->stacks;
    int num = size / stacks->size + (size % stacks->size != 0);
    int *bitmap = bitmap_addr(stacks->bitmap, 0);

    /* Find large enough space */
    int start = 0;
    int end = 0;
    while (end < stacks->num && end - start < num) {
        if (bitmap[end++]) {
            start = end;
        }
    }

    /* Allocate and record size */
    if (end - start == num) {
        while (start < end) {
            bitmap[--end] = 1;
            pool->sizes[end] = stacks->size * (start - end);
        }
        pool->sizes[start] = stacks->size * num;

        return stacks->data + stacks->size * start;
    }

    return NULL;
}
