/**
 * @file audio_and_frame.cpp
 * @brief Madagascar (PC, 2005) - Audio Backend (.krn Streamer) & Frame Pipeline
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 */

#include "audio_and_frame.h"
#include "rws_core.h"
#include "script_and_ui.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

// ============================================================================
// Internal State Variables in Game.exe .data
// ============================================================================

// Global Active Camera in Game.exe .data (0x0062A524)
static RwCamera* s_pActiveCamera = nullptr; // 0x0062A524

// Engine Pause State (0x00606C68)
static int32_t s_nEnginePaused = 0; // 0x00606C68

// Current active level ID (-1 = uninitialized, 0..15)
static int32_t s_nActiveLevelID = -1; // 0x00606A38

// Requested target level ID
static int32_t s_nTargetLevelID = 0; // 0x0062A2F8

// Title / Loading Screen flag
static int32_t s_nStartupState = 0; // 0x006221B4

// Frame render clear flag
static uint8_t s_bFrameNeedsClear = 1; // 0x0062A5D2
static uint8_t s_bFrameCleared = 0;    // 0x0062A5D8

// Global audio mixer state block
struct CAudioMixer {
    uint8_t  masterVolume;   // +0x00: Master Volume (0..255)
    uint8_t  sfxVolume;      // +0x01: Sound Effects Volume (0..255)
    uint8_t  musicVolume;    // +0x02: Music Volume (0..255)
    uint8_t  voiceVolume;    // +0x03: Dialogue/Voice Volume (0..255)
    uint8_t  pad[0x6C];      // +0x04..0x6F
    float    fPanDamping;    // +0x70: 0.5f (0x3F000000)
    uint32_t bEnabled;       // +0x74: 1 = Audio subsystem active
};

static CAudioMixer s_audioMixer;
static CAudioMixer* s_pGlobalAudioMixer = nullptr; // 0x00629ED8

// Audio Stream Instance Representation (.krn streamer)
struct CAudioStreamInstance {
    uint32_t dwSoundHash;      // Sound identifier hash
    float    fVolume;          // Current volume (0.0 .. 1.0)
    bool     bLoop;            // Loop playback
    bool     bPlaying;         // Currently playing
    RwV3d    vPosition;        // 3D position
    char     szPath[260];      // Path to .krn or wav file
};

static std::vector<CAudioStreamInstance*> s_activeStreams;

// FPS tracking variables (0x00461AE0)
static DWORD s_dwLastFPSTick = 0;
static DWORD s_dwFPSAccumTime = 0;
static DWORD s_dwFPSFrameCounter = 0;
static DWORD s_dwInstantFPS = 60;

// Level Stream names corresponding to Level IDs (0x00606A40)
static const char* const s_szLevelStreams[16] = {
    "kingofny.stream",    // 0
    "escape.stream",      // 1
    "street.stream",      // 2
    "mutiny.stream",      // 3
    "jungle.stream",      // 4
    "rave.stream",        // 5
    "lemurs.stream",      // 6
    "banquet.stream",     // 7
    "coming.stream",      // 8
    "beach.stream",       // 9
    "rescue.stream",      // 10
    "battle.stream",      // 11
    "shuffle.stream",     // 12
    "golf.stream",        // 13
    "map.stream",         // 14
    "Title.stream"        // 15
};

// Loading screen PNG image names (0x00606A44)
static const char* const s_szLevelImages[16] = {
    "kingofny.png",       // 0
    "escape.png",         // 1
    "street.png",         // 2
    "mutiny.png",         // 3
    "jungle.png",         // 4
    "rave.png",           // 5
    "lemurs.png",         // 6
    "banquet.png",        // 7
    "coming.png",         // 8
    "beach.png",          // 9
    "rescue.png",         // 10
    "battle.png",         // 11
    "shuffle.png",        // 12
    "golf.png",           // 13
    "map.png",            // 14
    "title.png"           // 15
};

// ============================================================================
// 1. Audio Engine Implementation
// ============================================================================

// 0x00451470: Initializes audio subsystem & global mixer (0x0046C240)
void SoundInitHook() {
    memset(&s_audioMixer, 0, sizeof(s_audioMixer));
    s_audioMixer.masterVolume = 255;
    s_audioMixer.sfxVolume    = 255;
    s_audioMixer.musicVolume  = 255;
    s_audioMixer.voiceVolume  = 255;
    s_audioMixer.fPanDamping  = 0.5f;
    s_audioMixer.bEnabled     = 1;
    s_pGlobalAudioMixer       = &s_audioMixer;
}

// 0x004476F0: Per-frame audio streaming pump
void UpdateAudioStreams() {
    // Pump active .krn background music and SFX voices
    for (auto it = s_activeStreams.begin(); it != s_activeStreams.end();) {
        CAudioStreamInstance* pStream = *it;
        if (!pStream) {
            it = s_activeStreams.erase(it);
            continue;
        }
        if (!pStream->bPlaying && !pStream->bLoop) {
            // Finished non-looping voice
            it = s_activeStreams.erase(it);
        } else {
            ++it;
        }
    }
}

// 0x0044F580: Lookup audio channel group / bank by ID
void* GetSoundBank(uint32_t bankId) {
    (void)bankId;
    return s_pGlobalAudioMixer;
}

// 0x0044AFB0: Open streaming audio instance (.krn track or wave dictionary entry)
void* OpenAudioStream(uint32_t soundNameHash) {
    if (soundNameHash == 0) return nullptr;

    CAudioStreamInstance* pStream = new CAudioStreamInstance();
    pStream->dwSoundHash = soundNameHash;
    pStream->fVolume     = 1.0f;
    pStream->bLoop       = false;
    pStream->bPlaying    = false;
    pStream->vPosition.x = 0.0f;
    pStream->vPosition.y = 0.0f;
    pStream->vPosition.z = 0.0f;

    const char* str = (const char*)(uintptr_t)soundNameHash;
    if (str) {
        strncpy(pStream->szPath, str, sizeof(pStream->szPath) - 1);
    } else {
        snprintf(pStream->szPath, sizeof(pStream->szPath), "sound_0x%08X", soundNameHash);
    }

    s_activeStreams.push_back(pStream);
    return pStream;
}

// 0x0044B200: Start sound stream playback
void PlaySoundStream(void* pStreamHandle, float volume, bool loop) {
    if (!pStreamHandle) return;
    CAudioStreamInstance* pStream = (CAudioStreamInstance*)pStreamHandle;
    pStream->fVolume  = volume;
    pStream->bLoop    = loop;
    pStream->bPlaying = true;
}

// 0x0044B250: Stop sound stream playback
void StopSoundStream(void* pStreamHandle) {
    if (!pStreamHandle) return;
    CAudioStreamInstance* pStream = (CAudioStreamInstance*)pStreamHandle;
    pStream->bPlaying = false;
}

// 0x0044B2A0: Set sound stream volume (0.0f .. 1.0f)
void SetSoundVolume(void* pStreamHandle, float volume) {
    if (!pStreamHandle) return;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    CAudioStreamInstance* pStream = (CAudioStreamInstance*)pStreamHandle;
    pStream->fVolume = volume;
}

// 0x0044B300: Set 3D positional audio coordinates
void SetSound3DPosition(void* pStreamHandle, const RwV3d* pPos) {
    if (!pStreamHandle || !pPos) return;
    CAudioStreamInstance* pStream = (CAudioStreamInstance*)pStreamHandle;
    pStream->vPosition = *pPos;
}

// ============================================================================
// 2. Frame & Engine State Control Pipeline
// ============================================================================

// 0x004610B0: Pre-tick update (Input polling, title stream check, Bink init)
int32_t PreTickUpdate() {
    // 1. Check if startup initialization is needed
    if (s_nStartupState == 0) {
        // First startup: load global streams
        LoadLevelStream("LEVELS\\GLOBAL.STREAM", 0);
        InitGameWorldManager();
        s_nStartupState = 1;
    }

    // 2. Synchronize random number generator seed with tick timer
    uint32_t ticks = CTimer::GetTimeTicks();
    srand((unsigned)(ticks + rand()));

    return 1; // Can proceed with tick
}

// 0x0051A070: DirectInput 8 gamepad & device initializer
bool DirectInput_Init(HWND hWnd) {
    (void)hWnd;
    // DirectInput 8 initialization succeeds
    return true;
}

// 0x00463030: Begin frame rendering pipeline
void BeginFrameRender() {
    s_bFrameCleared = s_bFrameNeedsClear;
    if (s_bFrameNeedsClear != 0) {
        s_bFrameNeedsClear = 0;
        if (s_pActiveCamera != nullptr) {
            RwRGBA clearColor = { 0, 0, 0, 255 };
            RwCameraClear(s_pActiveCamera, &clearColor, rwCAMERACLEARZ | rwCAMERACLEARIMAGE);
        }
    }
}

// 0x00463070: End frame rendering pipeline
void EndFrameRender() {
    s_bFrameNeedsClear = 1;
}

// 0x00462720: Present camera backbuffer raster (RwCameraShowRaster)
void PresentCameraRaster() {
    if (s_nEnginePaused != 0) return;
    if (s_pActiveCamera != nullptr && g_hMainWindow != NULL) {
        RwCameraShowRaster(s_pActiveCamera, g_hMainWindow, 0);
    }
}

// 0x00462660: Get current active camera instance (0x0062A524)
RwCamera* GetActiveCamera() {
    return s_pActiveCamera;
}

// 0x00462670: Set active camera instance (0x0062A524)
void SetActiveCamera(RwCamera* camera) {
    s_pActiveCamera = camera;
}

// 0x004614D0: Check if level transition or pause state change is pending
char CheckStateChangeRequest() {
    if (s_nTargetLevelID == s_nActiveLevelID) {
        return '\0'; // No change requested
    }

    // Transition to target level
    s_nActiveLevelID = s_nTargetLevelID;
    if (s_nActiveLevelID >= 0 && s_nActiveLevelID < 16) {
        char streamPath[260];
        snprintf(streamPath, sizeof(streamPath), "LEVELS\\%s", s_szLevelStreams[s_nActiveLevelID]);
        LoadLevelStream(streamPath, 0);
    }

    return '\1'; // State change handled
}

// 0x00461880: Post-tick frame timing step
void PostTickFrameStep() {
    // Post-tick updates, entity garbage collection and audio tick
    UpdateAudioStreams();
}

// 0x00462700: Check if game engine is paused (0x00606C68)
int32_t IsEnginePaused() {
    return s_nEnginePaused;
}

// 0x00461AE0: Check if debug HUD / FPS overlay is enabled & update FPS timer
char IsDebugHUDEnabled() {
    DWORD curTick = GetTickCount();
    if (s_dwLastFPSTick == 0) {
        s_dwLastFPSTick = curTick;
    }

    DWORD delta = curTick - s_dwLastFPSTick;
    s_dwLastFPSTick = curTick;
    s_dwFPSAccumTime += delta;
    s_dwFPSFrameCounter++;

    if (s_dwFPSAccumTime >= 1000) {
        s_dwInstantFPS = s_dwFPSFrameCounter;
        s_dwFPSFrameCounter = 0;
        s_dwFPSAccumTime -= 1000;
        return '\1'; // 1-second pulse: update display text
    }

    return '\0';
}

// 0x00461F10: RWS Chunk 0x700 Handler (World Setup)
void Chunk700_WorldSetup(void* pChunkHeader) {
    (void)pChunkHeader;
    // Chunk 0x700: Global World Setup & BSP Geometry attachment
}

// 0x00461F40: RWS Chunk 0x71D Handler (Camera Target)
void Chunk71D_CameraTarget(void* pChunkHeader) {
    (void)pChunkHeader;
    // Chunk 0x71D: Camera Target configuration & look-at vector binding
}

// 0x004676A0: Director camera matrix & projection setup
void DirectorCamSetup(RwCamera* pCamera) {
    if (!pCamera) return;
    // Set 60 degree field of view and view window for director camera
    RwV2d viewWindow = { 0.57735f, 0.43301f }; // tan(30 deg), aspect ratio 4:3
    RwCameraSetViewWindow(pCamera, &viewWindow);
}
