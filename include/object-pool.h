#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#define bitsof(type) (sizeof(type) * 8)

#define div_ceiling(a, b) ((a) / (b) + ((a) % (b) != 0))

#define DECLARE_OBJECT_POOL(type, _name, _num) \
    int _object_pool_##_name##_bitmap[div_ceiling(_num, bitsof(int))]; \
    type _object_pool_##_name##_data[_num]; \
    struct object_pool _name = { \
        .name = #_name, \
        .bitmap = _object_pool_##_name##_bitmap, \
        .size = sizeof(type), \
        .num = _num, \
        .data = _object_pool_##_name##_data, \
    };

struct object_pool {
    char *name;
    int *bitmap;
    int size;
    int num;
    void *data;
};

void object_pool_init(struct object_pool *pool);
void* object_pool_register(struct object_pool *pool, int n);
void* object_pool_allocate(struct object_pool *pool);
int object_pool_find(struct object_pool *pool, void *object);
void* object_pool_get(struct object_pool *pool, int n);
void object_pool_free(struct object_pool *pool, void *object);

#endif
