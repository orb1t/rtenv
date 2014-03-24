#include "stack-pool.h"



#include "string.h"


void stack_pool_init(struct stack_pool *pool, struct object_pool *stacks)
{
    int i;

    pool->stacks = stacks;

    for (i = 0; i < stacks->num; i++) {
        pool->sizes[i] = 0;
    }
}

void *stack_pool_allocate(struct stack_pool *pool, size_t size)
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

void *stack_pool_relocate(struct stack_pool *pool, size_t *size, void *stack)
{
    struct object_pool *stacks = pool->stacks;

    if (*size < stacks->size)
        return NULL;

    /* Align */
    int num = *size / stacks->size + (*size % stacks->size != 0);
    int *bitmap = bitmap_addr(stacks->bitmap, 0);

    int old_start = (stack - stacks->data) / stacks->size;
    int old_size = pool->sizes[old_start];
    int old_num = old_size / stacks->size;
    int old_end = old_start + old_num;

    if (old_num == num) {
        *size = stacks->size * num;
        return stack;
    }
    else if (old_num > num) {
        /* Shrink, release item */
        int end = old_start + old_num;
        int start = end - num;
        for (; old_start < start; old_start++) {
            bitmap[old_start] = 0;
            pool->sizes[old_start] = 0;
        }

        *size = stacks->size * num;
        return stacks->data + stacks->size * start;
    }
    else {
        /* Find large enough space */
        int start = 0;
        int end = 0;
        while (end < stacks->num && end - start < num) {
            if ((old_start <= end && end < old_end) || bitmap[end]) {
                start = end + 1;
            }
            end++;
        }

        /* Allocate and record size */
        if (end - start == num) {
            while (start < end) {
                bitmap[--end] = 1;
                pool->sizes[end] = stacks->size * (start - end);
            }
            pool->sizes[start] = stacks->size * num;

            /* Move data */
            end = start + num;
            if (end != old_end) {
                void *end_addr;
                void *old_end_addr;
                void *old_start_addr;
                end_addr = stacks->data + stacks->size * end;
                old_end_addr = stacks->data + stacks->size * old_end;
                old_start_addr = old_end_addr - old_size;

                while (old_start_addr < old_end_addr) {
                    end_addr -= stacks->size;
                    old_end_addr -= stacks->size;
                    memcpy(end_addr, old_end_addr, stacks->size);
                }
            }

            *size = stacks->size * num;
            return stacks->data + stacks->size * start;
        }

        return NULL;
    }
}

void stack_pool_free(struct stack_pool *pool, void *stack)
{
    struct object_pool *stacks = pool->stacks;

    int old_start = (stack - stacks->data) / stacks->size;
    int old_size = pool->sizes[old_start];
    int old_num = old_size / stacks->size;
    int old_end = old_start + old_num;

    for (; old_start < old_end; old_start++) {
        struct stack *stack = object_pool_get(stacks, old_start);
        object_pool_free(stacks, stack);
    }
}
