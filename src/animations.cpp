
struct Anim_Sequence_Def {
    u32 frame_count;
    Rectangle frames[10];
    bool loop;
    u32 next_seq;
};

enum Sprite_Anim_Sequences {
    PLAYER_STAND_RIGHT,
    PLAYER_STAND_LEFT,
    PLAYER_PUNCH_RIGHT,
    PLAYER_PUNCH_LEFT,
    PLAYER_RUN_RIGHT_FIST,
    PLAYER_RUN_LEFT_FIST,
    ROBOT_STAND,
    ROBOT_BLOWUP,
    ROBOT_DEAD,
    LOOTBOX,
    LOOTBOX_GLOW,
    LOOTBOX_OPEN,
    BIG_DOOR_CLOSED,
    BIG_DOOR_OPENING,
    BIG_DOOR_OPEN,
    DESERT_BUILDING_1,
    DESERT_BUILDING_2,
    DESERT_BUILDING_3,
    DESERT_BUILDING_4,
    DEKARD_BUILDING,
    TOWER_1,
    GIRL_1,
    GUN,
    DEKARD,
    DUNGEON_DOOR,
    DUNGEON_DOOR_OPEN,
    FLOAT_BOT_STAND,
    FLOAT_BOT_BLOWUP,
    FLOAT_BOT_DEAD,
};

static Anim_Sequence_Def a_sequences[] = {
    {1, {{0,0,32,32}}, true}, // PLAYER_STAND_RIGHT
    {1, {{352,0,32,32}}, true}, // PLAYER_STAND_LEFT
    {1, {{384,0,32,32}}, false, PLAYER_STAND_RIGHT}, // PLAYER_PUNCH_RIGHT
    {1, {{416,0,32,32}}, false, PLAYER_STAND_LEFT}, // PLAYER_PUNCH_LEFT
    {5, { // PLAYER_RUN_RIGHT_FIST
        {32, 0, 32, 32},
        {64, 0, 32, 32},
        {96, 0, 32, 32},
        {128, 0, 32, 32},
        {160, 0, 32, 32}
        }, true},
    {5, { // PLAYER_RUN_LEFT_FIST
        {192, 0, 32, 32},
        {224, 0, 32, 32},
        {256, 0, 32, 32},
        {288, 0, 32, 32},
        {320, 0, 32, 32}
        }, true},
    {3, { // ROBOT_STAND
        {0, 32, 32, 32},
        {32, 32, 32, 32},
        {64, 32, 32, 32}
        }, true}, // ROBOT_STAND
    {3, { // ROBOT_BLOWUP
        {0, 96, 32, 32},
        {32, 96, 32, 32},
        {64, 96, 32, 32}
        }, false, ROBOT_DEAD}, // ROBOT_BLOWUP
    {1, {{96, 96, 32, 32}}, true}, // ROBOT_DEAD
    {1, {{0, 480, 32, 32}}, true}, // LOOTBOX
    {1, {{32, 480, 32, 32}}, true}, // LOOTBOX_GLOW
    {1, {{64, 480, 32, 32}}, true}, // LOOTBOX_OPEN
    {1, {{96, 32, 64, 64}}, true}, // BIG_DOOR_CLOSED
    {4, { // BIG_DOOR_OPENING 
        {160, 32, 64, 64},
        {224, 32, 64, 64},
        {288, 32, 64, 64},
        {352, 32, 64, 64},
        }, false, BIG_DOOR_OPEN},
    {1, {{352, 32, 64, 64}}, true}, // BIG_DOOR_OPEN
    {1, {{416, 32, 64, 64}}, true}, // DESERT_BUILDING_1
    {1, {{416, 96, 64, 64}}, true}, // DESERT_BUILDING_2
    {1, {{416, 160, 64, 64}}, true}, // DESERT_BUILDING_3
    {1, {{416, 224, 64, 64}}, true}, // DESERT_BUILDING_3
    {1, {{256, 96, 96, 64}}, true}, // DEKARD_BUILDING 
    {1, {{480, 32, 32, 64}}, true}, // TOWER_1
    {1, {{0, 64, 32, 32}}, true}, // GIRL_1
    {1, {{32, 64, 32, 32}}, true}, // GUN
    {1, {{64, 64, 32, 32}}, true}, // DEKARD
    {1, {{352, 96, 64, 64}}, true}, // DUNGEON_DOOR
    {1, {{352, 160, 64, 64}}, true}, // DUNGEON_DOOR_OPEN
    {1, {{128, 96, 32, 32}}, true}, // FLOAT_BOT_STAND 
    {3, { // ROBOT_BLOWUP
        {160, 96, 32, 32},
        {192, 96, 32, 32},
        {224, 96, 32, 32}
        }, false, FLOAT_BOT_DEAD}, // FLOAT_BOT_BLOWUP
    {1, {{224, 96, 32, 32}}, true}, // FLOAT_BOT_DEAD 


};

struct Anim_Sprite {
    u32 sequence;
    u32 frame_index;
    f32 timer;
};

static Rectangle 
get_anim_sprite_rec(Anim_Sprite sprite) {
    return a_sequences[sprite.sequence].frames[sprite.frame_index];
}

static void
play_anim(Anim_Sprite *sprite, u32 sequence) {
    if(sprite->sequence == sequence) return;

    sprite->timer = 0;
    sprite->sequence = sequence;
    sprite->frame_index = 0;
}

static void
update_anim(Anim_Sprite *sprite, f32 dt) {
    sprite->timer += dt;
    Anim_Sequence_Def seq = a_sequences[sprite->sequence];
    if(sprite->timer >= ANIM_FRAME_RATE) {
        if(sprite->frame_index + 1 >= seq.frame_count && seq.loop == false) {
            play_anim(sprite, seq.next_seq);
        } else {
            sprite->frame_index = (sprite->frame_index + 1) % seq.frame_count;
            sprite->timer = 0;
        }
    }
}


