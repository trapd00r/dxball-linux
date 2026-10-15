#include "dxball.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *default_data(void) {
    const char *env = getenv("DXBALL_DATA");
    if (env != NULL && env[0] != '\0') {
        return env;
    }
    return "original/Dxball";
}

static void expand_frame(const Game *g, uint8_t *rgba) {
    int i;
    for (i = 0; i < SCREEN_W * SCREEN_H; i++) {
        const uint8_t *p = g->pal + g->fb[i] * 3;
        rgba[i * 4 + 0] = p[0];
        rgba[i * 4 + 1] = p[1];
        rgba[i * 4 + 2] = p[2];
        rgba[i * 4 + 3] = 255;
    }
}

int main(int argc, char **argv) {
    const char *data = default_data();
    Game game;
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    uint8_t *rgba;
    int running;
    int scale;
    int ctrl;
    int fullscreen;
    SDL_Event ev;

    if (argc > 1) {
        data = argv[1];
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (game_init(&game, data) != 0) {
        fprintf(stderr, "Could not load original DX-Ball files from %s\n", data);
        SDL_Quit();
        return 1;
    }
    scale = 2;
    win = SDL_CreateWindow("DX-Ball",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           SCREEN_W * scale, SCREEN_H * scale,
                           SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (win == NULL) {
        fprintf(stderr, "window: %s\n", SDL_GetError());
        game_free(&game);
        SDL_Quit();
        return 1;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (ren == NULL) {
        ren = SDL_CreateRenderer(win, -1, 0);
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
    SDL_SetWindowGrab(win, SDL_TRUE);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32,
                            SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    rgba = malloc((size_t)SCREEN_W * SCREEN_H * 4);
    if (tex == NULL || rgba == NULL) {
        fprintf(stderr, "texture alloc failed\n");
        game_free(&game);
        SDL_Quit();
        return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);
    SDL_StartTextInput();
    running = 1;
    ctrl = 0;
    fullscreen = 1;
    while (running) {
        uint32_t t0 = SDL_GetTicks();
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_KEYDOWN) {
                SDL_Keycode k = ev.key.keysym.sym;
                if (k == SDLK_LCTRL || k == SDLK_RCTRL) {
                    ctrl = 1;
                }
                if (k == SDLK_F11) {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(win, fullscreen ?
                                            SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    SDL_SetWindowGrab(win, SDL_TRUE);
                    SDL_ShowCursor(SDL_DISABLE);
                } else if (k == SDLK_ESCAPE) {
                    if (game.mode == MODE_PLAY || game.mode == MODE_EDIT ||
                        game.mode == MODE_SCORE) {
                        game_to_menu(&game);
                    } else {
                        running = 0;
                    }
                } else if (k == SDLK_SPACE) {
                    game_click(&game);
                } else {
                    game_key(&game, k, 1, ctrl);
                }
            } else if (ev.type == SDL_KEYUP) {
                if (ev.key.keysym.sym == SDLK_LCTRL ||
                    ev.key.keysym.sym == SDLK_RCTRL) {
                    ctrl = 0;
                }
            } else if (ev.type == SDL_TEXTINPUT) {
                game_text(&game, ev.text.text[0]);
            } else if (ev.type == SDL_MOUSEMOTION) {
                game.mouse_x = ev.motion.x;
                game.mouse_y = ev.motion.y;
            } else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                game.mouse_x = ev.button.x;
                game.mouse_y = ev.button.y;
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    game_click(&game);
                } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                    game.rclick = 1;
                }
            }
        }
        game_update(&game);
        game_draw(&game);
        expand_frame(&game, rgba);
        SDL_UpdateTexture(tex, NULL, rgba, SCREEN_W * 4);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        {
            uint32_t dt = SDL_GetTicks() - t0;
            if (dt < 16) {
                SDL_Delay(16 - dt);
            }
        }
    }
    SDL_StopTextInput();
    free(rgba);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    game_free(&game);
    SDL_Quit();
    return 0;
}
