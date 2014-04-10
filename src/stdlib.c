#include "stdlib.h"
#include "file.h"
#include "syscall.h"



int atoi(const char *nptr)
{
    int n = 0;
    int sign = 1;
    char c;

    if (*nptr == '-') {
        sign = -1;
        nptr++;
    }

    c = *nptr++;
    while ('0' <= c && c <= '9') {
        n = n * 10 + (c - '0');

        c = *nptr++;
    }

    return sign * n;
}



int execvpe(const char *file, char *const argv[], char *const envp[])
{
    int fd;
    void *addr;

    /* Open binary file */
    fd = open(file, 0);
    if (fd < 0)
        return -1;

    /* Get binary file address */
    addr = mmap(NULL, 1, 0, 0, fd, 0);

    close(fd);

    if (addr == (void *)-1)
        return -1;

    return exec_addr(addr, argv, envp);
}
