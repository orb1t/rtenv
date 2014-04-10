#include "program.h"
#include "syscall.h"
#include "file.h"
#include "string.h"


#define PS_PATH_LEN 16

int ps(int argc, char *argv[]);
void itoa(int n, char *dst, int base);
void write_blank(int blank_num);

PROGRAM_DECLARE(ps, ps);

int ps(int argc, char *argv[])
{
    int fdout;
    char ps_message[] = "PID STATUS PRIORITY";
    int ps_message_length = sizeof(ps_message);
    char next_line[3] = {'\n', '\r', '\0'};
    char proc_path[PS_PATH_LEN];
    int proc_file;
    int task_i;
    int pid;
    int status;
    int priority;

    fdout = open("/tmp/mqueue/out", 0);

    write(fdout, &ps_message , ps_message_length);
    write(fdout, &next_line , 3);

    for (task_i = 0; task_i < TASK_LIMIT; task_i++) {
        char task_info_pid[2];
        char task_info_status[2];
        char task_info_priority[3];

        strcpy(proc_path, "/proc/");
        itoa(task_i, proc_path + strlen(proc_path), 10);
        strcpy(proc_path + strlen(proc_path), "/stat");
        proc_file = open(proc_path, 0);

        if (proc_file != -1) {
            lseek(proc_file, 0, SEEK_SET);
            if (read(proc_file, &pid, sizeof(pid)) == -1)
                continue;
            if (read(proc_file, &status, sizeof(status)) == -1)
                continue;
            if (read(proc_file, &priority, sizeof(priority)) == -1)
                continue;

            task_info_pid[0] = '0' + pid;
            task_info_pid[1] = '\0';
            task_info_status[0] = '0' + status;
            task_info_status[1] = '\0';

            itoa(priority, task_info_priority, 10);

            write(fdout, &task_info_pid , 2);
            write_blank(3);
            write(fdout, &task_info_status , 2);
            write_blank(5);
            write(fdout, &task_info_priority , 3);

            write(fdout, &next_line , 3);

            close(proc_file);
        }
    }

    return 0;
}
