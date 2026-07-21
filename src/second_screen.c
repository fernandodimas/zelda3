// Game state access + art generation for the second screen. The SS_* core is
// plain C; the Android UI reaches it through the JNI wrappers at the bottom
// and the SDL UI (second_screen_sdl.c) calls it directly.
#include <string.h>
#include <stdbool.h>

#include "types.h"
#include "variables.h"
#include "features.h"
#include "hud.h"
#include "assets.h"
#include "load_gfx.h"
#include "messaging.h"
#include "config.h"
#include "zelda_rtl.h"
#include "second_screen_sdl.h"
#include "snes/ppu.h"
#include "second_screen_tables.h"

int SS_GetLinkX(void) { return link_x_coord; }
int SS_GetLinkY(void) { return link_y_coord; }

int SS_GetArea(void) {
  return player_is_indoors ? dungeon_room_index : overworld_screen_index;
}

int SS_GetModule(void) {
  return main_module_index | (submodule_index << 8);
}

bool SS_IsIndoors(void) { return player_is_indoors != 0; }

void SS_ReadSram(uint8 *out, int n) {
  if (n > 0x100) n = 0x100;
  memcpy(out, g_ram + 0xF300, n);
}

static int SS_ItemToSlot(uint8 item) {
  if (item >= 21) return 16;
  if (item == 0 || item > 20) return 0;
  if (hud_inventory_order[0] != 0) {
    for (int i = 0; i < 20; i++)
      if (hud_inventory_order[i] == item) return i + 1;
  }
  return item;
}

int SS_GetEquippedSlot(void) { return SS_ItemToSlot(hud_cur_item); }

int SS_GetEquippedSlotX(void) {
  if (!(enhanced_features0 & kFeatures0_SwitchLR)) return 0;
  return SS_ItemToSlot(hud_cur_item_x);
}

int SS_GetDungeon(void) {
  uint8 palace = (uint8)cur_palace_index_x2;
  return (palace == 0xff ? 0xff : palace >> 1) | ((dung_cur_floor & 0xFF) << 8);
}

void SS_ReadDungFlags(uint8 *out, int n) {
  if (n > 0x500) n = 0x500;
  memcpy(out, g_ram + 0xF000, n);
}

bool SS_GetIndoorExit(int *out) {
  if (!g_asset_ptrs[130] || !g_asset_ptrs[131] || !g_asset_ptrs[135] || !g_asset_ptrs[136])
    return false;
  int room = dungeon_room_index;
  int n = (int)(kExitDataRooms_SIZE / sizeof(uint16));
  for (int k = 0; k < n; k++) {
    if (kExitDataRooms[k] == room) {
      out[0] = kExitData_XCoord[k];
      out[1] = kExitData_YCoord[k];
      out[2] = kExitData_ScreenIndex[k];
      return true;
    }
  }
  return false;
}

static uint8 g_ss_tiles[384 * 64];
static bool g_ss_tiles_ready;

static bool SS_AssetsReady(void) {
  return g_asset_ptrs[57] && g_asset_ptrs[65] && g_asset_ptrs[66] && g_asset_ptrs[67] &&
         g_asset_ptrs[68] && g_asset_ptrs[81] && g_asset_ptrs[90] && g_asset_ptrs[91] &&
         g_asset_ptrs[92] && g_asset_ptrs[93];
}

static void SS_EnsureTiles(void) {
  if (g_ss_tiles_ready) return;
  static const uint8 kSheets[3] = {0x6a, 0x6b, 0x69};
  uint8 raw[0x1000];
  for (int s = 0; s < 3; s++) {
    Decompress(raw, kSprGfx(kSheets[s]).ptr);
    for (int t = 0; t < 128; t++) {
      const uint8 *tp = raw + t * 16;
      uint8 *out = g_ss_tiles + (s * 128 + t) * 64;
      for (int y = 0; y < 8; y++) {
        uint8 b0 = tp[y * 2], b1 = tp[y * 2 + 1];
        for (int x = 0; x < 8; x++)
          out[y * 8 + x] = ((b0 >> (7 - x)) & 1) | (((b1 >> (7 - x)) & 1) << 1);
      }
    }
  }
  g_ss_tiles_ready = true;
}

static uint32 SS_Snes555(uint16 w) {
  int r = w & 31, g = (w >> 5) & 31, b = (w >> 10) & 31;
  r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2);
  return 0xff000000u | (r << 16) | (g << 8) | b;
}

static uint32 SS_HudColor(int group, int pix) {
  return SS_Snes555(kHudPalData[group * 4 + pix]);
}

static void SS_DrawTile(uint32 *px, int stride, int x0, int y0, uint16 v) {
  int id = v & 0x3ff;
  if (id >= 384) return;
  const uint8 *t = g_ss_tiles + id * 64;
  int p = (v >> 10) & 7;
  for (int y = 0; y < 8; y++) {
    int sy = (v & 0x8000) ? 7 - y : y;
    for (int x = 0; x < 8; x++) {
      int sx = (v & 0x4000) ? 7 - x : x;
      uint8 pix = t[sy * 8 + sx];
      if (pix) px[(y0 + y) * stride + x0 + x] = SS_HudColor(p, pix);
    }
  }
}

static void SS_StripBg(uint32 *px, int stride, int x0, int y0) {
  bool dark[256], seen[256];
  for (int i = 0; i < 256; i++) {
    uint32 c = px[(y0 + i / 16) * stride + x0 + i % 16];
    int sum = ((c >> 16) & 0xff) + ((c >> 8) & 0xff) + (c & 0xff);
    dark[i] = (c >> 24) != 0 && sum < 60;
    seen[i] = false;
  }
  int stack[512], sp = 0;
  for (int i = 0; i < 16; i++) {
    stack[sp++] = i; stack[sp++] = 240 + i;
    stack[sp++] = i * 16; stack[sp++] = i * 16 + 15;
  }
  while (sp > 0) {
    int i = stack[--sp];
    if (seen[i] || !dark[i]) continue;
    seen[i] = true;
    px[(y0 + i / 16) * stride + x0 + i % 16] = 0;
    int y = i / 16, x = i % 16;
    if (y > 0 && sp < 508) stack[sp++] = i - 16;
    if (y < 15 && sp < 508) stack[sp++] = i + 16;
    if (x > 0 && sp < 508) stack[sp++] = i - 1;
    if (x < 15 && sp < 508) stack[sp++] = i + 1;
  }
}

bool SS_RenderIconSheet(uint32 *px) {
  int w = kIconCols * 16, h = ((kIconCount + kIconCols - 1) / kIconCols) * 16;
  if (!SS_AssetsReady()) return false;
  SS_EnsureTiles();
  memset(px, 0, (size_t)w * h * 4);
  for (int i = 0; i < kIconCount; i++) {
    int x0 = (i % kIconCols) * 16, y0 = (i / kIconCols) * 16;
    SS_DrawTile(px, w, x0, y0, kIconTilemap[i][0]);
    SS_DrawTile(px, w, x0 + 8, y0, kIconTilemap[i][1]);
    SS_DrawTile(px, w, x0, y0 + 8, kIconTilemap[i][2]);
    SS_DrawTile(px, w, x0 + 8, y0 + 8, kIconTilemap[i][3]);
    SS_StripBg(px, w, x0, y0);
  }
  return true;
}

bool SS_RenderGlyphSheet(uint32 *px) {
  int w = kGlyphCols * 8, h = ((kGlyphCount + kGlyphCols - 1) / kGlyphCols) * 8;
  if (!SS_AssetsReady()) return false;
  SS_EnsureTiles();
  memset(px, 0, (size_t)w * h * 4);
  for (int i = 0; i < kGlyphCount; i++)
    SS_DrawTile(px, w, (i % kGlyphCols) * 8, (i / kGlyphCols) * 8, kGlyphTiles[i]);
  return true;
}

bool SS_RenderLetterSheet(uint32 *px) {
  int w = 16 * 8, h = 2 * 8;
  if (!SS_AssetsReady()) return false;
  SS_EnsureTiles();
  memset(px, 0, (size_t)w * h * 4);
  for (int i = 0; i < 26; i++) {
    int id = i < 16 ? 0x150 + i : 0x160 + (i - 16);
    const uint8 *t = g_ss_tiles + id * 64;
    int x0 = (i % 16) * 8, y0 = (i / 16) * 8;
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++) {
        uint8 pix = t[y * 8 + x];
        if (pix == 1) px[(y0 + y) * w + x0 + x] = 0xff282830u;
        else if (pix == 2) px[(y0 + y) * w + x0 + x] = 0xfff8f8f8u;
      }
  }
  return true;
}

bool SS_RenderWorldMap(uint32 *px, bool dark) {
  if (!SS_AssetsReady()) return false;
  const uint8 *gfx = kOverworldMapGfx;
  const uint8 *light = kLightOverworldTilemap;
  const uint8 *darkm = kDarkOverworldTilemap;
  const uint16 *pal = kOverworldMapPaletteData + (dark ? 128 : 0);
  for (int ty = 0; ty < 64; ty++) {
    for (int tx = 0; tx < 64; tx++) {
      int q = (ty >= 32 ? 2 : 0) + (tx >= 32 ? 1 : 0);
      uint8 tid = light[q * 1024 + (ty & 31) * 32 + (tx & 31)];
      if (dark && ty >= 16 && ty < 48 && tx >= 16 && tx < 48)
        tid = darkm[(ty - 16) * 32 + (tx - 16)];
      const uint8 *t = gfx + tid * 64;
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
          px[(ty * 8 + y) * 512 + tx * 8 + x] = SS_Snes555(pal[t[y * 8 + x] & 0x7f]);
    }
  }
  return true;
}

bool SS_RenderLinkFace(uint32 *px, int chunk) {
  if (!SS_AssetsReady()) return false;
  const uint8 *gfx = kLinkGraphics;
  const uint16 *pal = kPalette_ArmorAndGloves;
  memset(px, 0, 16 * 16 * 4);
  static const int kOffs[4][2] = {{0, 0}, {8, 0}, {0, 8}, {8, 8}};
  for (int part = 0; part < 4; part++) {
    int tile = chunk + (part & 1) + (part >> 1) * 16;
    const uint8 *tp = gfx + tile * 32;
    for (int y = 0; y < 8; y++) {
      uint8 b0 = tp[y * 2], b1 = tp[y * 2 + 1], b2 = tp[16 + y * 2], b3 = tp[16 + y * 2 + 1];
      for (int x = 0; x < 8; x++) {
        int pix = ((b0 >> (7 - x)) & 1) | (((b1 >> (7 - x)) & 1) << 1) |
                  (((b2 >> (7 - x)) & 1) << 2) | (((b3 >> (7 - x)) & 1) << 3);
        if (pix)
          px[(kOffs[part][1] + y) * 16 + kOffs[part][0] + x] = SS_Snes555(pal[pix - 1]);
      }
    }
  }
  return true;
}

int SS_GetDungeonLayout(int palace, uint8 *out, int cap) {
  static const uint8 kBasements[14] = {1, 3, 0, 1, 0, 2, 1, 2, 2, 7, 0, 2, 3, 1};
  if (palace < 0 || palace >= 14 || !g_asset_ptrs[97]) return -1;
  MemBlk b = FindInAssetArray(97, palace);
  int n = cap;
  if ((int)b.size < n) n = (int)b.size;
  memcpy(out, b.ptr, n);
  return (int)(b.size / 25) | (kBasements[palace] << 8);
}

static uint8 g_ss_dmap_tiles[192 * 64];
static int g_ss_dmap_palace = -1;

static void SS_EnsureDmapTiles(int palace) {
  if (g_ss_dmap_palace == palace) return;
  const uint8 *packs = GetSpriteTilesetPacks(0x80 | palace);
  uint8 raw[0x1000];
  for (int s = 0; s < 3; s++) {
    Decomp_spr(raw, packs[s]);
    for (int t = 0; t < 64; t++) {
      const uint8 *tp = raw + t * 24;
      uint8 *out = g_ss_dmap_tiles + (s * 64 + t) * 64;
      for (int y = 0; y < 8; y++) {
        uint8 b0 = tp[y * 2], b1 = tp[y * 2 + 1], b2 = tp[16 + y];
        for (int x = 0; x < 8; x++)
          out[y * 8 + x] = ((b0 >> (7 - x)) & 1) | (((b1 >> (7 - x)) & 1) << 1) |
                           (((b2 >> (7 - x)) & 1) << 2);
      }
    }
  }
  g_ss_dmap_palace = palace;
}

static void SS_DmapDrawTile(uint32 *px, int stride, int x0, int y0, uint16 v) {
  int id = (v & 0x3ff) - 0x300;
  if (id < 0 || id >= 192) return;
  const uint8 *t = g_ss_dmap_tiles + id * 64;
  int p = (v >> 10) & 7;
  if (p < 2) p = 2;
  for (int y = 0; y < 8; y++) {
    int sy = (v & 0x8000) ? 7 - y : y;
    for (int x = 0; x < 8; x++) {
      int sx = (v & 0x4000) ? 7 - x : x;
      uint8 pix = t[sy * 8 + sx];
      if (pix) px[(y0 + y) * stride + x0 + x] = SS_Snes555(kPalette_PalaceMapBg[(p - 2) * 16 + pix]);
    }
  }
}

static uint16 SS_DmapQuad(uint16 e, bool visited, bool have_map) {
  if (visited || e == 0xB00)
    return (visited || have_map) ? e : 0xb00;
  if (!have_map)
    return 0xb00;
  return (e & 0x1000) ? (e & ~0x1c00) | 0xc00 : e + 0x400;
}

bool SS_RenderDungeonFloor(int palace, int floorIdx, uint32 *px) {
  if (palace < 0 || palace >= 14 || !SS_AssetsReady()) return false;
  MemBlk lay = FindInAssetArray(97, palace);
  MemBlk shapes = FindInAssetArray(98, palace);
  if (floorIdx < 0 || (size_t)(floorIdx + 1) * 25 > lay.size) return false;
  SS_EnsureDmapTiles(palace);
  bool have_map = (link_dungeon_map & (0x8000 >> palace)) != 0;
  memset(px, 0, 80 * 80 * 4);
  for (int i = 0; i < 25; i++) {
    uint8 v = lay.ptr[floorIdx * 25 + i];
    int visits = floorIdx, yv = 0x51;
    if (v != 0xf) {
      visits = save_dung_info[v] & 0xf;
      size_t count = 0, k = 0;
      while (k < lay.size && lay.ptr[k] != v)
        count += (lay.ptr[k++] != 0xf);
      if (count < shapes.size) yv = shapes.ptr[count];
      if (yv >= 186) yv = 0x51;
    }
    const uint16 *e = GetDungmapRoomShape(yv);
    int x0 = i % 5 * 16, y0 = i / 5 * 16;
    SS_DmapDrawTile(px, 80, x0, y0, SS_DmapQuad(e[0], (visits & 8) != 0, have_map));
    SS_DmapDrawTile(px, 80, x0 + 8, y0, SS_DmapQuad(e[1], (visits & 4) != 0, have_map));
    SS_DmapDrawTile(px, 80, x0, y0 + 8, SS_DmapQuad(e[2], (visits & 2) != 0, have_map));
    SS_DmapDrawTile(px, 80, x0 + 8, y0 + 8, SS_DmapQuad(e[3], (visits & 1) != 0, have_map));
  }
  return true;
}

bool SS_RenderMapIcons(int palace, uint32 *px) {
  if (palace < 0 || palace >= 14 || !SS_AssetsReady()) return false;
  SS_EnsureDmapTiles(palace);
  const uint8 *dot = g_ss_dmap_tiles + 0x34 * 64, *skull = g_ss_dmap_tiles + 0x31 * 64;
  memset(px, 0, 32 * 8 * 4);
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      uint8 pix = dot[y * 8 + x];
      if (pix)
        for (int p = 0; p < 3; p++)
          px[y * 32 + p * 8 + x] = SS_Snes555(kPalette_PalaceMapSpr[p * 7 + pix - 1]);
      pix = skull[y * 8 + x];
      if (pix) px[y * 32 + 24 + x] = SS_Snes555(main_palette_buffer[144 + pix]);
    }
  return true;
}

// ============ actions (UI thread -> game thread) ============

static volatile int g_pending_equip_slot;
static volatile int g_pending_assign_x_slot;
static volatile int g_pending_widescreen = -1;
static volatile int g_pending_hide_hud = -1;
static volatile int g_pending_controls_set;
static uint8 g_pending_controls[12];
static volatile uint32 g_pending_features_on, g_pending_features_off;
static bool g_ss_hud_refresh;

bool g_ss_hide_hud;
volatile int g_ss_capture_button = -1;

void SS_EquipSlot(int slot) {
  if (slot >= 1 && slot <= 20)
    g_pending_equip_slot = slot;
}

void SS_AssignSlotX(int slot) {
  if (slot >= 1 && slot <= 20)
    g_pending_assign_x_slot = slot;
}

void SS_SetWidescreen(bool on) { g_pending_widescreen = on ? 1 : 0; }

bool SS_IsWidescreen(void) {
  int pending = g_pending_widescreen;
  if (pending >= 0) return pending != 0;
  return g_zenv.ppu && g_zenv.ppu->extraLeftRight != 0;
}

void SS_SetHudHidden(bool hide) { g_pending_hide_hud = hide ? 1 : 0; }

bool SS_IsHudHidden(void) {
  // Auto-hide HUD on main screen when dual screen is active and showing second screen
  if (g_config.dual_screen && !g_config.show_hud_dual_screen && SecondScreenSDL_GetLayoutMode() != SS_LAYOUT_1SCREEN)
    return true;
  return g_ss_hide_hud;
}

void SS_RefreshHud(void) {
  if (main_module_index == 7 || main_module_index == 9 || main_module_index == 14) {
    if (SS_IsHudHidden()) {
      // Force NMI to write blank tiles every frame until HUD is hidden
      flag_update_hud_in_nmi++;
    } else {
      Hud_Rebuild();
    }
  }
}

uint32 SS_GetFeatures(void) {
  return (g_wanted_zelda_features | g_pending_features_on) & ~g_pending_features_off;
}

void SS_SetFeature(unsigned mask, bool on) {
  if (on) {
    g_pending_features_off &= ~(uint32)mask;
    g_pending_features_on |= (uint32)mask;
  } else {
    g_pending_features_on &= ~(uint32)mask;
    g_pending_features_off |= (uint32)mask;
  }
}

void SS_ArmButtonCapture(bool arm) { g_ss_capture_button = arm ? -2 : -1; }

int SS_GetCapturedButton(void) {
  int b = g_ss_capture_button;
  if (b >= 0) g_ss_capture_button = -1;
  return b;
}

void SS_GetGamepadControls(int *out) {
  uint8 btns[12];
  GamepadMap_GetControls(btns);
  for (int i = 0; i < 12; i++)
    out[i] = btns[i] == 0xff ? -1 : btns[i];
}

void SS_SetGamepadControls(const int *in) {
  for (int i = 0; i < 12; i++)
    g_pending_controls[i] = (in[i] >= 0 && in[i] < kGamepadBtn_Count) ? (uint8)in[i] : 0xff;
  g_pending_controls_set = 1;
}

void SecondScreen_RunFrameHook(void) {
  int ws = g_pending_widescreen;
  if (ws >= 0) {
    g_pending_widescreen = -1;
    extern void ZeldaSetWidescreen(bool enable);
    ZeldaSetWidescreen(ws != 0);
  }
  int hh = g_pending_hide_hud;
  if (hh >= 0) {
    g_pending_hide_hud = -1;
    g_ss_hide_hud = hh != 0;
    if (main_module_index == 7 || main_module_index == 9 || main_module_index == 14) {
      if (g_ss_hide_hud) {
        for (int i = 0; i < 165; i++)
          hud_tile_indices_buffer[i] = 0x207f;
        flag_update_hud_in_nmi++;
      } else {
        Hud_Rebuild();
      }
    }
  }
  if (g_pending_controls_set) {
    g_pending_controls_set = 0;
    GamepadMap_SetControls(g_pending_controls);
  }
  uint32 f_on = g_pending_features_on, f_off = g_pending_features_off;
  if (f_on | f_off) {
    g_pending_features_on &= ~f_on;
    g_pending_features_off &= ~f_off;
    uint32 old = g_wanted_zelda_features;
    uint32 nf = (old | f_on) & ~f_off;
    if (nf != old) {
      g_wanted_zelda_features = nf;
      g_config.features0 = nf;
      if ((nf ^ old) & kFeatures0_DimFlashes) {
        extern void ZeldaApplyDimFlashesPalette(bool enable);
        ZeldaApplyDimFlashesPalette((nf & kFeatures0_DimFlashes) != 0);
      }
      if ((nf ^ old) & (kFeatures0_SwitchLR | kFeatures0_ShowMaxItemsInYellow | kFeatures0_CarryMoreRupees))
        g_ss_hud_refresh = true;
    }
  }
  if (g_ss_hud_refresh && enhanced_features0 == g_wanted_zelda_features) {
    g_ss_hud_refresh = false;
    if (!g_ss_hide_hud && (main_module_index == 7 || main_module_index == 9 || main_module_index == 14))
      Hud_Rebuild();
  }
  bool in_gameplay = (main_module_index == 7 || main_module_index == 9) && submodule_index == 0;
  int slot = g_pending_equip_slot;
  if (slot) {
    g_pending_equip_slot = 0;
    if (in_gameplay) {
      hud_cur_item = hud_inventory_order[0] ? hud_inventory_order[slot - 1] : (uint8)slot;
      Hud_RefreshIcon();
    }
  }
  slot = g_pending_assign_x_slot;
  if (slot) {
    g_pending_assign_x_slot = 0;
    if (in_gameplay && (enhanced_features0 & kFeatures0_SwitchLR)) {
      hud_cur_item_x = hud_inventory_order[0] ? hud_inventory_order[slot - 1] : (uint8)slot;
      Hud_RefreshIcon();
    }
  }
}
