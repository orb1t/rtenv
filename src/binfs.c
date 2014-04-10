#include "module.h"
#include "kernel.h"
#include "syscall.h"
#include "fs.h"
#include "path.h"
#include "object-pool.h"
#include "program.h"
#include "string.h"
#include "file.h"


struct binfs_file {
    int fd;
    struct program *program;
};


void binfs_server();
void binfs_module_init();


MODULE_DECLARE(binfs, binfs_module_init);


void binfs_module_init()
{
    int pid;
    struct task_control_block *task;

    pid = kernel_create_task(binfs_server);
    if (pid < 0)
        return;

    task = task_get(pid);
    task_set_priority(task, 1);
}

struct binfs_file *binfs_get_file(struct object_pool *files, int fd)
{
    struct object_pool_cursor cursor;
    struct binfs_file *file;

    /* Find fd */
    object_pool_for_each(files, cursor, file) {
        if (file->fd == fd) {
            return file;
        }
    }

    return NULL;
}

void binfs_open(struct fs_request *request, struct object_pool *files)
{
    extern struct program _sprogram;
    extern struct program _eprogram;

    int from;
    int pos;
    int status;
    const char *filename;
    struct program *program;
    struct binfs_file *file;
    int fd;

    from = request->from;
    pos = request->pos; /* searching starting position */

    /* Search program */
    filename = request->path + pos;
    for (program = &_sprogram; program < &_eprogram; program++) {
        if (strcmp(filename, program->name) == 0)
            break;
    }
    if (program == &_eprogram) {
        goto error;
    }

    /* Register path */
    fd = path_register(request->path);
    if (fd < 0) {
        goto error_register;
    }

    /* Make regular file */
    status = mknod(fd, 0, S_IFREG);
    if (status != 0) {
        goto error_mknod;
    }

    /* Allocate fd to program mapping */
    file = object_pool_allocate(files);
    if (!file) {
        goto error_file;
    }

    /* Setup mapping */
    file->fd = fd;
    file->program = program;

    /* Response */
    write(from, &fd, sizeof(fd));
    return;


    /* Error handling */
error_file:
    rmnod(fd);

error_mknod:
    path_deregister(request->path);

error_register:
    ;

error:
    status = -1;
    write(from, &status, sizeof(status));
}

void binfs_mmap(struct fs_request *request, struct object_pool *files)
{
    int target;
    struct binfs_file *file;

    target = request->target;
    file = binfs_get_file(files, target);

    if (file) {
        mmap(file->program->main, request->size, 0, 0, target, request->pos);
    }
    else {
        mmap((void *)-1, 0, 0, 0, target, 0);
    }
}

void binfs_close(struct fs_request *request, struct object_pool *files)
{
    int target;
    struct binfs_file *file;

    target = request->target;
    file = binfs_get_file(files, target);

    if (file) {
        object_pool_free(files, file);
    }
}

void binfs_server()
{
    DECLARE_OBJECT_POOL(struct binfs_file, files, BINFS_FILE_LIMIT);
    int self;
    struct fs_request request;

    self = getpid() + 3;

    path_register_fs("binfs");

    while (1) {
        if (read(self, &request, sizeof(request)) != sizeof(request))
            continue;

        switch (request.cmd) {
        case FS_CMD_OPEN:
            binfs_open(&request, &files);
            break;
        case FS_CMD_MMAP:
            binfs_mmap(&request, &files);
            break;
        case FS_CMD_CLOSE:
            binfs_close(&request, &files);
            break;
        default:
            write(request.target, NULL, -1);
        }
    }
}
