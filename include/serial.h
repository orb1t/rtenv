#ifndef SERIAL_H
#define SERIAL_H

#include "stm32f10x.h"

void serialout(USART_TypeDef* uart, unsigned int intr);
void serialin(USART_TypeDef* uart, unsigned int intr);

#endif
