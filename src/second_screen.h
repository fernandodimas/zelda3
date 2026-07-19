#pragma once
#include <stdbool.h>
#include <stdint.h>

int  SS_GetLinkX(void);
int  SS_GetLinkY(void);
int  SS_GetArea(void);
int  SS_GetModule(void);
bool SS_IsIndoors(void);
void SS_ReadSram(uint8 *out, int n);
int  SS_GetEquippedSlot(void);
int  SS_GetEquippedSlotX(void);
int  SS_GetDungeon(void);
void SS_ReadDungFlags(uint8 *out, int n);
bool SS_GetIndoorExit(int *out);
int  SS_GetDungeonLayout(int palace, uint8 *out, int cap);

bool SS_RenderIconSheet(uint32 *px);
bool SS_RenderGlyphSheet(uint32 *px);
bool SS_RenderLetterSheet(uint32 *px);
bool SS_RenderWorldMap(uint32 *px, bool dark);
bool SS_RenderLinkFace(uint32 *px, int chunk);
bool SS_RenderDungeonFloor(int palace, int floorIdx, uint32 *px);
bool SS_RenderMapIcons(int palace, uint32 *px);

void SS_EquipSlot(int slot);
void SS_AssignSlotX(int slot);
void SS_SetWidescreen(bool on);
bool SS_IsWidescreen(void);
void SS_SetHudHidden(bool hide);
bool SS_IsHudHidden(void);
uint32 SS_GetFeatures(void);
void SS_SetFeature(unsigned mask, bool on);
void SS_ArmButtonCapture(bool arm);
int  SS_GetCapturedButton(void);
void SS_GetGamepadControls(int *out12);
void SS_SetGamepadControls(const int *in12);

void SecondScreen_RunFrameHook(void);

extern bool g_ss_hide_hud;
