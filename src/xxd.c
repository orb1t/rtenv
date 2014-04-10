#include "program.h"
#include "file.h"
#include "mqueue.h"
#include "syscall.h"
#include "string.h"


#define XXD_WIDTH 0x10

int xxd(int argc, char *argv[]);

PROGRAM_DECLARE(xxd, xxd);

char hexof(int dec)
{
    const char hextab[] = "0123456789abcdef";

    if (dec < 0 || dec > 15)
        return -1;

    return hextab[dec];
}

char char_filter(char c, char fallback)
{
    if (c < 0x20 || c > 0x7E)
        return fallback;

    return c;
}

int xxd(int argc, char *argv[])
{
    int fdout;
    int readfd = -1;
    char buf[XXD_WIDTH];
    char ch;
    char chout[2] = {0};
    int pos = 0;
    int size;
    int i;

    fdout = mq_open("/tmp/mqueue/out", 0);

    if (argc == 1) { /* fallback to stdin */
        readfd = open("/dev/tty0/in", 0);
    }
    else {   /* open file of argv[1] */
        readfd = open(argv[1], 0);

        if (readfd < 0) { /* Open error */
            write(fdout, "xxd: ", 6);
            write(fdout, argv[1], strlen(argv[1]) + 1);
            write(fdout, ": No such file or directory\r\n", 31);
            return -1;
        }
    }

    lseek(readfd, 0, SEEK_SET);
    while ((size = read(readfd, &ch, sizeof(ch))) && size != -1) {
        if (ch != -1 && ch != 0x04) { /* has something read */

            if (pos % XXD_WIDTH == 0) { /* new line, print address */
                for (i = sizeof(pos) * 8 - 4; i >= 0; i -= 4) {
                    chout[0] = hexof((pos >> i) & 0xF);
                    write(fdout, chout, 2);
                }

                write(fdout, ":", 2);
            }

            if (pos % 2 == 0) { /* whitespace for each 2 bytes */
                write(fdout, " ", 2);
            }

            /* higher bits */
            chout[0] = hexof(ch >> 4);
            write(fdout, chout, 2);

            /* lower bits*/
            chout[0] = hexof(ch & 0xF);
            write(fdout, chout, 2);

            /* store in buffer */
            buf[pos % XXD_WIDTH] = ch;

            pos++;

            if (pos % XXD_WIDTH == 0) { /* end of line */
                write(fdout, "  ", 3);

                for (i = 0; i < XXD_WIDTH; i++) {
                    chout[0] = char_filter(buf[i], '.');
                    write(fdout, chout, 2);
                }

                write(fdout, "\r\n", 3);
            }
        }
        else {   /* EOF */
            break;
        }
    }

    if (pos % XXD_WIDTH != 0) { /* rest */
        /* align */
        for (i = pos % XXD_WIDTH; i < XXD_WIDTH; i++) {
            if (i % 2 == 0) { /* whitespace for each 2 bytes */
                write(fdout, " ", 2);
            }
            write(fdout, "  ", 3);
        }

        write(fdout, "  ", 3);

        for (i = 0; i < pos % XXD_WIDTH; i++) {
            chout[0] = char_filter(buf[i], '.');
            write(fdout, chout, 2);
        }

        write(fdout, "\r\n", 3);
    }

    close(readfd);
    close(fdout);

    return 0;
}
