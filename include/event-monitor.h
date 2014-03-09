#ifndef EVENT_H
#define EVENT_H

#include "kconfig.h"
#include "task.h"
#include "object-pool.h"

struct event_monitor;

typedef int (*event_monitor_handler)(struct event_monitor *monitor,
                                     int event,
                                     struct task_control_block *task,
                                     void *data);

struct event {
    event_monitor_handler handler;
    void *data;
    struct list list;
};

struct event_monitor {
    struct object_pool *events;
    DECLARE_BITMAP(pending, EVENT_LIMIT);
    struct list *ready_list;
};

void event_monitor_init(struct event_monitor *monitor,
                        struct object_pool *events,
                        struct list *ready_list);
struct event* event_monitor_register(struct event_monitor *monitor, int n,
                                     event_monitor_handler handler, void *data);
struct event* event_monitor_allocate(struct event_monitor *monitor,
                                     event_monitor_handler handler, void *data);
int event_monitor_find(struct event_monitor *monitor, struct event *event);
void event_monitor_block(struct event_monitor *monitor, int event,
                         struct task_control_block *task);
void event_monitor_release(struct event_monitor *monitor, int event);
void event_monitor_serve(struct event_monitor *monitor);

#endif
