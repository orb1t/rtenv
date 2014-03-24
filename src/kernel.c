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
#include "stack-pool.h"
#include "procfs.h"
#include "shell.h"

#ifdef USE_TASK_STAT_HOOK
#include "task-stat-hook.h"
#endif /* USE_TASK_STAT_HOOK */


size_t task_count = 0;
struct task_control_block tasks[TASK_LIMIT];

void rs232_xmit_msg_task()
{
    int fdout;
    int fdin;
    char str[100];
    int curr_char;

    fdout = open("/dev/tty0/out", 0);
    fdin = mq_open("/tmp/mqueue/out", O_CREAT);
    setpriority(0, PRIORITY_DEFAULT - 2);

    while (1) {
        /* Read from the queue.  Keep trying until a message is
         * received.  This will block for a period of time (specified
         * by portMAX_DELAY). */
        read(fdin, str, 100);

        /* Write each character of the message to the RS232 port. */
        curr_char = 0;
        while (str[curr_char] != '\0') {
            write(fdout, &str[curr_char], 1);
            curr_char++;
        }
    }
}

void first()
{
    if (!fork()) {
        struct rlimit rlimit = {
            .rlim_cur = 256 * 4
        };

        setrlimit(RLIMIT_STACK, &rlimit);
        setpriority(0, 0);
        pathserver();
    }
    if (!fork()) {
        setpriority(0, 0);
        romdev_driver();
    }
    if (!fork()) {
        struct rlimit rlimit = {
            .rlim_cur = 256 * 4
        };

        setrlimit(RLIMIT_STACK, &rlimit);
        setpriority(0, 0);
        romfs_server();
    }
    if (!fork()) {
        struct rlimit rlimit = {
            .rlim_cur = 256 * 4
        };

        setrlimit(RLIMIT_STACK, &rlimit);
        setpriority(0, 0);
        procfs_server();
    }
    if (!fork()) {
        setpriority(0, 0);
        serialout(USART2, USART2_IRQn);
    }
    if (!fork()) {
        setpriority(0, 0);
        serialin(USART2, USART2_IRQn);
    }
    if (!fork()) {
        rs232_xmit_msg_task();
    }
    if (!fork()) {
        struct rlimit rlimit = {
            .rlim_cur = 512 * 4
        };

        setrlimit(RLIMIT_STACK, &rlimit);
        setpriority(0, PRIORITY_DEFAULT - 10);
        serial_test_task();
    }

    setpriority(0, PRIORITY_LIMIT);

    mount("/dev/rom0", "/", ROMFS_TYPE, 0);
    mount("", "/proc/", PROCFS_TYPE, 0);

    while (1);
}

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

/* System resources */
DECLARE_OBJECT_POOL(struct stack, stacks, STACK_LIMIT);
char memory_space[MEM_LIMIT];
struct file *files[FILE_LIMIT];
struct file_request requests[TASK_LIMIT];
struct list ready_list[PRIORITY_LIMIT + 1];  /* [0 ... 39] */

DECLARE_OBJECT_POOL(struct event, events, EVENT_LIMIT);

int main()
{
    //struct task_control_block tasks[TASK_LIMIT];
    struct stack_pool stack_pool;
    struct memory_pool memory_pool;
    struct event_monitor event_monitor;
    //size_t task_count = 0;
    size_t current_task = 0;
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
    tasks[task_count].stack = (void *)init_task(stack, &first, stack_size);
    tasks[task_count].stack_start = stack;
    tasks[task_count].stack_end = stack + stack_size;
    tasks[task_count].pid = 0;
    tasks[task_count].priority = PRIORITY_DEFAULT;
    list_init(&tasks[task_count].list);
    list_push(&ready_list[tasks[task_count].priority], &tasks[task_count].list);
    task_count++;

    while (1) {
        tasks[current_task].stack = activate(tasks[current_task].stack);
        tasks[current_task].status = TASK_READY;
        timeup = 0;

#ifdef USE_TASK_STAT_HOOK
        task_stat_hook(tasks, current_task);
#endif /* USE_TASK_STAT_HOOK */

        switch (tasks[current_task].stack->r7) {
        case 0x1: /* fork */
            if (task_count == TASK_LIMIT) {
                /* Cannot create a new task, return error */
                tasks[current_task].stack->r0 = -1;
            }
            else {
                /* Compute how much of the stack is used */
                size_t used = tasks[current_task].stack_end
                              - (void *)tasks[current_task].stack;
                /* New stack is END - used */
                stack_size = tasks[current_task].stack_end -
                             tasks[current_task].stack_start;
                stack = stack_pool_allocate(&stack_pool, stack_size);
                tasks[task_count].stack = stack + stack_size - used;
                tasks[task_count].stack_start = stack;
                tasks[task_count].stack_end = stack + stack_size;
                /* Copy only the used part of the stack */
                memcpy(tasks[task_count].stack, tasks[current_task].stack,
                       used);
                /* Set PID */
                tasks[task_count].pid = task_count;
                /* Set priority, inherited from forked task */
                tasks[task_count].priority = tasks[current_task].priority;
                /* Set return values in each process */
                tasks[current_task].stack->r0 = task_count;
                tasks[task_count].stack->r0 = 0;
                list_init(&tasks[task_count].list);
                list_push(&ready_list[tasks[task_count].priority],
                          &tasks[task_count].list);
                /* There is now one more task */
                task_count++;
            }
            break;
        case 0x2: /* getpid */
            tasks[current_task].stack->r0 = current_task;
            break;
        case 0x3: { /* write */
            /* Check fd is valid */
            int fd = tasks[current_task].stack->r0;
            if (fd < FILE_LIMIT && files[fd]) {
                /* Prepare file request, store reference in r0 */
                requests[current_task].task = &tasks[current_task];
                requests[current_task].buf =
                    (void *)tasks[current_task].stack->r1;
                requests[current_task].size = tasks[current_task].stack->r2;
                tasks[current_task].stack->r0 =
                    (int)&requests[current_task];

                /* Write */
                file_write(files[fd], &requests[current_task],
                           &event_monitor);
            }
            else {
                tasks[current_task].stack->r0 = -1;
            }
        }
        break;
        case 0x4: { /* read */
            /* Check fd is valid */
            int fd = tasks[current_task].stack->r0;
            if (fd < FILE_LIMIT && files[fd]) {
                /* Prepare file request, store reference in r0 */
                requests[current_task].task = &tasks[current_task];
                requests[current_task].buf =
                    (void *)tasks[current_task].stack->r1;
                requests[current_task].size = tasks[current_task].stack->r2;
                tasks[current_task].stack->r0 =
                    (int)&requests[current_task];

                /* Read */
                file_read(files[fd], &requests[current_task],
                          &event_monitor);
            }
            else {
                tasks[current_task].stack->r0 = -1;
            }
        }
        break;
        case 0x5: /* interrupt_wait */
            /* Enable interrupt */
            NVIC_EnableIRQ(tasks[current_task].stack->r0);
            /* Block task waiting for interrupt to happen */
            event_monitor_block(&event_monitor,
                                INTR_EVENT(tasks[current_task].stack->r0),
                                &tasks[current_task]);
            tasks[current_task].status = TASK_WAIT_INTR;
            break;
        case 0x6: { /* getpriority */
            int who = tasks[current_task].stack->r0;
            if (who > 0 && who < (int)task_count)
                tasks[current_task].stack->r0 = tasks[who].priority;
            else if (who == 0)
                tasks[current_task].stack->r0 = tasks[current_task].priority;
            else
                tasks[current_task].stack->r0 = -1;
        }
        break;
        case 0x7: { /* setpriority */
            int who = tasks[current_task].stack->r0;
            int value = tasks[current_task].stack->r1;
            value = (value < 0) ? 0
                    : ((value > PRIORITY_LIMIT) ? PRIORITY_LIMIT : value);
            if (who > 0 && who < (int)task_count) {
                tasks[who].priority = value;
                if (tasks[who].status == TASK_READY)
                    list_push(&ready_list[value], &tasks[who].list);
            }
            else if (who == 0) {
                tasks[current_task].priority = value;
                list_unshift(&ready_list[value], &tasks[current_task].list);
            }
            else {
                tasks[current_task].stack->r0 = -1;
                break;
            }
            tasks[current_task].stack->r0 = 0;
        }
        break;
        case 0x8: /* mknod */
            tasks[current_task].stack->r0 =
                file_mknod(tasks[current_task].stack->r0,
                           tasks[current_task].pid,
                           files,
                           tasks[current_task].stack->r2,
                           &memory_pool,
                           &event_monitor);
            break;
        case 0x9: /* sleep */
            if (tasks[current_task].stack->r0 != 0) {
                tasks[current_task].stack->r0 += tick_count;
                tasks[current_task].status = TASK_WAIT_TIME;
                event_monitor_block(&event_monitor, TIME_EVENT,
                                    &tasks[current_task]);
            }
            break;
        case 0xa: { /* lseek */
            /* Check fd is valid */
            int fd = tasks[current_task].stack->r0;
            if (fd < FILE_LIMIT && files[fd]) {
                /* Prepare file request, store reference in r0 */
                requests[current_task].task = &tasks[current_task];
                requests[current_task].buf = NULL;
                requests[current_task].size = tasks[current_task].stack->r1;
                requests[current_task].whence = tasks[current_task].stack->r2;
                tasks[current_task].stack->r0 =
                    (int)&requests[current_task];

                /* Read */
                file_lseek(files[fd], &requests[current_task],
                           &event_monitor);
            }
            else {
                tasks[current_task].stack->r0 = -1;
            }
        }
        break;
        case 0xb: { /* setrlimit */
            task = &tasks[current_task];
            unsigned int resource = task->stack->r0;
            if (resource == RLIMIT_STACK) {
                struct rlimit *rlimit = (void *)task->stack->r1;
                size_t used = task->stack_end - (void *)task->stack;
                size_t size = rlimit->rlim_cur;
                stack = stack_pool_relocate(&stack_pool, &size,
                                            task->stack_start);
                if (stack) {
                    task->stack_start = stack;
                    task->stack_end = stack + size;
                    task->stack = task->stack_end - used;
                }
                else {
                    tasks[current_task].stack->r0 = -1;
                }
            }
            else {
                tasks[current_task].stack->r0 = -1;
            }
        }
        break;
        case 0xc: { /* rmnod */
            /* Check fd is valid */
            int fd = tasks[current_task].stack->r0;
            if (fd < FILE_LIMIT && files[fd]) {
                /* Prepare file request, store reference in r0 */
                requests[current_task].task = &tasks[current_task];
                tasks[current_task].stack->r0 =
                    (int)&requests[current_task];

                file_rmnod(files[fd], &requests[current_task],
                           &event_monitor, files);
            }
            else {
                tasks[current_task].stack->r0 = -1;
            }
        }   break;
        default: /* Catch all interrupts */
            if ((int)tasks[current_task].stack->r7 < 0) {
                unsigned int intr = -tasks[current_task].stack->r7 - 16;

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
        task = &tasks[current_task];
        if (timeup && ready_list[task->priority].next == &task->list)
            list_push(&ready_list[task->priority], &tasks[current_task].list);

        /* Select next TASK_READY task */
        for (i = 0; list_empty(&ready_list[i]); i++);

        list = ready_list[i].next;
        task = list_entry(list, struct task_control_block, list);
        current_task = task->pid;
    }

    return 0;
}
