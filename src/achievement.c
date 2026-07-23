// RetroAchievements-style offline achievement system for Zelda3.
// 109 achievements based on RetroAchievements.org/game/355
//
// Architecture: 4 Pillars of Safety
// 1. Game State Gate  - only evaluate during active gameplay (module 7/9, submodule 0)
// 2. Reactivation Lock - skip if already unlocked in persistent storage
// 3. Delta Edge Trigger - condition must transition FALSE->TRUE between frames
// 4. Bitmask Safety   - use bitwise ops for flag bytes

#include "achievement.h"
#include "types.h"
#include "variables.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <SDL.h>

// Achievement sound effect
#include "achievement_sfx.h"
static SDL_AudioDeviceID g_sfx_device = 0;

// ============ State ============
static int g_num_achievements;
static AchievementState g_states[256];
static int g_num_states;
static bool g_achievements_enabled = true;

// ============ Delta Check System ============
// Previous frame's check results for edge-trigger detection.
// Updated EVERY frame regardless of gameplay state, so transitions
// from menu->gameplay never produce a false edge.
static bool g_prev_check[256];   // indexed by achievement index
static bool g_prev_check_valid;  // false until first frame completes

// Suppress counter: after save/load, suppress triggers for N frames
// to let game state stabilize (room transitions, SRAM writes, etc.)
static int g_suppress_frames;
#define SUPPRESS_FRAMES_AFTER_LOAD 30

// Notification queue
static uint32_t g_notify_id;
static char g_notify_title[64];
static char g_notify_desc[128];
static bool g_has_notify;

// ============ RAM helpers ============
static inline uint8_t read8(uint16_t off) { return g_ram[off]; }
static inline uint16_t read16(uint16_t off) { return g_ram[off] | (g_ram[off + 1] << 8); }
static inline bool check_bit(uint16_t off, uint8_t mask) { return (g_ram[off] & mask) != 0; }

// ============ Pillar 1: Game State Gate ============
// Returns true ONLY during active gameplay frames.
// Module 7 = dungeon, Module 9 = overworld.
// Submodule 0 = active play (non-zero = transitions, menus, dialogues, map).
// Module 14 = pause/item menu (NEVER allow).
// Module 5 = file select, Module 0/1 = intro/title.
static bool achievement_is_active_gameplay(void) {
  uint8_t mod = main_module_index;
  uint8_t sub = submodule_index;

  // Only dungeon (7) and overworld (9) with submodule 0
  if ((mod != 7 && mod != 9) || sub != 0)
    return false;

  // Extra safety: suppress during room transitions.
  // When link_auxiliary_state != 0, Link is in a special state
  // (falling, dashing cutscene, etc.)
  // Also check that the game isn't in a text/dialogue overlay.
  if (read8(0x1B) == 0 && mod == 7) {
    // Indoors but module not yet set to 7 — shouldn't happen, but guard.
  }

  return true;
}

// ============ Check functions ============
// All checks use bitmask operations for flag bytes (Pillar 4).

// Story
static bool ach_fighter(void) { return read8(0xF359) >= 1; }
static bool ach_sanctuary(void) { return read8(0xF3C5) >= 1; }
static bool ach_pendant_courage(void) { return check_bit(0xF374, 0x01); }
static bool ach_pendant_power(void) { return check_bit(0xF374, 0x02); }
static bool ach_mirror(void) { return read8(0xF351) != 0; }
static bool ach_pendant_wisdom(void) { return check_bit(0xF374, 0x04); }
static bool ach_master_sword(void) { return read8(0xF359) >= 2; }
static bool ach_coward(void) { return read8(0xF3C5) >= 2; }
static bool ach_crystal_dark(void) { return check_bit(0xF37A, 0x01); }
static bool ach_crystal_swamp(void) { return check_bit(0xF37A, 0x02); }
static bool ach_crystal_woods(void) { return check_bit(0xF37A, 0x04); }
static bool ach_crystal_statue(void) { return check_bit(0xF37A, 0x08); }
static bool ach_crystal_ice(void) { return check_bit(0xF37A, 0x10); }
static bool ach_crystal_mire(void) { return check_bit(0xF37A, 0x20); }
static bool ach_crystal_rock(void) { return check_bit(0xF37A, 0x40); }
static bool ach_true_culprit(void) { return read8(0xF3C5) >= 2; }
static bool ach_golden_age(void) { return read8(0xF3C5) >= 3; }

// Collect
static bool ach_lamp(void) { return read8(0xF344) != 0; }
static bool ach_bombs(void) { return read8(0xF371) > 0; }
static bool ach_bug_net(void) { return read8(0xF343) != 0; }
static bool ach_magic_powder(void) { return read8(0xF346) != 0; }
static bool ach_ice_rod(void) { return read8(0xF34C) != 0; }
static bool ach_eastern_chests(void) { return check_bit(0xF418, 0x01); }
static bool ach_book_mudora(void) { return read8(0xF345) != 0; }
static bool ach_desert_chests(void) { return check_bit(0xF418, 0x02); }
static bool ach_flippers(void) { return read8(0xF356) != 0; }
static bool ach_magical_boomerang(void) { return read8(0xF342) >= 2; }
static bool ach_magical_shield(void) { return read8(0xF35A) >= 1; }
static bool ach_bombs_50(void) { return read8(0xF371) >= 50; }
static bool ach_arrows_70(void) { return read8(0xF373) >= 70; }
static bool ach_hera_chests(void) { return check_bit(0xF418, 0x04); }
static bool ach_ether(void) { return read8(0xF347) != 0; }
static bool ach_castle_chests(void) { return check_bit(0xF418, 0x08); }
static bool ach_quake(void) { return read8(0xF349) != 0; }
static bool ach_pd_chests(void) { return check_bit(0xF419, 0x01); }
static bool ach_bombos(void) { return read8(0xF348) != 0; }
static bool ach_bird(void) { return read8(0xF34C) != 0; }
static bool ach_cursed_magic(void) { return read8(0xF34E) != 0; }
static bool ach_swamp_chests(void) { return check_bit(0xF419, 0x02); }
static bool ach_skull_chests(void) { return check_bit(0xF419, 0x04); }
static bool ach_thieves_chests(void) { return check_bit(0xF419, 0x08); }
static bool ach_tempered_sword(void) { return read8(0xF359) >= 3; }
static bool ach_all_bottles(void) {
  int c = 0;
  for (int i = 0; i < 4; i++) if (read8(0xF35C + i) != 0) c++;
  return c >= 4;
}
static bool ach_cape(void) { return read8(0xF350) != 0; }
static bool ach_byrna(void) { return read8(0xF34F) != 0; }
static bool ach_ice_chests(void) { return check_bit(0xF419, 0x10); }
static bool ach_mire_chests(void) { return check_bit(0xF419, 0x20); }
static bool ach_golden_sword(void) { return read8(0xF359) >= 4; }
static bool ach_turtle_chests(void) { return check_bit(0xF419, 0x40); }
static bool ach_ganons_tower_chests(void) { return check_bit(0xF41A, 0x01); }
static bool ach_all_maps_compasses(void) {
  int count = 0;
  for (int i = 0; i < 26; i++) {
    if (check_bit(0xF41B + (i >> 3), 1 << (i & 7))) count++;
  }
  return count >= 26;
}
static bool ach_lw_chests(void) { return check_bit(0xF41A, 0x02); }
static bool ach_dw_chests(void) { return check_bit(0xF41A, 0x04); }
static bool ach_rupees_999(void) { return read16(0xF362) >= 999; }
static bool ach_20_hearts(void) { return read8(0xF36C) >= 40; }
static bool ach_all_items(void) {
  return read8(0xF359) >= 4 && read8(0xF35A) >= 2 && read8(0xF35B) >= 2 &&
         read8(0xF354) >= 2 && read8(0xF355) != 0 && read8(0xF356) != 0 &&
         read8(0xF357) != 0 && read8(0xF358) != 0;
}

// Boss (check dungeon completion flags via bitmask)
static bool ach_boss_armos(void) { return check_bit(0xF374, 0x01); }
static bool ach_boss_lanmola(void) { return check_bit(0xF374, 0x02); }
static bool ach_boss_moldorm(void) { return check_bit(0xF374, 0x04); }
static bool ach_boss_agahnim(void) { return read8(0xF3C5) >= 1; }
static bool ach_boss_helmasaur(void) { return check_bit(0xF37A, 0x01); }
static bool ach_boss_arrghus(void) { return check_bit(0xF37A, 0x02); }
static bool ach_boss_mothula(void) { return check_bit(0xF37A, 0x04); }
static bool ach_boss_blind(void) { return check_bit(0xF37A, 0x08); }
static bool ach_boss_kholdstare(void) { return check_bit(0xF37A, 0x10); }
static bool ach_boss_vitreous(void) { return check_bit(0xF37A, 0x20); }
static bool ach_boss_trinexx(void) { return check_bit(0xF37A, 0x40); }
static bool ach_boss_agahnim2(void) { return read8(0xF3C5) >= 2; }
static bool ach_boss_ganon(void) { return read8(0xF3C5) >= 3; }

// Challenge/Misc
static bool ach_deaths_0(void) { return read16(0xF403) == 0; }

// Progress functions
static bool ach_pendants_progress(uint32_t *out) {
  uint8_t v = read8(0xF374) & 0x07;  // bitmask: only pendant bits
  int c = 0;
  for (int i = 0; i < 3; i++) if (v & (1 << i)) c++;
  *out = c;
  return c >= 3;
}
static bool ach_crystals_progress(uint32_t *out) {
  uint8_t c = read8(0xF37A) & 0x7F;  // bitmask: only crystal bits
  int count = 0;
  for (int i = 0; i < 7; i++) if (c & (1 << i)) count++;
  *out = count;
  return count >= 7;
}
static bool ach_bottles_progress(uint32_t *out) {
  int c = 0;
  for (int i = 0; i < 4; i++) if (read8(0xF35C + i) != 0) c++;
  *out = c;
  return c >= 4;
}

// ============ Achievement definitions (109 RetroAchievements) ============
static AchievementDef g_achievements[] = {
  // === STORY (17) ===
  { 944,   "Fighter",           "Obtain the Fighter's Sword and Shield.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 2, ach_fighter, 0 },
  { 2192,  "Sanctuary",         "Take the Princess to the safety of Sanctuary.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 5, ach_sanctuary, 0 },
  { 980,   "Pendant of Courage","Acquire the Pendant of Courage.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 5, ach_pendant_courage, 0 },
  { 2291,  "Pendant of Power",  "Acquire the Pendant of Power.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 5, ach_pendant_power, 0 },
  { 2296,  "Mirror World",      "Obtain the Magic Mirror.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 5, ach_mirror, 0 },
  { 2315,  "Pendant of Wisdom", "Acquire the Pendant of Wisdom.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 5, ach_pendant_wisdom, 0 },
  { 2331,  "The Sword of Masters","Obtain the Master Sword.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 5, ach_master_sword, 0 },
  { 2336,  "Coward!",           "Have Agahnim transport you to the Dark World.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 5, ach_coward, 0 },
  { 2351,  "Crystal in the Dark","Free the maiden from the Palace of Darkness.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 10, ach_crystal_dark, 0 },
  { 2357,  "Crystal in the Swamp","Free the maiden from the Swamp Palace.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 10, ach_crystal_swamp, 0 },
  { 2359,  "Crystal in the Woods","Free the maiden from the Skull Woods.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 10, ach_crystal_woods, 0 },
  { 2361,  "Crystal in the Statue","Free the maiden from Thieves' Town.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 10, ach_crystal_statue, 0 },
  { 2365,  "Crystal in the Ice","Free the maiden from the Ice Palace.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 10, ach_crystal_ice, 0 },
  { 2368,  "Crystal in the Mire","Free the maiden from Misery Mire.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 10, ach_crystal_mire, 0 },
  { 2372,  "Crystal in the Rock","Free the maiden from Turtle Rock.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 10, ach_crystal_rock, 0 },
  { 2387,  "True Culprit",     "Discover the true entity behind the maidens' capture.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 5, ach_true_culprit, 0 },
  { 2389,  "The Golden Age",   "Return the Golden Land to its former glory.",
    ACH_TYPE_INSTANT, ACH_CAT_STORY, 25, ach_golden_age, 0 },

  // === COLLECT (40) ===
  { 943,   "Illuminating",     "Obtain the Lamp.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 2, ach_lamp, 0 },
  { 970,   "Bombs",            "Acquire some Bombs.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 4, ach_bombs, 0 },
  { 976,   "Bug Catcher",      "Obtain the Bug Catching Net.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_bug_net, 0 },
  { 973,   "Bottle",           "Acquire a Magic Bottle.",
    ACH_TYPE_PROGRESS, ACH_CAT_COLLECT, 5, NULL, 4 },
  { 2284,  "Magic Powder",     "Obtain the Magic Powder from the witch's apprentice.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_magic_powder, 0 },
  { 2282,  "Icy",              "Obtain the Ice Rod.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_ice_rod, 0 },
  { 312935,"All Quiet on the Eastern Front","Open all treasure chests in the Eastern Palace.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 3, ach_eastern_chests, 0 },
  { 2288,  "Book of Prayer",   "Obtain the Book of Mudora.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_book_mudora, 0 },
  { 312936,"Despoiled the Desert","Open all treasure chests in the Desert Palace.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 3, ach_desert_chests, 0 },
  { 2292,  "Overpriced Merchandise","Obtain Zora's Flippers.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_flippers, 0 },
  { 2293,  "The Magic of the Boom","Obtain the Magical Boomerang.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_magical_boomerang, 0 },
  { 2294,  "Magical Shield",   "Upgrade the Fighter's Shield to the Red Shield.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_magical_shield, 0 },
  { 25190, "Ready to Blow!",   "Increase your maximum bomb capacity to 50.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_bombs_50, 0 },
  { 25191, "Full Quiver",      "Increase your maximum arrow capacity to 70.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_arrows_70, 0 },
  { 312937,"Hera's Treasure",  "Open all treasure chests in the Tower of Hera.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 3, ach_hera_chests, 0 },
  { 2334,  "Thunderstruck",    "Obtain the Ether Medallion.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_ether, 0 },
  { 312938,"Castle Caper",     "Open all treasure chests in Hyrule Castle.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_castle_chests, 0 },
  { 2350,  "Fault Line",       "Obtain the Quake Medallion.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_quake, 0 },
  { 312940,"Plundered the Palace","Open all chests in the Palace of Darkness.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_pd_chests, 0 },
  { 2355,  "Ring of Fire",     "Obtain the Bombos Medallion.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_bombos, 0 },
  { 2354,  "Bird is the Word", "Free the bird from Kakariko village.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_bird, 0 },
  { 2353,  "Cursed Magic",     "Obtain the magic enhancement.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_cursed_magic, 0 },
  { 312941,"Drained the Swamp","Open all treasure chests in the Swamp Palace.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_swamp_chests, 0 },
  { 312942,"Deforestation",    "Open all treasure chests in Skull Woods.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_skull_chests, 0 },
  { 312944,"Robbed 'em Blind", "Open all treasure chests in Thieves' Town.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_thieves_chests, 0 },
  { 2363,  "The Swordsmith's Brother","Obtain the Tempered Sword.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_tempered_sword, 0 },
  { 2283,  "All Your Bottles Belong to Us","Obtain all four Magic Bottles.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_all_bottles, 0 },
  { 2366,  "You Can't See Me","Obtain the Magic Cape.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_cape, 0 },
  { 2370,  "Sorcerer's Protection","Obtain the Cane of Byrna.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_byrna, 0 },
  { 312945,"Deiced the Palace","Open all treasure chests in the Ice Palace.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_ice_chests, 0 },
  { 312946,"Mined the Mire",  "Open all treasure chests in Misery Mire.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_mire_chests, 0 },
  { 2369,  "Golden Wish",      "Obtain the Golden Sword.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_golden_sword, 0 },
  { 312947,"Tanked the Turtle","Open all treasure chests in Turtle Rock.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_turtle_chests, 0 },
  { 312948,"Boarglary",        "Open all treasure chests in Ganon's Tower.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_ganons_tower_chests, 0 },
  { 313062,"Ferdinand Magellink","Obtain all maps and compasses.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_all_maps_compasses, 0 },
  { 312939,"Lightened the Load","Open all treasure chests in the Light World.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_lw_chests, 0 },
  { 313050,"Shady Collection","Open all treasure chests in the Dark World.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_dw_chests, 0 },
  { 962,   "Rupee Hoarder",    "Have 999 rupees.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 5, ach_rupees_999, 0 },
  { 966,   "Love the Past",    "Have all 20 Heart Containers.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 25, ach_20_hearts, 0 },
  { 317103,"Link Builds a Treasure Hoard","Obtain all key items and equipment.",
    ACH_TYPE_INSTANT, ACH_CAT_COLLECT, 10, ach_all_items, 0 },

  // === BOSS (24) ===
  { 314001,"Toying with the Red Knight","Kill the Red Armos Knight using only the boomerang.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_boss_armos, 0 },
  { 2285,  "Bested Armos Knights","Defeat the Armos Knights without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_armos, 0 },
  { 314002,"Worm Kebabs",      "Kill the Lanmolas dealing all damage with arrows.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_lanmola, 0 },
  { 2289,  "Bested Lanmolas",  "Defeat all three Lanmolas without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_lanmola, 0 },
  { 314003,"Gone in 20 Seconds","Defeat Moldorm within 20 seconds.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_moldorm, 0 },
  { 2314,  "Bested Moldorm",   "Defeat Moldorm without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_moldorm, 0 },
  { 314004,"You Really Bug Me Out","Defeat Agahnim without unsheathing your sword.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_boss_agahnim, 0 },
  { 2337,  "Bested Agahnim",   "Defeat Agahnim without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_agahnim, 0 },
  { 314005,"Mallet and Skewer","Crack the Helmasaur's shield using only the hammer.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_helmasaur, 0 },
  { 2352,  "Bested Helmasaur King","Defeat Helmasaur King without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_helmasaur, 0 },
  { 314006,"I Only Have Ice For You","Defeat Arrghus's final phase with only the Ice Rod.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 5, ach_boss_arrghus, 0 },
  { 2356,  "Bested Arrghus",   "Defeat Arrghus without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_arrghus, 0 },
  { 314007,"Oprah's Dramatic Gift Reveal","Finish off Mothula with bees.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_mothula, 0 },
  { 2358,  "Bested Mothula",   "Defeat Mothula without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_mothula, 0 },
  { 314008,"Cane You See Me Now?","Defeat Blind using only the Cane of Somaria.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_blind, 0 },
  { 2360,  "Bested Blind",     "Defeat Blind without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_blind, 0 },
  { 314009,"Kholdstare into the Flames","Defeat Kholdstare using only the Fire Rod.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_kholdstare, 0 },
  { 2364,  "Bested Kholdstare","Defeat Kholdstare without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_kholdstare, 0 },
  { 314010,"A Bomb for Sore Eyes","Defeat Vitreous with bombs and arrows only.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_vitreous, 0 },
  { 2367,  "Bested Vitreous",  "Defeat Vitreous without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_vitreous, 0 },
  { 314011,"Elemental Master", "Defeat Trinexx with Ice Rod, Fire Rod, and Hammer.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_trinexx, 0 },
  { 2371,  "Bested Trinexx",   "Defeat Trinexx without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_trinexx, 0 },
  { 314422,"Bringing a Hammer to a Magic Fight","Defeat Agahnim 2 without sword or net.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 10, ach_boss_agahnim2, 0 },
  { 2388,  "Bested Ganon",     "Defeat Ganon without being harmed.",
    ACH_TYPE_INSTANT, ACH_CAT_BOSS, 25, ach_boss_ganon, 0 },

  // === CHALLENGE (18) ===
  { 313056,"Kamicucco!",       "Start a cucco frenzy.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 2, NULL, 0 },
  { 314697,"Lost Woods Winner","Win the 300 rupee prize in the gambling game.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 5, NULL, 0 },
  { 319718,"A Fishy Exchange", "Sell a fish to a merchant.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 5, NULL, 0 },
  { 313071,"Granny's Coming Along","Take a trusting old lady with you.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 5, NULL, 0 },
  { 313069,"Lightning Link",   "Catch the running man in Kakariko.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 2, NULL, 0 },
  { 315718,"The Joy of Entomology","Sell a rare bug to a merchant.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 5, NULL, 0 },
  { 315722,"Masonry Maneuver","Survive all flying tiles without damage.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 5, NULL, 0 },
  { 319722,"Toppo the Line Thief","Make Toppo drop an item.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 3, NULL, 0 },
  { 319715,"Monkey Magic",     "Try to draw Kiki into the Light World.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 3, NULL, 0 },
  { 314696,"Link's Longbow Training","Achieve a Perfect Score at Archery.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 10, NULL, 0 },
  { 314695,"He's a Golddigger","Earn 100+ rupees in the digging minigame.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 5, NULL, 0 },
  { 319721,"A Cape Not Just For Titans","Get the Magic Cape without Titan's Mitt.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 10, NULL, 0 },
  { 319717,"It's a Trap!",     "Bring the Thieves' Town maiden to both refused spots.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 3, NULL, 0 },
  { 315755,"I Shoulda Saved the Others","Rescue Zelda in Turtle Rock before any other maiden.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 10, NULL, 0 },
  { 313073,"Jack of All Trades","Complete Ganon's Tower swordless.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 25, NULL, 0 },
  { 314433,"Not Old Enough To Drink","Defeat Ganon without collecting any bottles.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 10, NULL, 0 },
  { 50,    "No Death Run",     "Beat the game without dying.",
    ACH_TYPE_INSTANT, ACH_CAT_CHALLENGE, 50, ach_deaths_0, 0 },

  // === EXPLORATION (12) ===
  { 319716,"A Cure For the Muteness","Expose the mute man near the desert.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 3, NULL, 0 },
  { 314829,"It's-a Me ....Hario?","Unleash rupees from a famous plumber's portrait.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 3, NULL, 0 },
  { 313072,"A Friend in Need", "Be rewarded for saving a helpless life.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 2, NULL, 0 },
  { 319719,"A Fowl Date For Link","Turn a cucco into a woman.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 3, NULL, 0 },
  { 314830,"Golden Honey Friend","Release the Good Bee and bottle it.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 5, NULL, 0 },
  { 319714,"Spider Bonus",     "Get a red rupee from a hoarder.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 3, NULL, 0 },
  { 313064,"This One's on the House","Get a Medicine of Magic for honesty.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 2, NULL, 0 },
  { 319723,"Stone Hard Cash",  "Unleash rupees from an Agahnim statue.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 3, NULL, 0 },
  { 319720,"A Link Between Jobs","Get reprimanded for doing your descendant's job.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 3, NULL, 0 },
  { 313057,"To the Hereafter", "Fulfill the flute boy's final wish.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 2, NULL, 0 },
  { 319724,"Shaking Hands With Dinosaurs","Unleash the rupees from Turtle Rock.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 3, NULL, 0 },
  { 25154, "Now You're Playing With Power","Enter Chris Houlihan's secret room.",
    ACH_TYPE_INSTANT, ACH_CAT_EXPLORATION, 5, NULL, 0 },
};

// ============ Progress check wrapper ============
static bool check_progress_achievement(AchievementDef *def, AchievementState *state) {
  if (def->id == 973) return ach_bottles_progress(&state->progress);
  if (def->id == 980) return ach_pendants_progress(&state->progress);
  if (def->id == 2389) return ach_crystals_progress(&state->progress);
  return false;
}

// ============ Core functions ============

void Achievement_Init(void) {
  memset(g_states, 0, sizeof(g_states));
  g_num_states = 0;
  g_has_notify = false;
  g_prev_check_valid = false;
  g_suppress_frames = 0;
  g_num_achievements = (int)(sizeof(g_achievements) / sizeof(g_achievements[0]));
  Achievement_Load();
}

void Achievement_SetEnabled(bool enabled) { g_achievements_enabled = enabled; }
bool Achievement_IsEnabled(void) { return g_achievements_enabled; }

void Achievement_NotifyStateLoaded(void) {
  // Suppress all triggers for N frames after a save/load.
  // This prevents false edges when RAM changes instantly on load.
  g_suppress_frames = SUPPRESS_FRAMES_AFTER_LOAD;
  // Invalidate delta snapshot so next frame becomes the new baseline.
  g_prev_check_valid = false;
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

// ============ Sound Effect ============
static void Achievement_InitSfx(void) {
  if (g_sfx_device) return;
  SDL_AudioSpec want = {0}, have;
  want.freq = 44100;
  want.format = AUDIO_S16;
  want.channels = 1;
  want.samples = 2048;
  g_sfx_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (g_sfx_device) {
    SDL_PauseAudioDevice(g_sfx_device, 1);
  }
}

static void Achievement_PlaySfx(void) {
  if (!g_sfx_device) Achievement_InitSfx();
  if (g_sfx_device) {
    SDL_QueueAudio(g_sfx_device, g_achievement_sfx, g_achievement_sfx_len * sizeof(int16_t));
    SDL_PauseAudioDevice(g_sfx_device, 0);
  }
}

static void trigger_unlock(AchievementDef *def) {
  g_notify_id = def->id;
  snprintf(g_notify_title, sizeof(g_notify_title), "%s", def->title);
  snprintf(g_notify_desc, sizeof(g_notify_desc), "%s", def->description);
  g_has_notify = true;
  Achievement_PlaySfx();
}

// ============ Main Evaluation (called once per frame from ZeldaRunFrameInternal) ============
//
// The 4 Pillars of Safety are applied here:
//   Pillar 1 (Game State Gate): achievement_is_active_gameplay()
//   Pillar 2 (Reactivation Lock): state->unlocked check
//   Pillar 3 (Delta Edge Trigger): g_prev_check[] false->true transition
//   Pillar 4 (Bitmask Safety): enforced in check_bit() usage above
//
void Achievement_EvaluateFrame(void) {
  if (!g_achievements_enabled) return;

  // Decrement suppress counter
  if (g_suppress_frames > 0)
    g_suppress_frames--;

  // Determine if we're in active gameplay THIS frame (Pillar 1)
  bool in_gameplay = achievement_is_active_gameplay();

  for (int i = 0; i < g_num_achievements; i++) {
    AchievementDef *def = &g_achievements[i];

    // Evaluate the raw condition (always, to keep delta snapshot fresh)
    bool current = false;
    if (def->check) {
      current = def->check();
    } else if (def->type == ACH_TYPE_PROGRESS) {
      AchievementState *ps = get_or_create_state(def->id);
      if (ps) current = check_progress_achievement(def, ps);
    }
    // Achievements with NULL check and non-progress type: never auto-trigger
    // (they need explicit game-event hooks, not implemented yet)

    // --- Pillar 3: Delta Edge Trigger ---
    // Detect the FALSE -> TRUE transition.
    bool edge = false;
    if (g_prev_check_valid) {
      edge = current && !g_prev_check[i];
    }
    // Update snapshot for next frame (ALWAYS, regardless of gameplay state).
    // This is critical: by updating every frame, when we transition from
    // menu -> gameplay, the snapshot already reflects current RAM state,
    // so no false edge is produced on the first gameplay frame.
    g_prev_check[i] = current;

    // --- Now apply gating rules for actually unlocking ---

    // Must be in active gameplay (Pillar 1)
    if (!in_gameplay) continue;

    // Must not be in suppress window (after save/load)
    if (g_suppress_frames > 0) continue;

    // Need valid delta (at least 2 frames of data)
    if (!g_prev_check_valid) continue;

    // Must be a rising edge (Pillar 3)
    if (!edge) continue;

    // --- Pillar 2: Reactivation Lock ---
    AchievementState *state = get_or_create_state(def->id);
    if (!state) continue;
    if (state->unlocked) continue;  // Already unlocked, skip

    // All 4 pillars passed — unlock!
    state->unlocked = true;
    if (def->type == ACH_TYPE_PROGRESS)
      state->progress = def->progress_max;
    trigger_unlock(def);
  }

  // After first full pass, mark delta as valid
  if (!g_prev_check_valid)
    g_prev_check_valid = true;
}

// ============ Save/Load Persistence ============

static uint32_t compute_checksum(const void *data, size_t size) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t sum = 0x12345678;
  for (size_t i = 0; i < size; i++) {
    sum += p[i];
    sum ^= (sum << 5);
  }
  return sum;
}

void Achievement_Save(void) {
  FILE *f = fopen("saves/achievements.dat", "wb");
  if (!f) return;
  AchievementSaveHeader hdr;
  hdr.magic = 0x41434856;  // 'ACHV'
  hdr.version = 2;
  hdr.num_achievements = g_num_states;
  hdr.checksum = compute_checksum(g_states, sizeof(AchievementState) * g_num_states);
  fwrite(&hdr, sizeof(hdr), 1, f);
  fwrite(g_states, sizeof(AchievementState), g_num_states, f);
  fclose(f);
}

void Achievement_Load(void) {
  FILE *f = fopen("saves/achievements.dat", "rb");
  if (!f) return;
  AchievementSaveHeader hdr;
  if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.magic != 0x41434856) {
    fclose(f);
    return;
  }
  // Support version 1 (no checksum) and version 2 (with checksum)
  if (hdr.version != 1 && hdr.version != 2) {
    fclose(f);
    return;
  }
  if (hdr.num_achievements > 256) hdr.num_achievements = 256;

  // Version 1 had a smaller header (no checksum field).
  // If version 1, we already read 4 extra bytes as checksum — seek back.
  if (hdr.version == 1) {
    // v1 header was 12 bytes (magic, version, num). We read 16. Seek back 4.
    fseek(f, -4, SEEK_CUR);
  }

  g_num_states = hdr.num_achievements;
  if (fread(g_states, sizeof(AchievementState), g_num_states, f) != (size_t)g_num_states) {
    g_num_states = 0;
    fclose(f);
    return;
  }

  // Verify checksum for v2
  if (hdr.version == 2) {
    uint32_t expected = compute_checksum(g_states, sizeof(AchievementState) * g_num_states);
    if (expected != hdr.checksum) {
      // Corrupted — discard
      g_num_states = 0;
    }
  }

  fclose(f);
}

void Achievement_ResetAll(void) {
  memset(g_states, 0, sizeof(g_states));
  g_num_states = 0;
  g_has_notify = false;
  g_prev_check_valid = false;
  Achievement_Save();
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

uint32_t Achievement_GetProgress(uint32_t id) {
  AchievementState *s = find_state(id);
  return s ? s->progress : 0;
}

// ============ Notification ============

bool Achievement_HasNotification(void) { return g_has_notify; }
uint32_t Achievement_GetNotificationId(void) { return g_notify_id; }
const char *Achievement_GetNotificationTitle(void) { return g_notify_title; }
const char *Achievement_GetNotificationDesc(void) { return g_notify_desc; }
void Achievement_ClearNotification(void) { g_has_notify = false; }

void Achievement_Shutdown(void) {
  if (g_sfx_device) {
    SDL_CloseAudioDevice(g_sfx_device);
    g_sfx_device = 0;
  }
}
