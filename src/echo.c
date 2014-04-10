#include "program.h"
#include "string.h"
#include "syscall.h"
#include "file.h"



int echo(int argc, char *argv[]);

PROGRAM_DECLARE(echo, echo);


int echo(int argc, char *argv[])
{
    int fdout;
    char next_line[3] = {'\n', '\r', '\0'};
    const int _n = 1; /* Flag for "-n" option. */
    int flag = 0;
    int i;

    fdout = open("/tmp/mqueue/out", 0);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n"))
            flag |= _n;
        else
            break;
    }

    for (; i < argc; i++) {
        write(fdout, argv[i], strlen(argv[i]) + 1);
        if (i < argc - 1)
            write(fdout, " ", 2);
    }

    if (~flag & _n)
        write(fdout, next_line, 3);

    return 0;
}
