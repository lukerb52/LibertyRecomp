// Linked against the same CMake target as rexruntime, to catch allocator collisions.
#include <o1heap.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

_Static_assert(O1HEAP_ALIGNMENT == sizeof(void*) * 2U, "ReXGlue requires its O1Heap fork");
_Alignas(O1HEAP_ALIGNMENT) static unsigned char arena[65536];

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "O1Heap check failed at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

int main(void)
{
    O1HeapInstance* heap = o1heapInit(arena, sizeof(arena));
    CHECK(heap != NULL);
    unsigned char* data = o1heapAllocate(heap, 64);
    CHECK(data != NULL);
    CHECK((uintptr_t)data % O1HEAP_ALIGNMENT == 0);
    memset(data, 0xa5, 64);
    unsigned char* grown = o1heapReallocate(heap, data, 256);
    CHECK(grown != NULL);
    CHECK((uintptr_t)grown % O1HEAP_ALIGNMENT == 0);
    for (size_t i = 0; i < 64; ++i)
        CHECK(grown[i] == 0xa5);
    unsigned char* shrunk = o1heapReallocate(heap, grown, 32);
    CHECK(shrunk != NULL);
    CHECK(o1heapReallocate(heap, shrunk, sizeof(arena) * 2) == NULL);
    for (size_t i = 0; i < 32; ++i)
        CHECK(shrunk[i] == 0xa5);
    o1heapFree(heap, shrunk);
    CHECK(o1heapDoInvariantsHold(heap));
    puts("ReXGlue O1Heap integration checks passed");
    return 0;
}
