#ifndef RESOURCE_H
#define RESOURCE_H

#define RLIMIT_STACK 3

typedef unsigned int rlim_t;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

#endif
