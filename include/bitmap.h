#ifndef BITMAP_H
#define BITMAP_H

void* bitmap_addr(void *addr, int bit);
int bitmap_test(void *addr, int bit);
void bitmap_set(void *addr, int bit);
void bitmap_clear(void *addr, int bit);

#endif
