#include "kconfig.h"
#include "stm32f10x.h"
#include "resource.h"
#include "syscall.h"

#include "path.h"
#include "romdev.h"
#include "romfs.h"
#include "procfs.h"
#include "serial.h"
#include "output.h"
#include "shell.h"
#include "stdlib.h"
#include "file.h"
#include "ctype.h"
#include "string.h"


int read_str(int fd, char *str, int n, char *next)
{
    int i;
    char c;
    int status;

    c = *next;
    if (c < 0)
        return -1;

    /* Skip whitespace */
    while (isspace((int)c) && (status = read(fd, &c, 1)) > 0);
    if (status <= 0) {
        *next = -1;
        return 0;
    }

    i = 0;
    do {
        if (i < n)
            str[i++] = c;
    }
    while ((status = read(fd, &c, 1)) > 0 && !isspace((int)c));
    if (status <= 0) {
        *next = -1;
        return 0;
    }
    str[i] = 0;

    *next = c;

    return i;
}

void first_mount()
{
    int fstab;
    int try;
    char src[PATH_MAX + 1];
    char dst[PATH_MAX + 1];
    char type[FS_TYPE_MAX + 1];
    char c;
    int status;

    try = 3;
    while (try-- && (fstab = open("/etc/fstab", 0)) < 0)
        sleep(1);

    if (fstab < 0)
        return;

    c = ' ';
    while (1) {
        /* Read source path */
        if (read_str(fstab, src, PATH_MAX, &c) <= 0)
            break;

        /* Read destination path */
        if (read_str(fstab, dst, PATH_MAX, &c) <= 0)
            break;

        /* Read destination path */
        if (read_str(fstab, type, PATH_MAX, &c) <= 0)
            break;

        /* Null string means no device */
        if (strcmp("NULL", src) == 0)
            strcpy(src, "");

        /* Skip root */
        if (strcmp("/dev/rom0", src) != 0)
            mount(src, dst, type, 0);

        /* Skip until new line */
        while (c != '\n' && (status = read(fstab, &c, 1)) > 0);
        if (status <= 0)
            break;
    }
}

void first()
{
    mount("/dev/rom0", "/", ROMFS_TYPE, 0);

    first_mount();

    if (!fork()) {
        while (execvpe("/bin/serialin", NULL, NULL) < 0)
            sleep(1);
    }
    if (!fork()) {
        while (execvpe("/bin/serialout", NULL, NULL) < 0)
            sleep(1);
    }
    if (!fork()) {
        while (execvpe("/bin/output", NULL, NULL) < 0)
            sleep(1);
    }
    if (!fork()) {
        while (execvpe("/bin/shell", NULL, NULL) < 0)
            sleep(1);
    }

    setpriority(0, PRIORITY_LIMIT);

    while (1);
}
