#pragma once
#include <SDL.h>
#include <stdbool.h>

bool SecondScreenSDL_Init(SDL_Window *main_window);
void SecondScreenSDL_Destroy(void);
void SecondScreenSDL_Render(void);
bool SecondScreenSDL_HandleEvent(SDL_Event *event);
SDL_Window *SecondScreenSDL_GetWindow(void);
bool SecondScreenSDL_IsActive(void);
void SecondScreenSDL_Toggle(void);

enum { SS_LAYOUT_1SCREEN = 0, SS_LAYOUT_HORIZONTAL = 1, SS_LAYOUT_VERTICAL = 2 };
int  SecondScreenSDL_GetLayoutMode(void);
void SecondScreenSDL_CycleLayoutMode(void);
void SecondScreenSDL_SetLayoutMode(int mode);

#ifdef __SWITCH__
void SecondScreenSDL_SetRenderer(SDL_Renderer *renderer);
void SecondScreenSDL_RenderToMain(SDL_Renderer *main_renderer, int window_w, int window_h);
void SecondScreenSDL_RenderToTexture(SDL_Renderer *main_renderer, SDL_Texture *target);
#endif
