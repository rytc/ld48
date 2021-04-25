#define _AMD64_
#include <memoryapi.h>

static void* 
sys_alloc_page(u64 *size) {
    void* result = VirtualAlloc(0, *size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    return result;
}

static void 
sys_free_page(void* pointer, u64 size) {
    VirtualFree(pointer, 0, MEM_RELEASE);
}


