#include "task.h"

#include <stddef.h>
#include "object-pool.h"
#include "stack-pool.h"

unsigned int *init_task(unsigned int *stack, void (*start)(),
                        size_t stack_size)
{
    stack += stack_size / 4 - 9; /* End of stack, minus what we're about to push */
    stack[8] = (unsigned int)start;
    return stack;
}

struct task_control_block *task_get(int pid)
{
    extern struct object_pool tasks;

    return object_pool_get(&tasks, pid);
}

int task_set_stack_size(struct task_control_block *task, size_t size)
{
    extern struct stack_pool stack_pool;

    size_t used;
    void *stack;

    if (!task)
        return -1;

    used = task->stack_end - (void *)task->stack;
    stack = stack_pool_relocate(&stack_pool, &size, task->stack_start);
    if (stack) {
        task->stack_start = stack;
        task->stack_end = stack + size;
        task->stack = task->stack_end - used;

        return size;
    }

    return -1;
}
