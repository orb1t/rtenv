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


void first()
{
    mount("/dev/rom0", "/", ROMFS_TYPE, 0);
    mount("", "/proc/", PROCFS_TYPE, 0);
    mount("", "/bin/", "binfs", 0);

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
