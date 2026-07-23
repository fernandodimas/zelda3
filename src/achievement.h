#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Achievement types
typedef enum {
  ACH_TYPE_INSTANT,    // Triggers once when condition met (edge-triggered)
  ACH_TYPE_PROGRESS,   // Tracks progress toward a goal
  ACH_TYPE_CHALLENGE,  // Must be completed under special conditions
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
  uint32_t version;         // 2
  uint32_t num_achievements;
  uint32_t checksum;        // Simple checksum of state data
} AchievementSaveHeader;

// Initialize achievement system
void Achievement_Init(void);

// Call once per frame (from ZeldaRunFrameInternal).
// Internally gates on gameplay module — no external SetInGameplay needed.
void Achievement_EvaluateFrame(void);

// Call after loading a save state to suppress false triggers for N frames.
void Achievement_NotifyStateLoaded(void);

// Save/load persistence
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
uint32_t Achievement_GetProgress(uint32_t id);

// Enable/disable
void Achievement_SetEnabled(bool enabled);
bool Achievement_IsEnabled(void);

// Reset all achievement progress
void Achievement_ResetAll(void);

// Notification system
bool Achievement_HasNotification(void);
uint32_t Achievement_GetNotificationId(void);
const char *Achievement_GetNotificationTitle(void);
const char *Achievement_GetNotificationDesc(void);
void Achievement_ClearNotification(void);

// Cleanup
void Achievement_Shutdown(void);
