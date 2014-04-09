#include "module.h"


void module_run_init()
{
    extern struct module _smodule;
    extern struct module _emodule;

    struct module *module;

    for (module = &_smodule; module < &_emodule; module++) {
        module->init();
    }
}
