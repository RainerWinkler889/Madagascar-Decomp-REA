/**
 * @file game_loop.cpp
 * @brief Madagascar (PC, 2005) - Heartbeat Game Loop, World Manager & Level Streaming
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Activision / Beenox
 * Original Engine: RenderWare Studio 3.x (DirectX 8.1)
 *
 * Mapped Functions:
 *  - 0x004620D0: MainGameTick (Heartbeat loop: update, render, FPS overlay, screenshot, rave mini-game)
 *  - 0x00432CB0: InitGameWorldManager (Queries active world entity vector count)
 *  - 0x004627E0: LoadLevelStream (Synchronous & asynchronous .stream file parser)
 */

#include "common.h"
#include "rw_sdk.h"
#include "rws_core.h"
#include "script_and_ui.h"

// ============================================================================
// Global State Variables (Addresses in Game.exe .data)
// ============================================================================

// 0x006221B8: Number of simulation substeps per tick
static int32_t s_nSubstepCount = 1; // 0x006221B8

// 0x0062A4B0: Averaged Frame Rate
static uint32_t s_dwAveragedFPS = 60; // 0x0062A4B0

// 0x0062A4B4: Instantaneous Frame Rate
static uint32_t s_dwInstantFPS = 60; // 0x0062A4B4

// 0x0062A49A: Flag whether FPS / Build text box has been allocated
static uint8_t s_bFPSBoxInitialized = 0; // 0x0062A49A

// 0x0062A498: Flag whether FPS display is toggled ON (1) or OFF (0)
static uint8_t s_bShowFPS = 0; // 0x0062A498

// 0x0062A490: Trigger flag for F12 Screenshot Capture
static uint8_t s_bTakeScreenshot = 0; // 0x0062A490

// 0x0062A494: Sequential screenshot index counter ("Screen001.bmp")
static int32_t s_nScreenshotIndex = 0; // 0x0062A494

// 0x00606C44: Countdown timer for displaying "ID_SCREENSHOTERROR" on screen
static int32_t s_nScreenshotErrorTimer = -1; // 0x00606C44

// 0x00606A38: Current active Level ID (5 = "rave.stream")
static int32_t s_nCurrentLevelID = 0; // 0x00606A38

// 0x00602EA0: Active World Count returned by InitGameWorldManager
int32_t g_nActiveWorldCount = 0; // 0x00602EA0

// ============================================================================
// Forward Declarations of Local Engine Subsystem Callbacks
// ============================================================================

// 0x00462700: Check if game engine is paused
typedef int32_t (*IsEnginePausedFn)();

// 0x004610B0: Pre-tick update: physics, input, controller polling
typedef int32_t (*PreTickUpdateFn)();

// 0x00463030: Begin frame render target setup
typedef void (*BeginFrameRenderFn)();

// 0x004476F0: Audio subsystem stream update (BGM .krn & Bink audio)
typedef void (*UpdateAudioStreamsFn)();

// 0x00551380: RenderWare Collision & Physics Solver tick
typedef void (*SolvePhysicsAndCollisionsFn)();

// 0x00428270: Hierarchical Animation (HAnim) & Skeleton bone updates
typedef void (*UpdateHAnimSkeletonsFn)();

// 0x004614D0: Check if state change / level transition / pause was requested
typedef char (*CheckStateChangeRequestFn)();

// 0x00463070: End frame render & buffer cleanup
typedef void (*EndFrameRenderFn)();

// 0x00461880: Post-tick frame step
typedef void (*PostTickFrameStepFn)();

// 0x00461AE0: Check if debug HUD / FPS overlay is enabled
typedef char (*IsDebugHUDEnabledFn)();

// 0x00462720: Show Camera Raster / Present backbuffer (RwCameraShowRaster)
typedef void (*PresentCameraRasterFn)();

// 0x00445910: Check free disk space in bytes
typedef int32_t (*CheckFreeDiskSpaceFn)(int32_t driveId);

// 0x00462660: Get current active camera instance
typedef RwCamera* (*GetActiveCameraFn)();

// 0x004E34A0: RwImageCreate(width, height, depth)
typedef RwImage* (*RwImageCreateFn)(RwInt32 width, RwInt32 height, RwInt32 depth);

// 0x004E3590: RwImageAllocatePixels(image)
typedef RwImage* (*RwImageAllocatePixelsFn)(RwImage* image);

// 0x004E4F60: RwImageSetFromRaster(image, raster)
typedef RwImage* (*RwImageSetFromRasterFn)(RwImage* image, RwRaster* raster);

// 0x004E4480: RwImageWrite(image, filename)
typedef RwImage* (*RwImageWriteFn)(RwImage* image, const RwChar* filename);

// 0x004E3530: RwImageDestroy(image)
typedef RwBool (*RwImageDestroyFn)(RwImage* image);

// ============================================================================
// 0x00432CB0: InitGameWorldManager
//
// Liest den World-Entity-Manager Vektor an 0x0061F878 aus und liefert die
// Anzahl der aktiven Welten / Level-Szenen zurück.
// ============================================================================
int32_t InitGameWorldManager() {
    uintptr_t pVector = *(uintptr_t*)0x0061F878;
    if (pVector == 0) {
        return 0;
    }

    uintptr_t beginPtr = *(uintptr_t*)(pVector + 4);
    uintptr_t endPtr   = *(uintptr_t*)(pVector + 8);

    if (beginPtr == 0) {
        return 0;
    }

    // std::vector Element-Größe beträgt 8 Bytes (zwei Pointer pro World-Deskriptor)
    // (end - begin) >> 3 entspricht (end - begin) / 8
    int32_t activeWorldCount = static_cast<int32_t>((endPtr - beginPtr) >> 3);
    g_nActiveWorldCount = activeWorldCount;
    return activeWorldCount;
}

// ============================================================================
// 0x004627E0: LoadLevelStream
//
// Lädt eine `.stream` Level-Datei (z. B. "levels\global.stream" oder "levels\jungle.stream").
//
// Parameter:
//  - streamPath: Relativer Pfad zur Stream-Datei
//  - bAsyncMode: 0 = Synchron laden (Haupt-Thread blockiert), 1 = Asynchron im Hintergrund
// ============================================================================
bool LoadLevelStream(const char* streamPath, int flags) {
    char fullPathBuffer[256];

    // 1. Mipmapping aktivieren / Textur-Cache vor dem Laden bereinigen
    RwTextureSetMipmapping(RWTRUE); // 0x004E1FA0

    // 2. Asset-Wurzelpfad auflösen (0x00460C10)
    const char* assetRoot = (const char*)ResolveAssetPath();
    snprintf(fullPathBuffer, sizeof(fullPathBuffer), "%s%s", assetRoot ? assetRoot : "", streamPath);

    // 3. Asynchrones Laden anfordern (falls bAsyncMode != 0)
    if (flags != 0) {
        typedef int32_t (*LoadStreamAsyncFn)(const char*);
        int32_t asyncResult = ((LoadStreamAsyncFn)0x00465C40)(fullPathBuffer); // 0x00465C40
        return (asyncResult != 0);
    }

    // 4. Synchrones Laden: Öffne RenderWare Stream
    // rwSTREAMFILENAME = 2, rwSTREAMREAD = 1
    RwStream* pStream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, fullPathBuffer); // 0x004EFD00
    if (pStream != nullptr) {
        // 5. Verarbeite RWS Stream-Entities & Chunks (0x00552F70)
        uint8_t readOk = ProcessRWSStream(pStream);

        // 6. Schließe Stream
        RwStreamClose(pStream, (void*)streamPath); // 0x004EFBF0

        // 7. Mipmapping nach dem Laden auffrischen
        RwTextureSetMipmapping(RWTRUE); // 0x004E1FA0

        return (readOk != 0);
    }

    return false;
}

// ============================================================================
// 0x004620D0: MainGameTick
//
// Der zentrale Herzschlag (PowerPipe Tick) von Madagascar:
//
// 1. Frame-Timer & Substeps:
//    Berechnet delta time via 0x00464F40.
//    Führt für jeden Substep (DAT_006221B8) Physik, KI, Animationen und RWS-Nachrichten aus.
//
// 2. RWS-Nachrichten-Pipeline:
//    - "iMsgRunningTick" (0x00461E80): Entity-Updates & Logik
//    - Audio-Update (0x004476F0)
//    - RWS Message Queue Dispatch (0x005502C0)
//    - Physik & Kollision (0x00551380)
//    - HAnim Skelett-Pose-Update (0x00428270)
//
// 3. On-Screen Display (OSD):
//    - FPS-Anzeige ("FPS:%u")
//    - Build-Identifikation: "Build FC0025" (Final Candidate Build 25 vom 12.04.2005)
//
// 4. Screenshot-System:
//    Liest FrameBuffer-Raster in ein RwImage, erzeugt 24-Bit/32-Bit BMP ("ScreenXXX.bmp").
//    Prüft Festplattenspeicher (min. 8 MB frei), zeigt bei Fehlern "ID_SCREENSHOTERROR".
//
// 5. Minispiel-Support:
//    Für Level 5 ("rave.stream") werden die rhythmischen Tanz-Tasten ("ID_RAVE_KEY_1/2")
//    im OSD eingeblendet.
// ============================================================================
void MainGameTick() {
    char szBuildString[32];      // local_158
    char szFPSString[32];        // local_138
    char szScreenshotPath[280];  // local_118

    int32_t substeps = s_nSubstepCount; // 0x006221B8

    // 1. Ermittle Frame-Delta-Zeit
    // ========================================================================
    // X86-ASSEMBLER-ANALYSE (0x004620D0): DUAL-PHASEN ENGINE TIMER PROTOKOLL
    // ========================================================================
    // Der Timer 0x00464F40 (CTimer::Update, ECX = 0x0062A4B8) arbeitet in zwei
    // getakteten Hauptphasen:
    //
    // 1. Simulations-Phase:
    //    Liefert deltaTime > 0.0f (z. B. 0.01666f / 0.03333f).
    //    Sprung bei 0x004620f6 (jz 0x004624b0) führt die Physik-, KI- und
    //    RWS-Nachrichten-Schleife für alle Substeps (s_nSubstepCount) aus.
    //    -> Sprungziel am Ende von Substeps (0x004624fe): pop esi; add esp, 0x15c; RET!
    //    -> Der Tick beendet sich hier direkt (Return).
    //
    // 2. Render-Phase:
    //    Sobald ein Frame gerendert und präsentiert werden soll, liefert
    //    0x00464F40 (Zeile 60) EXPLIZIT -1.0f (< 0.0f) zurück!
    //    - 0x004620eb: fcomp 0.0f -> Sprung zu 0x004624b0 wird NICHT genommen.
    //    - 0x0046211c: fcomp 0.0f; test ah, 0x5; jp 0x0046249b
    //      Da deltaTime < 0.0f (-1.0f) ist, ist PF = 0 (Parity Odd).
    //      -> JP wird NICHT genommen (kein Sleep(0); return;)!
    //    - Durchlauf in 0x00462131: Dedizierter Rendering-Block!
    //      (IsDebugHUDEnabled, PresentCameraRaster 0x00462720, F12 Screenshot, Rave OSD)
    //
    // 3. Pausiert / Idle:
    //    Liefert 0.0f zurück. Falls pausiert (0x00462700 != 0), springt
    //    0x0046212b (jp 0x0046249b) an -> Sleep(0); return;
    // ========================================================================
    float deltaTime = g_gameTimer.Update(); // 0x00464F40

    // Assembler 0x004620eb - 0x00462116:
    // Wenn deltaTime > 0.0f ODER (deltaTime == 0.0f && !IsEnginePaused()):
    // Springe zu 0x004624b0 (Simulations-Schleife)
    if ((deltaTime > 0.0f) || (deltaTime == 0.0f && ((IsEnginePausedFn)0x00462700)() == 0)) {
        // Simulations-Schleife über alle Substeps (0x004624b0)
        do {
            int32_t bCanTick = ((PreTickUpdateFn)0x004610B0)(); // 0x004610B0
            if (bCanTick != 0) {
                // Begin Frame Render Target Setup
                ((BeginFrameRenderFn)0x00463030)(); // 0x00463030

                // Dispatch RWS "iMsgRunningTick"
                BroadcastRunningTick(szBuildString, 0); // 0x00461E80

                // Update Sound Subsystem
                ((UpdateAudioStreamsFn)0x004476F0)(); // 0x004476F0

                // Dispatch RWS Event Queue (0x0062FEC8)
                DispatchRWSMessages((void*)0x0062FEC8); // 0x005502C0

                // Update Entities & Behaviors
                UpdateRWSEntities(); // 0x0054FF60

                // Solve Collisions & Dynamics
                ((SolvePhysicsAndCollisionsFn)0x00551380)(); // 0x00551380

                // Update Skeletal Hierarchies
                ((UpdateHAnimSkeletonsFn)0x00428270)(); // 0x00428270

                // Prüfe, ob Pause- oder Level-Wechsel ansteht
                char bStateChanged = ((CheckStateChangeRequestFn)0x004614D0)(); // 0x004614D0
                if (bStateChanged != '\0') {
                    s_bFPSBoxInitialized = 0;
                    BroadcastModeChange(szBuildString, 0); // 0x00461BD0
                    Sleep(200);
                    ((EndFrameRenderFn)0x00463070)(); // 0x00463070
                    return; // 0x0046252c: Sprungziel bei Modus-Wechsel
                }
            }

            ((PostTickFrameStepFn)0x00461880)(); // 0x00461880
            substeps--;
        } while (substeps > 0);

        // Assembler 0x004624fe: Simulations-Tick beendet -> pop esi; add esp, 0x15c; ret
#ifndef MODERN_UNIFIED_FRAME
        return;
#endif
    }

    // Assembler 0x0046211c - 0x0046212b:
    // Wenn deltaTime == 0.0f und pausiert:
    // Springe zu 0x0046249b (Sleep(0); return;)
    if (deltaTime >= 0.0f) {
        Sleep(0); // 0x0046249b
        return;
    }

    // ========================================================================
    // RENDERING BLOCK (Assembler 0x00462131):
    // Erreicht wenn deltaTime < 0.0f (Timer liefert -1.0f als Render-Token)
    // oder unter MODERN_UNIFIED_FRAME.
    // ========================================================================

    // ========================================================================
    // 2. FPS & Build FC 25 Text Overlay
    // ========================================================================
    if (((IsDebugHUDEnabledFn)0x00461AE0)() != '\0') {
        uint32_t currentFps = s_dwInstantFPS;
        if (currentFps == 0) {
            currentFps = s_dwAveragedFPS;
        }

        snprintf(szFPSString, sizeof(szFPSString), "FPS:%u", currentFps);
        snprintf(szBuildString, sizeof(szBuildString), "%s%04u", "Build FC", 25); // FC Build 25

        if (s_bFPSBoxInitialized == 0) {
            // Text-Boxen erzeugen (0x00416AA0)
            CreateUITextBox("bx_fps", "FPS:???", 8.0f, 8.0f, 128.0f, 32.0f);
            CreateUITextBox("bx_build", szBuildString, 8.0f, 28.0f, 128.0f, 32.0f);
            s_bFPSBoxInitialized = 1;
        }

        const char* pBuildDisplay = "";
        if (s_bShowFPS == 0) {
            UpdateUITextBox("bx_fps", "", 8.0f, 8.0f, 128.0f, 32.0f);
        } else {
            UpdateUITextBox("bx_fps", szFPSString, 8.0f, 8.0f, 128.0f, 32.0f);
            pBuildDisplay = szBuildString;
        }
        UpdateUITextBox("bx_build", pBuildDisplay, 8.0f, 28.0f, 128.0f, 32.0f);
    }

    // 3. RenderWare Backbuffer anzeigen (RwCameraShowRaster)
    ((PresentCameraRasterFn)0x00462720)(); // 0x00462720

    // ========================================================================
    // 4. Screenshot Handler (F12)
    // ========================================================================
    if (s_bTakeScreenshot != 0) {
        // Prüfe freien Speicherplatz (min. 8000 KB ~ 8 MB)
        int32_t freeKb = ((CheckFreeDiskSpaceFn)0x00445910)(1);
        if (freeKb > 7999) {
            RwCamera* camera = ((GetActiveCameraFn)0x00462660)();
            if (camera != nullptr && camera->frameBuffer != nullptr) {
                RwRaster* raster = camera->frameBuffer;

                snprintf(szScreenshotPath, sizeof(szScreenshotPath), "Screen%03d.bmp", s_nScreenshotIndex);

                // Erzeuge RwImage aus dem FrameBuffer Raster
                RwImage* pImage = ((RwImageCreateFn)0x004E34A0)(raster->width, raster->height, 32);
                if (pImage != nullptr) {
                    ((RwImageAllocatePixelsFn)0x004E3590)(pImage);
                    ((RwImageSetFromRasterFn)0x004E4F60)(pImage, raster);

                    // Schreibe BMP-Datei
                    if (((RwImageWriteFn)0x004E4480)(pImage, szScreenshotPath) == nullptr) {
                        // Fehler beim Schreiben
                        const char* errStr = GetLocalizedString(s_pStringDict, "ID_SCREENSHOTERROR");
                        CreateUITextBox("bx_screenerror", errStr ? errStr : "Screenshot Error", 8.0f, 48.0f, 128.0f, 32.0f);
                        s_nScreenshotErrorTimer = 100;
                    } else {
                        s_nScreenshotIndex++;
                    }

                    ((RwImageDestroyFn)0x004E3530)(pImage);
                }
            }
            s_bTakeScreenshot = 0;
        } else {
            // Zu wenig Speicherplatz
            const char* errStr = GetLocalizedString(s_pStringDict, "ID_SCREENSHOTERROR");
            CreateUITextBox("bx_screenerror", errStr ? errStr : "Low Disk Space", 8.0f, 48.0f, 128.0f, 32.0f);
            s_nScreenshotErrorTimer = 100;
            s_bTakeScreenshot = 0;
        }
    }

    // Screenshot Error Timer verwalten
    if (s_nScreenshotErrorTimer >= 0) {
        if (s_nScreenshotErrorTimer == 0) {
            DestroyUITextBox("bx_screenerror");
            s_nScreenshotErrorTimer = -1;
        } else {
            s_nScreenshotErrorTimer--;
        }
    }

    // ========================================================================
    // 5. Minispiel / Level 5 ("rave.stream") Tanz-Tasten OSD
    // ========================================================================
    if (s_nCurrentLevelID == 5) {
        void* promptElement = FindUIElement("Instruct_Oops_Equal");
        if (promptElement != nullptr && IsUIElementVisible(promptElement)) {
            const char* key1Str = GetLocalizedString(s_pStringDict, "ID_RAVE_KEY_1");
            CreateUITextBox("bx_RAVE_KEY_1", key1Str ? key1Str : "KEY 1", 160.0f, 340.0f, 128.0f, 32.0f);

            const char* key2Str = GetLocalizedString(s_pStringDict, "ID_RAVE_KEY_2");
            CreateUITextBox("bx_RAVE_KEY_2", key2Str ? key2Str : "KEY 2", 160.0f, 370.0f, 128.0f, 32.0f);
            return;
        }

        DestroyUITextBox("bx_RAVE_KEY_1");
        DestroyUITextBox("bx_RAVE_KEY_2");
    }
}
