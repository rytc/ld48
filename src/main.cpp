#include "common.h"
#include <cmath>
#include <cstring> // memset
#include <raylib.h>

#ifdef PLATFORM_LINUX
#include "sys_linux.cpp"
#else
#include "sys_windows.cpp"
#endif

static constexpr f32 TIME_STEP = 1.f/60.f;
static constexpr s32 SCREEN_WIDTH = 1280;
static constexpr s32 SCREEN_HEIGHT = 720;
static constexpr f32 TERMINAL_VELOCITY = 4.f;
static constexpr s32 MAX_PROJECTILE_COUNT = 25;
static constexpr f32 ANIM_FRAME_RATE = 1.f/8.f;

#include "utils.cpp"
#include "animations.cpp"
#include "dialogs.cpp"

static Rand_State g_rand_state;
static s32 g_zone_load = -1;
static s32 g_current_zone = 1;

enum {
    SOUND_ROBOT,
    SOUND_DUNGEON_DOOR,
    SOUND_DOOR,
    SOUND_SHOOT,
    SOUND_EXPLOSION,
    SOUND_PICKUP,
    SOUND_COUNT
};

static Sound g_sounds[SOUND_COUNT];

enum {
    ITEM_NONE,
    ITEM_PISTOL,
    ITEM_KEY
};

struct Item {
    u16 type;
    s16 amount;
};

enum {
    PHYS_STATE_STANDING,
    PHYS_STATE_FALLING,
    PHYS_STATE_JUMPING,

    PHYS_STATE_NO_GRAVITY,
    PHYS_STATE_STATIONARY,
};

static bool 
is_falling(u32 phys_state) {
    return (phys_state == PHYS_STATE_JUMPING || phys_state == PHYS_STATE_FALLING);
}

enum {
    INTERACT_STATE_NONE,
    INTERACT_STATE_NEAR,
    INTERACT_STATE_TRIGGERED,
};

enum {
    ENTITY_FLAG_NONE = 0,
    ENTITY_FLAG_INVULNERABLE = (1<<1),
    ENTITY_FLAG_PICKUP = (1<<2),
    ENTITY_FLAG_NO_COLLIDE = (1<<3),
    ENTITY_FLAG_INTERACTABLE = (1<<4),
    ENTITY_FLAG_PLAYER = (1<<5),
    ENTITY_FLAG_GROUND = (1<<6),
    ENTITY_FLAG_CORPO = (1<<7),
};

typedef u32 Entity_ID;
struct Entity;
typedef void (*Entity_Interact_Proc)(Entity *entity, Entity *other, u8 interact_state);

struct Projectile {
    Vector2 dir;
    Vector2 pos;
    f32 lifetime;
    Entity_ID shooter;
};

struct Entity {
    Entity_ID id;
    u32 flags;

    u32 phys_state;
    Vector2 pos;
    Vector2 velocity;
    Color color;

    Anim_Sprite sprite;

    f32 hp;

    Rectangle collision_rec;
    Entity *last_ground;

    f32 interact_radius;
    u8 interact_state;
    Entity_Interact_Proc on_interact;

    //Item inventory[4];
    Dialog_Sequence dialog;
};

inline static Rectangle
get_bounds(Entity *entity) {
    return {entity->pos.x + entity->collision_rec.x, entity->pos.y + entity->collision_rec.y, entity->collision_rec.width, entity->collision_rec.height};
}

static void
apply_velocity(Entity *entity) {
    entity->pos = add_vec2(entity->pos, mul_vec2_f(entity->velocity, 2.f));
    Vector2 entity_col_circle = add_vec2(entity->pos, {16.f, 32.f});

    if(is_falling(entity->phys_state) && entity->velocity.y < TERMINAL_VELOCITY) {
        entity->velocity.y += 0.15f;

        if(entity->velocity.y >= 0.f) { 
            entity->phys_state = PHYS_STATE_FALLING;
        }
    } else {
        if(entity->last_ground) {
            Rectangle ground_bounds = get_bounds(entity->last_ground);
            if(!CheckCollisionCircleRec(entity_col_circle, 1.f, ground_bounds)) {
                entity->phys_state = PHYS_STATE_FALLING;
                entity->last_ground = nullptr;
            }
        }
    }

}

static bool
in_dialog(Entity *player_entity) {
    return (player_entity->dialog.id >= 0);
}

static void
continue_dialog(Entity *player_entity) {
    auto seq_def = d_sequences[player_entity->dialog.id];
    if(player_entity->dialog.line < seq_def.dialog_count - 1) {
        player_entity->dialog.line += 1;
    } else {
        player_entity->dialog.id = -1;
    }
}

static constexpr s32 MAX_ENTITY_COUNT = 64*1024;
#define INDEX_MASK 0xfff
#define NEW_ENTITY_ID_ADD 0x1000

struct Entity_Index {
    u32 id;
    u16 index;
    u16 next;
};

struct Entity_List {
   u32 entity_count;
   Entity entities[MAX_ENTITY_COUNT];
   Entity_Index indices[MAX_ENTITY_COUNT];
   u16 freelist_enqueue;
   u16 freelist_dequeue;
};
static Entity_List *g_entity_list;

static void
init_entity_list(Entity_List *entity_list) {
    entity_list->entity_count = 0;
    for(u32 i = 0; i < MAX_ENTITY_COUNT; i++) {
        entity_list->indices[i].id = i;
        entity_list->indices[i].next = i + 1;
    }
    memset(entity_list->entities, 0, sizeof(Entity)*MAX_ENTITY_COUNT);
    entity_list->freelist_dequeue = 0;
    entity_list->freelist_enqueue = MAX_ENTITY_COUNT - 1;
}

inline static bool
has_entity(Entity_List *entity_list, Entity_ID id) {
    Entity_Index in = entity_list->indices[id & INDEX_MASK];
    return (in.id == id && in.index != UINT16_MAX);
}

inline static Entity*
get_entity(Entity_List *entity_list, Entity_ID id) {
    return &entity_list->entities[entity_list->indices[id & INDEX_MASK].index];    
}

inline static Entity_ID
add_entity(Entity_List *entity_list) {
    Entity_Index *in = &entity_list->indices[entity_list->freelist_dequeue];
    entity_list->freelist_dequeue = in->next;
    in->id += NEW_ENTITY_ID_ADD;
    in->index = entity_list->entity_count++;

    Entity *entity = &entity_list->entities[in->index];
    entity->id = in->id;
    return entity->id;
}

inline static void
remove_entity(Entity_List *entity_list, Entity_ID id) {
    Entity_Index *in = &entity_list->indices[id & INDEX_MASK];

    Entity &entity = entity_list->entities[in->index];
    entity = entity_list->entities[--entity_list->entity_count];
    entity_list->indices[entity.id & INDEX_MASK].index = in->index;

    in->index = UINT16_MAX;
    entity_list->indices[entity_list->freelist_enqueue].next = id & INDEX_MASK;
    entity_list->freelist_enqueue = id & INDEX_MASK;
}

static Entity_ID 
add_player_entity(Entity_List *entity_list) {
    Entity_ID player_entity_id = add_entity(entity_list);
    Entity *player_entity = get_entity(entity_list, player_entity_id);
    player_entity->flags = ENTITY_FLAG_PLAYER;
    player_entity->pos = {0,0};
    player_entity->collision_rec = {9,0, 12, 32};
    player_entity->velocity = {0,0};
    player_entity->phys_state = PHYS_STATE_FALLING;
    player_entity->last_ground = nullptr;
    player_entity->color = BLUE;
    player_entity->hp = 100.f;
    player_entity->sprite = {};
    player_entity->sprite.sequence = PLAYER_STAND_RIGHT;
    player_entity->dialog.id = -1;

    return player_entity_id;
}

static Entity* 
add_enemy_entity(Entity_List *entity_list, u32 level, u32 type) {
    Entity_ID new_ent_id = add_entity(entity_list);
    Entity *entity = get_entity(entity_list, new_ent_id);

    entity->flags = ENTITY_FLAG_CORPO;
    entity->pos = {0, 0};
    entity->collision_rec = {6, 0, 18, 32};
    entity->velocity = {0,0};
    entity->phys_state = PHYS_STATE_FALLING;
    entity->last_ground = nullptr;
    entity->color = RED;
    entity->sprite = {};
    entity->on_interact = nullptr;

    switch(type) {
        case 0: {
            entity->hp = 100.f;
            entity->sprite.sequence = ROBOT_STAND;
        } break;
        case 1: {
            entity->hp = 25.f;
            entity->sprite.sequence = FLOAT_BOT_STAND;
        } break;
    };

    return entity;
}

static void 
npc_on_interact(Entity *entity, Entity *other, u8 interact_state) {
    if(entity->interact_state != INTERACT_STATE_TRIGGERED && interact_state == INTERACT_STATE_TRIGGERED) {
        entity->interact_state = INTERACT_STATE_TRIGGERED;
        entity->flags |= ENTITY_FLAG_NO_COLLIDE;
        entity->phys_state = PHYS_STATE_STATIONARY;
        other->dialog.id = entity->dialog.id;
        other->dialog.line = 0;
        other->dialog.giver = entity;
        PlaySound(g_sounds[SOUND_ROBOT]);
    } 
}

static void 
dekard_on_interact(Entity *entity, Entity *other, u8 interact_state) {
    if(entity->interact_state != INTERACT_STATE_TRIGGERED && interact_state == INTERACT_STATE_TRIGGERED) {
        entity->interact_state = INTERACT_STATE_TRIGGERED;
        entity->flags |= ENTITY_FLAG_NO_COLLIDE;
        entity->phys_state = PHYS_STATE_STATIONARY;
        other->dialog.id = entity->dialog.id;
        other->dialog.line = 0;
        other->dialog.giver = entity;
    } 

    if(other->dialog.id == -1 && entity->interact_state == INTERACT_STATE_TRIGGERED && interact_state == INTERACT_STATE_NEAR) {
        g_zone_load = 3;
    }
}

static Entity* 
add_npc_entity(Entity_List *entity_list) {
    Entity_ID new_ent_id = add_entity(entity_list);
    Entity *entity = get_entity(entity_list, new_ent_id);

    entity->flags = ENTITY_FLAG_INTERACTABLE;
    entity->pos = {0, 0};
    entity->collision_rec = {6, 0, 18, 32};
    entity->velocity = {0,0};
    entity->phys_state = PHYS_STATE_FALLING;
    entity->last_ground = nullptr;
    entity->color = RED;
    entity->hp = 100.f;
    entity->sprite = {};
    entity->sprite.sequence = ROBOT_STAND;
    entity->interact_radius = 16.f;
    //entity->on_interact = npc_on_interact;

    return entity;
}

static void 
door_zone_2_open(Entity *entity, Entity *other, u8 interact_state) {
    if(interact_state == INTERACT_STATE_TRIGGERED) {
        g_zone_load = 2;
        PlaySound(g_sounds[SOUND_DOOR]);
    }
}

static void
dungeon_door_open(Entity *entity, Entity *other, u8 interact_state) {
    if(interact_state == INTERACT_STATE_TRIGGERED) {
        PlaySound(g_sounds[SOUND_DUNGEON_DOOR]);
        g_zone_load = g_current_zone + 1; 
    }
}

static void
door_on_interact(Entity *entity, Entity *other, u8 interact_state) {
    if(entity->interact_state != INTERACT_STATE_TRIGGERED && interact_state == INTERACT_STATE_TRIGGERED) {
        entity->interact_state = INTERACT_STATE_TRIGGERED;
        entity->flags |= ENTITY_FLAG_NO_COLLIDE;
        play_anim(&entity->sprite, BIG_DOOR_OPENING);
        PlaySound(g_sounds[SOUND_DOOR]);
    }
}

static Entity* 
add_big_door_entity(Entity_List *entity_list, bool unlockable) {
    Entity_ID new_ent_id = add_entity(entity_list);
    Entity *entity = get_entity(entity_list, new_ent_id);

    entity->pos = {0, 0};
    entity->collision_rec = {28, 32, 8, 32};
    entity->phys_state = PHYS_STATE_STATIONARY;
    entity->sprite = {};
    entity->sprite.sequence = BIG_DOOR_CLOSED;
    entity->interact_radius = 24.f;

    if(unlockable) {
        entity->flags = ENTITY_FLAG_INTERACTABLE;
        entity->on_interact = door_on_interact;
    }

    return entity;
}

static Entity* 
add_building_entity(Entity_List *entity_list, u32 building_id) {
    Entity_ID new_ent_id = add_entity(entity_list);
    Entity *entity = get_entity(entity_list, new_ent_id);
    
    entity->flags = ENTITY_FLAG_NO_COLLIDE;
    entity->pos = {0, 0};
    entity->collision_rec = {32, 48, 32, 32};
    entity->phys_state = PHYS_STATE_STATIONARY;
    entity->sprite = {};
    entity->sprite.sequence = building_id;
    entity->interact_radius = 8.f;

    return entity;
}

static Entity*
add_ground_entity(Entity_List *entity_list) {
    Entity_ID new_ent_id = add_entity(entity_list);
    Entity *entity = get_entity(entity_list, new_ent_id);

    entity->pos = {0, 0};
    entity->collision_rec = {0,0, 10, 10};
    entity->phys_state = PHYS_STATE_STATIONARY;
    entity->flags = ENTITY_FLAG_INVULNERABLE | ENTITY_FLAG_GROUND;
    entity->color = WHITE;
    
    return entity;
}

static Entity*
add_item_drop_entity(Entity_List *entity_list, u32 item_id) {
    Entity_ID new_ent_id = add_entity(entity_list);
    Entity *drop = get_entity(entity_list, new_ent_id);

    drop->flags = (ENTITY_FLAG_INVULNERABLE | ENTITY_FLAG_PICKUP);
    drop->pos = {0, 0};
    drop->collision_rec = {0,0, 16, 16};
    drop->velocity = {0,-2.f};
    drop->phys_state = PHYS_STATE_FALLING;
    drop->last_ground = nullptr;
    drop->sprite.sequence = item_id;

    return drop;

}

static void
lootbox_on_interact(Entity *entity, Entity *other, u8 interact_state) {
    if((other->flags & ENTITY_FLAG_PLAYER) == 0) return;

    if(entity->interact_state != INTERACT_STATE_TRIGGERED) {
        if(interact_state == INTERACT_STATE_NEAR) {
            entity->interact_state = INTERACT_STATE_NEAR;
            play_anim(&entity->sprite, LOOTBOX_GLOW);
        } else if(interact_state == INTERACT_STATE_TRIGGERED) {
            entity->interact_state = INTERACT_STATE_TRIGGERED;
            play_anim(&entity->sprite, LOOTBOX_OPEN);
            Entity *item = add_item_drop_entity(g_entity_list, GUN);
            item->pos = add_vec2(entity->pos, {8.f, -32.f});
            PlaySound(g_sounds[SOUND_DUNGEON_DOOR]);
        } else if(interact_state == INTERACT_STATE_NONE && entity->interact_state == INTERACT_STATE_NEAR) {
            entity->interact_state = INTERACT_STATE_NONE;
            play_anim(&entity->sprite, LOOTBOX);
        }
    }
}

static Entity*
add_lootbox_entity(Entity_List *entity_list) {
    Entity_ID new_ent_id = add_entity(entity_list);
    Entity *entity = get_entity(entity_list, new_ent_id);

    entity->flags = ENTITY_FLAG_INVULNERABLE | ENTITY_FLAG_NO_COLLIDE | ENTITY_FLAG_INTERACTABLE; 
    entity->pos = {0, 0};
    entity->collision_rec = {0, 0, 32,32};
    entity->phys_state = PHYS_STATE_STATIONARY;
    entity->color = WHITE;
    entity->sprite = {};
    entity->sprite.sequence = LOOTBOX;
    entity->on_interact = lootbox_on_interact;
    entity->interact_radius = 16.f;
    
    return entity;
}

struct World { 
    Projectile *projctiles;
    s32 projectile_count;
};

static bool
is_interact_key() { return (!(IsKeyPressed(KEY_A) && IsKeyPressed(KEY_D)) && (IsKeyPressed(KEY_E) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))); }

static void 
tick_entities(Entity_List *entity_list) {

    for(s32 entity_index = 0; entity_index < entity_list->entity_count; entity_index++) {
        Entity *entity = &entity_list->entities[entity_index];

        if(entity->phys_state != PHYS_STATE_STATIONARY) { 
            apply_velocity(entity);            
        
            Rectangle bounds = get_bounds(entity);
            for(s32 other_entity_idx = 0; other_entity_idx < entity_list->entity_count; other_entity_idx++) {
                if(other_entity_idx == entity_index) continue; // @Optimize by folding this into the for loop

                Entity *other = &entity_list->entities[other_entity_idx];
                Rectangle other_bounds = get_bounds(other);

                if((other->flags & ENTITY_FLAG_NO_COLLIDE) == 0) {
                    
                    if(CheckCollisionRecs(bounds, other_bounds)) {
                        if(other->flags & ENTITY_FLAG_PICKUP) {
                            remove_entity(entity_list, other->id);
                            PlaySound(g_sounds[SOUND_PICKUP]);
                            continue;
                        }
                        Rectangle col_rect = GetCollisionRec(bounds, other_bounds);
                        Vector2 entity_col_circle = add_vec2(entity->pos, {16.f, 32.f});

                        if(entity->phys_state == PHYS_STATE_FALLING && CheckCollisionCircleRec(entity_col_circle, 2.f, other_bounds)) {
                            entity->velocity.y = 0.f;
                            entity->pos.y -= col_rect.height;
                            entity->phys_state = PHYS_STATE_STANDING;
                            entity->last_ground = other;
                        } else {
                            entity->pos.x += signof(entity->pos.x - other_bounds.x) * col_rect.width;
                            entity->velocity.x = 0.f;
                        }
                    }
                } 
               
                if(entity->flags & ENTITY_FLAG_PLAYER && other->flags & ENTITY_FLAG_INTERACTABLE) {
                    if(CheckCollisionCircleRec({other_bounds.x, other_bounds.y}, other->interact_radius, bounds)) {
                        if(other->on_interact) {
                            u8 interact_state = (is_interact_key() && fabsf(entity->velocity.x) == 0) ? INTERACT_STATE_TRIGGERED : INTERACT_STATE_NEAR;
                            other->on_interact(other, entity, interact_state);
                        }
                    } else {
                        if(other->on_interact) {
                            u8 interact_state = INTERACT_STATE_NONE;
                            other->on_interact(other, entity, interact_state);
                        }
                    }
                } // Interactable
            } // for each other entity

            
        } // not stationary


        update_anim(&entity->sprite, TIME_STEP);
    } // for each entity

}

static Texture2D t_sprites;
static Texture2D t_bg;
static Texture2D t_ground;
static Music m_music;

static void
draw_entities(Entity_List *entity_list) {
    for(s32 entity_index = 0; entity_index < entity_list->entity_count; entity_index++) {
        Entity *entity = &entity_list->entities[entity_index];
        if(entity->flags & ENTITY_FLAG_GROUND) {
            Rectangle bounds = get_bounds(entity);
            //DrawRectangleRec(bounds, entity->color);
            DrawTextureQuad(t_ground, {bounds.width / 64.f, 1}, {0,0}, bounds, WHITE);
        } else {
            Rectangle sprite_rec = get_anim_sprite_rec(entity->sprite);
            DrawTextureRec(t_sprites, sprite_rec, entity->pos, WHITE);
        }
    }
}

static void
draw_dialog(Entity *player_entity) {
    auto seq_def = d_sequences[player_entity->dialog.id];
    DrawRectangleRec({0,0, SCREEN_WIDTH, SCREEN_HEIGHT/4.f}, {0,0,0,128});
    DrawText(seq_def.lines[player_entity->dialog.line], 24, 64, 20, WHITE); 
}

static Entity_ID 
make_zone_1(void) {
    init_entity_list(g_entity_list);

    {
        Entity *entity = add_npc_entity(g_entity_list);
        entity->pos = {128, 300-64};
        entity->dialog.id = DIALOG_SEQUENCE_0;
        entity->on_interact = npc_on_interact;
        entity->dialog.id = DIALOG_SEQUENCE_0;

        
        // Ground0
        entity = add_ground_entity(g_entity_list);
        entity->pos = {-200, 264};
        entity->collision_rec = {0,0, 200, 128};

        // Ground1
        entity = add_ground_entity(g_entity_list);
        entity->pos = {0, 300};
        entity->collision_rec = {0, 0, 700, 128};
       
        // Ground2
        entity = add_ground_entity(g_entity_list);
        entity->pos = {700, 290};
        entity->collision_rec = {0, 0, 832, 128};

        entity = add_lootbox_entity(g_entity_list);
        entity->pos = {400, 300-30};
        entity->collision_rec = {0, 0, 32, 32};

        entity = add_big_door_entity(g_entity_list, true);
        entity->pos = {720, 227};

        entity = add_big_door_entity(g_entity_list, false);
        entity->pos = {1320, 227};

        entity = add_building_entity(g_entity_list, DESERT_BUILDING_1);
        entity->pos = {780, 227};

        entity = add_building_entity(g_entity_list, TOWER_1);
        entity->pos = {900, 228};

        entity = add_building_entity(g_entity_list, DESERT_BUILDING_2);
        entity->pos = {960, 227};
        entity->flags |= ENTITY_FLAG_INTERACTABLE;
        entity->on_interact = door_zone_2_open;

        entity = add_building_entity(g_entity_list, DESERT_BUILDING_3);
        entity->pos = {1040, 227};

        entity = add_building_entity(g_entity_list, DESERT_BUILDING_4);
        entity->pos = {1116, 227};


        entity = add_npc_entity(g_entity_list);
        entity->pos = {850, 259};
        entity->sprite.sequence = GIRL_1;
        entity->flags = ENTITY_FLAG_NO_COLLIDE | ENTITY_FLAG_INTERACTABLE;
        entity->phys_state = PHYS_STATE_STATIONARY;
        entity->dialog.id = DIALOG_SEQUENCE_2;
        entity->on_interact = npc_on_interact;

    } 
   
    Vector2 player_spawn = {32, 300-64};

    Entity_ID player_entity_id = add_player_entity(g_entity_list);
    Entity *player_entity = get_entity(g_entity_list, player_entity_id);
    player_entity->pos = player_spawn;

    return player_entity_id;
}

static Entity_ID 
make_zone_2(void) {
    init_entity_list(g_entity_list);

    UnloadTexture(t_ground);
    t_ground = LoadTexture("graphics/indoor_ground.png");

    UnloadTexture(t_bg);
    t_bg = LoadTexture("graphics/indoor_bg.png");


    // Ground0
    Entity *entity = add_ground_entity(g_entity_list);
    entity->pos = {-32, 300-64};
    entity->collision_rec = {0,0, 32, 128};

    // Ground1
    entity = add_ground_entity(g_entity_list);
    entity->pos = {0, 300};
    entity->collision_rec = {0, 0, 96, 128};
    
    // Ground2
    entity = add_ground_entity(g_entity_list);
    entity->pos = {96, 300-64};
    entity->collision_rec = {0,0, 32, 128};

    entity = add_building_entity(g_entity_list, DEKARD_BUILDING);
    entity->pos = {0, 300-64};
    entity->collision_rec = {0,0,96,64};

    entity = add_npc_entity(g_entity_list);
    entity->pos = {60, 268};
    entity->sprite.sequence = DEKARD;
    entity->flags = ENTITY_FLAG_INTERACTABLE;
    entity->phys_state = PHYS_STATE_STATIONARY;
    entity->dialog.id = DIALOG_DEKARD;
    entity->on_interact = dekard_on_interact;

    Vector2 player_spawn = {2, 300-34};

    Entity_ID player_entity_id = add_player_entity(g_entity_list);
    Entity *player_entity = get_entity(g_entity_list, player_entity_id);
    player_entity->pos = player_spawn;

    return player_entity_id;
}

static Entity_ID 
make_dungeon(void) {
    init_entity_list(g_entity_list);

    u32 level = g_current_zone - 2;

    UnloadTexture(t_ground);
    t_ground = LoadTexture("graphics/cyberpink_ground.png");

    UnloadTexture(t_bg);
    t_bg = LoadTexture("graphics/cyberpink_bg.png");

    // Ground0
    Entity *entity = add_ground_entity(g_entity_list);
    entity->pos = {-200, 232};
    entity->collision_rec = {0,0, 200, 196};

    // Ground1
    entity = add_ground_entity(g_entity_list);
    entity->pos = {0, 300};
    entity->collision_rec = {0, 0, 1000, 128};
    
    // Ground2
    entity = add_ground_entity(g_entity_list);
    entity->pos = {1000, 232};
    entity->collision_rec = {0,0, 200, 196};

    Vector2 player_spawn = {32, 300-34};

    entity = add_building_entity(g_entity_list, DUNGEON_DOOR_OPEN);
    entity->pos = {16, 300-64};

    entity = add_building_entity(g_entity_list, DUNGEON_DOOR);
    entity->pos = {static_cast<f32>(get_rand(&g_rand_state) % 300) + 600, 300-64};
    entity->flags |= ENTITY_FLAG_INTERACTABLE;
    entity->on_interact = dungeon_door_open;

    // 
    u32 enemy_count = (get_rand(&g_rand_state) % (level * 2)) + 1;
    for(u32 e_idx = 0; e_idx < enemy_count; e_idx++) {
        entity = add_enemy_entity(g_entity_list, level, get_rand(&g_rand_state) % 2);
        entity->pos = {static_cast<f32>(get_rand(&g_rand_state) % 950), 264};
    }

    Entity_ID player_entity_id = add_player_entity(g_entity_list);
    Entity *player_entity = get_entity(g_entity_list, player_entity_id);
    player_entity->pos = player_spawn;

    return player_entity_id;

}

static Entity_ID 
make_zone_end(void) {
    init_entity_list(g_entity_list);

    UnloadTexture(t_ground);
    t_ground = LoadTexture("graphics/indoor_ground.png");

    UnloadTexture(t_bg);
    t_bg = LoadTexture("graphics/indoor_bg.png");

    // Ground0
    Entity *entity = add_ground_entity(g_entity_list);
    entity->pos = {-32, 300-64};
    entity->collision_rec = {0,0, 32, 128};

    // Ground1
    entity = add_ground_entity(g_entity_list);
    entity->pos = {0, 300};
    entity->collision_rec = {0, 0, 96, 128};
    
    // Ground2
    entity = add_ground_entity(g_entity_list);
    entity->pos = {96, 300-64};
    entity->collision_rec = {0,0, 32, 128};

    entity = add_building_entity(g_entity_list, DEKARD_BUILDING);
    entity->pos = {0, 300-64};
    entity->collision_rec = {0,0,96,64};

    entity = add_npc_entity(g_entity_list);
    entity->pos = {60, 268};
    entity->sprite.sequence = DEKARD;
    entity->flags = ENTITY_FLAG_INTERACTABLE;
    entity->phys_state = PHYS_STATE_STATIONARY;
    entity->dialog.id = DIALOG_DEKARD_END;
    entity->on_interact = npc_on_interact;

    Vector2 player_spawn = {2, 300-34};

    Entity_ID player_entity_id = add_player_entity(g_entity_list);
    Entity *player_entity = get_entity(g_entity_list, player_entity_id);
    player_entity->pos = player_spawn;

    return player_entity_id;
}



int main(int argc, char **argv) {
    Allocator mem = make_allocator(MB(256));
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "ld48");
    HideCursor();
    SetTargetFPS(60.f);
    InitAudioDevice();
    SetMasterVolume(0.5);

    t_sprites = LoadTexture("graphics/sprites.png");
    t_bg = LoadTexture("graphics/desert_bg.png");
    t_ground = LoadTexture("graphics/desert_ground.png");
    
    g_sounds[SOUND_ROBOT] = LoadSound("sound/robot.wav");
    g_sounds[SOUND_DUNGEON_DOOR] = LoadSound("sound/dungeon_door.wav");
    g_sounds[SOUND_DOOR] = LoadSound("sound/door.wav");
    g_sounds[SOUND_SHOOT] = LoadSound("sound/shoot.wav");
    g_sounds[SOUND_EXPLOSION] = LoadSound("sound/explosion.wav");
    g_sounds[SOUND_PICKUP] = LoadSound("sound/pickup.wav");

    m_music = LoadMusicStream("sound/music_desert.mp3");
    SetMusicVolume(m_music, 0.5);
    PlayMusicStream(m_music);

    init_rand(&g_rand_state);

    Camera2D cam = {};
    cam.target = {0, 0};
    cam.rotation = 0.f;
    cam.zoom = 4.f;

    g_entity_list = alloc(&mem, Entity_List);
   
    Entity_ID player_entity_id = make_zone_1();
    
    const s32 max_projectile_count = 4;
    Projectile *projectiles = alloc_array(&mem, Projectile, max_projectile_count);
    s32 projectile_count = 0;
    
    Vector2 last_mouse_pos = {};

    f64 accumulator = 0.0;
    while(!WindowShouldClose()) {
        accumulator += GetFrameTime();

        UpdateMusicStream(m_music);

        BeginDrawing();
        ClearBackground(BLACK);

        if(g_zone_load > 1) {
            printf("!!!!!!!!!!\n !!! ZONE LOAD %u\n!!!!!!!!!!!!!\n", g_zone_load);
            if(g_zone_load == 2) {
                player_entity_id = make_zone_2();
                g_current_zone = 2;
            } else if(g_current_zone >= 27) {
                g_current_zone += 1;
                player_entity_id = make_zone_end();
                
            } else {
                g_current_zone += 1;
                player_entity_id = make_dungeon();
            }

            g_zone_load = -1;
        }

        DrawTextureEx(t_bg, {0,0}, 0, 2.f, WHITE);
        
                    
        while(accumulator > TIME_STEP) {

            tick_entities(g_entity_list);

            for(u32 idx = 0; idx < projectile_count;) {
                Projectile *projectile = &projectiles[idx]; 

                projectile->lifetime += TIME_STEP;
                projectile->pos = add_vec2(projectile->pos, mul_vec2_f(projectile->dir, 20.f));

                bool collided = false;
                for(s32 entity_idx = 0; entity_idx < g_entity_list->entity_count; entity_idx++) {
                    Entity *entity = &g_entity_list->entities[entity_idx];

                    if(projectile->shooter == entity->id) continue;
                                        
                    Rectangle entity_rect = get_bounds(entity);
                    collided = CheckCollisionCircleRec(projectile->pos, 8.f, entity_rect);
                    
                    if(collided && (entity->flags & ENTITY_FLAG_NO_COLLIDE) == 0) {
                        if((entity->flags & ENTITY_FLAG_INVULNERABLE) == 0) {
                            entity->hp -= 25.f;
                            if(entity->hp <= 0.f) {
                                //remove_entity(g_entity_list, entity->id); // temp
                                entity->flags = entity->flags | (ENTITY_FLAG_INVULNERABLE | ENTITY_FLAG_NO_COLLIDE);
                                entity->phys_state = PHYS_STATE_STATIONARY;
                                entity->collision_rec = {0,0,0,0};
                                if(entity->sprite.sequence == ROBOT_STAND) {
                                    play_anim(&entity->sprite, ROBOT_BLOWUP);
                                } else if(entity->sprite.sequence == FLOAT_BOT_STAND) {
                                    play_anim(&entity->sprite, FLOAT_BOT_BLOWUP);
                                }
                                PlaySound(g_sounds[SOUND_EXPLOSION]);

                            }
                        }
                        break;
                    } else { collided = false; }
                }

                if(projectile->lifetime > 0.15f || collided) { 
                    if(idx != projectile_count - 1) {
                        projectiles[idx] = projectiles[projectile_count - 1];
                        projectile_count--;
                        continue;
                    } else {
                        projectile_count--;
                    }
                }
                idx += 1;
            } // for each projectile
 

            accumulator -= TIME_STEP;
        } // while accumulator

        Entity *player_entity = get_entity(g_entity_list, player_entity_id);
        update_camera(&cam, player_entity->pos);
        
        if(is_interact_key() && fabsf(player_entity->velocity.x) == 0.f) {
            if(in_dialog(player_entity)) {
                continue_dialog(player_entity);
            } else if(player_entity->sprite.sequence == PLAYER_STAND_RIGHT ||
               player_entity->sprite.sequence == PLAYER_RUN_RIGHT_FIST) {
                play_anim(&player_entity->sprite, PLAYER_PUNCH_RIGHT);
            } else if(player_entity->sprite.sequence == PLAYER_STAND_LEFT ||
                player_entity->sprite.sequence == PLAYER_RUN_LEFT_FIST){
                play_anim(&player_entity->sprite, PLAYER_PUNCH_LEFT);
            }

            if(g_current_zone > 2 && g_current_zone < 28) {
                last_mouse_pos = GetMousePosition();
                Vector2 screen_center = {SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f};
                last_mouse_pos.x -= screen_center.x;
                last_mouse_pos.y -= screen_center.y;

                if(projectile_count < max_projectile_count) {
                    Projectile *bullet = projectiles + projectile_count;
                    bullet->pos = add_vec2(player_entity->pos, {16.f, 16.f});
                    bullet->dir = normalize(last_mouse_pos); 
                    bullet->lifetime = 0.f;
                    bullet->shooter = player_entity_id;
                    projectile_count += 1;
                    PlaySound(g_sounds[SOUND_SHOOT]);
                }
            }
        }

        if(!in_dialog(player_entity)) {
            if(IsKeyDown(KEY_D)) {
                if(player_entity->phys_state == PHYS_STATE_STANDING) {
                    player_entity->velocity.x = 1.f;
                } else {
                    player_entity->velocity.x = 0.5f;
                }
                play_anim(&player_entity->sprite, PLAYER_RUN_RIGHT_FIST);
            } else if(IsKeyDown(KEY_A)) {
                if(player_entity->phys_state == PHYS_STATE_STANDING) {
                    player_entity->velocity.x = -1.f;
                } else {
                    player_entity->velocity.x = -0.5f;
                }
                play_anim(&player_entity->sprite, PLAYER_RUN_LEFT_FIST);
            } else {
                if(player_entity->sprite.sequence == PLAYER_RUN_RIGHT_FIST) {
                    play_anim(&player_entity->sprite, PLAYER_STAND_RIGHT);
                } else if(player_entity->sprite.sequence == PLAYER_RUN_LEFT_FIST) {
                    play_anim(&player_entity->sprite, PLAYER_STAND_LEFT);
                }
                player_entity->velocity.x = 0.f;
            }

            if(IsKeyPressed(KEY_SPACE) && player_entity->phys_state != PHYS_STATE_FALLING) {
                player_entity->velocity.y = -1.5f;
                player_entity->last_ground = nullptr;
                player_entity->phys_state = PHYS_STATE_JUMPING;
            }
        }
        
        BeginMode2D(cam);

        draw_entities(g_entity_list);

        for(u32 idx = 0; idx < projectile_count; idx++) {
            Projectile projectile = projectiles[idx]; 
            DrawLineEx(projectile.pos, add_vec2(projectile.pos, mul_vec2_f(projectile.dir, 50)), 0.5f, YELLOW);
        }

        EndMode2D();

        if(in_dialog(player_entity)) {
            draw_dialog(player_entity);
        }
      
        if(g_current_zone > 2 && g_current_zone < 28) {
            char buf[32];
            snprintf(buf, 32, "LeveL: %u", g_current_zone - 2);
            DrawText(buf, 10, 10, 20, WHITE); 
        }

        DrawTextureRec(t_sprites, {448, 0, 32, 32}, add_vec2(GetMousePosition(), {-16,-16}), WHITE);

        EndDrawing();
    }

    CloseWindow();
    destroy_allocator(&mem);
    return 0;
}

