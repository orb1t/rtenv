#include "path.h"

#include "kconfig.h"
#include "string.h"
#include "syscall.h"
#include "fs.h"
#include "object-pool.h"
#include "module.h"
#include "kernel.h"


#define PATH_STACK_SIZE 1024

struct mount {
    int fs;
    int dev;
    char path[PATH_MAX];
};

struct path {
    char name[PATH_MAX];
    int driver;
    int is_cache;
    int ref_count;
};


void pathserver();
void path_module_init();

MODULE_DECLARE(path, path_module_init);
DECLARE_OBJECT_POOL(struct path, paths, PATH_LIMIT);

void path_module_init()
{
    int pid;
    struct task_control_block *task;

    pid = kernel_create_task(pathserver);
    if (pid < 0)
        return;

    task = task_get(pid);
    task_set_priority(task, 0);
    task_set_stack_size(task, PATH_STACK_SIZE);
}

inline int path_get_fd(struct object_pool *paths, struct path *path)
{
    int i = object_pool_find(paths, path);

    if (i != -1)
        return i + 3 + TASK_LIMIT;
    else
        return -1;
}

inline struct path* path_get_by_fd(struct object_pool *paths, int fd)
{
    return object_pool_get(paths, fd - 3 - TASK_LIMIT);
}

/*
 * pathserver assumes that all files are FIFOs that were registered
 * with mkfifo.  It also assumes a global tables of FDs shared by all
 * processes.  It would have to get much smarter to be generally useful.
 *
 * The first TASK_LIMIT FDs are reserved for use by their respective tasks.
 * 0-2 are reserved FDs and are skipped.
 * The server registers itself at /sys/pathserver
 */
void pathserver()
{
    struct path *path;
    int fs_fds[FS_LIMIT];
    char fs_types[FS_LIMIT][FS_TYPE_MAX];
    int nfs_types = 0;
    struct mount mounts[MOUNT_LIMIT];
    int nmounts = 0;
    int i = 0;
    int cmd = 0;
    unsigned int plen = 0;
    unsigned int replyfd = 0;
    char pathname[PATH_MAX];
    int dev = 0;
    int newfd = 0;
    char fs_type[FS_TYPE_MAX];
    int status;
    struct object_pool_cursor cursor;

    /* register itself */
    path = object_pool_register(&paths, 0);
    memcpy(path->name, PATH_SERVER_NAME, sizeof(PATH_SERVER_NAME));

    while (1) {
        read(PATHSERVER_FD, &cmd, 4);
        read(PATHSERVER_FD, &replyfd, 4);

        switch (cmd) {
        case PATH_CMD_MKFILE:
            read(PATHSERVER_FD, &plen, 4);
            read(PATHSERVER_FD, pathname, plen);
            read(PATHSERVER_FD, &dev, 4);
            path = object_pool_allocate(&paths);
            newfd = path_get_fd(&paths, path);

            if (path && mknod(newfd, 0, dev) == 0) {
                memcpy(path->name, pathname, plen);
                path->driver = -1;
                path->is_cache = 0;
            }
            else {
                object_pool_free(&paths, path);
            }
            write(replyfd, &newfd, 4);
            break;

        case PATH_CMD_OPEN:
            read(PATHSERVER_FD, &plen, 4);
            read(PATHSERVER_FD, pathname, plen);
            /* Search for path */
            object_pool_for_each (&paths, cursor, path) {
                if (*path->name && strcmp(pathname, path->name) == 0) {
                    if (path->is_cache)
                        path->ref_count++;

                    status = path_get_fd(&paths, path);
                    write(replyfd, &status, 4);
                    break;
                }
            }

            if (!object_pool_cursor_end(&paths, cursor)) {
                break;
            }

            /* Search for mount point */
            for (i = nmounts - 1; i >= 0; i--) {
                if (*mounts[i].path
                    && strncmp(pathname, mounts[i].path,
                               strlen(mounts[i].path)) == 0) {
                    int mlen = strlen(mounts[i].path);
                    struct fs_request request;
                    request.cmd = FS_CMD_OPEN;
                    request.from = replyfd;
                    request.device = mounts[i].dev;
                    request.pos = mlen; /* search starting position */
                    memcpy(request.path, pathname, plen);
                    write(mounts[i].fs, &request, sizeof(request));
                    i = 0;
                    break;
                }
            }

            if (i < 0) {
                i = -1; /* Error: not found */
                write(replyfd, &i, 4);
            }
            break;

        case PATH_CMD_REGISTER_PATH:
            read(PATHSERVER_FD, &plen, 4);
            read(PATHSERVER_FD, pathname, plen);
            path = object_pool_allocate(&paths);
            newfd = path_get_fd(&paths, path);
            if (path) {
                memcpy(path->name, pathname, plen);
                path->driver = replyfd;

                /* Check whether is file system path cache */
                for (i = 0; i < nmounts; i++) {
                    if (mounts[i].fs == replyfd)
                        break;
                }
                if (i < nmounts) {
                    path->is_cache = 1;
                    path->ref_count = 1;
                }
                else {
                    path->is_cache = 0;
                }
            }
            else {
                object_pool_free(&paths, path);
            }
            write(replyfd, &newfd, 4);
            break;

        case PATH_CMD_REGISTER_FS:
            read(PATHSERVER_FD, &plen, 4);
            read(PATHSERVER_FD, fs_type, plen);
            fs_fds[nfs_types] = replyfd;
            memcpy(fs_types[nfs_types], fs_type, plen);
            nfs_types++;
            i = 0;
            write(replyfd, &i, 4);
            break;

        case PATH_CMD_MOUNT: {
            int slen;
            int dlen;
            int tlen;
            char src[PATH_MAX];
            char dst[PATH_MAX];
            char type[FS_TYPE_MAX];
            read(PATHSERVER_FD, &slen, 4);
            read(PATHSERVER_FD, src, slen);
            read(PATHSERVER_FD, &dlen, 4);
            read(PATHSERVER_FD, dst, dlen);
            read(PATHSERVER_FD, &tlen, 4);
            read(PATHSERVER_FD, type, tlen);

            /* Search for filesystem types */
            for (i = 0; i < nfs_types; i++) {
                if (*fs_types[i] && strcmp(type, fs_types[i]) == 0) {
                    break;
                }
            }

            if (i >= nfs_types) {
                status = -1; /* Error: not found */
                write(replyfd, &status, 4);
                break;
            }

            mounts[nmounts].fs = fs_fds[i];


            if (*src) {
                /* Search for device */
                object_pool_for_each (&paths, cursor, path) {
                    if (*path->name && strcmp(src, path->name) == 0) {
                        break;
                    }
                }

                if (object_pool_cursor_end(&paths, cursor)) {
                    status = -1; /* Error: not found */
                    write(replyfd, &status, 4);
                    break;
                }

                dev = path_get_fd(&paths, path);
            }
            else {
                dev = -1;
            }

            /* Store mount point */
            mounts[nmounts].dev = dev;
            memcpy(mounts[nmounts].path, dst, dlen);
            nmounts++;

            status = 0;
            write(replyfd, &status, 4);
        }
        break;

        case PATH_CMD_CLOSE:
            read(PATHSERVER_FD, &i, 4);
            path = path_get_by_fd(&paths, i);
            status = 0;

            if (path) {
                if (path->is_cache && !--path->ref_count) {
                    struct fs_request request;
                    request.cmd = FS_CMD_CLOSE;
                    request.target = i;
                    write(path->driver, &request, sizeof(request));

                    object_pool_free(&paths, path);
                    rmnod(i);
                }
            }
            else {
                status = -1;
            }

            write(replyfd, &status, 4);
            break;

        default:
            ;
        }
    }
}

int path_register(const char *pathname)
{
    int cmd = PATH_CMD_REGISTER_PATH;
    unsigned int replyfd = getpid() + 3;
    size_t plen = strlen(pathname)+1;
    int fd = -1;
    char buf[4+4+4+PATH_MAX];
    int pos = 0;

    path_write_data(buf, &cmd, 4, pos);
    path_write_data(buf, &replyfd, 4, pos);
    path_write_data(buf, &plen, 4, pos);
    path_write_data(buf, pathname, plen, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &fd, 4);

    return fd;
}

int path_register_fs(const char *type)
{
    int cmd = PATH_CMD_REGISTER_FS;
    unsigned int replyfd = getpid() + 3;
    size_t plen = strlen(type)+1;
    int fd = -1;
    char buf[4+4+4+PATH_MAX];
    int pos = 0;

    path_write_data(buf, &cmd, 4, pos);
    path_write_data(buf, &replyfd, 4, pos);
    path_write_data(buf, &plen, 4, pos);
    path_write_data(buf, type, plen, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &fd, 4);

    return fd;
}

int mount(const char *src, const char *dst, const char *type, int flags)
{
    int cmd = PATH_CMD_MOUNT;
    unsigned int replyfd = getpid() + 3;
    size_t slen = strlen(src)+1;
    size_t dlen = strlen(dst) + 1;
    size_t tlen = strlen(type) + 1;
    int status;
    char buf[4 + 4 + 4 + PATH_MAX + 4 + PATH_MAX + 4 + FS_TYPE_MAX];
    int pos = 0;

    path_write_data(buf, &cmd, 4, pos);
    path_write_data(buf, &replyfd, 4, pos);
    path_write_data(buf, &slen, 4, pos);
    path_write_data(buf, src, slen, pos);
    path_write_data(buf, &dlen, 4, pos);
    path_write_data(buf, dst, dlen, pos);
    path_write_data(buf, &tlen, 4, pos);
    path_write_data(buf, type, tlen, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &status, 4);

    return status;
}

