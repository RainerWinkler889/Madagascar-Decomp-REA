#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// Madagascar (PC) - Common Types & Engine Constants
// ============================================================================

// Original Executable Base Address: 0x00400000
#define ORIGINAL_IMAGE_BASE 0x00400000

#include "rw_sdk.h"

// Global Application State (Addresses in Game.exe .data)
extern HWND g_hMainWindow;        // 0x0062F138
extern HWND g_hDeviceWindow;      // 0x0062A5DC
extern bool g_bQuit;              // 0x0062F140
extern bool g_bActive;            // 0x0062F141
extern bool g_bFullscreen;        // 0x0062A5B0
extern uint32_t g_dwScreenWidth;  // 0x0062A5C8
extern uint32_t g_dwScreenHeight; // 0x0062A5CC
extern void* s_pStringDict;       // 0x0062A5AC: Localized String Dictionary

// ============================================================================
// Core Subsystem Prototypes (Synchronized across main, video, game_loop)
// ============================================================================

// 0x00462AA0: Initialize RenderWare Graphics Engine (D3D8)
int32_t RwEngine_Initialize(int param1, void** pFuncTable, uint32_t flags, uint32_t* pModeParams, uint32_t depth, HWND hWnd);

// 0x00462BF0: Select Video Mode
int32_t RwSelectVideoMode(int32_t* pWidth, int32_t* pHeight, int32_t reqDepth, uint32_t reqFlags);

// 0x00462880: Register Camera & Raster Image Decoders (PNG, BMP, RAS, TIF)
int32_t RwCreateCameraAndRasters();

// 0x0044E8E0: Create Default Camera & Rasters (ZBuffer, FrameBuffer, FOV)
RwCamera* RwCreateDefaultCameraAndRasters(int32_t width, int32_t height, int32_t depth);

// 0x00461F70: Post-Engine Startup (Camera setup, RWS message pipeline init)
void InitPostEngineStart(uint32_t width, uint32_t height, uint32_t depth);

// 0x00432CB0: Initialize Game World Manager (Queries active world count from vector 0x0061F878)
int32_t InitGameWorldManager();

// 0x004627E0: Load Level Stream (.stream file loader, sync or async)
bool LoadLevelStream(const char* streamPath, int flags);

// 0x004620D0: Main Game Loop Frame Tick (Heartbeat PowerPipe Tick)
void MainGameTick();

// 0x00460D50: Game shutdown & level unload
void ShutdownGame();

// 0x00462BC0: RenderWare engine shutdown
void ShutdownRenderWare();

// 0x00463360: Local file system & drive root path initializer
int32_t SetupFileSystemDrives();

// 0x00460C10: Resolve asset path / global resolver context (0x0062A1F8)
void* ResolveAssetPath();

// 0x004634E0: Initialize Save & Settings Manager ("Activision\Madagascar\Save")
void* InitSettingsManager();

// 0x00519D20: Read integer from Registry / Settings
int32_t GetSettingInt(const char* key, int32_t defaultValue);

// 0x00519CB0: Read string from Registry / Settings
void GetSettingString(const char* key, const char* defaultVal, char* outBuffer, uint32_t maxLen);

// 0x00519C90: Save integer to Registry / Settings
void SetSettingInt(const char* key, int32_t value);

// 0x004025F0: Get rumble / force-feedback state
int32_t GetRumbleSetting();

// 0x0051C940: Flush settings to disk/registry
void FlushSettings();

// 0x0051A430: Look up localized string by ID ("ID_WINNAME" -> "Madagascar")
const char* GetLocalizedString(void* pDict, const char* strId);

// 0x0051A450: Free string dictionary
void FreeStringDict(void* pDict);

// 0x0051A1A0: Input subsystem cleanup
void CleanupInput();
