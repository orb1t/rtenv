#include "serial.h"

#include "syscall.h"
#include "file.h"
#include "fifo.h"



void serialout(USART_TypeDef *uart, unsigned int intr)
{
    int fd;
    char c;
    int doread = 1;
    mkfifo("/dev/tty0/out", 0);
    fd = open("/dev/tty0/out", 0);

    if (fd < 0) {
        const char *p = "Can not open tty0 :-(\n";
        while (*p) {
            while (USART_GetFlagStatus(uart, USART_FLAG_TXE) == RESET);
            USART_SendData(uart, *p++);
        }
        while (1);
    }

    while (1) {
        if (doread)
            read(fd, &c, 1);
        doread = 0;
        if (USART_GetFlagStatus(uart, USART_FLAG_TXE) == SET) {
            USART_SendData(uart, c);
            USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
            doread = 1;
        }
        interrupt_wait(intr);
        USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
    }
}

void serialin(USART_TypeDef *uart, unsigned int intr)
{
    int fd;
    char c;
    mkfifo("/dev/tty0/in", 0);
    fd = open("/dev/tty0/in", 0);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    while (1) {
        interrupt_wait(intr);
        if (USART_GetFlagStatus(uart, USART_FLAG_RXNE) == SET) {
            c = USART_ReceiveData(uart);
            write(fd, &c, 1);
        }
    }
}

