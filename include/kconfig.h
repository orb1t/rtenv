#ifndef KCONFIG_H
#define KCONFIG_H

/* Task */
#define TASK_LIMIT 8  /* Max number of tasks we can handle */
#define STACK_SIZE 384 /* Size of task stacks in words */

/* Pipe file */
#define PIPE_LIMIT (TASK_LIMIT * 2)
#define PIPE_BUF   64 /* Size of largest atomic pipe message */

/* Block device */
#define BLOCK_BUF 64

/* Regular file */
#define REGFILE_BUF 64

/* General file */
#define FREG_LIMIT 16 /* Other types file limit */
#define FILE_LIMIT (PIPE_LIMIT + FREG_LIMIT)

/* Memory pool */
#define MEM_LIMIT (2048)

/* Path server */
#define PATH_MAX   32 /* Longest absolute path */
#define PATHSERVER_FD (TASK_LIMIT + 3)
	/* File descriptor of pipe to pathserver */
/* File system type and Mount point */
#define FS_LIMIT 8
#define FS_TYPE_MAX 8
#define MOUNT_LIMIT 4

/* Rom file system */
#define ROMFS_FILE_LIMIT 8

/* Interrupt */
#define INTR_LIMIT 58 /* IRQn = [-15 ... 42] */

/* Event */
#define EVENT_LIMIT (FILE_LIMIT * 2 + INTR_LIMIT + 1)
    /* Read and write event for each file, intr events and time event */

/* Scheduling */
#define PRIORITY_DEFAULT 20
#define PRIORITY_LIMIT (PRIORITY_DEFAULT * 2 - 1)

#endif
