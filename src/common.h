
#include <cstdint>
#include <cstdio>

typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

#define KB(n) (n * 1024LL)
#define MB(n) (KB(n) * 1024LL)
#define signof(f) (((f) < 0.f) ? -1 : 1)

#ifdef DEBUG
    #define d_assert(cond) if(!(cond)) { *(int*)0 = 0; }
#else
    #define d_assert(cond)
#endif

static 
u64 align_up(u64 address, u32 multiple) {
    if(multiple == 0) return address;

    u64 result = address;
    
    if(result % multiple != 0) {
        u64 remainder = result % multiple;
        result = result + multiple - remainder;
    }
    
    return result;
}


