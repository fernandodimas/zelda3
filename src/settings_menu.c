// Pre-game settings menu for Zelda3.
// Renders a tabbed UI with config options and ROM detection.
// Uses the same letter/glyph texture system as the second screen.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <dirent.h>
#include <SDL.h>

#include "types.h"
#include "config.h"
#include "second_screen.h"
#include "second_screen_tables.h"
#include "platform/linux/ss_sheets.h"

extern SDL_Renderer *g_renderer;

// ============ Colors ============
#define COL(r,g,b) (0xff000000u | ((r) << 16) | ((g) << 8) | (b))
#define COL_BG         COL(18, 18, 24)
#define COL_BOX        COL(34, 30, 22)
#define COL_BORDER     COL(86, 78, 48)
#define COL_BORDER2    COL(136, 122, 64)
#define COL_TEXT       COL(220, 210, 170)
#define COL_TEXT_DIM   COL(120, 110, 80)
#define COL_HIGHLIGHT  COL(70, 130, 50)
#define COL_ACCENT     COL(180, 140, 40)
#define COL_WHITE      COL(255, 255, 255)
#define COL_BLACK      COL(0, 0, 0)
#define COL_GREEN      COL(80, 180, 60)
#define COL_RED        COL(180, 60, 50)

// ============ State ============
static SDL_Renderer *menu_r;
static SDL_Texture  *tex_letters;
static SDL_Texture  *tex_glyphs;
static float menu_u;
static int menu_W, menu_H;

// Local copies of config values that are uint8/bool in Config struct
// (we can't use int* to point at uint8/bool fields - UB)
static int local_fullscreen;
static int local_window_scale;
static int local_extended_aspect_ratio;
static int local_enhanced_mode7;
static int local_new_renderer;
static int local_no_sprite_limits;
static int local_linear_filtering;
static int local_ignore_aspect_ratio;
static int local_enable_audio;
static int local_enable_msu;
static int local_autosave;
static int local_save_slot;
static int local_extend_y;
static int local_run_without_emu;
static int local_display_perf_title;
static int local_dual_screen;
static int local_language;

// ============ Draw primitives ============
static void set_color(uint32 c) {
  SDL_SetRenderDrawColor(menu_r, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff, (c >> 24) & 0xff);
}

static void fill_rect(float x, float y, float w, float h, uint32 c) {
  SDL_FRect r = {x, y, w, h};
  set_color(c);
  SDL_RenderFillRectF(menu_r, &r);
}

static void fill_round(float x, float y, float w, float h, float rad, uint32 c) {
  if (rad > w / 2) rad = w / 2;
  if (rad > h / 2) rad = h / 2;
  if (rad < 1) { fill_rect(x, y, w, h, c); return; }
  set_color(c);
  SDL_FRect mid = {x, y + rad, w, h - 2 * rad};
  if (mid.h > 0) SDL_RenderFillRectF(menu_r, &mid);
  for (int i = 0; i < (int)rad; i++) {
    float dy = rad - i;
    float dx = rad - sqrtf(rad * rad - dy * dy);
    float lw = w - 2 * dx;
    if (lw < 1) continue;
    SDL_FRect t = {x + dx, y + i, lw, 1};
    SDL_FRect b = {x + dx, y + h - 1 - i, lw, 1};
    SDL_RenderFillRectF(menu_r, &t);
    SDL_RenderFillRectF(menu_r, &b);
  }
}

static void fill_circle(float cx, float cy, float r, uint32 c) {
  set_color(c);
  for (int dy = (int)-r; dy <= (int)r; dy++) {
    float dx = sqrtf(r * r - dy * dy);
    SDL_FRect seg = {cx - dx, cy + dy, dx * 2, 1};
    SDL_RenderFillRectF(menu_r, &seg);
  }
}

static void draw_cell(SDL_Texture *tex, int cell, int cellpx, int cols, float x, float y, float s) {
  if (cell < 0 || !tex) return;
  SDL_Rect src = {(cell % cols) * cellpx, (cell / cols) * cellpx, cellpx, cellpx};
  SDL_FRect dst = {x, y, cellpx * s, cellpx * s};
  SDL_RenderCopyF(menu_r, tex, &src, &dst);
}

static void draw_glyph(int cell, float x, float y, float s) {
  draw_cell(tex_glyphs, cell, 8, SS_GLYPH_COLS, x, y, s);
}

static float text_width(const char *s, float sc) {
  float w = 0;
  for (; *s; s++) w += (*s == ' ' ? 5 : 8) * sc;
  return w;
}

static void draw_char_fallback(char ch, float x, float y, float sc) {
  float s = 6 * sc;
  float ox = x + 1 * sc;
  float oy = y + 1 * sc;
  set_color(COL_TEXT);
  if (ch == ':') {
    fill_rect(ox + s / 2, oy + s / 3, 1, 1, COL_TEXT);
    fill_rect(ox + s / 2, oy + s * 2 / 3, 1, 1, COL_TEXT);
  } else if (ch == '-') {
    fill_rect(ox, oy + s / 2, s, 1, COL_TEXT);
  } else if (ch == '_') {
    fill_rect(ox, oy + s - 1, s, 1, COL_TEXT);
  } else if (ch == '.') {
    fill_rect(ox + s / 2, oy + s - 2, 2, 2, COL_TEXT);
  }
}

static void draw_text(const char *s, float x, float y, float sc) {
  static const int kDigitGlyph[10] = {
    SS_GLYPH_DIGIT0, SS_GLYPH_DIGIT1, SS_GLYPH_DIGIT2, SS_GLYPH_DIGIT3, SS_GLYPH_DIGIT4,
    SS_GLYPH_DIGIT5, SS_GLYPH_DIGIT6, SS_GLYPH_DIGIT7, SS_GLYPH_DIGIT8, SS_GLYPH_DIGIT9,
  };
  float cx = x;
  for (; *s; s++) {
    char ch = *s;
    if (ch == ' ') { cx += 5 * sc; continue; }
    if (ch >= '0' && ch <= '9') draw_glyph(kDigitGlyph[ch - '0'], cx, y, sc);
    else if (ch >= 'A' && ch <= 'Z') draw_cell(tex_letters, kSS_LetterCell[ch - 'A'], 8, SS_LETTER_COLS, cx, y, sc);
    else if (ch >= 'a' && ch <= 'z') draw_cell(tex_letters, kSS_LetterCell[ch - 'a'], 8, SS_LETTER_COLS, cx, y, sc);
    else { draw_char_fallback(ch, cx, y, sc); }
    cx += 8 * sc;
  }
}

static void draw_text_dim(const char *s, float x, float y, float sc) {
  // Draw text with dim color by drawing a dark shadow first
  draw_text(s, x + 1, y + 1, sc);
}

static void menu_box(float x, float y, float w, float h, uint32 border) {
  fill_round(x, y, w, h, 10 * menu_u, COL_BLACK);
  fill_round(x + 3 * menu_u, y + 3 * menu_u, w - 6 * menu_u, h - 6 * menu_u, 8 * menu_u, border);
  fill_round(x + 7 * menu_u, y + 7 * menu_u, w - 14 * menu_u, h - 14 * menu_u, 6 * menu_u, COL(60, 55, 40));
  fill_round(x + 9 * menu_u, y + 9 * menu_u, w - 18 * menu_u, h - 18 * menu_u, 6 * menu_u, COL_BOX);
  float d = 3.5f * menu_u;
  fill_circle(x + 8 * menu_u, y + 8 * menu_u, d, COL_WHITE);
  fill_circle(x + w - 8 * menu_u, y + 8 * menu_u, d, COL_WHITE);
  fill_circle(x + 8 * menu_u, y + h - 8 * menu_u, d, COL_WHITE);
  fill_circle(x + w - 8 * menu_u, y + h - 8 * menu_u, d, COL_WHITE);
}

// ============ Textures ============
static SDL_Texture *make_tex_from_sheet(const uint32 *px, int w, int h) {
  SDL_Texture *t = SDL_CreateTexture(menu_r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  if (!t) return NULL;
  SDL_UpdateTexture(t, NULL, px, w * 4);
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  return t;
}

static bool load_menu_textures(void) {
  // Render letter and glyph sheets using the same code as second screen
  // We call the SS_Render* functions indirectly by including the second_screen.c
  // approach: render into a static buffer, then create textures
  static uint32 buf[512 * 512];

  SS_RenderGlyphSheet(buf);
  tex_glyphs = make_tex_from_sheet(buf, SS_GLYPH_COLS * 8, ((kGlyphCount + kGlyphCols - 1) / kGlyphCols) * 8);

  SS_RenderLetterSheet(buf);
  tex_letters = make_tex_from_sheet(buf, 16 * 8, 2 * 8);

  return tex_glyphs != NULL && tex_letters != NULL;
}

static void destroy_menu_textures(void) {
  if (tex_letters) { SDL_DestroyTexture(tex_letters); tex_letters = NULL; }
  if (tex_glyphs)  { SDL_DestroyTexture(tex_glyphs);  tex_glyphs = NULL; }
}

// ============ ROM Detection ============
#define MAX_ROMS 16
typedef struct {
  char filename[128];
  char label[128];
} RomEntry;

static RomEntry g_roms[MAX_ROMS];
static int g_rom_count;

static const char *identify_rom(const char *name) {
  // Simple name-based identification
  if (strstr(name, "zelda3.sfc") || strstr(name, "Zelda3.sfc")) return "US (USA)";
  if (strstr(name, "_pt.") || strstr(name, "portuguese") || strstr(name, "Portuguese")) return "PT (Portugues)";
  if (strstr(name, "_es.") || strstr(name, "spanish") || strstr(name, "Spanish")) return "ES (Espanhol)";
  if (strstr(name, "_de.") || strstr(name, "german") || strstr(name, "German")) return "DE (Alemao)";
  if (strstr(name, "_fr.") || strstr(name, "french") || strstr(name, "French")) return "FR (Frances)";
  if (strstr(name, "_ja.") || strstr(name, "japanese") || strstr(name, "Japanese")) return "JA (Japones)";
  if (strstr(name, "_pl.") || strstr(name, "polish") || strstr(name, "Polish")) return "PL (Polones)";
  if (strstr(name, "_nl.") || strstr(name, "dutch") || strstr(name, "Dutch")) return "NL (Holandes)";
  if (strstr(name, "_sv.") || strstr(name, "swedish") || strstr(name, "Swedish")) return "SV (Sueco)";
  if (strstr(name, "redux") || strstr(name, "Redux")) return "Redux";
  if (strstr(name, "europe") || strstr(name, "Europe")) return "EN (Europa)";
  if (strstr(name, "canada") || strstr(name, "Canada")) return "FR-C (Canada)";
  return "Desconhecido";
}

static void scan_roms(void) {
  g_rom_count = 0;
  DIR *d = opendir(".");
  if (!d) return;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL && g_rom_count < MAX_ROMS) {
    char *name = ent->d_name;
    size_t len = strlen(name);
    if (len > 4 && (strstr(name + len - 4, ".sfc") || strstr(name + len - 4, ".SFC"))) {
      strncpy(g_roms[g_rom_count].filename, name, sizeof(g_roms[0].filename) - 1);
      snprintf(g_roms[g_rom_count].label, sizeof(g_roms[0].label), "%s [%s]", name, identify_rom(name));
      g_rom_count++;
    }
  }
  closedir(d);
}

// ============ Language Detection ============
#define MAX_LANGS 16
static char g_lang_codes[MAX_LANGS][8];
static const char *g_lang_labels[MAX_LANGS];
static int g_lang_count;

static const char *lang_name(const char *code) {
  if (strcmp(code, "us") == 0) return "English (US)";
  if (strcmp(code, "pt") == 0) return "Portugues";
  if (strcmp(code, "es") == 0) return "Espanol";
  if (strcmp(code, "de") == 0) return "Alemao";
  if (strcmp(code, "fr") == 0) return "Frances";
  if (strcmp(code, "ja") == 0) return "Japones";
  if (strcmp(code, "pl") == 0) return "Polones";
  if (strcmp(code, "nl") == 0) return "Holandes";
  if (strcmp(code, "sv") == 0) return "Sueco";
  return code;
}

static void scan_langs(void) {
  g_lang_count = 0;
  // Always include "us" (no langpack needed)
  strncpy(g_lang_codes[g_lang_count], "us", 7);
  g_lang_labels[g_lang_count] = lang_name("us");
  g_lang_count++;
  // Scan for langpack files
  DIR *d = opendir(".");
  if (!d) return;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL && g_lang_count < MAX_LANGS) {
    char *name = ent->d_name;
    if (strncmp(name, "zelda3_langpack_", 16) == 0) {
      size_t len = strlen(name);
      if (len > 20 && strcmp(name + len - 4, ".dat") == 0) {
        char code[8];
        strncpy(code, name + 16, 7);
        code[len - 20] = 0;
        // Skip if already have "us" or duplicate
        bool dup = (strcmp(code, "us") == 0);
        for (int i = 0; i < g_lang_count && !dup; i++)
          if (strcmp(g_lang_codes[i], code) == 0) dup = true;
        if (!dup) {
          strncpy(g_lang_codes[g_lang_count], code, 7);
          g_lang_labels[g_lang_count] = lang_name(code);
          g_lang_count++;
        }
      }
    }
  }
  closedir(d);
}

// ============ Settings definitions ============
enum { TAB_GRAPHICS = 0, TAB_AUDIO, TAB_GAMEPLAY, TAB_SWITCH, TAB_COUNT };
static const char *kTabNames[] = { "GRAFICOS", "AUDIO", "GAMEPLAY", "SWITCH" };

typedef enum { OPT_TOGGLE, OPT_LIST, OPT_INFO } OptType;

typedef struct {
  const char *label;
  OptType type;
  int *value_ptr;           // pointer to g_config field
  int toggle_count;         // for OPT_LIST: number of options
  const char **toggle_labels; // for OPT_LIST: option labels
  int min_val, max_val;     // for OPT_LIST: value range
} SettingItem;

// Graphics settings
static const char *kFullscreen[] = { "JANELA", "FS DESKTOP", "FS EXCLUSIVO" };
static const char *kResolution[] = { "640X480", "960X544", "1280X720", "1920X1080" };
static const char *kAspect[] = { "4:3", "16:9", "16:10" };
static const char *kYesNo[] = { "SIM", "NAO" };

static int g_res_index = 2; // default 1280x720

#ifdef __SWITCH__
static SettingItem g_graphics_items[] = {
  { "PROPORCAO",      OPT_LIST, &local_extended_aspect_ratio, 3, kAspect, 0, 2 },
  { "MODO SETE",      OPT_TOGGLE, &local_enhanced_mode7, 0, NULL, 0, 1 },
  { "RENDERER NOVO",  OPT_TOGGLE, &local_new_renderer, 0, NULL, 0, 1 },
  { "SEM LIMITE SP",  OPT_TOGGLE, &local_no_sprite_limits, 0, NULL, 0, 1 },
  { "FILTRO BILINEAR",OPT_TOGGLE, &local_linear_filtering, 0, NULL, 0, 1 },
};
#else
static SettingItem g_graphics_items[] = {
  { "TELA CHEIA",     OPT_LIST, &local_fullscreen, 3, kFullscreen, 0, 2 },
  { "RESOLUCAO",      OPT_LIST, &g_res_index, 4, kResolution, 0, 3 },
  { "ESCALA",         OPT_LIST, &local_window_scale, 4, NULL, 1, 4 },
  { "PROPORCAO",      OPT_LIST, &local_extended_aspect_ratio, 3, kAspect, 0, 2 },
  { "MODO SETE",      OPT_TOGGLE, &local_enhanced_mode7, 0, NULL, 0, 1 },
  { "RENDERER NOVO",  OPT_TOGGLE, &local_new_renderer, 0, NULL, 0, 1 },
  { "SEM LIMITE SP",  OPT_TOGGLE, &local_no_sprite_limits, 0, NULL, 0, 1 },
  { "FILTRO BILINEAR",OPT_TOGGLE, &local_linear_filtering, 0, NULL, 0, 1 },
  { "IGNORAR ASPECTO",OPT_TOGGLE, &local_ignore_aspect_ratio, 0, NULL, 0, 1 },
};
#endif
#define GRAPHICS_COUNT (sizeof(g_graphics_items) / sizeof(g_graphics_items[0]))

// Audio settings
static const char *kFreq[] = { "22050", "32000", "44100", "48000" };
static const char *kChannels[] = { "MONO", "STEREO" };
static const char *kBuffer[] = { "512", "1024", "2048", "4096" };

static int g_freq_index = 2;
static int g_chan_index = 1;
static int g_buf_index = 2;

static SettingItem g_audio_items[] = {
  { "AUDIO",          OPT_TOGGLE, &local_enable_audio, 0, NULL, 0, 1 },
  { "FREQUENCIA",     OPT_LIST, &g_freq_index, 4, kFreq, 0, 3 },
  { "CANAIS",         OPT_LIST, &g_chan_index, 2, kChannels, 0, 1 },
  { "BUFFER",         OPT_LIST, &g_buf_index, 4, kBuffer, 0, 3 },
  { "MSU",            OPT_TOGGLE, &local_enable_msu, 0, NULL, 0, 1 },
};
#define AUDIO_COUNT (sizeof(g_audio_items) / sizeof(g_audio_items[0]))

// Gameplay settings
static SettingItem g_gameplay_items[] = {
  { "LINGUAGEM",      OPT_LIST, &local_language, 0, NULL, 0, 0 },
  { "AUTOSAVE",       OPT_TOGGLE, &local_autosave, 0, NULL, 0, 1 },
  { "SLOT PADRAO",    OPT_LIST, &local_save_slot, 10, NULL, 0, 9 },
  { "EXTEND Y",       OPT_TOGGLE, &local_extend_y, 0, NULL, 0, 1 },
  { "SEM FLASH",      OPT_TOGGLE, (int*)&g_config.features0, 0, NULL, 0, 1 },
  { "SEM EMULACAO",   OPT_TOGGLE, &local_run_without_emu, 0, NULL, 0, 1 },
  { "MOSTRAR FPS",    OPT_TOGGLE, &local_display_perf_title, 0, NULL, 0, 1 },
};
#define GAMEPLAY_COUNT (sizeof(g_gameplay_items) / sizeof(g_gameplay_items[0]))

// Switch settings
static SettingItem g_switch_items[] = {
  { "TELA DIVIDIDA",  OPT_TOGGLE, &local_dual_screen, 0, NULL, 0, 1 },
};
#define SWITCH_COUNT (sizeof(g_switch_items) / sizeof(g_switch_items[0]))

static SettingItem *g_tab_items[] = {
  g_graphics_items, g_audio_items, g_gameplay_items, g_switch_items
};
static int g_tab_counts[] = { GRAPHICS_COUNT, AUDIO_COUNT, GAMEPLAY_COUNT, SWITCH_COUNT };

// ============ Menu state ============
static int g_cur_tab;
static int g_cur_row;
static float g_scroll_y;
static float g_scroll_target;
static uint8 g_last_hat;
static int g_stick_cooldown;

// ============ Draw settings row ============
static void draw_setting_row(SettingItem *item, float x, float y, float w, float h, bool selected) {
  uint32 bg = selected ? COL(50, 46, 34) : COL(34, 30, 22);
  uint32 border = selected ? COL_ACCENT : COL_BORDER;

  fill_round(x, y, w, h, 3 * menu_u, bg);
  fill_round(x + 1, y + 1, w - 2, h - 2, 2 * menu_u, border);

  // Label
  draw_text(item->label, x + 8 * menu_u, y + (h - 5 * menu_u) / 2, 0.7f * menu_u);

  // Value on the right
  float rx = x + w - 8 * menu_u;
  if (item->type == OPT_TOGGLE) {
    bool on = *item->value_ptr != 0;
    const char *label = on ? "ON" : "OFF";
    uint32 col = on ? COL_GREEN : COL_RED;
    float tw = text_width(label, 0.7f * menu_u);
    fill_rect(rx - tw - 2 * menu_u, y + (h - 6 * menu_u) / 2, tw + 4 * menu_u, 6 * menu_u, col);
    draw_text(label, rx - tw, y + (h - 5 * menu_u) / 2, 0.7f * menu_u);
  } else if (item->type == OPT_LIST) {
    int val = *item->value_ptr;
    const char *label;
    char buf[32];
    if (item->toggle_labels) {
      label = (val >= 0 && val < item->toggle_count) ? item->toggle_labels[val] : "?";
    } else {
      snprintf(buf, sizeof(buf), "%d", val);
      label = buf;
    }
    // Right-aligned: value with small arrow indicators
    float tw = text_width(label, 0.7f * menu_u);
    float ay = y + (h - 5 * menu_u) / 2;
    // Draw right arrow >
    float arx = rx - 1 * menu_u;
    set_color(selected ? COL_ACCENT : COL_BORDER);
    for (int i = 0; i < 3; i++) {
      SDL_FRect r1 = {arx - i, ay + 2 * menu_u + i, 1, 1};
      SDL_FRect r2 = {arx - i, ay + 2 * menu_u + (5 * menu_u - 2) - i, 1, 1};
      SDL_RenderFillRectF(menu_r, &r1);
      SDL_RenderFillRectF(menu_r, &r2);
    }
    draw_text(label, rx - 5 * menu_u - tw, ay, 0.7f * menu_u);
  }
}

// ============ Draw ROM list ============
static void draw_rom_section(float x, float y, float w) {
  float row_h = 10 * menu_u;
  float label_h = 16 * menu_u;
  float rom_h = g_rom_count * row_h;
  float total_h = label_h + rom_h + 4 * menu_u;
  if (total_h < 30 * menu_u) total_h = 30 * menu_u;

  menu_box(x, y, w, total_h, COL_BORDER);
  draw_text("ROMS ENCONTRADAS:", x + 10 * menu_u, y + 6 * menu_u, 0.7f * menu_u);

  if (g_rom_count == 0) {
    draw_text("NENHUM .SFC ENCONTRADO", x + 10 * menu_u, y + 18 * menu_u, 0.5f * menu_u);
  } else {
    for (int i = 0; i < g_rom_count; i++) {
      float ry = y + label_h + i * row_h;
      uint32 col = COL_TEXT;
      if (g_config.rom_path && strcmp(g_roms[i].filename, g_config.rom_path) == 0)
        col = COL_GREEN;
      draw_text(g_roms[i].label, x + 14 * menu_u, ry, 0.5f * menu_u);
    }
  }
}

// ============ Draw tabs ============
static void draw_tab_bar(float y, float h) {
  float tab_w = (menu_W - 20 * menu_u) / TAB_COUNT;
  for (int i = 0; i < TAB_COUNT; i++) {
    float tx = 10 * menu_u + i * tab_w;
    bool sel = (i == g_cur_tab);
    uint32 bg = sel ? COL_ACCENT : COL(50, 46, 34);
    fill_rect(tx + 2, y + 2, tab_w - 4, h - 4, bg);

    float tw = text_width(kTabNames[i], 0.7f * menu_u);
    draw_text(kTabNames[i], tx + (tab_w - tw) / 2, y + (h - 5 * menu_u) / 2, 0.7f * menu_u);
  }
}

// ============ Draw footer ============
static void draw_footer(float y, float h) {
  fill_rect(0, y, menu_W, h, COL(20, 18, 14));

  float cx = menu_W / 2;
  draw_text("A:CONFIRMAR  B:VOLTAR  L/R:TROCA ABA  START:JOGAR", cx - 100 * menu_u, y + (h - 5 * menu_u) / 2, 0.5f * menu_u);
}

// ============ Apply list value ============
static void apply_audio_settings(void) {
  static const int kFreqs[] = { 22050, 32000, 44100, 48000 };
  static const int kChans[] = { 1, 2 };
  static const int kBufs[] = { 512, 1024, 2048, 4096 };
  g_config.audio_freq = kFreqs[g_freq_index];
  g_config.audio_channels = kChans[g_chan_index];
  g_config.audio_samples = kBufs[g_buf_index];
}

static void apply_resolution(void) {
#ifndef __SWITCH__
  static const int kRes[][2] = { {640,480}, {960,544}, {1280,720}, {1920,1080} };
  g_config.window_width = kRes[g_res_index][0];
  g_config.window_height = kRes[g_res_index][1];
#endif
}

// ============ INI persistence ============
static bool starts_with(const char *line, const char *key) {
  while (*key) {
    if (*line != *key) return false;
    line++; key++;
  }
  return true;
}

static void write_ini_value(FILE *f, const char *key, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(f, "%s = ", key);
  vfprintf(f, fmt, args);
  fprintf(f, "\n");
  va_end(args);
}

static void save_ini(void) {
  // Sync local copies back to config
  g_config.fullscreen = (uint8)local_fullscreen;
  g_config.window_scale = (uint8)local_window_scale;
  // Convert aspect ratio menu index back to pixel value
  {
    static const int kAspectPixels[] = { 0, 71, 51 }; // 4:3=0, 16:9=71, 16:10=51
    int idx = local_extended_aspect_ratio;
    g_config.extended_aspect_ratio = (idx >= 0 && idx < 3) ? kAspectPixels[idx] : 0;
  }
  g_config.enhanced_mode7 = local_enhanced_mode7 != 0;
  g_config.new_renderer = local_new_renderer != 0;
  g_config.no_sprite_limits = local_no_sprite_limits != 0;
  g_config.linear_filtering = local_linear_filtering != 0;
  g_config.ignore_aspect_ratio = local_ignore_aspect_ratio != 0;
  g_config.enable_audio = local_enable_audio != 0;
  g_config.enable_msu = (uint8)local_enable_msu;
  g_config.autosave = local_autosave != 0;
  g_config.save_slot = (uint8)local_save_slot;
  g_config.extend_y = local_extend_y != 0;
  g_config.run_without_emu = local_run_without_emu != 0;
  g_config.display_perf_title = local_display_perf_title != 0;
  g_config.dual_screen = local_dual_screen != 0;
  // Save language selection
  if (local_language >= 0 && local_language < g_lang_count)
    g_config.language = g_lang_codes[local_language];
  apply_audio_settings();
  apply_resolution();

  // Read existing file lines
  char lines[512][256];
  int line_count = 0;
  FILE *f = fopen("zelda3.ini", "r");
  if (f) {
    while (line_count < 512 && fgets(lines[line_count], 256, f))
      line_count++;
    fclose(f);
  }

  // Write back, updating known keys
  f = fopen("zelda3.ini", "w");
  if (!f) return;

  for (int i = 0; i < line_count; i++) {
    char *line = lines[i];

    // Update known keys
    if (starts_with(line, "Autosave"))            { write_ini_value(f, "Autosave", "%d", g_config.autosave); continue; }
    if (starts_with(line, "SaveSlot"))            { write_ini_value(f, "SaveSlot", "%d", g_config.save_slot); continue; }
    if (starts_with(line, "DualScreen"))          { write_ini_value(f, "DualScreen", "%d", g_config.dual_screen); continue; }
    if (starts_with(line, "ShowSettingsMenu"))    { write_ini_value(f, "ShowSettingsMenu", "%d", g_config.show_settings_menu); continue; }
    if (starts_with(line, "RunWithoutEmu"))       { write_ini_value(f, "RunWithoutEmu", "%d", g_config.run_without_emu); continue; }
    if (starts_with(line, "DisplayPerfInTitle"))  { write_ini_value(f, "DisplayPerfInTitle", "%d", g_config.display_perf_title); continue; }
    if (starts_with(line, "Language"))            { fprintf(f, "Language = %s\n", g_config.language ? g_config.language : "us"); continue; }
    if (starts_with(line, "ExtendedAspectRatio")) {
      int v = g_config.extended_aspect_ratio;
      const char *ar = (v == 0) ? "4:3" : (v > 60) ? "16:9" : "16:10";
      write_ini_value(f, "ExtendedAspectRatio", "%s", ar);
      continue;
    }
    if (starts_with(line, "Fullscreen"))          { write_ini_value(f, "Fullscreen", "%d", g_config.fullscreen); continue; }
    if (starts_with(line, "WindowSize"))          { write_ini_value(f, "WindowSize", "%dx%d", g_config.window_width, g_config.window_height); continue; }
    if (starts_with(line, "WindowScale"))         { write_ini_value(f, "WindowScale", "%d", g_config.window_scale); continue; }
    if (starts_with(line, "NewRenderer"))         { write_ini_value(f, "NewRenderer", "%d", g_config.new_renderer); continue; }
    if (starts_with(line, "EnhancedMode7"))       { write_ini_value(f, "EnhancedMode7", "%d", g_config.enhanced_mode7); continue; }
    if (starts_with(line, "NoSpriteLimits"))      { write_ini_value(f, "NoSpriteLimits", "%d", g_config.no_sprite_limits); continue; }
    if (starts_with(line, "LinearFiltering"))     { write_ini_value(f, "LinearFiltering", "%d", g_config.linear_filtering); continue; }
    if (starts_with(line, "IgnoreAspectRatio"))   { write_ini_value(f, "IgnoreAspectRatio", "%d", g_config.ignore_aspect_ratio); continue; }
    if (starts_with(line, "EnableAudio"))         { write_ini_value(f, "EnableAudio", "%d", g_config.enable_audio); continue; }
    if (starts_with(line, "AudioFreq"))           { write_ini_value(f, "AudioFreq", "%d", g_config.audio_freq); continue; }
    if (starts_with(line, "AudioChannels"))       { write_ini_value(f, "AudioChannels", "%d", g_config.audio_channels); continue; }
    if (starts_with(line, "AudioSamples"))        { write_ini_value(f, "AudioSamples", "%d", g_config.audio_samples); continue; }
    if (starts_with(line, "EnableMSU"))           { write_ini_value(f, "EnableMSU", "%d", g_config.enable_msu); continue; }
    if (starts_with(line, "RomPath"))             { fprintf(f, "RomPath = %s\n", g_config.rom_path ? g_config.rom_path : ""); continue; }

    // Pass through unchanged
    fputs(line, f);
  }

  fclose(f);
}

// ============ Main menu loop ============
bool SettingsMenu_Run(SDL_Renderer *renderer, SDL_Window *window) {
  menu_r = renderer;
  g_cur_tab = 0;
  g_cur_row = 0;

  // Get window size
  SDL_GetRendererOutputSize(menu_r, &menu_W, &menu_H);

  // Compute the logical size the renderer uses (matches SNES coordinates)
  int snes_w = g_config.extended_aspect_ratio * 2 + 256;
  int snes_h = g_config.extend_y ? 240 : 224;
#ifndef __SWITCH__
  if (!g_config.ignore_aspect_ratio) {
    menu_W = snes_w;
    menu_H = snes_h;
  }
#endif

  menu_u = (menu_W < menu_H ? menu_W : menu_H) / 240.0f;
  if (menu_u < 0.01f) menu_u = 1.0f;

  // Load text textures
  load_menu_textures();

  // Scan for ROMs
  scan_roms();

  // Scan for available languages
  scan_langs();
  // Update language item toggle_labels and toggle_count
  g_gameplay_items[0].toggle_labels = g_lang_labels;
  g_gameplay_items[0].toggle_count = g_lang_count;
  g_gameplay_items[0].max_val = g_lang_count - 1;

  // Initialize list indices and local copies from current config
  local_fullscreen = g_config.fullscreen;
  local_window_scale = g_config.window_scale;
  // Convert aspect ratio pixel value back to menu index
  // config stores pixel width: 0=4:3, ~71=16:9, ~51=16:10
  {
    int v = g_config.extended_aspect_ratio;
    local_extended_aspect_ratio = (v == 0) ? 0 : (v > 60) ? 1 : 2;
  }
  local_enhanced_mode7 = g_config.enhanced_mode7;
  local_new_renderer = g_config.new_renderer;
  local_no_sprite_limits = g_config.no_sprite_limits;
  local_linear_filtering = g_config.linear_filtering;
  local_ignore_aspect_ratio = g_config.ignore_aspect_ratio;
  local_enable_audio = g_config.enable_audio;
  local_enable_msu = g_config.enable_msu;
  local_autosave = g_config.autosave;
  local_save_slot = g_config.save_slot;
  local_extend_y = g_config.extend_y;
  local_run_without_emu = g_config.run_without_emu;
  local_display_perf_title = g_config.display_perf_title;
  local_dual_screen = g_config.dual_screen;
  // Find language index from config
  local_language = 0;
  if (g_config.language) {
    for (int i = 0; i < g_lang_count; i++) {
      if (strcmp(g_lang_codes[i], g_config.language) == 0) {
        local_language = i;
        break;
      }
    }
  }
  g_res_index = 2; // default 1280x720
  if (g_config.window_width == 640) g_res_index = 0;
  else if (g_config.window_width == 960) g_res_index = 1;
  else if (g_config.window_width == 1920) g_res_index = 3;

  g_freq_index = 2; // default 44100
  if (g_config.audio_freq == 22050) g_freq_index = 0;
  else if (g_config.audio_freq == 32000) g_freq_index = 1;
  else if (g_config.audio_freq == 48000) g_freq_index = 3;

  g_chan_index = g_config.audio_channels == 1 ? 0 : 1;

  g_buf_index = 2; // default 2048
  if (g_config.audio_samples == 512) g_buf_index = 0;
  else if (g_config.audio_samples == 1024) g_buf_index = 1;
  else if (g_config.audio_samples == 4096) g_buf_index = 3;

  bool running = true;
  bool started = false;
  SDL_Event event;

  while (running && !started) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
          // Start game
          save_ini();
          started = true;
          break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
          // Cancel
          running = false;
          break;
        case SDLK_TAB:
        case SDLK_RIGHT:
          // Next tab
          g_cur_tab = (g_cur_tab + 1) % TAB_COUNT;
          g_cur_row = 0;
          g_scroll_y = 0;
          break;
        case SDLK_LEFT:
          // Prev tab
          g_cur_tab = (g_cur_tab + TAB_COUNT - 1) % TAB_COUNT;
          g_cur_row = 0;
          g_scroll_y = 0;
          break;
        case SDLK_UP:
          if (g_cur_row > 0) g_cur_row--;
          break;
        case SDLK_DOWN:
          if (g_cur_row < g_tab_counts[g_cur_tab] - 1) g_cur_row++;
          break;
        case SDLK_LEFTBRACKET:  // [
        case SDLK_MINUS:
          // Decrease value
          {
            SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
            if (item->type == OPT_TOGGLE) {
              *item->value_ptr = !(*item->value_ptr);
            } else if (item->type == OPT_LIST) {
              int *val = item->value_ptr;
              if (*val > item->min_val) (*val)--;
              else *val = item->max_val;
            }
          }
          break;
        case SDLK_RIGHTBRACKET:  // ]
        case SDLK_EQUALS:
          // Increase value
          {
            SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
            if (item->type == OPT_TOGGLE) {
              *item->value_ptr = !(*item->value_ptr);
            } else if (item->type == OPT_LIST) {
              int *val = item->value_ptr;
              if (*val < item->max_val) (*val)++;
              else *val = item->min_val;
            }
          }
          break;
        case SDLK_SPACE:
          // Toggle / cycle
          {
            SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
            if (item->type == OPT_TOGGLE) {
              *item->value_ptr = !(*item->value_ptr);
            } else if (item->type == OPT_LIST) {
              int *val = item->value_ptr;
              if (*val < item->max_val) (*val)++;
              else *val = item->min_val;
            }
          }
          break;
        default:
          break;
        }
        break;

      case SDL_CONTROLLERBUTTONDOWN:
        switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_START:
          save_ini();
          started = true;
          break;
#ifdef __SWITCH__
        case SDL_CONTROLLER_BUTTON_A:
          // A on Switch = cancel
          running = false;
          break;
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_Y:
          // B on Switch = toggle/cycle
          {
            SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
            if (item->type == OPT_TOGGLE) {
              *item->value_ptr = !(*item->value_ptr);
            } else if (item->type == OPT_LIST) {
              int *val = item->value_ptr;
              if (*val < item->max_val) (*val)++;
              else *val = item->min_val;
            }
          }
          break;
#else
        case SDL_CONTROLLER_BUTTON_B:
          running = false;
          break;
        case SDL_CONTROLLER_BUTTON_A:
        case SDL_CONTROLLER_BUTTON_Y:
          // Toggle / cycle
          {
            SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
            if (item->type == OPT_TOGGLE) {
              *item->value_ptr = !(*item->value_ptr);
            } else if (item->type == OPT_LIST) {
              int *val = item->value_ptr;
              if (*val < item->max_val) (*val)++;
              else *val = item->min_val;
            }
          }
          break;
#endif
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
          g_cur_tab = (g_cur_tab + TAB_COUNT - 1) % TAB_COUNT;
          g_cur_row = 0;
          g_scroll_y = 0;
          break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
          g_cur_tab = (g_cur_tab + 1) % TAB_COUNT;
          g_cur_row = 0;
          g_scroll_y = 0;
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
          if (g_cur_row > 0) g_cur_row--;
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
          if (g_cur_row < g_tab_counts[g_cur_tab] - 1) g_cur_row++;
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
          {
            SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
            if (item->type == OPT_LIST) {
              int *val = item->value_ptr;
              if (*val > item->min_val) (*val)--;
              else *val = item->max_val;
            } else if (item->type == OPT_TOGGLE) {
              *item->value_ptr = !(*item->value_ptr);
            }
          }
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
          {
            SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
            if (item->type == OPT_LIST) {
              int *val = item->value_ptr;
              if (*val < item->max_val) (*val)++;
              else *val = item->min_val;
            } else if (item->type == OPT_TOGGLE) {
              *item->value_ptr = !(*item->value_ptr);
            }
          }
          break;
        default:
          break;
        }
        break;

      case SDL_JOYHATMOTION:
        {
          uint8 hat = event.jhat.value;
          uint8 changed = hat ^ g_last_hat;
          g_last_hat = hat;
          if (changed & hat) {  // Only on press, not release
            if (hat & SDL_HAT_UP) {
              if (g_cur_row > 0) g_cur_row--;
            } else if (hat & SDL_HAT_DOWN) {
              if (g_cur_row < g_tab_counts[g_cur_tab] - 1) g_cur_row++;
            } else if (hat & SDL_HAT_LEFT) {
              SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
              if (item->type == OPT_LIST) {
                int *val = item->value_ptr;
                if (*val > item->min_val) (*val)--;
                else *val = item->max_val;
              } else if (item->type == OPT_TOGGLE) {
                *item->value_ptr = !(*item->value_ptr);
              }
            } else if (hat & SDL_HAT_RIGHT) {
              SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
              if (item->type == OPT_LIST) {
                int *val = item->value_ptr;
                if (*val < item->max_val) (*val)++;
                else *val = item->min_val;
              } else if (item->type == OPT_TOGGLE) {
                *item->value_ptr = !(*item->value_ptr);
              }
            }
          }
        }
        break;

      case SDL_CONTROLLERAXISMOTION:
        // Left stick for navigation (with deadzone and cooldown)
        if (g_stick_cooldown > 0) { g_stick_cooldown--; break; }
        {
          int threshold = 16000;
          if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            if (event.caxis.value < -threshold) {
              if (g_cur_row > 0) g_cur_row--;
              g_stick_cooldown = 12;
            } else if (event.caxis.value > threshold) {
              if (g_cur_row < g_tab_counts[g_cur_tab] - 1) g_cur_row++;
              g_stick_cooldown = 12;
            }
          } else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
            if (event.caxis.value < -threshold) {
              if (item->type == OPT_LIST) {
                int *val = item->value_ptr;
                if (*val > item->min_val) (*val)--;
                else *val = item->max_val;
              } else if (item->type == OPT_TOGGLE) {
                *item->value_ptr = !(*item->value_ptr);
              }
              g_stick_cooldown = 12;
            } else if (event.caxis.value > threshold) {
              if (item->type == OPT_LIST) {
                int *val = item->value_ptr;
                if (*val < item->max_val) (*val)++;
                else *val = item->min_val;
              } else if (item->type == OPT_TOGGLE) {
                *item->value_ptr = !(*item->value_ptr);
              }
              g_stick_cooldown = 12;
            }
          }
        }
        break;

      case SDL_FINGERDOWN:
        {
          float tx = event.tfinger.x * menu_W;
          float ty = event.tfinger.y * menu_H;

          // Check tab bar
          float tab_h = 28 * menu_u;
          if (ty >= 36 * menu_u && ty < 36 * menu_u + tab_h) {
            float tab_w = (menu_W - 20 * menu_u) / TAB_COUNT;
            int new_tab = (int)((tx - 10 * menu_u) / tab_w);
            if (new_tab >= 0 && new_tab < TAB_COUNT) {
              g_cur_tab = new_tab;
              g_cur_row = 0;
              g_scroll_y = 0;
            }
          }

          // Check setting rows
          float content_y = 28 * menu_u + tab_h + 4 * menu_u;
          float row_h = 18 * menu_u;
          float row_w = menu_W - 24 * menu_u;
          float row_gap = 2 * menu_u;
          float row_step = row_h + row_gap;
          for (int i = 0; i < g_tab_counts[g_cur_tab]; i++) {
            float ry = content_y + i * row_step - g_scroll_y;
            if (ty >= ry && ty < ry + row_h && tx >= 20 * menu_u && tx < 20 * menu_u + row_w) {
              g_cur_row = i;
              SettingItem *item = &g_tab_items[g_cur_tab][g_cur_row];
              // Right side = change value
              if (tx > 20 * menu_u + row_w * 0.6f) {
                if (item->type == OPT_TOGGLE) {
                  *item->value_ptr = !(*item->value_ptr);
                } else if (item->type == OPT_LIST) {
                  int *val = item->value_ptr;
                  if (*val < item->max_val) (*val)++;
                  else *val = item->min_val;
                }
              }
            }
          }
        }
        break;
      }
    }

    // Render
    SDL_SetRenderDrawColor(menu_r, 0, 0, 0, 255);
    if (SDL_RenderClear(menu_r) < 0) break;

    // Background
    fill_rect(0, 0, menu_W, menu_H, COL_BG);

    // Title
    draw_text("ZELDA3 - CONFIGURACOES", menu_W / 2 - text_width("ZELDA3 - CONFIGURACOES", 1.0 * menu_u) / 2, 8 * menu_u, 1.0 * menu_u);

    // Tab bar
    float tab_h = 22 * menu_u;
    draw_tab_bar(28 * menu_u, tab_h);

    // Content area
    float content_y = 28 * menu_u + tab_h + 4 * menu_u;
    float row_h = 18 * menu_u;
    float row_w = menu_W - 24 * menu_u;
    float row_gap = 2 * menu_u;
    float row_step = row_h + row_gap;

    // Calculate visible area height (from content_y to footer)
    float footer_h = 16 * menu_u;
    float visible_h = menu_H - footer_h - 2 * menu_u - content_y;

    // Settings rows with scroll
    SettingItem *items = g_tab_items[g_cur_tab];
    int count = g_tab_counts[g_cur_tab];

    // Update scroll target to keep selected row visible
    float target_y = g_cur_row * row_step;
    if (target_y < g_scroll_y)
      g_scroll_y = target_y;
    if (target_y + row_h > g_scroll_y + visible_h)
      g_scroll_y = target_y + row_h - visible_h;
    if (g_scroll_y < 0) g_scroll_y = 0;
    float max_scroll = count * row_step - visible_h;
    if (max_scroll < 0) max_scroll = 0;
    if (g_scroll_y > max_scroll) g_scroll_y = max_scroll;

    // Draw visible rows only
    SDL_Rect clip = {0, (int)content_y, menu_W, (int)visible_h};
    SDL_RenderSetClipRect(menu_r, &clip);
    for (int i = 0; i < count; i++) {
      float ry = content_y + i * row_step - g_scroll_y;
      if (ry + row_h < content_y || ry > content_y + visible_h) continue;
      draw_setting_row(&items[i], 12 * menu_u, ry, row_w, row_h, i == g_cur_row);
    }
    SDL_RenderSetClipRect(menu_r, NULL);

    // ROM section (below settings)
    float rom_y = content_y + count * row_step - g_scroll_y + 4 * menu_u;
    float rom_w = menu_W - 24 * menu_u;
    if (rom_y + 20 * menu_u < content_y + visible_h)
      draw_rom_section(12 * menu_u, rom_y, rom_w);

    // Footer
    draw_footer(menu_H - footer_h - 2 * menu_u, footer_h);

    SDL_RenderPresent(menu_r);
  }

  destroy_menu_textures();
  return started;
}

void SettingsMenu_RenderNotify(SDL_Renderer *renderer, const char *text) {
  if (!text || !text[0]) return;
  // Ensure textures are loaded
  if (!tex_letters || !tex_glyphs) load_menu_textures();
  if (!tex_letters || !tex_glyphs) return;

  menu_r = renderer;
  int ww, wh;
  SDL_GetRendererOutputSize(renderer, &ww, &wh);
  float u = (ww < wh ? ww : wh) / 240.0f;
  if (u < 1.0f) u = 1.0f;

  float pad = 8 * u;
  float bh = 18 * u;
  float tw = text_width(text, 0.7f * u);
  float bw = tw + 12 * u;
  float bx = ww - bw - pad;
  float by = wh - bh - pad;

  // Semi-transparent background
  set_color(0xC0000000);
  SDL_FRect bg = {bx, by, bw, bh};
  SDL_RenderFillRectF(renderer, &bg);

  // Green text
  draw_text(text, bx + 6 * u, by + (bh - 5 * u) / 2, 0.7f * u);
}
