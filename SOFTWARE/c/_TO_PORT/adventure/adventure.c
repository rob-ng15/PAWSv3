#include "PAWSlibrary.h"
#include "PAWSdefinitions.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// Physical Screen
#define SCREEN_W 640
#define SCREEN_H 480

// Logical Game Space (Everything is calculated in this 320x240 space)
#define LOGICAL_W 320
#define LOGICAL_H 240
#define CENTER_X 152
#define CENTER_Y 112

#define WORLD_TILES_X 64
#define WORLD_TILES_Y 64
#define TILE_PX 16
#define WORLD_PX_W (WORLD_TILES_X * TILE_PX)
#define WORLD_PX_H (WORLD_TILES_Y * TILE_PX)

// Tile Indices
#define TILE_EMPTY        0
#define TILE_GRASS        1
#define TILE_WALL         2
#define TILE_GATE         3
#define TILE_DARK_FLOOR   4
#define TILE_MAZE_WALL    5

// Sprite Hardware Indices (0 - 63)
#define SPRITE_PLAYER  0
#define SPRITE_SWORD   1
#define SPRITE_KEY     2
#define SPRITE_CHALICE 3
#define SPRITE_DRAGON  4
#define SPRITE_BAT     5

// Item Types
#define ITEM_NONE    0
#define ITEM_SWORD   1
#define ITEM_KEY     2
#define ITEM_CHALICE 3

// Tilemap Layers
#define TM_BASE 0
#define TM_MAZE 1

// --- World Data ---
static uint8_t world_map[WORLD_TILES_Y][WORLD_TILES_X];
static uint8_t maze_mask[WORLD_TILES_Y][WORLD_TILES_X];

// --- Entities ---
typedef struct {
    int wx, wy;
    int type;
    int carried_by_player;
    int carried_by_bat;
} Item;

typedef struct {
    int wx, wy;
    int alive;
    int state;
    int timer;
} Dragon;

typedef struct {
    int wx, wy;
    int vx, vy;
    int holding_item;
} Bat;

static int player_wx = 10 * TILE_PX;
static int player_wy = 10 * TILE_PX;
static int held_item = ITEM_NONE;
static int gate_open = 0;
static int game_won = 0;

static Item items[4];
static Dragon yorgle = { .wx = 20 * TILE_PX, .wy = 18 * TILE_PX, .alive = 1, .state = 0, .timer = 0 };
static Bat bat = { .wx = 15 * TILE_PX, .wy = 15 * TILE_PX, .vx = 1, .vy = 1, .holding_item = ITEM_NONE };

static int last_reveal_tx = -1;
static int last_reveal_ty = -1;

// --- Procedural Graphics Builders ---

static void build_tile_bitmap(unsigned char *buf, unsigned char fill_col, unsigned char border_col) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (border_col != fill_col && (x == 0 || y == 0 || x == 15 || y == 15)) {
                buf[y * 16 + x] = border_col;
            } else {
                buf[y * 16 + x] = fill_col;
            }
        }
    }
}

static void init_graphics(void) {
    unsigned char tile_buf[256];

    // Tile 0: Fully Transparent
    memset(tile_buf, TRANSPARENT, 256);
    set_tilemap_bitmap(TM_BASE, TILE_EMPTY, tile_buf);
    set_tilemap_bitmap(TM_MAZE, TILE_EMPTY, tile_buf);

    // Tile 1: Grass Floor
    build_tile_bitmap(tile_buf, GREEN2, GREEN3);
    set_tilemap_bitmap(TM_BASE, TILE_GRASS, tile_buf);

    // Tile 2: Exterior Castle Wall
    build_tile_bitmap(tile_buf, BLUE, DKBLUE);
    set_tilemap_bitmap(TM_BASE, TILE_WALL, tile_buf);

    // Tile 3: Portcullis / Gate
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            tile_buf[y * 16 + x] = (x % 4 == 0 || y % 4 == 0) ? YELLOW : BLACK;
        }
    }
    set_tilemap_bitmap(TM_BASE, TILE_GATE, tile_buf);

    // Tile 4: Dark Maze Floor
    memset(tile_buf, BLACK, 256);
    set_tilemap_bitmap(TM_BASE, TILE_DARK_FLOOR, tile_buf);

    // Tile 5: Maze Wall (Illuminated in Red)
    build_tile_bitmap(tile_buf, RED, DKRED);
    set_tilemap_bitmap(TM_MAZE, TILE_MAZE_WALL, tile_buf);

    // --- Sprites (8 tiles per block) ---
    unsigned char sprite_block[2048];
    memset(sprite_block, TRANSPARENT, 2048);

    // Sprite Tile 0: Player
    for (int y = 4; y < 12; y++) {
        for (int x = 4; x < 12; x++) {
            sprite_block[0 * 256 + y * 16 + x] = GREEN7;
        }
    }

    // Sprite Tile 1: Sword
    for (int y = 3; y < 13; y++) {
        sprite_block[1 * 256 + y * 16 + 7] = WHITE;
        sprite_block[1 * 256 + y * 16 + 8] = WHITE;
    }
    for (int x = 4; x < 12; x++) {
        sprite_block[1 * 256 + 10 * 16 + x] = WHITE;
    }
    sprite_block[1 * 256 + 4 * 16 + 6] = sprite_block[1 * 256 + 4 * 16 + 9] = WHITE;
    sprite_block[1 * 256 + 5 * 16 + 5] = sprite_block[1 * 256 + 5 * 16 + 10] = WHITE;

    // Sprite Tile 2: Key
    for (int y = 5; y < 11; y++) sprite_block[2 * 256 + y * 16 + 5] = YELLOW;
    for (int x = 5; x < 12; x++) sprite_block[2 * 256 + 8 * 16 + x] = YELLOW;
    sprite_block[2 * 256 + 9 * 16 + 10] = YELLOW;
    sprite_block[2 * 256 + 9 * 16 + 12] = YELLOW;

    // Sprite Tile 3: Chalice
    for (int x = 4; x < 12; x++) sprite_block[3 * 256 + 4 * 16 + x] = YELLOW;
    for (int y = 5; y < 9; y++) {
        sprite_block[3 * 256 + y * 16 + 4] = YELLOW;
        sprite_block[3 * 256 + y * 16 + 11] = YELLOW;
    }
    for (int x = 5; x < 11; x++) sprite_block[3 * 256 + 9 * 16 + x] = YELLOW;
    for (int y = 10; y < 13; y++) {
        sprite_block[3 * 256 + y * 16 + 7] = YELLOW;
        sprite_block[3 * 256 + y * 16 + 8] = YELLOW;
    }
    for (int x = 5; x < 11; x++) sprite_block[3 * 256 + 13 * 16 + x] = YELLOW;

    // Sprite Tile 4: Dragon
    for (int y = 3; y < 13; y++) {
        for (int x = 3; x < 13; x++) {
            sprite_block[4 * 256 + y * 16 + x] = DKYELLOW;
        }
    }
    sprite_block[4 * 256 + 5 * 16 + 5] = BLACK;
    sprite_block[4 * 256 + 7 * 16 + 2] = sprite_block[4 * 256 + 8 * 16 + 2] = RED;

    // Sprite Tile 5: Dead Dragon
    for (int y = 5; y < 11; y++) {
        for (int x = 3; x < 13; x++) {
            sprite_block[5 * 256 + y * 16 + x] = GREY3;
        }
    }

    // Sprite Tile 6: Bat
    for (int x = 2; x < 14; x++) sprite_block[6 * 256 + 7 * 16 + x] = GREY5;
    for (int y = 5; y < 10; y++) {
        sprite_block[6 * 256 + y * 16 + 7] = GREY5;
        sprite_block[6 * 256 + y * 16 + 8] = GREY5;
    }

    set_sprite_bitmaps(0, sprite_block, 2048);
}

// --- World Map & Maze Setup ---

static void init_world(void) {
    memset(world_map, TILE_GRASS, sizeof(world_map));
    memset(maze_mask, 0, sizeof(maze_mask));

    for (int x = 0; x < WORLD_TILES_X; x++) {
        world_map[0][x] = TILE_WALL;
        world_map[WORLD_TILES_Y - 1][x] = TILE_WALL;
    }
    for (int y = 0; y < WORLD_TILES_Y; y++) {
        world_map[y][0] = TILE_WALL;
        world_map[y][WORLD_TILES_X - 1] = TILE_WALL;
    }

    for (int y = 4; y <= 14; y++) {
        for (int x = 18; x <= 28; x++) {
            if (x == 18 || x == 28 || y == 4 || y == 14) {
                world_map[y][x] = TILE_WALL;
            }
        }
    }
    world_map[14][23] = TILE_GATE;

    for (int y = 32; y <= 60; y++) {
        for (int x = 32; x <= 60; x++) {
            world_map[y][x] = TILE_DARK_FLOOR;
            if (x == 32 || x == 60 || y == 32 || y == 60) {
                maze_mask[y][x] = TILE_MAZE_WALL;
            } else if ((x % 3 == 0 && y % 2 == 0) || (x % 4 == 0 && y % 3 == 0)) {
                maze_mask[y][x] = TILE_MAZE_WALL;
            }
        }
    }
    maze_mask[32][45] = 0;

    for (int y = 0; y < WORLD_TILES_Y; y++) {
        for (int x = 0; x < WORLD_TILES_X; x++) {
            set_tilemap_tile_abs(TM_BASE, x, y, world_map[y][x], 0);
        }
    }

    tm_cs(TM_MAZE);

    items[ITEM_SWORD] = (Item){ .wx = 12 * TILE_PX, .wy = 14 * TILE_PX, .type = ITEM_SWORD };
    items[ITEM_KEY] = (Item){ .wx = 6 * TILE_PX, .wy = 6 * TILE_PX, .type = ITEM_KEY };
    items[ITEM_CHALICE] = (Item){ .wx = 48 * TILE_PX, .wy = 48 * TILE_PX, .type = ITEM_CHALICE };
}

// --- Flashlight Maze Dynamic Reveal ---

static void update_maze_visibility(int player_tx, int player_ty) {
    if (player_tx == last_reveal_tx && player_ty == last_reveal_ty) return;

    int radius = 2; // 5x5 reveal window

    if (last_reveal_tx != -1) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int tx = last_reveal_tx + dx;
                int ty = last_reveal_ty + dy;
                if (tx >= 0 && tx < WORLD_TILES_X && ty >= 0 && ty < WORLD_TILES_Y) {
                    set_tilemap_tile_abs(TM_MAZE, tx, ty, TILE_EMPTY, 0);
                }
            }
        }
    }

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int tx = player_tx + dx;
            int ty = player_ty + dy;
            if (tx >= 0 && tx < WORLD_TILES_X && ty >= 0 && ty < WORLD_TILES_Y) {
                if (maze_mask[ty][tx] == TILE_MAZE_WALL) {
                    set_tilemap_tile_abs(TM_MAZE, tx, ty, TILE_MAZE_WALL, 0);
                }
            }
        }
    }

    last_reveal_tx = player_tx;
    last_reveal_ty = player_ty;
}

// --- Collision Logic ---

static int is_tile_solid(int tx, int ty) {
    if (tx < 0 || tx >= WORLD_TILES_X || ty < 0 || ty >= WORLD_TILES_Y) return 1;
    if (world_map[ty][tx] == TILE_WALL) return 1;
    if (world_map[ty][tx] == TILE_GATE && !gate_open) return 1;
    if (maze_mask[ty][tx] == TILE_MAZE_WALL) return 1;
    return 0;
}

static int check_world_collision(int wx, int wy, int size) {
    int left   = (wx + 4) / TILE_PX;
    int right  = (wx + size - 5) / TILE_PX;
    int top    = (wy + 4) / TILE_PX;
    int bottom = (wy + size - 5) / TILE_PX;

    for (int ty = top; ty <= bottom; ty++) {
        for (int tx = left; tx <= right; tx++) {
            if (is_tile_solid(tx, ty)) return 1;
        }
    }
    return 0;
}

// --- Game Logic Updates ---

static void update_player(void) {
    unsigned short buttons = get_buttons();
    int dx = 0, dy = 0;
    int speed = 2;

    if (buttons & JOY_UP)    dy -= speed;
    if (buttons & JOY_DOWN)  dy += speed;
    if (buttons & JOY_LEFT)  dx -= speed;
    if (buttons & JOY_RIGHT) dx += speed;

    if (dx != 0 && !check_world_collision(player_wx + dx, player_wy, 16)) {
        player_wx += dx;
    }
    if (dy != 0 && !check_world_collision(player_wx, player_wy + dy, 16)) {
        player_wy += dy;
    }

    if ((buttons & JOY_FIRE1) && held_item != ITEM_NONE) {
        items[held_item].carried_by_player = 0;
        items[held_item].wx = player_wx + 16;
        items[held_item].wy = player_wy;
        held_item = ITEM_NONE;
        beep(CHANNEL_LEFT_0, WAVE_SQUARE, 55, 60, 6);
    }

    for (int i = 1; i <= 3; i++) {
        if (!items[i].carried_by_player && !items[i].carried_by_bat) {
            if (abs(player_wx - items[i].wx) < 14 && abs(player_wy - items[i].wy) < 14) {
                if (held_item == ITEM_NONE) {
                    held_item = i;
                    items[i].carried_by_player = 1;
                    beep(CHANNEL_LEFT_0, WAVE_SQUARE, 72, 80, 7);
                }
            }
        }
    }

    if (held_item == ITEM_KEY && !gate_open) {
        int tx = (player_wx + 8) / TILE_PX;
        int ty = (player_wy + 8) / TILE_PX;
        if (abs(tx - 23) <= 1 && abs(ty - 14) <= 1) {
            gate_open = 1;
            world_map[14][23] = TILE_GRASS;
            set_tilemap_tile_abs(TM_BASE, 23, 14, TILE_GRASS, 0);
            beep(CHANNEL_RIGHT_0, WAVE_TRIANGLE, 84, 250, 7);
        }
    }

    if (held_item == ITEM_CHALICE && !game_won) {
        int tx = player_wx / TILE_PX;
        int ty = player_wy / TILE_PX;
        if (tx >= 20 && tx <= 26 && ty >= 6 && ty <= 12) {
            game_won = 1;
            beep(CHANNEL_LEFT_0, WAVE_SINE, 96, 600, 7);
        }
    }
}

static void update_dragon(void) {
    if (!yorgle.alive) return;

    int dist_x = player_wx - yorgle.wx;
    int dist_y = player_wy - yorgle.wy;
    int dist = abs(dist_x) + abs(dist_y);

    if (dist < 180) {
        yorgle.state = 1;
        int step_x = (dist_x > 0) ? 1 : (dist_x < 0) ? -1 : 0;
        int step_y = (dist_y > 0) ? 1 : (dist_y < 0) ? -1 : 0;

        if (!check_world_collision(yorgle.wx + step_x, yorgle.wy, 16)) yorgle.wx += step_x;
        if (!check_world_collision(yorgle.wx, yorgle.wy + step_y, 16)) yorgle.wy += step_y;
    }

    if (held_item == ITEM_SWORD) {
        int sword_wx = items[ITEM_SWORD].wx;
        int sword_wy = items[ITEM_SWORD].wy;
        if (abs(sword_wx - yorgle.wx) < 14 && abs(sword_wy - yorgle.wy) < 14) {
            yorgle.alive = 0;
            beep(CHANNEL_LEFT_1, WAVE_SAW, 30, 300, 7);
        }
    }

    if (yorgle.alive && dist < 8) {
        player_wx = 10 * TILE_PX;
        player_wy = 10 * TILE_PX;
        beep(CHANNEL_LEFT_1, WAVE_NOISE, 15, 400, 7);
    }
}

static void update_bat(void) {
    bat.wx += bat.vx;
    bat.wy += bat.vy;

    if (bat.wx < 2 * TILE_PX || bat.wx > 60 * TILE_PX) bat.vx = -bat.vx;
    if (bat.wy < 2 * TILE_PX || bat.wy > 60 * TILE_PX) bat.vy = -bat.vy;

    if (abs(bat.wx - player_wx) < 12 && abs(bat.wy - player_wy) < 12) {
        if (held_item != ITEM_NONE && bat.holding_item == ITEM_NONE) {
            bat.holding_item = held_item;
            items[held_item].carried_by_player = 0;
            items[held_item].carried_by_bat = 1;
            held_item = ITEM_NONE;
            beep(CHANNEL_RIGHT_1, WAVE_SQUARE, 80, 50, 6);
        }
    }
}

// --- Render Pipeline ---

static void render(void) {
    // 1. Calculate camera using LOGICAL space
    int cam_x = player_wx - CENTER_X;
    int cam_y = player_wy - CENTER_Y;

    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > WORLD_PX_W - LOGICAL_W) cam_x = WORLD_PX_W - LOGICAL_W;
    if (cam_y > WORLD_PX_H - LOGICAL_H) cam_y = WORLD_PX_H - LOGICAL_H;

    // 2. Tilemap Hardware (Accepts logical unscaled offsets)
    unsigned char base_x = (cam_x / TILE_PX) & 0x3f;
    unsigned char base_y = (cam_y / TILE_PX) & 0x3f;
    short fine_x = cam_x % TILE_PX;
    short fine_y = cam_y % TILE_PX;

    tilemap_setbase(TM_BASE, base_x, base_y, fine_x, fine_y);
    tilemap_setbase(TM_MAZE, base_x, base_y, fine_x, fine_y);

    // 3. Update flashlight reveal
    update_maze_visibility((player_wx + 8) / TILE_PX, (player_wy + 8) / TILE_PX);

    // 4. Draw Player Sprite (Scale position to 640x480 space by multiplying by 2)
    int p_screen_x = (player_wx - cam_x) * 2;
    int p_screen_y = (player_wy - cam_y) * 2;
    set_sprite(SPRITE_PLAYER, SPRITE_SHOW, p_screen_x, p_screen_y, 0, SPRITE_LAYER_0 | SPRITE_DOUBLE);

    // 5. Update and Draw Items (Scaled to Screen Space)
    for (int i = 1; i <= 3; i++) {
        if (items[i].carried_by_player) {
            items[i].wx = player_wx + 10;
            items[i].wy = player_wy;
        } else if (items[i].carried_by_bat) {
            items[i].wx = bat.wx;
            items[i].wy = bat.wy;
        }

        int sx = (items[i].wx - cam_x) * 2;
        int sy = (items[i].wy - cam_y) * 2;

        if (sx > -32 && sx < SCREEN_W && sy > -32 && sy < SCREEN_H) {
            set_sprite(i, SPRITE_SHOW, sx, sy, i, SPRITE_LAYER_0 | SPRITE_DOUBLE);
        } else {
            set_sprite(i, SPRITE_HIDE, 0, 0, 0, 0);
        }
    }

    // 6. Draw Dragon (Scaled)
    int d_sx = (yorgle.wx - cam_x) * 2;
    int d_sy = (yorgle.wy - cam_y) * 2;
    if (d_sx > -32 && d_sx < SCREEN_W && d_sy > -32 && d_sy < SCREEN_H) {
        int tile = yorgle.alive ? 4 : 5;
        set_sprite(SPRITE_DRAGON, SPRITE_SHOW, d_sx, d_sy, tile, SPRITE_LAYER_0 | SPRITE_DOUBLE);
    } else {
        set_sprite(SPRITE_DRAGON, SPRITE_HIDE, 0, 0, 0, 0);
    }

    // 7. Draw Bat (Scaled)
    int b_sx = (bat.wx - cam_x) * 2;
    int b_sy = (bat.wy - cam_y) * 2;
    if (b_sx > -32 && b_sx < SCREEN_W && b_sy > -32 && b_sy < SCREEN_H) {
        set_sprite(SPRITE_BAT, SPRITE_SHOW, b_sx, b_sy, 6, SPRITE_LAYER_0 | SPRITE_DOUBLE);
    } else {
        set_sprite(SPRITE_BAT, SPRITE_HIDE, 0, 0, 0, 0);
    }

    // 8. TPU HUD
    tpu_set(1, 1, TRANSPARENT, WHITE, TPU_BOLD);
    if (game_won) {
        tpu_printf(TPU_BLINK | TPU_BOLD, "** QUEST COMPLETE! **");
    } else {
        const char *item_names[] = { "NONE", "SWORD", "KEY", "CHALICE" };
        tpu_printf(TPU_NORMAL, "HELD:%-7s GATE:%s", item_names[held_item], gate_open ? "OPEN" : "LOCKED");
    }
}

int main(void) {
    screen_mode(MODE_RGBM);
    screen_order(LAYER_TILEMAP_0, LAYER_TILEMAP_1, LAYER_SPRITES_0, LAYER_CHARACTERMAP, 0, 0, 0, 0, 0, 0, 0);

    // Hardware Scaling Initialisation using the new 4-tilemap flags
    tilemap_2040(TM0_LOW | TM1_LOW);
    tpu_4080(1);

    tpu_cs();
    tpu_showcursor(0);

    init_graphics();
    init_world();

    while (1) {
        await_vblank();

        update_player();
        update_dragon();
        update_bat();

        render();
    }

    return 0;
}
