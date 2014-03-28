#include "kconfig.h"
#include "kernel.h"
#include "stm32f10x.h"
#include "stm32_p103.h"
#include "RTOSConfig.h"

#include "syscall.h"

#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include "string.h"
#include "task.h"
#include "memory-pool.h"
#include "path.h"
#include "pipe.h"
#include "fifo.h"
#include "mqueue.h"
#include "block.h"
#include "romdev.h"
#include "event-monitor.h"
#include "romfs.h"
#include "object-pool.h"
#include "serial.h"
#include "output.h"
#include "stack-pool.h"
#include "procfs.h"
#include "shell.h"
#include "first.h"

#ifdef USE_TASK_STAT_HOOK
#include "task-stat-hook.h"
#endif /* USE_TASK_STAT_HOOK */

#define INTR_EVENT(intr) (FILE_LIMIT + (intr) + 15) /* see INTR_LIMIT */
#define INTR_EVENT_REVERSE(event) ((event) - FILE_LIMIT - 15)
#define TIME_EVENT (FILE_LIMIT + INTR_LIMIT)

int intr_release(struct event_monitor *monitor, int event,
                 struct task_control_block *task, void *data)
{
    return 1;
}

int time_release(struct event_monitor *monitor, int event,
                 struct task_control_block *task, void *data)
{
    int *tick_count = data;
    return task->stack->r0 == *tick_count;
}

int exit_release(struct event_monitor *monitor, int event,
                 struct task_control_block *task, void *data)
{
    int *status = data;
    *((int *)task->stack->r1) = *status;
    task->status = TASK_READY;

    return 1;
}

/* System resources */
DECLARE_OBJECT_POOL(struct task_control_block, tasks, TASK_LIMIT);
DECLARE_OBJECT_POOL(struct stack, stacks, STACK_LIMIT);
char memory_space[MEM_LIMIT];
struct file *files[FILE_LIMIT];
struct file_request requests[TASK_LIMIT];
struct list ready_list[PRIORITY_LIMIT + 1];  /* [0 ... 39] */

DECLARE_OBJECT_POOL(struct event, events, EVENT_LIMIT);

int main()
{
    struct stack_pool stack_pool;
    struct memory_pool memory_pool;
    struct event_monitor event_monitor;
    struct task_control_block *current_task;
    int i;
    struct list *list;
    struct task_control_block *task;
    int timeup;
    unsigned int tick_count = 0;
    void *stack;
    size_t stack_size;

    SysTick_Config(configCPU_CLOCK_HZ / configTICK_RATE_HZ);

    init_rs232();
    __enable_irq();

    /* Initialize stack */
    stack_pool_init(&stack_pool, &stacks);

    /* Initialize memory pool */
    memory_pool_init(&memory_pool, MEM_LIMIT, memory_space);

    /* Initialize all files */
    for (i = 0; i < FILE_LIMIT; i++)
        files[i] = NULL;

    /* Initialize ready lists */
    for (i = 0; i <= PRIORITY_LIMIT; i++)
        list_init(&ready_list[i]);

    /* Initialise event monitor */
    event_monitor_init(&event_monitor, &events, ready_list);

    /* Initialize fifos */
    for (i = 0; i <= PATHSERVER_FD; i++)
        file_mknod(i, -1, files, S_IFIFO, &memory_pool, &event_monitor);

    /* Register IRQ events, see INTR_LIMIT */
    for (i = -15; i < INTR_LIMIT - 15; i++)
        event_monitor_register(&event_monitor, INTR_EVENT(i), intr_release, 0);

    event_monitor_register(&event_monitor, TIME_EVENT, time_release,
                           &tick_count);

    /* Initialize first thread */
    stack_size = STACK_DEFAULT_SIZE;
    stack = stack_pool_allocate(&stack_pool, stack_size); /* unsigned int */
    task = object_pool_allocate(&tasks);
    task->stack = (void *)init_task(stack, &first, stack_size);
    task->stack_start = stack;
    task->stack_end = stack + stack_size;
    task->pid = 0;
    task->priority = PRIORITY_DEFAULT;
    task->exit_event = -1;
    list_init(&task->list);
    list_push(&ready_list[task->priority], &task->list);
    current_task = task;

    while (1) {
        current_task->stack = activate(current_task->stack);
        current_task->status = TASK_READY;
        timeup = 0;

#ifdef USE_TASK_STAT_HOOK
        task_stat_hook(tasks.data, current_task->pid);
#endif /* USE_TASK_STAT_HOOK */

        switch (current_task->stack->r7) {
        case 0x1: /* fork */
            /* Get new task */
            task = object_pool_allocate(&tasks);
            if (!task) {
                current_task->stack->r0 = -1;
                break;
            }

            /* Get new stack */
            /* Compute how much of the stack is used */
            size_t used = current_task->stack_end - (void *)current_task->stack;
            /* New stack is END - used */
            stack_size = current_task->stack_end - current_task->stack_start;
            stack = stack_pool_allocate(&stack_pool, stack_size);
            if (!stack) {
                object_pool_free(&tasks, task);
                current_task->stack->r0 = -1;
                break;
            }

            /* Setup stack */
            task->stack = stack + stack_size - used;
            task->stack_start = stack;
            task->stack_end = stack + stack_size;
            /* Copy only the used part of the stack */
            memcpy(task->stack, current_task->stack, used);
            /* Set PID */
            task->pid = object_pool_find(&tasks, task);
            /* Set priority, inherited from forked task */
            task->priority = current_task->priority;
            /* Clear exit event */
            task->exit_event = -1;
            /* Set return values in each process */
            current_task->stack->r0 = task->pid;
            task->stack->r0 = 0;
            /* Push to ready list */
            list_init(&task->list);
            list_push(&ready_list[task->priority], &task->list);
            break;
        case 0x2: /* getpid */
            current_task->stack->r0 = current_task->pid;
            break;
        case 0x3: { /* write */
            /* Check fd is valid */
            int fd = current_task->stack->r0;
            if (fd < FILE_LIMIT && files[fd]) {
                struct file_request *request = &requests[current_task->pid];
                /* Prepare file request, store reference in r0 */
                request->task = current_task;
                request->buf = (void *)current_task->stack->r1;
                request->size = current_task->stack->r2;
                current_task->stack->r0 = (int)request;

                /* Write */
                file_write(files[fd], request, &event_monitor);
            }
            else {
                current_task->stack->r0 = -1;
            }
        }
        break;
        case 0x4: { /* read */
            /* Check fd is valid */
            int fd = current_task->stack->r0;
            if (fd < FILE_LIMIT && files[fd]) {
                struct file_request *request = &requests[current_task->pid];
                /* Prepare file request, store reference in r0 */
                request->task = current_task;
                request->buf = (void *)current_task->stack->r1;
                request->size = current_task->stack->r2;
                current_task->stack->r0 = (int)request;

                /* Read */
                file_read(files[fd], request, &event_monitor);
            }
            else {
                current_task->stack->r0 = -1;
            }
        }
        break;
        case 0x5: /* interrupt_wait */
            /* Enable interrupt */
            NVIC_EnableIRQ(current_task->stack->r0);
            /* Block task waiting for interrupt to happen */
            event_monitor_block(&event_monitor,
                                INTR_EVENT(current_task->stack->r0),
                                current_task);
            current_task->status = TASK_WAIT_INTR;
            break;
        case 0x6: { /* getpriority */
            int who = current_task->stack->r0;
            if (who == 0) {
                current_task->stack->r0 = current_task->priority;
            }
            else {
                task = object_pool_get(&tasks, who);
                if (task)
                    current_task->stack->r0 = task->priority;
                else
                    current_task->stack->r0 = -1;
            }
        }
        break;
        case 0x7: { /* setpriority */
            int who = current_task->stack->r0;
            int value = current_task->stack->r1;
            value = (value < 0) ? 0
                    : ((value > PRIORITY_LIMIT) ? PRIORITY_LIMIT : value);
            if (who == 0) {
                current_task->priority = value;
                list_unshift(&ready_list[value], &current_task->list);
            }
            else {
                task = object_pool_get(&tasks, who);
                if (task) {
                    task->priority = value;
                    if (task->status == TASK_READY)
                        list_push(&ready_list[value], &task->list);
                }
                else {
                    current_task->stack->r0 = -1;
                    break;
                }
            }
            current_task->stack->r0 = 0;
        }
        break;
        case 0x8: /* mknod */
            current_task->stack->r0 =
                file_mknod(current_task->stack->r0,
                           current_task->pid,
                           files,
                           current_task->stack->r2,
                           &memory_pool,
                           &event_monitor);
            break;
        case 0x9: /* sleep */
            if (current_task->stack->r0 != 0) {
                current_task->stack->r0 += tick_count;
                event_monitor_block(&event_monitor, TIME_EVENT, current_task);
                current_task->status = TASK_WAIT_TIME;
            }
            break;
        case 0xa: { /* lseek */
            /* Check fd is valid */
            int fd = current_task->stack->r0;
            if (fd < FILE_LIMIT && files[fd]) {
                struct file_request *request = &requests[current_task->pid];
                /* Prepare file request, store reference in r0 */
                request->task = current_task;
                request->buf = NULL;
                request->size = current_task->stack->r1;
                request->whence = current_task->stack->r2;
                current_task->stack->r0 = (int)request;

                /* Read */
                file_lseek(files[fd], request, &event_monitor);
            }
            else {
                current_task->stack->r0 = -1;
            }
        }
        break;
        case 0xb: { /* setrlimit */
            unsigned int resource = current_task->stack->r0;
            if (resource == RLIMIT_STACK) {
                struct rlimit *rlimit = (void *)current_task->stack->r1;
                size_t used = current_task->stack_end
                            - (void *)current_task->stack;
                size_t size = rlimit->rlim_cur;
                stack = stack_pool_relocate(&stack_pool, &size,
                                            current_task->stack_start);
                if (stack) {
                    current_task->stack_start = stack;
                    current_task->stack_end = stack + size;
                    current_task->stack = current_task->stack_end - used;
                }
                else {
                    current_task->stack->r0 = -1;
                }
            }
            else {
                current_task->stack->r0 = -1;
            }
        }
        break;
        case 0xc: { /* rmnod */
            /* Check fd is valid */
            int fd = current_task->stack->r0;
            if (fd < FILE_LIMIT && files[fd]) {
                struct file_request *request = &requests[current_task->pid];
                /* Prepare file request, store reference in r0 */
                request->task = current_task;
                current_task->stack->r0 = (int)request;

                file_rmnod(files[fd], request, &event_monitor, files);
            }
            else {
                current_task->stack->r0 = -1;
            }
        }   break;
        case 0xd: { /* exit */
            list_remove(&current_task->list);
            stack_pool_free(&stack_pool, current_task->stack_start);
            current_task->pid = -1;
            if (current_task->exit_event != -1)
                event_monitor_release(&event_monitor, current_task->exit_event);
            object_pool_free(&tasks, current_task);

            current_task = NULL;
        }
        break;
        case 0xe: { /* waitpid */
            int pid = current_task->stack->r0;

            task = object_pool_get(&tasks, pid);
            if (task) {
                if (task->exit_event == -1) {
                    /* Allocate if does not have one */
                    struct event *event = event_monitor_allocate(&event_monitor, exit_release, &task->status);
                    task->exit_event = event_monitor_find(&event_monitor, event);
                }
                if (task->exit_event != -1) {
                    event_monitor_block(&event_monitor, task->exit_event, current_task);
                    current_task->status = TASK_WAIT_CHILD;
                    break;
                }
            }

            /* Failed to wait */
            current_task->stack->r0 = -1;
            current_task->status = TASK_READY;
        }
        break;
        default: /* Catch all interrupts */
            if ((int)current_task->stack->r7 < 0) {
                unsigned int intr = -current_task->stack->r7 - 16;

                if (intr == SysTick_IRQn) {
                    /* Never disable timer. We need it for pre-emption */
                    timeup = 1;
                    tick_count++;
                    event_monitor_release(&event_monitor, TIME_EVENT);
                }
                else {
                    /* Disable interrupt, interrupt_wait re-enables */
                    NVIC_DisableIRQ(intr);
                }
                event_monitor_release(&event_monitor, INTR_EVENT(intr));
            }
        }

        /* Rearrange ready list and event list */
        event_monitor_serve(&event_monitor);

        /* Check whether to context switch */
        if (current_task) {
            task = current_task;
            if (timeup && ready_list[task->priority].next == &task->list)
                list_push(&ready_list[task->priority], &task->list);
        }

        /* Select next TASK_READY task */
        for (i = 0; list_empty(&ready_list[i]); i++);

        list = ready_list[i].next;
        current_task = list_entry(list, struct task_control_block, list);
    }

    return 0;
}
