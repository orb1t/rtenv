#include "event-monitor.h"

#include "list.h"
#include "bitmap.h"


void event_monitor_init(struct event_monitor *monitor,
                        struct object_pool *events,
                        struct list *ready_list)
{
    int i;
    struct event *data = events->data;

    monitor->events = events;
    monitor->ready_list = ready_list;

    for (i = 0; i < monitor->events->num; i++) {
        bitmap_clear(monitor->pending, i);
        data[i].handler = 0;
        data[i].data = 0;
        list_init(&data[i].list);
    }
}

struct event *event_monitor_register(struct event_monitor *monitor, int n,
                                     event_monitor_handler handler, void *data)
{
    struct event *event = object_pool_register(monitor->events, n);

    if (event) {
        event->handler = handler;
        event->data = data;
    }

    return event;
}

struct event *event_monitor_allocate(struct event_monitor *monitor,
                                     event_monitor_handler handler, void *data)
{
    struct event *event = object_pool_allocate(monitor->events);

    if (event) {
        event->handler = handler;
        event->data = data;
    }

    return event;
}

void event_monitor_free(struct event_monitor *monitor, int event_id)
{
    struct event *event = object_pool_get(monitor->events, event_id);

    object_pool_free(monitor->events, event);
}

int event_monitor_find(struct event_monitor *monitor, struct event *event)
{
    return object_pool_find(monitor->events, event);
}

void event_monitor_block(struct event_monitor *monitor, int event_id,
                         struct task_control_block *task)
{
    if (task->status == TASK_READY) {
        struct event *event = object_pool_get(monitor->events, event_id);

        if (event)
            list_push(&event->list, &task->list);
    }
}

void event_monitor_release(struct event_monitor *monitor, int event_id)
{
    struct event *event = object_pool_get(monitor->events, event_id);

    if (event)
        bitmap_set(monitor->pending, event_id);
}

void event_monitor_serve(struct event_monitor *monitor)
{
    struct event *events = monitor->events->data;

    int i;
    for (i = 0; i < monitor->events->num; i++) {
        if (bitmap_test(monitor->pending, i)) {
            struct event *event = &events[i];
            struct task_control_block *task;
            struct list *curr, *next;

            list_for_each_safe(curr, next, &event->list) {
                task = list_entry(curr, struct task_control_block, list);
                if (event->handler
                    && event->handler(monitor, i, task, event->data)) {
                    list_push(&monitor->ready_list[task->priority], &task->list);
                    task->status = TASK_READY;
                }
            }

            bitmap_clear(monitor->pending, i);

            /* If someone pending events, rescan events */
            i = 0;
        }
    }
}
