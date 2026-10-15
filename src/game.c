#include "dxball.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "scroller.inc"

static void load_level(Game *g, int level);
static void reset_balls(Game *g);
static void set_paddle_tier(Game *g, int tier);
static void apply_vel(Ball *b);
static void spawn_explo(Game *g, int col, int row, int kind);
static void spawn_parts(Game *g, int x, int y, int n, int color);
static int hit_brick(Game *g, int px, int py, int fireball);
static void insert_score(Game *g);
static void schedule_zap(Game *g);
static void update_force_field(Game *g);
static void update_zap(Game *g);

static const int k_tile_sprite[23] = {
    0, 3, 4, 5, 6, 7, 8, 19, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 56, 57, 58, 59, 60
};

static const int k_paddle_w[] = {36, 73, 146, 218, 292};

static const char *k_wav_slot[] = {
    /* 0 */ "BOING.WAV",
    /* 1 */ "EFFECT.WAV",
    /* 2 */ "BANG.WAV",
    /* 3 */ "AO-LASER.WAV",
    /* 4 */ "BASSDRUM.WAV",
    /* 5 */ "BYEBALL.WAV",
    /* 6 */ NULL,
    /* 7 */ "WOWPULSE.WAV",
    /* 8 */ "SAUCER.WAV",
    /* 9 */ "ORCHESTR.WAV",
    /* 10 */ "EFFECT2.WAV",
    /* 11 */ "SWEEPDOW.WAV",
    /* 12 */ "PEOW!.WAV",
    /* 13 */ "FANFARE.WAV",
    /* 14 */ "PADEXPLO.WAV",
    /* 15 */ "RICOCHET.WAV",
    /* 16 */ "SWORDSWI.WAV",
    /* 17 */ "GUNFIRE.WAV",
    /* 18 */ "HUMM.WAV",
    /* 19 */ "GLASS.WAV",
    /* 20 */ "ORCHBLAS.WAV",
    /* 21 */ "VOLTAGE.WAV",
    /* 22 */ "THUDCLAP.WAV"
};

int tile_to_sprite(int tile) {
    if (tile <= 0 || tile > 22) {
        return 0;
    }
    return k_tile_sprite[tile];
}

static void path_join(char *dst, size_t n, const char *dir, const char *name) {
    snprintf(dst, n, "%s/%s", dir, name);
}

static int irand(int n) {
    if (n <= 1) {
        return 0;
    }
    return rand() % n;
}

static void blit_pcx_at(Game *g, const PcxImage *img, int dx, int dy, int key) {
    int x, y;
    int w = img->w;
    int h = img->h;
    if (img->pixels == NULL) {
        return;
    }
    for (y = 0; y < h; y++) {
        int sy = y + dy;
        if (sy < 0 || sy >= SCREEN_H) {
            continue;
        }
        for (x = 0; x < w; x++) {
            int sx = x + dx;
            uint8_t p;
            if (sx < 0 || sx >= SCREEN_W) {
                continue;
            }
            p = img->pixels[y * w + x];
            if (key && p == 0) {
                continue;
            }
            g->fb[sy * SCREEN_W + sx] = p;
        }
    }
}

static void blit_pcx(Game *g, const PcxImage *img, int dx, int dy) {
    blit_pcx_at(g, img, dx, dy, 0);
}

static void blit_sprite_bank_w(Game *g, const SpriteBank *bank, int idx,
                               int dx, int dy, int clip_w) {
    const Sprite *sp;
    int x, y;
    int w;
    if (idx < 0 || idx >= bank->count) {
        return;
    }
    sp = &bank->sprites[idx];
    if (sp->pixels == NULL) {
        return;
    }
    w = sp->w;
    if (clip_w > 0 && clip_w < w) {
        w = clip_w;
    }
    for (y = 0; y < sp->h; y++) {
        int sy = y + dy;
        if (sy < 0 || sy >= SCREEN_H) {
            continue;
        }
        for (x = 0; x < w; x++) {
            int sx = x + dx;
            uint8_t p;
            if (sx < 0 || sx >= SCREEN_W) {
                continue;
            }
            p = sp->pixels[y * sp->w + x];
            if (p != 0) {
                g->fb[sy * SCREEN_W + sx] = p;
            }
        }
    }
}

static void blit_sprite_bank(Game *g, const SpriteBank *bank, int idx, int dx, int dy) {
    blit_sprite_bank_w(g, bank, idx, dx, dy, 0);
}

static void blit_sprite(Game *g, int idx, int dx, int dy) {
    blit_sprite_bank(g, &g->mball, idx, dx, dy);
}

static int font_index(const SpriteBank *bank, int ch) {
    int i;
    for (i = 0; i < bank->count; i++) {
        if (bank->sprites[i].ch == (uint8_t)ch) {
            return i;
        }
    }
    return -1;
}

static int draw_char(Game *g, const SpriteBank *bank, int ch, int x, int y) {
    int idx = font_index(bank, ch);
    if (idx < 0) {
        idx = font_index(bank, toupper(ch));
    }
    if (idx < 0) {
        return 8;
    }
    blit_sprite_bank(g, bank, idx, x, y);
    return bank->sprites[idx].w + 1;
}

static void draw_text(Game *g, const SpriteBank *bank, int x, int y, const char *s) {
    while (*s) {
        x += draw_char(g, bank, (unsigned char)*s, x, y);
        s++;
    }
}

static void put_pixel(Game *g, int x, int y, uint8_t c) {
    if (x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) {
        return;
    }
    g->fb[y * SCREEN_W + x] = c;
}

static void apply_vel(Ball *b) {
    double rad = (double)b->angle * 3.141592653589793 / 180.0;
    double spd = (double)b->speed;
    b->vx = (int)(spd * cos(rad));
    b->vy = (int)(-spd * sin(rad));
    if (b->vx == 0) {
        b->vx = (b->angle > 90) ? -1 : 1;
    }
    if (b->vy == 0) {
        b->vy = -1;
    }
}

static int ball_spr_idx(const Game *g, const Ball *b) {
    int s = b->spr;
    if (s <= 0 || s >= g->mball.count || g->mball.sprites[s].pixels == NULL) {
        return 1;
    }
    return s;
}

static void ball_size(const Game *g, const Ball *b, int *w, int *h) {
    const Sprite *sp = &g->mball.sprites[ball_spr_idx(g, b)];
    *w = sp->w > 0 ? sp->w : 9;
    *h = sp->h > 0 ? sp->h : 9;
}

static int any_stuck(const Game *g) {
    int i;
    for (i = 0; i < MAX_BALLS; i++) {
        if (g->balls[i].alive && g->balls[i].stuck) {
            return 1;
        }
    }
    return 0;
}

static void paddle_bounce(Game *g, Ball *b) {
    int bw, bh;
    ball_size(g, b, &bw, &bh);
    double f;
    int left = g->paddle_x - g->paddle_w / 2;
    f = ((double)(b->x + bw / 2) - (double)left) / (double)g->paddle_w;
    if (f <= 0.07) {
        b->angle = 150;
        audio_play(g, 0x10, b->x);
    } else if (f <= 0.20) {
        b->angle = 135;
    } else if (f <= 0.35) {
        b->angle = 120;
    } else if (f <= 0.50) {
        b->angle = 105;
    } else if (f <= 0.65) {
        b->angle = 75;
    } else if (f <= 0.80) {
        b->angle = 60;
    } else if (f <= 0.93) {
        b->angle = 45;
    } else {
        b->angle = 30;
        audio_play(g, 0x10, b->x);
    }
    apply_vel(b);
}

static int paddle_sprite(const Game *g) {
    int base;
    if (g->fire) {
        base = 0x68;
    } else if (g->sticky || any_stuck(g)) {
        base = 0x54;
    } else {
        base = 0x40;
    }
    return base + g->paddle_tier * 4 + g->paddle_anim;
}

static void set_paddle_tier(Game *g, int tier) {
    if (tier < 0) {
        tier = 0;
    }
    if (tier > 4) {
        tier = 4;
    }
    g->paddle_tier = tier;
    g->paddle_w = k_paddle_w[tier];
}

static void reset_balls(Game *g) {
    memset(g->balls, 0, sizeof(g->balls));
    g->nballs = 1;
    g->balls[0].alive = 1;
    g->balls[0].stuck = 1;
    g->balls[0].stuck_off = 0;
    g->balls[0].spr = 1;
    g->balls[0].speed = 5;
    g->balls[0].angle = 60;
    apply_vel(&g->balls[0]);
}

static void try_drop_bonus(Game *g, int col, int row) {
    int kind;
    int spr;
    if (g->bonus.alive) {
        return;
    }
    if (irand(10) >= 2) {
        return;
    }
    kind = irand(19);
    if (kind == 0 || kind == 1) {
        if (irand(5) != 1) {
            kind = 0xd;
        }
    }
    spr = 0x23 + kind;
    if (kind == 0xd) {
        spr = 0x30;
    }
    if (spr >= g->mball.count) {
        spr = 35;
    }
    g->bonus.alive = 1;
    g->bonus.kind = kind;
    g->bonus.spr = spr;
    g->bonus.x = col * TILE_W + FIELD_X;
    g->bonus.y = row * TILE_H + FIELD_Y;
    g->bonus.vx = irand(5) - 2;
    g->bonus.vy = 1;
    g->bonus.grav_t = 0;
}

static void spawn_explo(Game *g, int col, int row, int kind) {
    int i;
    for (i = 0; i < MAX_EXPLO; i++) {
        if (!g->expos[i].alive) {
            g->expos[i].alive = 1;
            g->expos[i].x = col * TILE_W + FIELD_X;
            g->expos[i].y = row * TILE_H + FIELD_Y;
            if (kind == 0) {
                g->expos[i].spr = 0x16;
                g->expos[i].frames = 6;
            } else {
                g->expos[i].spr = 0x14;
                g->expos[i].frames = 3;
            }
            g->expos[i].tick = 0;
            g->expos[i].frame = 0;
            return;
        }
    }
}

static void spawn_parts(Game *g, int x, int y, int n, int color) {
    int i;
    while (n-- > 0) {
        for (i = 0; i < MAX_PART; i++) {
            if (!g->parts[i].alive) {
                g->parts[i].alive = 1;
                g->parts[i].x = x + irand(30);
                g->parts[i].y = y + irand(15);
                g->parts[i].vx = 2 - irand(5);
                g->parts[i].vy = 2 - irand(5);
                g->parts[i].color = color;
                g->parts[i].grav = 1;
                g->parts[i].grav_t = 0;
                g->parts[i].life = 20 + irand(20);
                break;
            }
        }
    }
}

static void clear_tile(Game *g, int r, int c) {
    uint8_t tile;
    if (r < 0 || r >= BOARD_ROWS || c < 0 || c >= BOARD_COLS) {
        return;
    }
    tile = g->tiles[r][c];
    if (tile == 0 || tile == 2) {
        return;
    }
    g->tiles[r][c] = 0;
    g->score += 10;
    spawn_explo(g, c, r, 1);
}

static void explode_bomb(Game *g, int r, int c) {
    int dr, dc;
    g->tiles[r][c] = 0;
    audio_play(g, 0x1e + irand(3), c * TILE_W + FIELD_X);
    spawn_explo(g, c, r, 0);
    spawn_parts(g, c * TILE_W + FIELD_X, r * TILE_H + FIELD_Y, 8, 0x5f);
    for (dr = -1; dr <= 1; dr++) {
        for (dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) {
                continue;
            }
            clear_tile(g, r + dr, c + dc);
        }
    }
    g->score += 10;
    try_drop_bonus(g, c, r);
}

static int hit_brick(Game *g, int px, int py, int fireball) {
    int c, r, tile;
    if (px < FIELD_X || py < FIELD_Y) {
        return 0;
    }
    c = (px - FIELD_X) / TILE_W;
    r = (py - FIELD_Y) / TILE_H;
    if (c < 0 || c >= BOARD_COLS || r < 0 || r >= BOARD_ROWS) {
        return 0;
    }
    tile = g->tiles[r][c];
    if (tile == 0) {
        return 0;
    }
    /* FUN_004116c0: fireball turns the brick into a bomb first. */
    if (fireball) {
        g->tiles[r][c] = 8;
        tile = 8;
    }
    if (tile == 2) {
        spawn_explo(g, c, r, 1);
        if (g->through) {
            g->tiles[r][c] = 0;
            g->score += 10;
            try_drop_bonus(g, c, r);
            audio_play(g, 7, px);
            return 2;
        }
        audio_play(g, 3, px);
        return 1;
    }
    if (tile == 8) {
        explode_bomb(g, r, c);
        return 2;
    }
    if (tile == 3) {
        g->tiles[r][c] = g->through ? 0 : 4;
        audio_play(g, 1, px);
        spawn_explo(g, c, r, 1);
        if (g->through) {
            g->score += 10;
            return 2;
        }
        return 1;
    }
    if (tile == 4) {
        g->tiles[r][c] = g->through ? 0 : 5;
        audio_play(g, 1, px);
        spawn_explo(g, c, r, 1);
        if (g->through) {
            g->score += 10;
            return 2;
        }
        return 1;
    }
    if (tile == 7) {
        g->tiles[r][c] = g->through ? 0 : 6;
        audio_play(g, 0x13, px);
        spawn_parts(g, c * TILE_W + FIELD_X, r * TILE_H + FIELD_Y, 8, 0x77);
        spawn_explo(g, c, r, 1);
        if (g->through) {
            g->score += 10;
            return 2;
        }
        return 1;
    }
    if (tile == 0x15) {
        g->tiles[r][c] = g->through ? 0 : 2;
        audio_play(g, 1, px);
        spawn_explo(g, c, r, 1);
        return 1;
    }
    g->tiles[r][c] = 0;
    g->score += 10;
    try_drop_bonus(g, c, r);
    spawn_explo(g, c, r, 1);
    audio_play(g, 7, px);
    return 2;
}

static void weaken_bricks(Game *g) {
    int r, c;
    g->drop_bricks = 0;
    for (r = 0; r < BOARD_ROWS; r++) {
        for (c = 0; c < BOARD_COLS; c++) {
            if (g->tiles[r][c] == 2 || g->tiles[r][c] == 0x15) {
                g->tiles[r][c] = 0x14;
            }
            if (g->tiles[r][c] == 7) {
                g->tiles[r][c] = 6;
            }
            if (g->tiles[r][c] == 3 || g->tiles[r][c] == 4) {
                g->tiles[r][c] = 5;
            }
        }
    }
}

static void spread_bombs(Game *g) {
    uint8_t src[BOARD_ROWS][BOARD_COLS];
    int r, c;
    memcpy(src, g->tiles, sizeof(src));
    for (r = 0; r < BOARD_ROWS; r++) {
        for (c = 0; c < BOARD_COLS; c++) {
            if (src[r][c] != 8) {
                continue;
            }
            if (c > 0 && src[r][c - 1] != 8) {
                g->tiles[r][c - 1] = 8;
            }
            if (c < BOARD_COLS - 1 && src[r][c + 1] != 8) {
                g->tiles[r][c + 1] = 8;
            }
            if (r > 0 && src[r - 1][c] != 8) {
                g->tiles[r - 1][c] = 8;
            }
            if (r < BOARD_ROWS - 1 && src[r + 1][c] != 8) {
                g->tiles[r + 1][c] = 8;
            }
        }
    }
}

static void detonate_greens(Game *g) {
    int r, c;
    for (r = 0; r < BOARD_ROWS; r++) {
        for (c = 0; c < BOARD_COLS; c++) {
            if (g->tiles[r][c] == 8) {
                explode_bomb(g, r, c);
            }
        }
    }
}

static void drop_bricks_once(Game *g) {
    int r, c;
    int moved = 0;
    for (c = 0; c < BOARD_COLS; c++) {
        for (r = BOARD_ROWS - 2; r >= 0; r--) {
            if (g->tiles[r][c] != 0 && g->tiles[r + 1][c] == 0) {
                g->tiles[r + 1][c] = g->tiles[r][c];
                g->tiles[r][c] = 0;
                moved = 1;
            }
        }
    }
    if (moved) {
        audio_play(g, 0x14, 320);
    }
}

static void split_balls(Game *g) {
    Ball copies[MAX_BALLS];
    int n = 0;
    int i;
    memcpy(copies, g->balls, sizeof(copies));
    memset(g->balls, 0, sizeof(g->balls));
    for (i = 0; i < MAX_BALLS && n < MAX_BALLS; i++) {
        if (!copies[i].alive) {
            continue;
        }
        g->balls[n] = copies[i];
        n++;
        if (n < MAX_BALLS) {
            g->balls[n] = copies[i];
            g->balls[n].vx = -copies[i].vx;
            if (g->balls[n].speed < 4) {
                g->balls[n].speed = 4;
            }
            n++;
        }
    }
    g->nballs = n;
}

static void scale_ball_speed(Game *g, int speed, int keep_fireball) {
    int i;
    for (i = 0; i < MAX_BALLS; i++) {
        int sx, sy;
        if (!g->balls[i].alive) {
            continue;
        }
        sx = (g->balls[i].vx < 0) ? -1 : 1;
        sy = (g->balls[i].vy < 0) ? -1 : 1;
        g->balls[i].speed = speed;
        apply_vel(&g->balls[i]);
        g->balls[i].vx = abs(g->balls[i].vx) * sx;
        g->balls[i].vy = abs(g->balls[i].vy) * sy;
        if (!keep_fireball || g->balls[i].spr != 0x3d) {
            if (speed <= 4) {
                g->balls[i].spr = 1;
            }
        }
    }
}

static void go_score(Game *g);

static void apply_bonus(Game *g, int kind) {
    int i;
    g->score += 100;
    switch (kind) {
    case 0:
        g->lives++;
        if (g->lives > 20) {
            g->lives = 20;
        }
        g->through = 0;
        g->sticky = 0;
        g->fire = 0;
        audio_play(g, 0xd, g->paddle_x);
        break;
    case 1:
        audio_play(g, 8, g->paddle_x);
        load_level(g, g->level + 1);
        break;
    case 2:
        audio_play(g, 8, g->paddle_x);
        weaken_bricks(g);
        break;
    case 3:
        audio_play(g, 8, g->paddle_x);
        scale_ball_speed(g, 4, 1);
        break;
    case 4:
        audio_play(g, 8, g->paddle_x);
        spread_bombs(g);
        break;
    case 5:
        audio_play(g, 8, g->paddle_x);
        g->through = 1;
        break;
    case 6:
        audio_play(g, 8, g->paddle_x);
        detonate_greens(g);
        break;
    case 7:
        audio_play(g, 8, g->paddle_x);
        g->fireball = 1;
        for (i = 0; i < MAX_BALLS; i++) {
            if (g->balls[i].alive) {
                g->balls[i].spr = 0x3d;
            }
        }
        break;
    case 8:
        audio_play(g, 8, g->paddle_x);
        g->fire = 1;
        break;
    case 9:
        audio_play(g, 8, g->paddle_x);
        g->sticky = 1;
        break;
    case 10:
        audio_play(g, 10, g->paddle_x);
        set_paddle_tier(g, g->paddle_tier + 1);
        break;
    case 11:
        audio_play(g, 0xb, g->paddle_x);
        set_paddle_tier(g, g->paddle_tier - 1);
        break;
    case 12:
        audio_play(g, 0xc, g->paddle_x);
        split_balls(g);
        break;
    case 13:
        audio_play(g, 0xe, g->paddle_x);
        g->lives--;
        if (g->lives <= 0) {
            go_score(g);
        } else {
            reset_balls(g);
        }
        break;
    case 14:
    case 15:
        audio_play(g, 9, g->paddle_x);
        for (i = 0; i < MAX_BALLS; i++) {
            int sx, sy;
            if (!g->balls[i].alive) {
                continue;
            }
            sx = (g->balls[i].vx < 0) ? -1 : 1;
            sy = (g->balls[i].vy < 0) ? -1 : 1;
            g->balls[i].speed += 2;
            if (g->balls[i].speed > 9) {
                g->balls[i].speed = 9;
            }
            apply_vel(&g->balls[i]);
            g->balls[i].vx = abs(g->balls[i].vx) * sx;
            g->balls[i].vy = abs(g->balls[i].vy) * sy;
        }
        break;
    case 16:
        audio_play(g, 0xb, g->paddle_x);
        set_paddle_tier(g, 0);
        g->paddle_w = k_paddle_w[1] / 2;
        g->paddle_tier = 0;
        break;
    case 17:
        audio_play(g, 9, g->paddle_x);
        g->drop_bricks = 1;
        break;
    case 18:
        audio_play(g, 9, g->paddle_x);
        g->bigball = 1;
        for (i = 0; i < MAX_BALLS; i++) {
            if (g->balls[i].alive) {
                g->balls[i].spr = 0x37;
            }
        }
        break;
    default:
        break;
    }
}

static void update_bonus(Game *g) {
    Sprite *sp;
    int bw, bh, half;
    if (!g->bonus.alive) {
        return;
    }
    sp = &g->mball.sprites[g->bonus.spr];
    bw = sp->w > 0 ? sp->w : 32;
    bh = sp->h > 0 ? sp->h : 30;
    g->bonus.x += g->bonus.vx;
    g->bonus.y += g->bonus.vy;
    g->bonus.grav_t++;
    if (g->bonus.grav_t > 20) {
        g->bonus.vy++;
        g->bonus.grav_t = 0;
    }
    if (g->bonus.x < 20) {
        g->bonus.x = 20;
        g->bonus.vx = abs(g->bonus.vx);
        audio_play(g, 4, g->bonus.x);
    }
    if (g->bonus.x > 619 - bw) {
        g->bonus.x = 619 - bw;
        g->bonus.vx = -abs(g->bonus.vx);
        audio_play(g, 4, g->bonus.x);
    }
    if (g->bonus.y > SCREEN_H - bh) {
        g->bonus.alive = 0;
        return;
    }
    half = g->paddle_w / 2;
    if (g->bonus.x + bw > g->paddle_x - half &&
        g->bonus.x < g->paddle_x + half &&
        g->bonus.y + bh >= PADDLE_Y &&
        g->bonus.y < PADDLE_Y + 14) {
        apply_bonus(g, g->bonus.kind);
        g->bonus.alive = 0;
    }
}

static void update_explo(Game *g) {
    int i;
    for (i = 0; i < MAX_EXPLO; i++) {
        if (!g->expos[i].alive) {
            continue;
        }
        g->expos[i].tick++;
        if (g->expos[i].tick > 2) {
            g->expos[i].tick = 0;
            g->expos[i].frame++;
            if (g->expos[i].frame >= g->expos[i].frames) {
                g->expos[i].alive = 0;
            }
        }
    }
}

static void update_parts(Game *g) {
    int i;
    for (i = 0; i < MAX_PART; i++) {
        Part *p = &g->parts[i];
        if (!p->alive) {
            continue;
        }
        p->x += p->vx;
        p->y += p->vy;
        p->grav_t++;
        if (p->grav_t > 4) {
            p->vy += p->grav;
            p->grav_t = 0;
        }
        p->life--;
        if (p->life <= 0 || p->y > SCREEN_H || p->x < 0 || p->x >= SCREEN_W) {
            p->alive = 0;
        }
    }
}

static void fire_shots(Game *g) {
    int i;
    int n = 0;
    int off;
    if (!g->fire) {
        return;
    }
    for (i = 0; i < MAX_SHOTS; i++) {
        if (g->shots[i].alive) {
            n++;
        }
    }
    if (n >= 6) {
        return;
    }
    off = g->paddle_w / 4;
    for (i = 0; i < MAX_SHOTS && n < 6; i++) {
        if (!g->shots[i].alive) {
            g->shots[i].alive = 1;
            g->shots[i].x = g->paddle_x - off - 2;
            g->shots[i].y = PADDLE_Y - 8;
            n++;
            break;
        }
    }
    for (i = 0; i < MAX_SHOTS && n < 6; i++) {
        if (!g->shots[i].alive) {
            g->shots[i].alive = 1;
            g->shots[i].x = g->paddle_x + off - 2;
            g->shots[i].y = PADDLE_Y - 8;
            n++;
            break;
        }
    }
    g->nshots = n;
    audio_play(g, 0x11, g->paddle_x);
}

static void update_shots(Game *g) {
    int i;
    int n = 0;
    for (i = 0; i < MAX_SHOTS; i++) {
        Shot *s = &g->shots[i];
        int hit;
        if (!s->alive) {
            continue;
        }
        s->y -= 8;
        if (s->y < 0) {
            s->alive = 0;
            continue;
        }
        hit = hit_brick(g, s->x + 2, s->y, 0);
        if (hit == 0) {
            hit = hit_brick(g, s->x + 2, s->y + 6, 0);
        }
        if (hit != 0) {
            g->score += 4;
            if (!g->through) {
                s->alive = 0;
                continue;
            }
        }
        n++;
    }
    g->nshots = n;
}

static int bricks_left(const Game *g) {
    int r, c, n = 0;
    for (r = 0; r < BOARD_ROWS; r++) {
        for (c = 0; c < BOARD_COLS; c++) {
            if (g->tiles[r][c] != 0 && g->tiles[r][c] != 2) {
                n++;
            }
        }
    }
    return n;
}

static void load_level(Game *g, int level) {
    if (level < 0) {
        level = 0;
    }
    if (level >= BOARD_COUNT) {
        go_score(g);
        return;
    }
    g->level = level;
    memcpy(g->tiles, g->boards[level], sizeof(g->tiles));
    g->bonus.alive = 0;
    g->through = 0;
    g->sticky = 0;
    g->fire = 0;
    g->drop_bricks = 0;
    g->fireball = 0;
    g->bigball = 0;
    memset(g->shots, 0, sizeof(g->shots));
    memset(g->expos, 0, sizeof(g->expos));
    memset(g->parts, 0, sizeof(g->parts));
    set_paddle_tier(g, 1);
    reset_balls(g);
    schedule_zap(g);
}

static void start_play(Game *g) {
    memcpy(g->pal, g->background.pal, 768);
    memcpy(g->pal_base, g->background.pal, 768);
    g->lives = 3;
    g->score = 0;
    g->mode = MODE_PLAY;
    g->paused = 0;
    g->entering_name = 0;
    load_level(g, 0);
    midi_stop(g);
}

static int qualifies(const Game *g) {
    return (uint32_t)g->score >= g->scores[HISCORE_N - 1].score;
}

static void insert_score(Game *g) {
    int i, j;
    char path[640];
    if (!qualifies(g)) {
        return;
    }
    for (i = HISCORE_N - 1; i >= 0; i--) {
        if ((uint32_t)g->score < g->scores[i].score) {
            break;
        }
    }
    i++;
    for (j = HISCORE_N - 1; j > i; j--) {
        g->scores[j] = g->scores[j - 1];
    }
    memset(g->scores[i].name, 0, HISCORE_NAME);
    snprintf(g->scores[i].name, HISCORE_NAME, "%s", g->name_buf);
    g->scores[i].score = (uint32_t)g->score;
    path_join(path, sizeof(path), g->data_dir, "SCORE.DAT");
    save_hiscores(path, g->scores);
}

static void go_score(Game *g) {
    memcpy(g->pal, g->highscore.pal, 768);
    memcpy(g->pal_base, g->highscore.pal, 768);
    g->mode = MODE_SCORE;
    g->paused = 0;
    if (qualifies(g)) {
        g->entering_name = 1;
        g->name_pos = 0;
        memset(g->name_buf, 0, sizeof(g->name_buf));
    } else {
        g->entering_name = 0;
    }
    audio_play(g, 0xd, 320);
}

static void go_menu(Game *g) {
    memcpy(g->pal, g->menu.pal, 768);
    memcpy(g->pal_base, g->menu.pal, 768);
    g->mode = MODE_MENU;
    g->paused = 0;
    g->entering_name = 0;
    midi_start(g, 1);
}

void game_to_menu(Game *g) {
    go_menu(g);
}

static void launch_stuck(Game *g) {
    int i;
    for (i = 0; i < MAX_BALLS; i++) {
        if (g->balls[i].alive && g->balls[i].stuck) {
            g->balls[i].stuck = 0;
            g->balls[i].stuck_t = 0;
            if (g->balls[i].vy >= 0) {
                paddle_bounce(g, &g->balls[i]);
            }
        }
    }
}

static int ball_spr(const Game *g, const Ball *b) {
    return ball_spr_idx(g, b);
}

static void update_play(Game *g) {
    int i, live;
    int half;

    if (g->paused) {
        return;
    }

    half = g->paddle_w / 2;
    g->paddle_x = g->mouse_x;
    if (g->paddle_x < half + 21) {
        g->paddle_x = half + 21;
    }
    if (g->paddle_x > 618 - half) {
        g->paddle_x = 618 - half;
    }

    if (SDL_GetTicks() - g->paddle_anim_t >= 64) {
        g->paddle_anim_t = SDL_GetTicks();
        g->paddle_anim = (g->paddle_anim + 1) & 3;
    }
    update_force_field(g);
    update_zap(g);

    if (g->click) {
        if (g->fire) {
            int any_stuck = 0;
            for (i = 0; i < MAX_BALLS; i++) {
                if (g->balls[i].alive && g->balls[i].stuck) {
                    any_stuck = 1;
                }
            }
            if (any_stuck) {
                launch_stuck(g);
            } else {
                fire_shots(g);
            }
        } else {
            launch_stuck(g);
        }
        g->click = 0;
    }

    live = 0;
    for (i = 0; i < MAX_BALLS; i++) {
        Ball *b = &g->balls[i];
        int hit;
        int dbw, dbh;
        int fire;
        if (!b->alive) {
            continue;
        }
        ball_size(g, b, &dbw, &dbh);
        fire = (b->spr == 0x3d);
        if (b->stuck) {
            b->x = g->paddle_x + b->stuck_off - dbw / 2;
            b->y = PADDLE_Y - dbh;
            b->stuck_t++;
            if (b->stuck_t > 300) {
                b->stuck = 0;
                paddle_bounce(g, b);
            }
            live++;
            continue;
        }
        b->x += b->vx;
        b->y += b->vy;
        if (b->x < 20) {
            b->x = 20;
            b->vx = abs(b->vx);
            b->hits++;
            audio_play(g, 4, b->x);
        }
        if (b->x > 619 - dbw) {
            b->x = 619 - dbw;
            b->vx = -abs(b->vx);
            b->hits++;
            audio_play(g, 4, b->x);
        }
        if (b->y < 0) {
            b->y = 0;
            b->vy = abs(b->vy);
            b->hits++;
            audio_play(g, 4, b->x);
        }
        if (b->hits >= 8 && b->speed < 9) {
            int sx = (b->vx < 0) ? -1 : 1;
            int sy = (b->vy < 0) ? -1 : 1;
            b->speed++;
            b->hits = 0;
            apply_vel(b);
            b->vx = abs(b->vx) * sx;
            b->vy = abs(b->vy) * sy;
        }
        if (b->y > SCREEN_H - dbh) {
            b->alive = 0;
            audio_play(g, 5, b->x);
            continue;
        }
        if (b->spr == 0x3d && irand(11) == 0) {
            spawn_parts(g, b->x, b->y, 1, 0x5f);
        }
        if (b->vy > 0) {
            int left = g->paddle_x - half;
            int right = g->paddle_x + half;
            if (b->x + dbw > left && b->x < right &&
                b->y + dbh >= PADDLE_Y && b->y < PADDLE_Y + 7) {
                int cx = b->x + dbw / 2;
                int off = cx - g->paddle_x;
                b->y = PADDLE_Y - dbh;
                if (g->drop_bricks) {
                    drop_bricks_once(g);
                }
                if (g->sticky) {
                    b->stuck = 1;
                    b->stuck_off = off;
                    b->stuck_t = 0;
                    audio_play(g, 0x12, b->x);
                } else {
                    paddle_bounce(g, b);
                    audio_play(g, 0, b->x);
                }
            }
        }
        hit = hit_brick(g, b->x + dbw / 2, b->y, fire);
        if (hit == 0) {
            hit = hit_brick(g, b->x + dbw / 2, b->y + dbh, fire);
        }
        if (hit == 0) {
            hit = hit_brick(g, b->x, b->y + dbh / 2, fire);
        }
        if (hit == 0) {
            hit = hit_brick(g, b->x + dbw, b->y + dbh / 2, fire);
        }
        if (hit != 0 && !g->through && b->spr != 0x3d) {
            if (b->vy < 0) {
                b->vy = abs(b->vy);
            } else {
                b->vy = -abs(b->vy);
            }
        }
        live++;
    }

    update_bonus(g);
    update_shots(g);
    update_explo(g);
    update_parts(g);

    if (live == 0) {
        g->lives--;
        if (g->lives <= 0) {
            go_score(g);
        } else {
            reset_balls(g);
        }
    } else if (bricks_left(g) == 0) {
        audio_play(g, 0xd, 320);
        load_level(g, g->level + 1);
    }
}

static void copper(Game *g) {
    int y;
    int x;
    uint8_t c;
    g->copper_a = (g->copper_a + 4) % 360;
    for (y = 0; y < 12; y++) {
        c = (uint8_t)(32 + ((y * 7 + g->copper_a / 8) & 31));
        for (x = 0; x < SCREEN_W; x++) {
            int sy = 420 + y + (int)(sin((x + g->copper_a) * 0.05) * 3);
            put_pixel(g, x, sy, c);
        }
    }
}

static void draw_scroller(Game *g) {
    int x;
    const char *s = k_scroller;
    int n = (int)strlen(s);
    g->scroller_x -= 4;
    if (g->scroller_x < -8) {
        g->scroller_x += 8;
        g->scroller_i = (g->scroller_i + 1) % n;
    }
    x = g->scroller_x + 40;
    {
        int i = g->scroller_i;
        while (x < 620) {
            int ch = (unsigned char)s[i];
            int w = draw_char(g, &g->sysfont, ch, x, 456);
            x += w;
            i = (i + 1) % n;
        }
    }
}

static void draw_boing(Game *g) {
    int i;
    for (i = 0; i < 3; i++) {
        int x = 80 + i * 180;
        int y = 120 + (int)(sin((g->frame * 0.08) + i) * 40);
        blit_sprite(g, 1, x, y);
    }
}

static void draw_force_field(Game *g) {
    int spr;
    int y;
    int x;
    if (!any_stuck(g)) {
        return;
    }
    /* FUN_00412ec0: only while a ball is stuck (DAT_0043a8fc). */
    spr = 0x7c + g->paddle_tier * 4 + g->field_frame;
    y = PADDLE_Y - 14;
    x = g->paddle_x - g->paddle_w / 2;
    blit_sprite_bank_w(g, &g->mball, spr, x, y, g->paddle_w);
}

static void update_force_field(Game *g) {
    if (SDL_GetTicks() - g->field_t >= 33) {
        g->field_t = SDL_GetTicks();
        g->field_frame = (g->field_frame + 1) & 3;
    }
}

static void schedule_zap(Game *g) {
    g->zap_at = SDL_GetTicks() + 40000u + (uint32_t)irand(20000);
    g->bolt_life = 0;
}

static void update_zap(Game *g) {
    int c, r;
    int pc, pr;
    int found;
    if (g->bolt_life > 0) {
        g->bolt_life--;
        return;
    }
    if (g->zap_at == 0) {
        schedule_zap(g);
        return;
    }
    if (SDL_GetTicks() < g->zap_at) {
        return;
    }
    found = 0;
    pc = 0;
    pr = 0;
    for (c = 0; c < BOARD_COLS; c++) {
        for (r = 0; r < BOARD_ROWS; r++) {
            if (g->tiles[r][c] != 0 && g->tiles[r][c] != 2) {
                pc = c;
                pr = r;
                found = 1;
            }
        }
    }
    if (!found) {
        schedule_zap(g);
        return;
    }
    g->bolt_x = FIELD_X + pc * TILE_W + TILE_W / 2;
    g->bolt_y = FIELD_Y + pr * TILE_H + TILE_H / 2;
    g->bolt_life = 8;
    audio_play(g, 0x15, g->bolt_x);
    audio_play(g, 0x16, g->bolt_x);
    spawn_parts(g, FIELD_X + pc * TILE_W, FIELD_Y + pr * TILE_H, 30, 0x10);
    hit_brick(g, g->bolt_x, g->bolt_y, 0);
    schedule_zap(g);
}

static void draw_bolt(Game *g) {
    if (g->bolt_life <= 0 || g->bigbolt.pixels == NULL) {
        return;
    }
    blit_pcx_at(g, &g->bigbolt, g->bolt_x - g->bigbolt.w / 2, 0, 1);
}

static void draw_hud(Game *g) {
    char buf[32];
    int n;
    int i;
    int x;
    snprintf(buf, sizeof(buf), "%d", g->score);
    draw_text(g, &g->thefont, 30, 8, buf);
    n = g->lives;
    if (n > 20) {
        n = 20;
    }
    x = 598;
    for (i = 1; i < n; i++) {
        blit_sprite(g, 0x1f, x, (i > 10) ? 10 : 2);
        x -= 22;
        if (i == 10) {
            x = 598;
        }
    }
}

static void draw_play(Game *g) {
    int r, c, i;
    int yoff;
    int spr;
    blit_pcx(g, &g->background, 0, 0);
    for (r = 0; r < BOARD_ROWS; r++) {
        for (c = 0; c < BOARD_COLS; c++) {
            /* Tile 7 is invisible during play (FUN_004119f0 case 7). */
            if (g->tiles[r][c] == 0 || g->tiles[r][c] == 7) {
                continue;
            }
            blit_sprite(g, tile_to_sprite(g->tiles[r][c]),
                        FIELD_X + c * TILE_W, FIELD_Y + r * TILE_H);
        }
    }
    for (i = 0; i < MAX_EXPLO; i++) {
        if (g->expos[i].alive) {
            blit_sprite(g, g->expos[i].spr + g->expos[i].frame,
                        g->expos[i].x, g->expos[i].y);
        }
    }
    for (i = 0; i < MAX_PART; i++) {
        if (g->parts[i].alive) {
            put_pixel(g, g->parts[i].x, g->parts[i].y, (uint8_t)g->parts[i].color);
            put_pixel(g, g->parts[i].x + 1, g->parts[i].y, (uint8_t)g->parts[i].color);
            put_pixel(g, g->parts[i].x, g->parts[i].y + 1, (uint8_t)g->parts[i].color);
            put_pixel(g, g->parts[i].x + 1, g->parts[i].y + 1, (uint8_t)g->parts[i].color);
        }
    }
    spr = paddle_sprite(g);
    yoff = 0;
    if (g->fire) {
        yoff = 15;
    } else if (g->sticky || any_stuck(g)) {
        yoff = 11;
    }
    /* Electric bar sits on top of the paddle (sprites 124+). */
    draw_force_field(g);
    blit_sprite(g, spr, g->paddle_x - g->paddle_w / 2, PADDLE_Y - yoff);
    draw_bolt(g);
    for (i = 0; i < MAX_BALLS; i++) {
        if (g->balls[i].alive) {
            blit_sprite(g, ball_spr(g, &g->balls[i]), g->balls[i].x, g->balls[i].y);
        }
    }
    for (i = 0; i < MAX_SHOTS; i++) {
        if (g->shots[i].alive) {
            blit_sprite(g, 0x20, g->shots[i].x, g->shots[i].y);
        }
    }
    if (g->bonus.alive) {
        blit_sprite(g, g->bonus.spr, g->bonus.x, g->bonus.y);
    }
    draw_hud(g);
    if (g->paused) {
        draw_text(g, &g->thefont, 200, 220, "PAUSED");
    }
}

static void draw_scores(Game *g) {
    int i;
    char line[80];
    blit_pcx(g, &g->highscore, 0, 0);
    for (i = 0; i < HISCORE_N; i++) {
        snprintf(line, sizeof(line), "%2d  %-28s  %u",
                 i + 1, g->scores[i].name, g->scores[i].score);
        draw_text(g, &g->sysfont, 70, 70 + i * 14, line);
    }
    if (g->entering_name) {
        draw_text(g, &g->thefont, 70, 230, g->name_buf);
        if ((g->frame / 16) & 1) {
            draw_text(g, &g->sysfont, 70 + g->name_pos * 8, 268, "_");
        }
        draw_text(g, &g->sysfont, 70, 290, "type your name, enter to save");
    }
}

static void draw_editor(Game *g) {
    int r, c;
    char buf[64];
    blit_pcx(g, &g->background, 0, 0);
    for (r = 0; r < BOARD_ROWS; r++) {
        for (c = 0; c < BOARD_COLS; c++) {
            if (g->tiles[r][c] == 0) {
                continue;
            }
            blit_sprite(g, tile_to_sprite(g->tiles[r][c]),
                        FIELD_X + c * TILE_W, FIELD_Y + r * TILE_H);
        }
    }
    snprintf(buf, sizeof(buf), "BOARD %d  TILE %d  +/- L S BKSP",
             g->level + 1, g->edit_tile);
    draw_text(g, &g->sysfont, 20, 8, buf);
}

static void editor_paint(Game *g, int erase) {
    int c, r;
    if (g->mouse_x < FIELD_X || g->mouse_y < FIELD_Y) {
        return;
    }
    c = (g->mouse_x - FIELD_X) / TILE_W;
    r = (g->mouse_y - FIELD_Y) / TILE_H;
    if (c < 0 || c >= BOARD_COLS || r < 0 || r >= BOARD_ROWS) {
        return;
    }
    g->tiles[r][c] = erase ? 0 : (uint8_t)g->edit_tile;
    memcpy(g->boards[g->level], g->tiles, sizeof(g->tiles));
}

int game_init(Game *g, const char *data_dir) {
    char path[640];
    SDL_AudioSpec want;
    int i;

    memset(g, 0, sizeof(*g));
    snprintf(g->data_dir, sizeof(g->data_dir), "%s", data_dir);
    path_join(path, sizeof(path), data_dir, "INTRO.PCX");
    if (load_pcx(path, &g->intro) != 0) {
        fprintf(stderr, "failed to load INTRO.PCX\n");
        return -1;
    }
    path_join(path, sizeof(path), data_dir, "MAINMENU.PCX");
    if (load_pcx(path, &g->menu) != 0) {
        fprintf(stderr, "failed to load MAINMENU.PCX\n");
        return -1;
    }
    path_join(path, sizeof(path), data_dir, "MBBKGRND.PCX");
    if (load_pcx(path, &g->background) != 0) {
        fprintf(stderr, "failed to load MBBKGRND.PCX\n");
        return -1;
    }
    path_join(path, sizeof(path), data_dir, "HIGHSCOR.PCX");
    if (load_pcx(path, &g->highscore) != 0) {
        fprintf(stderr, "failed to load HIGHSCOR.PCX\n");
        return -1;
    }
    path_join(path, sizeof(path), data_dir, "BIGBOLT.PCX");
    load_pcx(path, &g->bigbolt);
    path_join(path, sizeof(path), data_dir, "MBALL2.SBK");
    if (load_sbk(path, &g->mball) != 0) {
        fprintf(stderr, "failed to load MBALL2.SBK\n");
        return -1;
    }
    path_join(path, sizeof(path), data_dir, "THEFONT.SBK");
    load_sbk(path, &g->thefont);
    path_join(path, sizeof(path), data_dir, "SYSFONT.SBK");
    load_sbk(path, &g->sysfont);
    path_join(path, sizeof(path), data_dir, "SFONT.SBK");
    load_sbk(path, &g->sfont);
    path_join(path, sizeof(path), data_dir, "DEFAULT.BDS");
    if (load_bds(path, g->boards) != 0) {
        fprintf(stderr, "failed to load DEFAULT.BDS\n");
        return -1;
    }
    path_join(path, sizeof(path), data_dir, "SCORE.DAT");
    if (load_hiscores(path, g->scores) != 0) {
        for (i = 0; i < HISCORE_N; i++) {
            snprintf(g->scores[i].name, HISCORE_NAME, "empty");
            g->scores[i].score = (uint32_t)((HISCORE_N - i) * 10);
        }
    }
    memcpy(g->pal, g->intro.pal, 768);
    memcpy(g->pal_base, g->intro.pal, 768);
    g->mode = MODE_INTRO;
    g->scroller_x = 600;
    g->edit_tile = 1;
    srand((unsigned)SDL_GetTicks());
    g->paddle_x = SCREEN_W / 2;
    g->mouse_x = SCREEN_W / 2;
    set_paddle_tier(g, 1);

    for (i = 0; i < 23; i++) {
        if (k_wav_slot[i] != NULL) {
            path_join(path, sizeof(path), data_dir, k_wav_slot[i]);
            load_wav(path, &g->sounds[i]);
        }
    }
    path_join(path, sizeof(path), data_dir, "TANK.WAV");
    load_wav(path, &g->sounds[0x1e]);
    path_join(path, sizeof(path), data_dir, "XPLOSHT1.WAV");
    load_wav(path, &g->sounds[0x1f]);
    path_join(path, sizeof(path), data_dir, "XPLOSHOR.WAV");
    load_wav(path, &g->sounds[0x20]);

    SDL_zero(want);
    want.freq = MIX_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = audio_cb;
    want.userdata = g;
    g->audio = SDL_OpenAudioDevice(NULL, 0, &want, &g->audio_spec, 0);
    if (g->audio != 0) {
        SDL_PauseAudioDevice(g->audio, 0);
    }
    midi_init(g);
    midi_start(g, 1);
    return 0;
}

void game_free(Game *g) {
    int i;
    midi_free(g);
    if (g->audio != 0) {
        SDL_CloseAudioDevice(g->audio);
        g->audio = 0;
    }
    free_pcx(&g->intro);
    free_pcx(&g->menu);
    free_pcx(&g->background);
    free_pcx(&g->highscore);
    free_pcx(&g->bigbolt);
    free_sbk(&g->mball);
    free_sbk(&g->thefont);
    free_sbk(&g->sysfont);
    free_sbk(&g->sfont);
    for (i = 0; i < MAX_SOUNDS; i++) {
        free_sound(&g->sounds[i]);
    }
}

void game_click(Game *g) {
    g->click = 1;
}

void game_text(Game *g, char ch) {
    if (g->mode != MODE_SCORE || !g->entering_name) {
        return;
    }
    if (ch == '\r' || ch == '\n') {
        insert_score(g);
        g->entering_name = 0;
        return;
    }
    if (ch == '\b') {
        if (g->name_pos > 0) {
            g->name_pos--;
            g->name_buf[g->name_pos] = 0;
        }
        return;
    }
    if (g->name_pos >= 30) {
        return;
    }
    if (ch == ' ' || (ch >= '0' && ch <= '9') ||
        (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
        g->name_buf[g->name_pos++] = ch;
        g->name_buf[g->name_pos] = 0;
    }
}

void game_key(Game *g, SDL_Keycode sym, int down, int ctrl) {
    if (!down) {
        return;
    }
    if (sym == SDLK_F5) {
        midi_start(g, g->midi_track + 1);
        return;
    }
    if (sym == SDLK_F6) {
        midi_stop(g);
        return;
    }
    if (g->mode == MODE_SCORE && g->entering_name) {
        if (sym == SDLK_RETURN) {
            game_text(g, '\r');
        } else if (sym == SDLK_BACKSPACE) {
            game_text(g, '\b');
        }
        return;
    }
    if (g->mode == MODE_PLAY) {
        if (sym == SDLK_p) {
            g->paused = !g->paused;
            return;
        }
        if (ctrl && sym == SDLK_F1) {
            g->sticky = 0;
            g->fire = 0;
            set_paddle_tier(g, 1);
        }
        if (ctrl && sym == SDLK_F2) {
            g->sticky = 1;
        }
        if (ctrl && sym == SDLK_F3) {
            g->fire = 1;
        }
        if (ctrl && sym == SDLK_F4) {
            set_paddle_tier(g, g->paddle_tier + 1);
        }
        return;
    }
    if (g->mode == MODE_EDIT) {
        if (sym == SDLK_BACKSPACE) {
            memset(g->tiles, 0, sizeof(g->tiles));
            memcpy(g->boards[g->level], g->tiles, sizeof(g->tiles));
        } else if (sym == SDLK_EQUALS || sym == SDLK_PLUS || sym == SDLK_KP_PLUS) {
            memcpy(g->boards[g->level], g->tiles, sizeof(g->tiles));
            if (g->level < BOARD_COUNT - 1) {
                g->level++;
                memcpy(g->tiles, g->boards[g->level], sizeof(g->tiles));
            }
        } else if (sym == SDLK_MINUS || sym == SDLK_KP_MINUS) {
            memcpy(g->boards[g->level], g->tiles, sizeof(g->tiles));
            if (g->level > 0) {
                g->level--;
                memcpy(g->tiles, g->boards[g->level], sizeof(g->tiles));
            }
        } else if (sym == SDLK_l) {
            char path[640];
            path_join(path, sizeof(path), g->data_dir, "DEFAULT.BDS");
            load_bds(path, g->boards);
            memcpy(g->tiles, g->boards[g->level], sizeof(g->tiles));
        } else if (sym == SDLK_s) {
            char path[640];
            memcpy(g->boards[g->level], g->tiles, sizeof(g->tiles));
            path_join(path, sizeof(path), g->data_dir, "DEFAULT.BDS");
            save_bds(path, g->boards);
        } else if (sym >= SDLK_0 && sym <= SDLK_9) {
            g->edit_tile = (int)(sym - SDLK_0);
        }
        return;
    }
    if (g->mode == MODE_MENU) {
        if (sym == SDLK_h) {
            memcpy(g->pal, g->highscore.pal, 768);
            g->mode = MODE_SCORE;
            g->entering_name = 0;
        } else if (sym == SDLK_e || (ctrl && sym == SDLK_F1)) {
            memcpy(g->pal, g->background.pal, 768);
            g->mode = MODE_EDIT;
            g->level = 0;
            memcpy(g->tiles, g->boards[0], sizeof(g->tiles));
        }
    }
}

static void rotate_pal(Game *g, int a, int b) {
    uint8_t r, gv, bl;
    int i;
    r = g->pal[a * 3];
    gv = g->pal[a * 3 + 1];
    bl = g->pal[a * 3 + 2];
    for (i = a; i < b; i++) {
        g->pal[i * 3] = g->pal[(i + 1) * 3];
        g->pal[i * 3 + 1] = g->pal[(i + 1) * 3 + 1];
        g->pal[i * 3 + 2] = g->pal[(i + 1) * 3 + 2];
    }
    g->pal[b * 3] = r;
    g->pal[b * 3 + 1] = gv;
    g->pal[b * 3 + 2] = bl;
}

void game_update(Game *g) {
    g->frame++;
    if (g->mode == MODE_PLAY) {
        /* FUN_0040a970(0xe0, 0xe7, 1) about every 20ms. */
        if ((g->frame & 1) == 0) {
            rotate_pal(g, 0xe0, 0xe7);
        }
    }
    if (g->mode == MODE_INTRO || g->mode == MODE_MENU) {
        if (g->click) {
            g->click = 0;
            if (g->mode == MODE_INTRO) {
                go_menu(g);
            } else {
                start_play(g);
            }
        }
        return;
    }
    if (g->mode == MODE_SCORE) {
        if (g->click && !g->entering_name) {
            g->click = 0;
            go_menu(g);
        }
        g->click = 0;
        return;
    }
    if (g->mode == MODE_EDIT) {
        if (g->click) {
            editor_paint(g, 0);
            g->click = 0;
        }
        if (g->rclick) {
            editor_paint(g, 1);
            g->rclick = 0;
        }
        return;
    }
    update_play(g);
}

void game_draw(Game *g) {
    if (g->mode == MODE_INTRO) {
        blit_pcx(g, &g->intro, 0, 0);
        copper(g);
        draw_scroller(g);
        draw_boing(g);
        return;
    }
    if (g->mode == MODE_MENU) {
        blit_pcx(g, &g->menu, 0, 0);
        copper(g);
        draw_scroller(g);
        draw_text(g, &g->sysfont, 140, 400, "click to play   H scores   E editor");
        return;
    }
    if (g->mode == MODE_SCORE) {
        draw_scores(g);
        return;
    }
    if (g->mode == MODE_EDIT) {
        draw_editor(g);
        return;
    }
    draw_play(g);
}
