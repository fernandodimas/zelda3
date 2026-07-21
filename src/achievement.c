#include "achievement.h"
#include "types.h"
#include "variables.h"
#include <stdio.h>
#include <string.h>

// ============ State ============
static AchievementDef g_achievements[];
static int g_num_achievements;
static AchievementState g_states[256];
static int g_num_states;

// Notification queue
static char g_notify_title[64];
static char g_notify_desc[128];
static bool g_has_notify;

// ============ RAM helpers ============
static inline uint8_t read8(uint16_t off) { return g_ram[off]; }
static inline uint16_t read16(uint16_t off) { return g_ram[off] | (g_ram[off+1] << 8); }
static inline bool check_bit(uint16_t off, uint8_t mask) { return (g_ram[off] & mask) != 0; }

// ============ Achievement check functions ============

// Story achievements
static bool ach_beat_game(void) {
  return read8(0xF3C5) >= 3;
}

static bool ach_got_master_sword(void) {
  return read8(0xF359) >= 1;
}

static bool ach_got_golden_power(void) {
  return read8(0xF359) >= 3;
}

// Collect achievements
static bool ach_4_bottles(void) {
  int count = 0;
  for (int i = 0; i < 4; i++)
    if (read8(0xF35C + i) != 0) count++;
  return count >= 4;
}

static bool ach_all_pendants(void) {
  return (read8(0xF374) & 7) == 7;
}

static bool ach_all_crystals(void) {
  return (read8(0xF37A) & 127) == 127;
}

static bool ach_all_medallions(void) {
  return check_bit(0xF347, 1) && check_bit(0xF348, 1) && check_bit(0xF349, 1);
}

static bool ach_mirrored_shield(void) {
  return read8(0xF35A) >= 2;
}

static bool ach_gold_armor(void) {
  return read8(0xF35B) >= 2;
}

static bool ach_titans_mitt(void) {
  return read8(0xF354) >= 2;
}

static bool ach_flippers(void) {
  return read8(0xF356) != 0;
}

static bool ach_moon_pearl(void) {
  return read8(0xF357) != 0;
}

static bool ach_magic_200(void) {
  return read8(0xF36E) >= 128; // 200 magic = 128 units (each unit = 1.5625)
}

static bool ach_master_sword_beams(void) {
  return check_bit(0xF379, 0x10);
}

static bool ach_20_hearts(void) {
  return read8(0xF36C) >= 40; // 20 hearts = 40 half-hearts
}

// Boss achievements
static bool ach_defeat_armos(void) {
  // Armos Knights - dungeon 0 boss, room 7
  return check_bit(0x402, 0x80) && read16(0x40C) == 0;
}

static bool ach_defeat_lanmola(void) {
  return check_bit(0x402, 0x80) && read16(0x40C) == 2;
}

static bool ach_defeat_moldorm(void) {
  return check_bit(0x402, 0x80) && read16(0x40C) == 4;
}

static bool ach_defeat_helmasaur(void) {
  return check_bit(0x402, 0x80) && read16(0x40C) == 6;
}

static bool ach_defeat_arrghus(void) {
  return check_bit(0x402, 0x80) && read16(0x40C) == 8;
}

static bool ach_defeat_mothula(void) {
  return check_bit(0x402, 0x80) && read16(0x40C) == 10;
}

static bool ach_defeat_blind(void) {
  return check_bit(0x402, 0x80) && read16(0x40C) == 12;
}

static bool ach_defeat_kholdstare(void) {
  return check_bit(0x402, 0x80) && read16(0x40C) == 14;
}

static bool ach_defeat_azerus(void) {
  return check_bit(0x402, 0x80) && read16(0x40C) == 16;
}

static bool ach_defeat_trinexx(void) {
  return check_bit(0x402, 0x80) && read16(0x40C) == 18;
}

static bool ach_defeat_agahnim(void) {
  // Agahnim 2 in pyramid
  return read8(0xF3C5) >= 2;
}

static bool ach_defeat_ganon(void) {
  return read8(0xF3C5) >= 3;
}

// Challenge achievements
static bool ach_no_death_run(void) {
  return ach_beat_game() && read16(0xF403) == 0;
}

static bool ach_rupee_999(void) {
  return read16(0xF362) >= 999;
}

static bool ach_hearths_full(void) {
  return read8(0xF36D) >= read8(0xF36C);
}

// Progress-type check functions (return current progress)
static bool ach_heart_pieces_progress(uint32_t *out) {
  *out = read8(0xF36B);
  return *out >= 24;
}

static bool ach_crystals_progress(uint32_t *out) {
  int count = 0;
  uint8_t c = read8(0xF37A);
  for (int i = 0; i < 7; i++) if (c & (1 << i)) count++;
  *out = count;
  return count >= 7;
}

static bool ach_pendants_progress(uint32_t *out) {
  *out = read8(0xF374) & 7;
  return (*out & 7) == 7;
}

// ============ Achievement definitions ============
static AchievementDef g_achievements[] = {
  // Story
  { 1,  "A Link to the Past",    "Return the Golden Land to its former glory",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 25, ach_beat_game },
  { 2,  "Master Sword",          "Obtain the Master Sword",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 10, ach_got_master_sword },
  { 3,  "Golden Power",          "Obtain the Golden Sword",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 10, ach_got_golden_power },

  // Collect - Items
  { 10, "Bottle Collector",      "Find all 4 bottles",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 15, ach_4_bottles },
  { 11, "Pendant Trio",          "Collect all 3 Pendants of Virtue",
    ACH_TYPE_PROGRESS, ACH_CAT_COLLECT, 15, NULL, 3 },
  { 12, "Crystal Collection",    "Gather all 7 Crystals",
    ACH_TYPE_PROGRESS, ACH_CAT_COLLECT, 20, NULL, 7 },
  { 13, "Medallion Master",      "Collect all 3 Medallions",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_all_medallions },
  { 14, "Mirrored Shield",       "Upgrade to the Mirror Shield",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_mirrored_shield },
  { 15, "Golden Armor",          "Upgrade to the Gold Mail",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_gold_armor },
  { 16, "Titans Mitt",           "Obtain the Titans Mitt",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_titans_mitt },
  { 17, "Flipper Fun",           "Obtain the Zora Flippers",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_flippers },
  { 18, "Moon Pearl",            "Obtain the Moon Pearl",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_moon_pearl },
  { 19, "Sorcerer's Protection", "Obtain the Cane of Byrna",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, NULL },
  { 20, "Dark World Cape",       "Obtain the Magic Cape",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, NULL },
  { 21, "Master Sword Beams",    "Unlock the Master Sword beam attack",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_master_sword_beams },
  { 22, "200 Magic",             "Upgrade to 200 Magic Power",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_magic_200 },
  { 23, "Heart Container Hunter","Collect 20 Heart Containers",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_20_hearts },

  // Bosses
  { 30, "Armos Defeated",        "Defeat the Armos Knights",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_armos },
  { 31, "Lanmola Defeated",      "Defeat the Lanmolas",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_lanmola },
  { 32, "Moldorm Defeated",      "Defeat Moldorm",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_moldorm },
  { 33, "Helmasaur Defeated",    "Defeat the Helmasaur King",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_helmasaur },
  { 34, "Arrghus Defeated",      "Defeat Arrghus",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_arrghus },
  { 35, "Mothula Defeated",      "Defeat Mothula",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_mothula },
  { 36, "Blind Defeated",        "Defeat Blind",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_blind },
  { 37, "Kholdstare Defeated",   "Defeat Kholdstare",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_kholdstare },
  { 38, "Azerus Defeated",       "Defeat Azerus",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_azerus },
  { 39, "Trinexx Defeated",      "Defeat Trinexx",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_defeat_trinexx },
  { 40, "Agahnim Defeated",      "Defeat Agahnim",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_defeat_agahnim },
  { 41, "Bested Ganon",          "Defeat Ganon",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 25, ach_defeat_ganon },

  // Challenge
  { 50, "No Death Run",           "Beat the game without dying",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 50, ach_no_death_run },
  { 51, "Rupee Hoarder",          "Accumulate 999 Rupees",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 10, ach_rupee_999 },
  { 52, "Full Health",            "Have full health",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 5, ach_hearths_full },

  // Exploration (progress-based)
  { 60, "Heart Piece Collector",  "Find 24 Heart Pieces",
    ACH_TYPE_PROGRESS, ACH_CAT_EXPLORATION, 15, NULL, 24 },
};

static int g_num_achievements = sizeof(g_achievements) / sizeof(g_achievements[0]);

// ============ Progress check wrapper ============
static bool check_progress_achievement(AchievementDef *def, AchievementState *state) {
  if (def->id == 11) return ach_pendants_progress(&state->progress);
  if (def->id == 12) return ach_crystals_progress(&state->progress);
  if (def->id == 60) return ach_heart_pieces_progress(&state->progress);
  return false;
}

// ============ Core functions ============

void Achievement_Init(void) {
  memset(g_states, 0, sizeof(g_states));
  g_num_states = 0;
  g_has_notify = false;
  Achievement_Load();
}

static AchievementState *find_state(uint32_t id) {
  for (int i = 0; i < g_num_states; i++)
    if (g_states[i].id == id) return &g_states[i];
  return NULL;
}

static AchievementState *get_or_create_state(uint32_t id) {
  AchievementState *s = find_state(id);
  if (s) return s;
  if (g_num_states >= 256) return NULL;
  s = &g_states[g_num_states++];
  s->id = id;
  s->unlocked = false;
  s->progress = 0;
  return s;
}

static void trigger_unlock(AchievementDef *def) {
  snprintf(g_notify_title, sizeof(g_notify_title), "%s", def->title);
  snprintf(g_notify_desc, sizeof(g_notify_desc), "%s", def->description);
  g_has_notify = true;
}

void Achievement_EvaluateFrame(void) {
  for (int i = 0; i < g_num_achievements; i++) {
    AchievementDef *def = &g_achievements[i];
    AchievementState *state = get_or_create_state(def->id);
    if (!state || state->unlocked) continue;

    bool triggered = false;

    if (def->type == ACH_TYPE_PROGRESS) {
      if (check_progress_achievement(def, state)) {
        state->progress = def->progress_max;
        triggered = true;
      }
    } else if (def->check) {
      triggered = def->check();
    }

    if (triggered) {
      state->unlocked = true;
      state->progress = def->progress_max;
      trigger_unlock(def);
    }
  }
}

// ============ Save/Load ============

void Achievement_Save(void) {
  FILE *f = fopen("saves/achievements.dat", "wb");
  if (!f) return;

  AchievementSaveHeader hdr;
  hdr.magic = 0x41434856; // 'ACHV'
  hdr.version = 1;
  hdr.num_achievements = g_num_states;

  fwrite(&hdr, sizeof(hdr), 1, f);
  fwrite(g_states, sizeof(AchievementState), g_num_states, f);
  fclose(f);
}

void Achievement_Load(void) {
  FILE *f = fopen("saves/achievements.dat", "rb");
  if (!f) return;

  AchievementSaveHeader hdr;
  if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.magic != 0x41434856 || hdr.version != 1) {
    fclose(f);
    return;
  }

  if (hdr.num_achievements > 256) hdr.num_achievements = 256;
  g_num_states = hdr.num_achievements;
  fread(g_states, sizeof(AchievementState), g_num_states, f);
  fclose(f);
}

// ============ UI helpers ============

int Achievement_GetTotal(void) { return g_num_achievements; }

int Achievement_GetUnlockedCount(void) {
  int c = 0;
  for (int i = 0; i < g_num_states; i++)
    if (g_states[i].unlocked) c++;
  return c;
}

int Achievement_GetTotalPoints(void) {
  int p = 0;
  for (int i = 0; i < g_num_achievements; i++)
    p += g_achievements[i].points;
  return p;
}

int Achievement_GetUnlockedPoints(void) {
  int p = 0;
  for (int i = 0; i < g_num_achievements; i++) {
    AchievementState *s = find_state(g_achievements[i].id);
    if (s && s->unlocked) p += g_achievements[i].points;
  }
  return p;
}

const AchievementDef *Achievement_GetDef(int index) {
  if (index < 0 || index >= g_num_achievements) return NULL;
  return &g_achievements[index];
}

const AchievementState *Achievement_GetState(uint32_t id) {
  return find_state(id);
}

bool Achievement_IsUnlocked(uint32_t id) {
  AchievementState *s = find_state(id);
  return s && s->unlocked;
}

// ============ Notification ============

bool Achievement_HasNotification(void) { return g_has_notify; }
const char *Achievement_GetNotificationTitle(void) { return g_notify_title; }
const char *Achievement_GetNotificationDesc(void) { return g_notify_desc; }
void Achievement_ClearNotification(void) { g_has_notify = false; }
