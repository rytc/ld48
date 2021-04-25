
#include <random>

struct Rand_State {
    u64 seed;
};

void init_rand(Rand_State *state) {
    state->seed = (u64)(100000.0 *GetTime());
}

u64 get_rand(Rand_State *state) {
    u64 result = state->seed * 0xd989bcacc137dcd5ull;
    state->seed ^= state->seed >> 11;
    state->seed ^= state->seed << 31;
    state->seed ^= state->seed >> 18;
    return u64(result >> 32ull);
}

struct Allocator {
    void *mem;
    u64 size;
    u64 offset;
};

static Allocator 
make_allocator(u64 size) {
    Allocator result = {};
    result.size = size;
    result.mem = sys_alloc_page(&result.size);
    return result;
}

static void 
destroy_allocator(Allocator *allocator) {
    sys_free_page(allocator->mem, allocator->size);
    allocator->mem = nullptr;
}

static void* 
alloc_raw(Allocator *allocator, u64 size, u64 alignment = 0) {
    if(allocator->size < allocator->offset + size) {
        return nullptr;
    }

    u64 aligned_offset = align_up(allocator->offset, alignment);
    void *result = (void*)((u8*)allocator->mem + aligned_offset);

    allocator->offset += (aligned_offset - allocator->offset) + size;
    return result;
}
#define alloc(A, T) (T*)alloc_raw(A, sizeof(T), alignof(T))
#define alloc_array(A, T, C) (T*)alloc_raw(A, sizeof(T)*C, alignof(T))

static Vector2 add_vec2(Vector2 a, Vector2 b) {
    return {a.x + b.x, a.y + b.y};
}

static Vector2 mul_vec2_f(Vector2 a, f32 b) {
    return {a.x * b, a.y * b};
}

static Vector2 normalize(Vector2 a) {
    f32 len = sqrtf((a.x*a.x) + (a.y*a.y));
    return {a.x / len, a.y / len};
}

static void update_camera(Camera2D *cam, Vector2 player_pos) {
    cam->target.x = (player_pos.x + 16.f) - ((SCREEN_WIDTH / cam->zoom) / 2.f);
    cam->target.y = (player_pos.y + 16.f) - ((SCREEN_HEIGHT / cam->zoom) / 2.f);
}


