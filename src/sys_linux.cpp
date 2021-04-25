
#include <sys/mman.h> // mmap
#include <unistd.h>   // _SC_PAGESIZE

static void* 
sys_alloc_page(u64 *size) {
    void* result = NULL;
    *size = align_up(*size, sysconf(_SC_PAGESIZE));
    result = mmap(0, *size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return result;
}

static void 
sys_free_page(void* pointer, u64 size) {
    munmap(pointer, size);
}

