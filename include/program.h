#ifndef PROGRAM_H
#define PROGRAM_H

#define PROGRAM_DECLARE(_name, _main) \
    __attribute__((section(".program"))) \
    struct program _program_##_name = { \
        .name = #_name, \
        .main = _main \
    }

struct program {
    char *name;
    void (*main)();
};

#endif
