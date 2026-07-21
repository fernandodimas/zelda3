#pragma once
#include <SDL.h>
#include <stdbool.h>

// Run the settings menu loop (pre-game). Returns true if user started the game,
// false if user pressed B/cancel (game should still start with current config).
bool SettingsMenu_Run(SDL_Renderer *renderer, SDL_Window *window);

// Render a notification text overlay on the given renderer (bottom-right).
// Uses the same glyph font as the settings menu.
void SettingsMenu_RenderNotify(SDL_Renderer *renderer, const char *text);
