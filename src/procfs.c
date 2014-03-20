#include "procfs.h"

#include "fs.h"
#include "kconfig.h"
#include "stdlib.h"
#include "string.h"
#include "syscall.h"
#include "path.h"
#include "task.h"
#include "file.h"


struct procfs_file {
    int fd;
    int pid;
    int status;
    int priority;
};

void procfs_server()
{
    extern struct task_control_block tasks[];
    struct procfs_file files[PROCFS_FILE_LIMIT];
    int nfiles = 0;
    int self;
    struct fs_request request;
    int cmd;
    int from;
    int target;
    int pos;
    int size;
    int pid;
    int status;
    const char *filename;
    void *data;
    int data_start = sizeof(files[0].fd);
    int data_len = sizeof(files[0]) - data_start;
    int i;

    self = getpid() + 3;

    path_register_fs(PROCFS_TYPE);

    while (1) {
        if (read(self, &request, sizeof(request)) == sizeof(request)) {
            cmd = request.cmd;
            switch (cmd) {
            case FS_CMD_OPEN:
                from = request.from;
                pos = request.pos; /* searching starting position */

                status = -1;

                /* Get pid */
                filename = request.path + pos;
                if (*filename == '-' || ('0' <= *filename && *filename <= '9'))
                    pid = atoi(request.path + pos);
                else
                    pid = TASK_LIMIT;

                if (pid < TASK_LIMIT && tasks[pid].pid == pid) {
                    /* Get filename */

                    while (*filename && *filename != '/')
                        filename++;

                    if (*filename == '/') {
                        filename++;

                        if (strcmp(filename, "stat") == 0) {
                            /* Register */
                            status = path_register(request.path);

                            if (status != -1) {
                                if (mknod(status, 0, S_IFREG) == 0) {
                                    files[nfiles].fd = status;
                                    files[nfiles].pid = tasks[pid].pid;
                                    files[nfiles].status = tasks[pid].status;
                                    files[nfiles].priority = tasks[pid].priority;
                                    nfiles++;
                                }
                                else {
                                    status = -1;
                                }
                            }
                        }
                    }
                }

                /* Response */
                write(from, &status, sizeof(status));
                break;
            case FS_CMD_READ:
                from = request.from;
                target = request.target;
                size = request.size;
                pos = request.pos;

                /* Find fd */
                for (i = 0; i < nfiles; i++) {
                    if (files[i].fd == target) {
                        data = (void*)&files[i] + data_start;

                        /* Check boundary */
                        if (pos < 0) {
                            i = nfiles;
                        }

                        if (pos > data_len) {
                            pos = data_len;
                        }

                        if (pos + size > data_len) {
                            size = data_len - pos;
                        }
                        break;
                    }
                }
                if (i >= nfiles) {
                    write(target, NULL, -1);
                    break;
                }

                /* Response */
                write(target, data + pos, size);
                break;

            case FS_CMD_SEEK:
                target = request.target;
                size = request.size;
                pos = request.pos;

                /* Find fd */
                for (i = 0; i < nfiles; i++) {
                    if (files[i].fd == target) {
                        break;
                    }
                }
                if (i >= nfiles) {
                    lseek(target, -1, SEEK_SET);
                    break;
                }

                if (pos == 0) { /* SEEK_SET */
                }
                else if (pos < 0) {   /* SEEK_END */
                    size = data_len + size;
                }
                else {   /* SEEK_CUR */
                    size = pos + size;
                }
                lseek(target, size, SEEK_SET);
                break;

            case FS_CMD_WRITE: /* readonly */
            default:
                write(target, NULL, -1);
            }
        }
    }
}
