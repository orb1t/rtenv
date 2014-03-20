#include "bitmap.h"

#include <stdint.h>


void *bitmap_addr(void *addr, int bit)
{
    return (void *)((uint32_t)addr & 0xf0000000) /* region base */
           + 0x02000000   /* alias base */
           + (((uint32_t)addr & 0x000fffff) << 5)
           /* byte offset: 1 byte(8 bits) -> 32 bytes(8 words)  */
           + (bit * 4);   /* bit offset: 1 bit -> 4 bytes(1 word) */
}

int bitmap_test(void *addr, int bit)
{
    return *(int *)bitmap_addr(addr, bit) & 1;
}

void bitmap_set(void *addr, int bit)
{
    *(int *)bitmap_addr(addr, bit) = 1;
}

void bitmap_clear(void *addr, int bit)
{
    *(int *)bitmap_addr(addr, bit) = 0;
}
