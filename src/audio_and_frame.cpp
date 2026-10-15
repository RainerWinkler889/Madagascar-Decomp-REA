/**
 * @file audio_and_frame.cpp
 * @brief Madagascar (PC, 2005) - Audio Backend (.krn Streamer) & Frame Pipeline
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 */

#include "audio_and_frame.h"
#include "rw_sdk.h"
#include "script_and_ui.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Forward declarations of engine core symbols
void BroadcastRunningTick(void* param1, int32_t param2);
void UpdateRWSEntities();
void SolvePhysicsAndCollisions();
void UpdateHAnimSkeletons();
void DispatchRWSMessages(void* pQueue);
extern void* g_pRWSMessageQueueHead; // 0x0062FEC8

// ============================================================================
// Internal State Variables in Game.exe .data
// ============================================================================

// Global Active Camera in Game.exe .data (0x0062A524)
static RwCamera* s_pActiveCamera = nullptr; // 0x0062A524

// Engine Pause State (0x00606C68)
static int32_t s_nEnginePaused = 0; // 0x00606C68

// Current active level ID (-1 = uninitialized, 0..15, 0x00606A38)
static int32_t s_nActiveLevelID = -1; // DAT_00606a38

// Requested target level ID (0x0062A2F8)
static int32_t s_nTargetLevelID = 0; // DAT_0062a2f8

// Title / Startup state flag (0x006221B4)
static int32_t s_nStartupState = 0; // DAT_006221b4

// Level transition coordinate scalers (0x0062A1E8, 0x0062A1EC)
static float s_fLevelSplashScaleX = 204.0f; // _DAT_0062a1e8
static float s_fLevelSplashScaleY = 178.0f; // _DAT_0062a1ec
static int32_t s_nLevelTransPending1 = 0;   // DAT_0062a1e0
static int32_t s_nLevelTransPending2 = 0;   // DAT_0062a1e4
static int32_t s_nStateModeFlag = 0;        // DAT_0062a1f0

// Frame tick timing counter & post-tick state (0x0062A30C, etc.)
static uint32_t s_dwFrameTickCounter = 0;    // DAT_0062a30c
static uint32_t s_dwLastTickCounter = 0;     // DAT_0062a370
static int32_t  s_nTickActive1 = 1;          // DAT_00606c40
static int32_t  s_nTickActive2 = 1;          // DAT_0062972c

// Post-tick event listener pointers (0x0062A364, 0x0062A310, etc.)
static void* s_pPostTickListener1 = nullptr; // DAT_0062a364
static void* s_pPostTickSentinel  = nullptr; // DAT_0062a310
static void* s_pPostTickListener2 = nullptr; // DAT_0062a36c
static void* s_pPostTickListener3 = nullptr; // DAT_0062a374

// Active world count from InitGameWorldManager (0x00602EA0)
static int32_t s_nActiveWorldCount = 0;      // DAT_00602ea0

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

// Audio Stream Ring Buffer and Stream Slot Arrays (0x00606554..0x00606560, 0x00626560, 0x00603660)
static float s_fAudioStreamVolume[3] = { -1.0f, -1.0f, -1.0f }; // 0x00606558, 0x0060655C, 0x00606560
static uint32_t s_audioRingBuffer[3000];                          // 0x00626560: 1000 triplets
static uint32_t* s_pAudioRingBufferPos = nullptr;                // 0x00629440
static int32_t s_nActiveAudioStreamCount = 1;                    // 0x00603660
static uint32_t s_activeAudioStreamHandles[64] = { 0 };          // 0x00603668

// Wave dictionary registry array (0x00629658: 16 entries)
static int32_t s_waveDictionaries[16] = { 0 }; // 0x00629658

// Audio Stream Representation for voice tracking
struct CAudioVoice {
    uint32_t dwSoundHash;
    float    fVolume;
    bool     bLoop;
    bool     bPlaying;
    RwV3d    vPosition;
};

static std::vector<CAudioVoice*> s_activeVoices;

// FPS tracking variables (0x00461AE0)
static DWORD s_dwLastFPSTick = 0;
static DWORD s_dwFPSAccumTime = 0;
static DWORD s_dwFPSFrameCounter = 0;
static DWORD s_dwInstantFPS = 60;

// Level Stream names corresponding to Level IDs (0x00606A40: 5 DWORDS per level entry)
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

// Level splash screen default dimensions (0x00606A4C & 0x00606A50)
static const float s_fLevelSplashWidths[16] = {
    204.0f, 204.0f, 204.0f, 204.0f,
    204.0f, 204.0f, 204.0f, 204.0f,
    204.0f, 204.0f, 204.0f, 204.0f,
    204.0f, 204.0f, 204.0f, 204.0f
};

static const float s_fLevelSplashHeights[16] = {
    178.0f, 178.0f, 178.0f, 178.0f,
    178.0f, 178.0f, 178.0f, 178.0f,
    178.0f, 178.0f, 178.0f, 178.0f,
    178.0f, 178.0f, 178.0f, 178.0f
};

// Helper: Clear camera with specified flags
static void ClearCamera(RwCamera* camera, RwRGBA* color, int32_t clearFlags) {
    if (camera != nullptr) {
        RwRGBA defaultColor = { 0, 0, 0, 255 };
        RwCameraClear(camera, color ? color : &defaultColor, clearFlags);
    }
}

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
    // Reset volume levels and streaming ring buffer
    s_fAudioStreamVolume[0] = -1.0f; // 0x00606558: 0xff7fffff
    s_fAudioStreamVolume[1] = -1.0f; // 0x0060655C
    s_fAudioStreamVolume[2] = -1.0f; // 0x00606560
    
    uint32_t* pRing = s_audioRingBuffer;
    s_pAudioRingBufferPos = pRing;
    int32_t count = 1000;
    do {
        *pRing = (uint32_t)(uintptr_t)(pRing + 3);
        pRing += 3;
        count--;
    } while (count != 0);

    s_pAudioRingBufferPos = nullptr;

    // Pump voices for all registered streams
    if (s_nActiveAudioStreamCount > 1) {
        for (int32_t i = 1; i < s_nActiveAudioStreamCount; ++i) {
            uint32_t stream = s_activeAudioStreamHandles[i];
            if (stream != 0) {
                uint32_t sub = *(uint32_t*)(uintptr_t)(stream + 0x0C);
                if (sub != 0) {
                    uint32_t obj = *(uint32_t*)(uintptr_t)sub;
                    if (obj != 0) {
                        typedef void (*AudioVoiceTickFn)(uint32_t);
                        AudioVoiceTickFn pfn = *(AudioVoiceTickFn*)(uintptr_t)(*(uint32_t*)(uintptr_t)obj + 8);
                        if (pfn) pfn(0xFFFFFFFF);
                    }
                }
            }
        }
    }
    s_nActiveAudioStreamCount = 1;
}

// 0x0044F580: Lookup audio channel group / bank by ID
void* GetSoundBank(uint32_t bankId) {
    (void)bankId;
    return s_pGlobalAudioMixer;
}

// 0x0044AFB0: Open streaming audio instance (.krn track or wave dictionary entry)
void* OpenAudioStream(uint32_t soundNameHash) {
    if (soundNameHash == 0) return nullptr;

    // Search up to 16 wave dictionaries at 0x00629658
    int32_t handle = 0;
    uint32_t offset = 0;
    while (true) {
        int32_t dict = s_waveDictionaries[offset >> 2];
        if (dict != 0) {
            typedef int32_t (*LookupWaveDictFn)(int32_t, uint32_t);
            handle = ((LookupWaveDictFn)0x0050D320)(dict, soundNameHash);
            if (handle != 0) break;
        }
        offset += 4;
        if (offset > 0x3F) {
            if (handle == 0) {
                #ifdef _DEBUG
                printf("The sound <%u> was not found in the wave dictionary.\n", soundNameHash);
                #endif
            }
            break;
        }
    }

    // Allocate voice handle
    CAudioVoice* pVoice = new CAudioVoice();
    pVoice->dwSoundHash = soundNameHash;
    pVoice->fVolume     = 1.0f;
    pVoice->bLoop       = false;
    pVoice->bPlaying    = (handle != 0);
    pVoice->vPosition   = { 0.0f, 0.0f, 0.0f };
    s_activeVoices.push_back(pVoice);

    return pVoice;
}

// 0x0044B200: Start sound stream playback
void PlaySoundStream(void* pStreamHandle, float volume, bool loop) {
    if (!pStreamHandle) return;
    CAudioVoice* pVoice = (CAudioVoice*)pStreamHandle;
    pVoice->fVolume  = volume;
    pVoice->bLoop    = loop;
    pVoice->bPlaying = true;
}

// 0x0044B250: Stop sound stream playback
void StopSoundStream(void* pStreamHandle) {
    if (!pStreamHandle) return;
    CAudioVoice* pVoice = (CAudioVoice*)pStreamHandle;
    pVoice->bPlaying = false;
}

// 0x0044B2A0: Set sound stream volume (0.0f .. 1.0f)
void SetSoundVolume(void* pStreamHandle, float volume) {
    if (!pStreamHandle) return;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    CAudioVoice* pVoice = (CAudioVoice*)pStreamHandle;
    pVoice->fVolume = volume;
}

// 0x0044B300: Set 3D positional audio coordinates
void SetSound3DPosition(void* pStreamHandle, const RwV3d* pPos) {
    if (!pStreamHandle || !pPos) return;
    CAudioVoice* pVoice = (CAudioVoice*)pStreamHandle;
    pVoice->vPosition = *pPos;
}

// ============================================================================
// 2. Frame & Engine State Control Pipeline
// ============================================================================

// 0x004610B0: Pre-tick update (Input polling, title stream check, Bink init)
int32_t PreTickUpdate() {
    // Check if target level equals active level
    if (s_nTargetLevelID == s_nActiveLevelID) {
        if (s_nActiveLevelID == 0xF) { // Title level 15
            typedef char (*CheckTitleFn)();
            typedef void (*SetLevelReadyFn)(int);
            char c = ((CheckTitleFn)0x00461DA0)();
            if (c != '\0') {
                ((SetLevelReadyFn)0x00402860)(1);
                return 0;
            }
            ((SetLevelReadyFn)0x00402860)(0);
        }
        return 0;
    }

    // Set state mode flags based on target level
    uint32_t mode = 0;
    if (s_nTargetLevelID == 1) {
        s_nStateModeFlag = 1;
        mode = 0;
    } else {
        s_nStateModeFlag = 0;
        if (s_nTargetLevelID == 0xE) mode = 2;
        else if (s_nTargetLevelID == 5) mode = 3;
        else if (s_nTargetLevelID == 0xC) mode = 5;
        else if (s_nTargetLevelID == 0xD) mode = 4;
        else mode = 0;
    }
    typedef void (*SetStateModeFn)(uint32_t);
    ((SetStateModeFn)0x00461DB0)(mode);

    // Initial load: level -1 (uninitialized)
    if (s_nActiveLevelID == -1) {
        if (s_nStartupState == 0) {
            char titleStreamPath[260];
            snprintf(titleStreamPath, sizeof(titleStreamPath), "LEVELS\\Title.stream");
            bool loaded = LoadLevelStream(titleStreamPath, 1);
            if (!loaded) {
                s_nStartupState = 1;
            }
            LoadLevelStream("LEVELS\\GLOBAL.STREAM", 0);
            s_nActiveWorldCount = InitGameWorldManager(); // 0x00432CB0

            typedef void (*EngineStepFn)(int);
            ((EngineStepFn)0x00406640)(1);
            ((SetStateModeFn)0x00461DB0)(7);

            typedef void (*PluginHookFn)();
            ((PluginHookFn)0x005074B0)();
            ((EngineStepFn)0x0051DE10)((int)(uintptr_t)g_hMainWindow);

            // Bink sound initialization
            typedef void  (*BinkSetSoundSystemFn)(void*, void*);
            BinkSetSoundSystemFn pfnBinkSetSound = (BinkSetSoundSystemFn)0x00557450;
            if (pfnBinkSetSound) {
                pfnBinkSetSound((void*)0x00557470, (void*)0x0062F1E0);
            }

            // Iterate BGM tracks (PTR_s_bgm_02_krn_005d5a90: up to 4 tracks)
            for (uint32_t u = 0; u < 0x10; u += 4) {
                typedef int32_t (*PlayBGMTrackFn)(uint32_t, int);
                typedef float   (*CheckAudioReadyFn)(int, int, int);
                int32_t bgmRes = ((PlayBGMTrackFn)0x00460B90)(0x005D5A90 + u, 1);
                if (bgmRes == 0) {
                    float f = ((CheckAudioReadyFn)0x00402880)(0, 0, 0);
                    if (f != 0.0f) break;
                }
            }

            ((PluginHookFn)0x0051DE40)();
            ((PluginHookFn)0x005074B0)();
            ((SetStateModeFn)0x00461DB0)(0);

            typedef void (*AudioWaitFn)(uint32_t);
            ((AudioWaitFn)0x004028A0)(0xFA);
        } else if (s_nStartupState == 2) {
            // Demo loop mode: load GLOBAL.STREAM and run demo screen
            RwCamera* cam = GetActiveCamera();
            LoadLevelStream("LEVELS\\GLOBAL.STREAM", 0);
            s_nActiveWorldCount = InitGameWorldManager();
            typedef void (*DemoSetupFn)();
            ((DemoSetupFn)0x00450620)();

            typedef void (*DemoImageFn)(const char*);
            ((DemoImageFn)0x004628D0)("demo.png");
            DirectorCamSetup(cam);
        }
    } else {
        if (s_nStartupState > 1) {
            return 0;
        }
        if (s_nActiveLevelID == 0xB && s_nTargetLevelID == 0x12) {
            typedef int32_t (*PlayBgmFn)(const char*);
            ((PlayBgmFn)0x00460C50)("bgm\\01.krn");
            s_nTargetLevelID = 0xE;
        }
    }

    // Update RNG seed from CTimer
    uint32_t ticks = CTimer::GetTimeTicks();
    srand((unsigned)(ticks + rand()));
    return 1;
}

// 0x0051A070: DirectInput 8 gamepad & device initializer
bool DirectInput_Init(HWND hWnd) {
    (void)hWnd;
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
    if (s_nActiveLevelID == -1) {
        if (s_nStartupState == 0) {
            s_nActiveLevelID = 0xF; // Default to Title.stream (level 15)
        } else {
            s_nActiveLevelID = (s_nStartupState != 1) ? 9 : 0;
        }
    } else {
        bool notTitle = (s_nActiveLevelID != 0xF);
        s_nActiveLevelID = s_nTargetLevelID;
        if (notTitle && s_nTargetLevelID == 0x10) {
            s_nActiveLevelID = 0;
        }
    }

    const char* splashImage = nullptr;
    if (s_nActiveLevelID > 0xF) {
        s_nTargetLevelID = 0xE;
        s_nActiveLevelID = 0xE;
        s_fLevelSplashScaleX = 204.0f;
        s_fLevelSplashScaleY = 178.0f;
        splashImage = "zooshop.png";
    } else {
        s_fLevelSplashScaleX = s_fLevelSplashWidths[s_nActiveLevelID];
        s_fLevelSplashScaleY = s_fLevelSplashHeights[s_nActiveLevelID];
        splashImage = s_szLevelImages[s_nActiveLevelID];
    }

    // Scale splash screen coordinates against camera frame buffer resolution
    RwCamera* pCamera = GetActiveCamera();
    if (pCamera != nullptr && pCamera->frameBuffer != nullptr) {
        s_fLevelSplashScaleX = (float)pCamera->frameBuffer->width  * s_fLevelSplashScaleX * 0.0015625f;
        s_fLevelSplashScaleY = (float)pCamera->frameBuffer->height * s_fLevelSplashScaleY * 0.0020833334f;
    }

    // Render splash screen raster
    typedef void (*LoadImageFn)(const char*);
    typedef void (*RenderSplashFn)(RwCamera*);
    ((LoadImageFn)0x004628D0)(splashImage);

    if (pCamera != nullptr) {
        ((RenderSplashFn)0x004629A0)(pCamera);
        ClearCamera(pCamera, nullptr, rwCAMERACLEARIMAGE);
        ((RenderSplashFn)0x004629A0)(pCamera);
        ClearCamera(pCamera, nullptr, rwCAMERACLEARIMAGE);
    }

    // Load actual .stream file
    char streamPath[260];
    snprintf(streamPath, sizeof(streamPath), "LEVELS\\%s", s_szLevelStreams[s_nActiveLevelID]);
    bool bOk = LoadLevelStream(streamPath, 0);

    s_nLevelTransPending1 = 0;
    s_nLevelTransPending2 = 0;

    // Reset splash screen
    typedef void (*UnloadSplashFn)();
    ((UnloadSplashFn)0x00462970)();

    if (bOk && s_nActiveLevelID == 0 && s_nTargetLevelID == 0x10 && s_nStartupState == 0) {
        typedef int32_t (*PlayBgmFn)(const char*);
        ((PlayBgmFn)0x00460C50)("bgm\\06.krn");
    }

    s_nTargetLevelID = s_nActiveLevelID;
    typedef void (*SetLevelReadyFn)(int);
    ((SetLevelReadyFn)0x00402860)(1);
    SetCapture(NULL);

    return bOk ? '\1' : '\0';
}

// 0x00461880: Post-tick frame timing step
void PostTickFrameStep() {
    if (s_nTickActive1 != 0 && s_nTickActive2 != 0) {
        typedef void (*BroadcastListenerFn)(void**);
        if (s_pPostTickListener1 != s_pPostTickSentinel && s_pPostTickListener1 != nullptr) {
            ((BroadcastListenerFn)0x00550BB0)(&s_pPostTickListener1);
        }

        s_dwLastTickCounter = s_dwFrameTickCounter;
        s_dwFrameTickCounter++;

        if (s_pPostTickListener2 != s_pPostTickSentinel && s_pPostTickListener2 != nullptr) {
            ((BroadcastListenerFn)0x00550BB0)(&s_pPostTickListener2);
        }
        if (s_pPostTickListener3 != s_pPostTickSentinel && s_pPostTickListener3 != nullptr) {
            ((BroadcastListenerFn)0x00550BB0)(&s_pPostTickListener3);
        }

        // Dispatch RWS Event Queue (0x0062FED0 / 0x0062FEC8)
        extern void* g_pRWSMessageQueueHead;
        DispatchRWSMessages(&g_pRWSMessageQueueHead);
    }
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
    BroadcastRunningTick(pChunkHeader, 0);
    UpdateAudioStreams();
    extern void* g_pRWSMessageQueueHead;
    DispatchRWSMessages(&g_pRWSMessageQueueHead);
    UpdateRWSEntities();
    SolvePhysicsAndCollisions();
    UpdateHAnimSkeletons();
}

// 0x00461F40: RWS Chunk 0x71D Handler (Camera Target)
void Chunk71D_CameraTarget(void* pChunkHeader) {
    BroadcastRunningTick(pChunkHeader, 0);
    UpdateAudioStreams();
    typedef void (*RWSFn)();
    ((RWSFn)0x0054FF90)();
    extern void* g_pRWSMessageQueueHead;
    DispatchRWSMessages(&g_pRWSMessageQueueHead);
    ((RWSFn)0x00551410)();
    UpdateHAnimSkeletons();
}

// 0x004676A0: Director camera matrix & projection setup
void DirectorCamSetup(RwCamera* pCamera) {
    typedef void (*CamHelperFn1)(int);
    typedef void (*CamHelperFn2)();
    typedef void (*CamHelperFn3)(RwCamera*);

    ((CamHelperFn1)0x00467680)(0);
    ((CamHelperFn2)0x004719C0)();
    ((CamHelperFn2)0x0046F7F0)();
    ((CamHelperFn2)0x0046C1F0)();
    ((CamHelperFn2)0x00469800)();
    ((CamHelperFn2)0x00470300)();
    ((CamHelperFn2)0x004677A0)();
    ((CamHelperFn3)0x00467530)(pCamera);
}
