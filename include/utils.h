#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define container_of(ptr, type, member) \
    ((type *)(((void *)ptr) - offsetof(type, member)))

#define array_size(arr) \
    (sizeof(arr) / sizeof(*(arr)))

#endif
