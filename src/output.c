#include "kconfig.h"
#include "file.h"
#include "mqueue.h"
#include "syscall.h"
#include "program.h"

void rs232_xmit_msg_task();
PROGRAM_DECLARE(output, rs232_xmit_msg_task);

void rs232_xmit_msg_task()
{
    int fdout;
    int fdin;
    char str[100];
    int curr_char;

    fdout = open("/dev/tty0/out", 0);
    fdin = mq_open("/tmp/mqueue/out", O_CREAT);
    setpriority(0, PRIORITY_DEFAULT - 2);

    while (1) {
        /* Read from the queue.  Keep trying until a message is
         * received.  This will block for a period of time (specified
         * by portMAX_DELAY). */
        read(fdin, str, 100);

        /* Write each character of the message to the RS232 port. */
        curr_char = 0;
        while (str[curr_char] != '\0') {
            write(fdout, &str[curr_char], 1);
            curr_char++;
        }
    }
}
