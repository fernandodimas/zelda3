#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Achievement types
typedef enum {
  ACH_TYPE_INSTANT,    // Triggers once when condition met
  ACH_TYPE_PROGRESS,   // Tracks progress toward a goal
  ACH_TYPE_CHALLENGE,  // Must be完成ed under special conditions
} AchievementType;

// Achievement categories
typedef enum {
  ACH_CAT_STORY,
  ACH_CAT_COLLECT,
  ACH_CAT_BOSS,
  ACH_CAT_CHALLENGE,
  ACH_CAT_EXPLORATION,
  ACH_CAT_SECRET,
} AchievementCategory;

// Condition check function pointer
typedef bool (*AchievementCheckFunc)(void);

// Achievement definition
typedef struct {
  uint32_t id;
  const char *title;
  const char *description;
  AchievementType type;
  AchievementCategory category;
  uint32_t points;
  AchievementCheckFunc check;  // Custom check function (can be NULL for progress-type)
  uint32_t progress_max;       // For ACH_TYPE_PROGRESS: target count
} AchievementDef;

// Saved state for one achievement
typedef struct {
  uint32_t id;
  bool unlocked;
  uint32_t progress;
} AchievementState;

// Save file header
typedef struct {
  uint32_t magic;           // 'ACHV'
  uint32_t version;         // 1
  uint32_t num_achievements;
} AchievementSaveHeader;

// Initialize achievement system
void Achievement_Init(void);

// Call once per frame (from ZeldaRunFrameInternal)
void Achievement_EvaluateFrame(void);

// Save/load
void Achievement_Save(void);
void Achievement_Load(void);

// UI helpers
int Achievement_GetTotal(void);
int Achievement_GetUnlockedCount(void);
int Achievement_GetTotalPoints(void);
int Achievement_GetUnlockedPoints(void);
const AchievementDef *Achievement_GetDef(int index);
const AchievementState *Achievement_GetState(uint32_t id);
bool Achievement_IsUnlocked(uint32_t id);

// Notification system
bool Achievement_HasNotification(void);
const char *Achievement_GetNotificationTitle(void);
const char *Achievement_GetNotificationDesc(void);
void Achievement_ClearNotification(void);
