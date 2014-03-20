#include "object-pool.h"

#include "bitmap.h"
#include <stddef.h>


void object_pool_init(struct object_pool *pool)
{
    int *mapped = bitmap_addr(pool->bitmap, 0);
    int i;

    for (i = 0; i < pool->num; i++) {
        mapped[i] = 0;
    }
}

void *object_pool_register(struct object_pool *pool, int n)
{
    if (n < 0 || n >= pool->num)
        return NULL;

    if (bitmap_test(pool->bitmap, n))
        return NULL;

    bitmap_set(pool->bitmap, n);
    return pool->data + pool->size * n;
}

void *object_pool_allocate(struct object_pool *pool)
{
    int *mapped = bitmap_addr(pool->bitmap, 0);
    int i;

    for (i = 0; i < pool->num; i++) {
        if (!mapped[i])
            break;
    }
    if (i == pool->num)
        return NULL;

    bitmap_set(pool->bitmap, i);
    return pool->data + pool->size * i;
}

int object_pool_find(struct object_pool *pool, void *object)
{
    int n = (object - pool->data) / pool->size;

    if (n < 0 || n > pool->num)
        return -1;

    return n;
}

void *object_pool_get(struct object_pool *pool, int n)
{
    if (n < 0 || n > pool->num)
        return NULL;

    if (bitmap_test(pool->bitmap, n))
        return pool->data + pool->size * n;

    return NULL;
}

void object_pool_free(struct object_pool *pool, void *object)
{
    int n = object_pool_find(pool, object);

    if (n != -1) {
        bitmap_clear(pool->bitmap, n);
    }
}
