#ifndef KCONFIG_H
#define KCONFIG_H

/* Task */
#define TASK_LIMIT 10  /* Max number of tasks we can handle */
#define STACK_DEFAULT_SIZE 512

/* Stack Chunk */
#define STACK_TOTAL 0x3000
#define STACK_CHUNK_SIZE 512
#define STACK_LIMIT (STACK_TOTAL / STACK_CHUNK_SIZE)

/* FIFO file */
#define FIFO_LIMIT_RESERVED (3 + TASK_LIMIT + 1)
#define FIFO_LIMIT_FREE 2
#define FIFO_LIMIT (FIFO_LIMIT_RESERVED + FIFO_LIMIT_FREE)

/* Message Queue */
#define MQUEUE_LIMIT 2

/* Pipe file */
#define PIPE_LIMIT (FIFO_LIMIT + MQUEUE_LIMIT)
#define PIPE_BUF   64 /* Size of largest atomic pipe message */

/* Block device */
#define BLOCK_LIMIT 1
#define BLOCK_BUF 64

/* Regular file */
#define REGFILE_LIMIT 6
#define REGFILE_BUF 64

/* General file */
#define FILE_LIMIT (PIPE_LIMIT + BLOCK_LIMIT + REGFILE_LIMIT)

/* Memory pool */
#define MEM_LIMIT 0x000

/* Path server */
#define PATH_LIMIT (FILE_LIMIT - TASK_LIMIT - 3)
#define PATH_MAX   32 /* Longest absolute path */
#define PATHSERVER_FD (TASK_LIMIT + 3)
	/* File descriptor of pipe to pathserver */
/* File system type and Mount point */
#define FS_LIMIT 8
#define FS_TYPE_MAX 8
#define MOUNT_LIMIT 4

/* Rom file system */
#define ROMFS_FILE_LIMIT 8

/* Proc file system */
#define PROCFS_FILE_LIMIT TASK_LIMIT

/* Interrupt */
#define INTR_LIMIT 58 /* IRQn = [-15 ... 42] */

/* Event */
#define EVENT_LIMIT_RESERVED (INTR_LIMIT + PIPE_LIMIT * 2 + BLOCK_LIMIT + REGFILE_LIMIT)
#define EVENT_LIMIT_FREE 4
#define EVENT_LIMIT (EVENT_LIMIT_RESERVED + EVENT_LIMIT_FREE)
    /* Read and write event for each file, intr events and time event */

/* Scheduling */
#define PRIORITY_DEFAULT 20
#define PRIORITY_LIMIT (PRIORITY_DEFAULT * 2 - 1)

/* Task statistic hook (with debugger) */
#define USE_TASK_STAT_HOOK 1

#endif
