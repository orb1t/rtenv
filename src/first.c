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


void first()
{
    mount("/dev/rom0", "/", ROMFS_TYPE, 0);
    mount("", "/proc/", PROCFS_TYPE, 0);

    if (!fork()) {
        setpriority(0, 0);
        serialout(USART2, USART2_IRQn);
    }
    if (!fork()) {
        setpriority(0, 0);
        serialin(USART2, USART2_IRQn);
    }
    if (!fork()) {
        rs232_xmit_msg_task();
    }
    if (!fork()) {
        struct rlimit rlimit = {
            .rlim_cur = 512 * 4
        };

        setrlimit(RLIMIT_STACK, &rlimit);
        setpriority(0, PRIORITY_DEFAULT - 10);
        serial_test_task();
    }

    setpriority(0, PRIORITY_LIMIT);

    while (1);
}
