#ifndef DXBALL_H
#define DXBALL_H

#include <SDL.h>
#include <stdint.h>

#define SCREEN_W 640
#define SCREEN_H 480
#define BOARD_COLS 20
#define BOARD_ROWS 20
#define BOARD_COUNT 50
#define TILE_W 30
#define TILE_H 15
#define FIELD_X 20
#define FIELD_Y 50
#define PADDLE_Y 450
#define MAX_BALLS 16
#define MAX_SHOTS 8
#define MAX_SPRITES 256
#define MAX_SOUNDS 48
#define MAX_VOICES 12
#define MAX_EXPLO 48
#define MAX_PART 96
#define MAX_MIDI_EV 24000
#define HISCORE_N 15
#define HISCORE_NAME 40
#define MIX_RATE 22050

#define MODE_INTRO 0
#define MODE_MENU 1
#define MODE_PLAY 2
#define MODE_SCORE 3
#define MODE_EDIT 4

typedef struct {
    int w;
    int h;
    uint8_t ch;
    int extra;
    uint8_t *pixels;
} Sprite;

typedef struct {
    int count;
    Sprite sprites[MAX_SPRITES];
} SpriteBank;

typedef struct {
    int w;
    int h;
    uint8_t *pixels;
    uint8_t pal[768];
} PcxImage;

typedef struct {
    int alive;
    int x;
    int y;
    int vx;
    int vy;
    int angle;
    int speed;
    int stuck;
    int stuck_off;
    int stuck_t;
    int hits;
    int spr;
} Ball;

typedef struct {
    int alive;
    int kind;
    int spr;
    int x;
    int y;
    int vx;
    int vy;
    int grav_t;
} Bonus;

typedef struct {
    int alive;
    int x;
    int y;
} Shot;

typedef struct {
    int alive;
    int x;
    int y;
    int spr;
    int frames;
    int tick;
    int frame;
} Explo;

typedef struct {
    int alive;
    int x;
    int y;
    int vx;
    int vy;
    int color;
    int grav;
    int grav_t;
    int life;
} Part;

typedef struct {
    int16_t *pcm;
    int frames;
} Sound;

typedef struct {
    const int16_t *pcm;
    int frames;
    int pos;
    int vol_l;
    int vol_r;
} Voice;

typedef struct {
    uint32_t tick;
    uint32_t msg;
} MidiEv;

typedef struct {
    char name[HISCORE_NAME];
    uint32_t score;
} HiScore;

typedef struct Game Game;

struct Game {
    char data_dir[512];
    PcxImage intro;
    PcxImage menu;
    PcxImage background;
    PcxImage highscore;
    PcxImage bigbolt;
    SpriteBank mball;
    SpriteBank thefont;
    SpriteBank sysfont;
    SpriteBank sfont;
    uint8_t boards[BOARD_COUNT][BOARD_ROWS][BOARD_COLS];
    uint8_t tiles[BOARD_ROWS][BOARD_COLS];
    uint8_t fb[SCREEN_W * SCREEN_H];
    uint8_t pal[768];
    uint8_t pal_base[768];
    Ball balls[MAX_BALLS];
    int nballs;
    Shot shots[MAX_SHOTS];
    int nshots;
    Bonus bonus;
    Explo expos[MAX_EXPLO];
    Part parts[MAX_PART];
    int paddle_x;
    int paddle_w;
    int paddle_tier;
    int paddle_anim;
    uint32_t paddle_anim_t;
    int level;
    int lives;
    int score;
    int mode;
    int paused;
    int mouse_x;
    int mouse_y;
    int click;
    int rclick;
    int through;
    int sticky;
    int fire;
    int drop_bricks;
    int fireball;
    int bigball;
    int edit_tile;
    int fade;
    int frame;
    int scroller_i;
    int scroller_x;
    int copper_a;
    int name_pos;
    int entering_name;
    char name_buf[32];
    HiScore scores[HISCORE_N];
    Sound sounds[MAX_SOUNDS];
    Voice voices[MAX_VOICES];
    SDL_AudioDeviceID audio;
    SDL_AudioSpec audio_spec;
    MidiEv midi_ev[MAX_MIDI_EV];
    int midi_nev;
    int midi_pos;
    uint32_t midi_tick;
    uint32_t midi_frac;
    uint32_t midi_tempo;
    uint32_t midi_ppqn;
    int midi_on;
    int midi_track;
    void *fluid_settings;
    void *fluid_synth;
    uint32_t field_t;
    int field_frame;
    uint32_t zap_at;
    int bolt_life;
    int bolt_x;
    int bolt_y;
};

int load_pcx(const char *path, PcxImage *out);
int load_sbk(const char *path, SpriteBank *out);
int load_bds(const char *path, uint8_t boards[BOARD_COUNT][BOARD_ROWS][BOARD_COLS]);
int save_bds(const char *path, uint8_t boards[BOARD_COUNT][BOARD_ROWS][BOARD_COLS]);
int load_wav(const char *path, Sound *out);
void free_pcx(PcxImage *img);
void free_sbk(SpriteBank *bank);
void free_sound(Sound *snd);

int load_hiscores(const char *path, HiScore *out);
int save_hiscores(const char *path, const HiScore *in);
int load_mids(const char *path, MidiEv *ev, int max, uint32_t *ppqn);

int game_init(Game *g, const char *data_dir);
void game_free(Game *g);
void game_click(Game *g);
void game_update(Game *g);
void game_draw(Game *g);
void game_key(Game *g, SDL_Keycode sym, int down, int ctrl);
void game_text(Game *g, char ch);
void game_to_menu(Game *g);

void audio_play(Game *g, int slot, int x);
void audio_cb(void *userdata, Uint8 *stream, int len);
void midi_init(Game *g);
void midi_free(Game *g);
void midi_start(Game *g, int track);
void midi_stop(Game *g);

int tile_to_sprite(int tile);

#endif
