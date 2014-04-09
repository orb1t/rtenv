#include "task.h"

#include <stddef.h>
#include "object-pool.h"

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
