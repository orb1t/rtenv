#include "task.h"

#include <stddef.h>

unsigned int *init_task(unsigned int *stack, void (*start)(), size_t stack_size)
{
	stack += stack_size - 9; /* End of stack, minus what we're about to push */
	stack[8] = (unsigned int)start;
	return stack;
}
