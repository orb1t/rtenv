#include "regfile.h"

#include "utils.h"
#include "string.h"
#include "syscall.h"
#include "file.h"

struct regfile_response {
    int transfer_len;
    char *buf;
};

int regfile_mmap(struct file *file,
                 struct file_request *request,
                 struct event_monitor *monitor);
int regfile_driver_read(struct regfile *regfile,
                        struct file_request *request,
                        struct event_monitor *monitor);
int regfile_driver_write(struct regfile *regfile,
                         struct file_request *request,
                         struct event_monitor *monitor);
int regfile_driver_lseek(struct regfile *regfile,
                         struct file_request *request,
                         struct event_monitor *monitor);
int regfile_driver_do_mmap(struct regfile *regfile,
                           struct file_request *request,
                           struct event_monitor *monitor);
int regfile_request_read(struct regfile *regfile,
                         struct file_request *request,
                         struct event_monitor *monitor);
int regfile_request_write(struct regfile *regfile,
                          struct file_request *request,
                          struct event_monitor *monitor);
int regfile_request_lseek(struct regfile *regfile,
                          struct file_request *request,
                          struct event_monitor *monitor);

static struct file_operations regfile_ops = {
    .deinit = regfile_deinit,
    .read = regfile_readable,
    .write = regfile_writable,
    .lseek = regfile_lseekable,
    .mmap = regfile_mmap,
};

DECLARE_OBJECT_POOL(struct regfile, regfiles, REGFILE_LIMIT);

int regfile_driver_readable(struct regfile *regfile,
                            struct file_request *request,
                            struct event_monitor *monitor)
{
    if (regfile->buzy)
        return regfile_driver_read(regfile, request, monitor);
    else
        return FILE_ACCESS_ERROR;
}

int regfile_driver_writable(struct regfile *regfile,
                            struct file_request *request,
                            struct event_monitor *monitor)
{
    if (regfile->buzy)
        return regfile_driver_write(regfile, request, monitor);
    else
        return FILE_ACCESS_ERROR;
}

int regfile_driver_lseekable(struct regfile *regfile,
                             struct file_request *request,
                             struct event_monitor *monitor)
{
    if (regfile->buzy)
        return regfile_driver_lseek(regfile, request, monitor);
    else
        return FILE_ACCESS_ERROR;
}

int regfile_driver_mmap(struct regfile *regfile,
                        struct file_request *request,
                        struct event_monitor *monitor)
{
    if (regfile->buzy)
        return regfile_driver_do_mmap(regfile, request, monitor);
    else
        return FILE_ACCESS_ERROR;
}

int regfile_driver_read(struct regfile *regfile, struct file_request *request,
                        struct event_monitor *monitor)
{
    int size = request->size;
    if (size > REGFILE_BUF)
        size = REGFILE_BUF;

    memcpy(request->buf, regfile->buf, size);

    /* still buzy until driver write response */
    return size;
}

int regfile_driver_write(struct regfile *regfile, struct file_request *request,
                         struct event_monitor *monitor)
{
    char *data_buf = request->buf;
    int len = request->size;
    if (len > REGFILE_BUF)
        len = REGFILE_BUF;

    if (len > 0) {
        memcpy(regfile->buf, data_buf, len);
    }
    regfile->transfer_len = len;
    regfile->buzy = 0;
    event_monitor_release(monitor, regfile->event);
    return len;
}

int regfile_driver_lseek(struct regfile *regfile, struct file_request *request,
                         struct event_monitor *monitor)
{
    regfile->transfer_len = request->size;
    regfile->buzy = 0;
    event_monitor_release(monitor, regfile->event);
    return request->size;
}

int regfile_driver_do_mmap(struct regfile *regfile, struct file_request *request,
                           struct event_monitor *monitor)
{
    regfile->transfer_len = (int)request->buf;
    regfile->buzy = 0;
    event_monitor_release(monitor, regfile->event);
    return regfile->transfer_len;
}

/*
 * Steps:
 *  1. Send request to ipc file of block driver
 *  2. Driver read data from device
 *  3. Driver set transfer_len
 *  4. Driver write data to buffer
 *  5. Get transfer_len
 *  6. Read data from buffer
 */
int regfile_request_readable(struct regfile *regfile,
                             struct file_request *request,
                             struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (regfile->request_pid == -1) {
        /* try to send request */
        struct file *driver = regfile->driver_file;
        int size = request->size;
        if (size > REGFILE_BUF)
            size = REGFILE_BUF;

        struct fs_request fs_request = {
            .cmd = FS_CMD_READ,
            .from = task->pid,
            .target = regfile->file.fd,
            .size = size,
            .pos = regfile->pos
        };

        struct file_request file_request = {
            .task = NULL,
            .buf = (char *) &fs_request,
            .size = sizeof(fs_request),
        };
        if (file_write(driver, &file_request, monitor) == 1) {
            regfile->request_pid = task->pid;
            regfile->buzy = 1;
        }
    }
    else if (regfile->request_pid == task->pid && !regfile->buzy) {
        return regfile_request_read(regfile, request, monitor);
    }

    event_monitor_block(monitor, regfile->event, task);
    return FILE_ACCESS_BLOCK;
}

/*
 * Steps:
 *  1. Send request to ipc file of block driver
 *  2. Write data to buffer
 *  3. Driver read data from buffer
 *  4. Driver write data to device
 *  5. Driver set transfer_len
 *  6. Driver write empty data to buffer
 *  7. Get transfer_len
 */
int regfile_request_writable(struct regfile *regfile,
                             struct file_request *request,
                             struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (regfile->request_pid == -1) {
        /* try to send request */
        struct file *driver = regfile->driver_file;
        int size = request->size;
        if (size > REGFILE_BUF)
            size = REGFILE_BUF;

        struct fs_request fs_request = {
            .cmd = FS_CMD_WRITE,
            .from = task->pid,
            .target = regfile->file.fd,
            .size = size,
            .pos = regfile->pos
        };

        struct file_request file_request = {
            .task = NULL,
            .buf = (char *) &fs_request,
            .size = sizeof(fs_request),
        };

        if (file_write(driver, &file_request, monitor) == 1) {

            memcpy(regfile->buf, request->buf, size);

            regfile->request_pid = task->pid;
            regfile->buzy = 1;
        }
    }
    else if (regfile->request_pid == task->pid && !regfile->buzy) {
        return regfile_request_write(regfile, request, monitor);
    }

    event_monitor_block(monitor, regfile->event, task);
    return FILE_ACCESS_BLOCK;
}

int regfile_request_lseekable(struct regfile *regfile,
                              struct file_request *request,
                              struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (regfile->request_pid == -1) {
        /* try to send request */
        struct file *driver = regfile->driver_file;
        int size = request->size;
        if (size > REGFILE_BUF)
            size = REGFILE_BUF;

        int pos;
        switch (request->whence) {
        case SEEK_SET:
            pos = 0;
            break;
        case SEEK_CUR:
            pos = regfile->pos;
            break;
        case SEEK_END:
            pos = -1;
            break;
        default:
            return FILE_ACCESS_ERROR;
        }

        struct fs_request fs_request = {
            .cmd = FS_CMD_SEEK,
            .from = task->pid,
            .target = regfile->file.fd,
            .size = request->size,
            .pos = pos,
        };

        struct file_request file_request = {
            .task = NULL,
            .buf = (char *) &fs_request,
            .size = sizeof(fs_request),
        };

        if (file_write(driver, &file_request, monitor) == 1) {
            regfile->request_pid = task->pid;
            regfile->buzy = 1;
        }
    }
    else if (regfile->request_pid == task->pid && !regfile->buzy) {
        return regfile_request_lseek(regfile, request, monitor);
    }

    event_monitor_block(monitor, regfile->event, task);
    return FILE_ACCESS_BLOCK;
}

int regfile_request_mmap(struct regfile *regfile,
                         struct file_request *request,
                         struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (regfile->request_pid == -1) {
        /* try to send request */
        struct file *driver = regfile->driver_file;

        struct fs_request fs_request = {
            .cmd = FS_CMD_MMAP,
            .from = task->pid,
            .target = regfile->file.fd,
            .size = request->size,
            .pos = request->whence,
        };

        struct file_request file_request = {
            .task = NULL,
            .buf = (char *) &fs_request,
            .size = sizeof(fs_request),
        };

        if (file_write(driver, &file_request, monitor) == 1) {
            regfile->request_pid = task->pid;
            regfile->buzy = 1;
        }
    }
    else if (regfile->request_pid == task->pid && !regfile->buzy) {
        regfile->request_pid = -1;
        return regfile->transfer_len;
    }

    event_monitor_block(monitor, regfile->event, task);
    return FILE_ACCESS_BLOCK;
}

int regfile_request_read(struct regfile *regfile, struct file_request *request,
                         struct event_monitor *monitor)
{
    if (regfile->transfer_len > 0) {
        memcpy(request->buf, regfile->buf, regfile->transfer_len);

        regfile->pos += regfile->transfer_len;
    }

    regfile->request_pid = -1;
    return regfile->transfer_len;
}

int regfile_request_write(struct regfile *regfile,
                          struct file_request *request,
                          struct event_monitor *monitor)
{
    if (regfile->transfer_len > 0) {
        regfile->pos += regfile->transfer_len;
    }

    regfile->request_pid = -1;
    return regfile->transfer_len;
}

int regfile_request_lseek(struct regfile *regfile,
                          struct file_request *request,
                          struct event_monitor *monitor)
{
    if (regfile->transfer_len >= 0) {
        regfile->pos = regfile->transfer_len;
    }

    regfile->request_pid = -1;
    return regfile->transfer_len;
}

int regfile_event_release(struct event_monitor *monitor, int event,
                          struct task_control_block *task, void *data)
{
    struct file *file = data;
    struct file_request *request = (void *)task->stack->r0;

    switch (task->stack->r7) {
    case 0x04:
        return file_read(file, request, monitor);
    case 0x03:
        return file_write(file, request, monitor);
    case 0x0a:
        return file_lseek(file, request, monitor);
    case 0x0f:
        return file_lseek(file, request, monitor);
    default:
        return 0;
    }
}

int regfile_init(int fd, int driver_pid, struct file *files[],
                 struct memory_pool *memory_pool, struct event_monitor *monitor)
{
    struct regfile *regfile;
    struct event *event;

    regfile = object_pool_allocate(&regfiles);

    if (!regfile)
        return -1;

    regfile->driver_pid = driver_pid;
    regfile->driver_file = files[driver_pid + 3];
    regfile->request_pid = -1;
    regfile->buzy = 0;
    regfile->pos = 0;
    regfile->file.ops = &regfile_ops;
    files[fd] = &regfile->file;

    event = event_monitor_allocate(monitor, regfile_event_release, files[fd]);
    regfile->event = event_monitor_find(monitor, event);

    return 0;
}

int regfile_deinit(struct file *file, struct file_request *request,
                   struct event_monitor *monitor)
{
    struct regfile *regfile = container_of(file, struct regfile, file);

    event_monitor_free(monitor, regfile->event);

    object_pool_free(&regfiles, regfile);

    return 0;
}

int regfile_response(int fd, char *buf, int len)
{
    struct regfile_response response = {
        .transfer_len = len,
        .buf = buf
    };
    return write(fd, &response, sizeof(response));
}

int regfile_readable(struct file *file, struct file_request *request,
                     struct event_monitor *monitor)
{
    struct regfile *regfile = container_of(file, struct regfile, file);
    if (regfile->driver_pid == request->task->pid) {
        return regfile_driver_readable(regfile, request, monitor);
    }
    else {
        return regfile_request_readable(regfile, request, monitor);
    }
}

int regfile_writable(struct file *file, struct file_request *request,
                     struct event_monitor *monitor)
{
    struct regfile *regfile = container_of(file, struct regfile, file);
    if (regfile->driver_pid == request->task->pid) {
        return regfile_driver_writable(regfile, request, monitor);
    }
    else {
        return regfile_request_writable(regfile, request, monitor);
    }
}

int regfile_read(struct file *file, struct file_request *request,
                 struct event_monitor *monitor)
{
    struct regfile *regfile = container_of(file, struct regfile, file);
    if (regfile->driver_pid == request->task->pid) {
        return regfile_driver_read(regfile, request, monitor);
    }
    else {
        return regfile_request_read(regfile, request, monitor);
    }
}

int regfile_write(struct file *file, struct file_request *request,
                  struct event_monitor *monitor)
{
    struct regfile *regfile = container_of(file, struct regfile, file);
    if (regfile->driver_pid == request->task->pid) {
        return regfile_driver_write(regfile, request, monitor);
    }
    else {
        return regfile_request_write(regfile, request, monitor);
    }
}

int regfile_lseekable(struct file *file, struct file_request *request,
                      struct event_monitor *monitor)
{
    struct regfile *regfile = container_of(file, struct regfile, file);

    if (regfile->driver_pid == request->task->pid) {
        return regfile_driver_lseekable(regfile, request, monitor);
    }
    else {
        return regfile_request_lseekable(regfile, request, monitor);
    }
}

int regfile_lseek(struct file *file, struct file_request *request,
                  struct event_monitor *monitor)
{
    struct regfile *regfile = container_of(file, struct regfile, file);

    if (regfile->driver_pid == request->task->pid) {
        return regfile_driver_lseek(regfile, request, monitor);
    }
    else {
        return regfile_request_lseek(regfile, request, monitor);
    }
}

int regfile_mmap(struct file *file, struct file_request *request,
                 struct event_monitor *monitor)
{
    struct regfile *regfile = container_of(file, struct regfile, file);
    if (regfile->driver_pid == request->task->pid) {
        return regfile_driver_mmap(regfile, request, monitor);
    }
    else {
        return regfile_request_mmap(regfile, request, monitor);
    }
}

