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

#ifdef __SWITCH__
void SecondScreenSDL_SetRenderer(SDL_Renderer *renderer);
void SecondScreenSDL_RenderToMain(SDL_Renderer *main_renderer, int window_w, int window_h);
#endif
