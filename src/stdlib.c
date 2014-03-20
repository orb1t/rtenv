#include "stdlib.h"



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
