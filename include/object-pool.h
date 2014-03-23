#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include "bitmap.h"

#define DECLARE_OBJECT_POOL(type, _name, _num) \
    DECLARE_BITMAP(_object_pool_##_name##_bitmap, _num); \
    type _object_pool_##_name##_data[_num]; \
    struct object_pool _name = { \
        .name = #_name, \
        .bitmap = _object_pool_##_name##_bitmap, \
        .size = sizeof(type), \
        .num = _num, \
        .data = _object_pool_##_name##_data, \
    };

#define object_pool_for_each(pool, cursor, item) \
    for ((cursor).curr = bitmap_addr((pool)->bitmap, 0), \
            (cursor).last = bitmap_addr((pool)->bitmap, (pool)->num), \
            (item) = (pool)->data; \
         (cursor).curr < (cursor).last; \
         (cursor).curr++, (item)++) \
        if (*(cursor).curr)

#define object_pool_cursor_end(pool, cursor) ((cursor).curr >= (cursor).last)

struct object_pool {
    char *name;
    int *bitmap;
    int size;
    int num;
    void *data;
};

struct object_pool_cursor {
    int *curr;
    int *last;
};

void object_pool_init(struct object_pool *pool);
void* object_pool_register(struct object_pool *pool, int n);
void* object_pool_allocate(struct object_pool *pool);
int object_pool_find(struct object_pool *pool, void *object);
void* object_pool_get(struct object_pool *pool, int n);
void object_pool_free(struct object_pool *pool, void *object);

#endif
