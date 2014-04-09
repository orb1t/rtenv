#ifndef MODULE_H
#define MODULE_H

#define MODULE_DECLARE(_name, _init) \
    __attribute__((section(".module"))) \
    struct module _module_##_name = { \
        .init = _init, \
    }

struct module {
    void (*init)();
};

void module_run_init();

#endif
