#ifndef BITMAP_H
#define BITMAP_H

#define bitsof(type) (sizeof(type) * 8)

#define div_ceiling(a, b) ((a) / (b) + ((a) % (b) != 0))

#define DECLARE_BITMAP(name, num) int name[div_ceiling(num, bitsof(int))]

void* bitmap_addr(void *addr, int bit);
int bitmap_test(void *addr, int bit);
void bitmap_set(void *addr, int bit);
void bitmap_clear(void *addr, int bit);

#endif
