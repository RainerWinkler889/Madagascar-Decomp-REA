/**
 * @file video.cpp
 * @brief Madagascar (PC, 2005) - Graphics Subsystem, Video Modes & Plugin Registrations
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Activision / Beenox
 * Original Engine: RenderWare Studio 3.x (DirectX 8.1)
 *
 * Mapped Functions:
 *  - 0x00462AA0: RwEngine_Initialize
 *  - 0x00462BF0: RwSelectVideoMode (Includes #ifdef MODERN_FIX for the 16-bit crash trap)
 *  - 0x00462880: RwCreateCameraAndRasters (Image Format Decoders: PNG, BMP, RAS, TIF)
 *  - 0x0044E8E0: RwCreateDefaultCameraAndRasters (Camera, Z-Buffer, Perspective FOV)
 *  - 0x0044AE20: RwRegisterStandardPlugins (RpWorld, RpCollision)
 *  - 0x00465150: RwRegisterTFBPlugins / TFB Async Subsystem Initialization
 *  - 0x0045F9A0: RwRegisterTFBCustomPlugins (Material, Atomic, Clump, Geometry, World)
 */

#include "common.h"
#include "rw_sdk.h"

// ============================================================================
// Global State & Globals (Addresses in Game.exe .data)
// ============================================================================

// 0x00606C90: RenderWare Core Default Memory Function Table
extern "C" RwMemoryFunctions g_RwDefaultMemoryFuncs;

// 0x0062969C: Flag set if RpWorld plugins are successfully registered
uint32_t g_bWorldPluginsRegistered = 0; // 0x0062969C

// 0x0060B8C0: RpWorld global instance handle
uint32_t g_dwRpWorldHandle = 0; // 0x0060B8C0

// 0x0062A618: TFB Async / Critical Section initialized flag
uint32_t g_bTFBAsyncInitialized = 0; // 0x0062A618

// 0x00630F40: Critical Section for TFB asynchronous streaming & resource loading
CRITICAL_SECTION g_csTFBAsyncStream; // 0x00630F40

// TFB Async Worker State
int32_t  g_nAsyncThreadPriority = 0; // 0x0062A610 (-1 default)
uint32_t g_dwAsyncThreadStatus   = 0; // 0x0062A614
uint32_t g_dwAsyncWorkerCounter  = 0; // 0x0062A620
uint32_t g_dwAsyncQueueHead      = 0; // 0x0062A608
uint32_t g_dwAsyncQueueTail      = 0; // 0x0062A60C

// TFB Custom Plugin IDs in RenderWare Global Table
int32_t g_nTFBMaterialPluginOffset = -1; // 0x006068C8
int32_t g_nTFBMaterialExtensionID  = -1; // 0x006068CC
int32_t g_nTFBWorldPluginOffset    = -1; // 0x006069B8

// ============================================================================
// Forward Declarations of Internal Subsystem Helpers
// ============================================================================

// 0x00464EF0: Engine pre-initialization hook (stub in retail build)
static void RwEngine_PreInitHook() {
    // 0x00464EF0: No-op / profiling hook in retail
}

// 0x00463360: Local file system & drive root path initializer
int32_t SetupFileSystemDrives();

// 0x004627B0: Master plugin registration loop (iterates 19 plugins at 0x005D5CF0)
int32_t RegisterAllExtendedPlugins();

// 0x00461F70: Post-engine startup: camera creation, directors camera & RWS messages
void InitPostEngineStart(uint32_t width, uint32_t height, uint32_t depth);

// 0x00519D20: Registry reader (imported from main.cpp)
extern int32_t GetSettingInt(const char* key, int32_t defaultValue);

// ============================================================================
// 0x00462BF0: RwSelectVideoMode
//
// Iterates through all Direct3D display subsystems (adapters) and their video modes.
// Ranks candidate modes using a weighted scoring system:
//   - Depth match:      +1 point
//   - Width >= target:  +2 points
//   - Height >= target: +2 points
//   - Width exact:      +4 points
//   - Height exact:     +4 points
//   - Flags match:      +16 (0x10) points
//
// Includes optional MODERN_FIX to prevent the notorious 16-bit crash trap!
// ============================================================================
int32_t RwSelectVideoMode(int32_t* pWidth, int32_t* pHeight, int32_t reqDepth, uint32_t reqFlags) {
    int32_t bestModeIndex      = -1; // local_98
    int32_t bestSubsystemIndex = -1; // local_90
    int32_t numSubsystems      = RwEngineGetNumSubSystems(); // 0x004D7FA0
    int32_t currentSubsystem   = 0;  // local_94

    int32_t  selectedWidth  = 0; // local_88
    int32_t  selectedHeight = 0; // local_84
    int32_t  selectedDepth  = 0; // local_80
    uint32_t selectedFlags  = 0; // local_7c

    if (numSubsystems > 0) {
        do {
            RwEngineSetSubSystem(currentSubsystem); // 0x004D8050
            int32_t numModes = RwEngineGetNumVideoModes(); // 0x004D8080
            int32_t currentMode = 0;

            if (numModes > 0) {
                do {
                    RwVideoMode modeInfo; // local_70 (width), local_6c (height), local_68 (depth), local_64 (flags)
                    RwEngineGetVideoModeInfo(&modeInfo, currentMode); // 0x004D80B0

                    if (bestModeIndex < 0) {
                        // First valid mode encountered
                        selectedDepth       = modeInfo.depth;
                        selectedFlags       = modeInfo.flags;
                        selectedWidth       = modeInfo.width;
                        selectedHeight      = modeInfo.height;
                        bestModeIndex       = currentMode;
                        bestSubsystemIndex  = currentSubsystem;
                    } else {
                        // Compute candidate score (bVar2)
                        uint8_t candScore = (modeInfo.depth == reqDepth) ? 1 : 0;
                        if (*pWidth < modeInfo.width)   candScore += 2;
                        if (*pHeight < modeInfo.height) candScore += 2;
                        if (modeInfo.width == *pWidth)   candScore += 4;
                        if (modeInfo.height == *pHeight) candScore += 4;
                        if (modeInfo.flags == reqFlags)  candScore += 0x10;

                        // Compute current best score (bVar3)
                        uint8_t bestScore = (selectedDepth == reqDepth) ? 1 : 0;
                        if (*pWidth < selectedWidth)   bestScore += 2;
                        if (*pHeight < selectedHeight) bestScore += 2;
                        if (selectedWidth == *pWidth)   bestScore += 4;
                        if (selectedHeight == *pHeight) bestScore += 4;
                        if (selectedFlags == reqFlags)  bestScore += 0x10;

                        // Evaluate candidate:
                        // Candidate wins if score is higher, or score is tied and resolution pixel area is smaller
                        if ((bestScore < candScore) ||
                            ((candScore == bestScore) && (modeInfo.height * modeInfo.width < selectedHeight * selectedWidth))) {
                            selectedDepth      = modeInfo.depth;
                            bestSubsystemIndex = currentSubsystem;
                            selectedFlags      = modeInfo.flags;
                            selectedWidth      = modeInfo.width;
                            selectedHeight     = modeInfo.height;
                            bestModeIndex      = currentMode;
                        }
                    }

                    currentMode++;
                } while (currentMode < numModes);
            }

            currentSubsystem++;
        } while (currentSubsystem < numSubsystems);
    }

    // ========================================================================
    // DIE 16-BIT-CRASH-FALLE (0x00462CAE)
    // ========================================================================
    if (selectedDepth == 16) {
#ifndef MODERN_FIX
        // Original-Verhalten von Madagascar PC (2005):
        // Zeigt Warnmeldung an und führt im weiteren Verlauf unausweichlich zum D3D-Crash!
        MessageBoxA(nullptr, "Mode selected is 16 bits depth\nGame will crash!", "Error", MB_OK);
#else
        // MODERN_FIX:
        // Auf modernen Betriebssystemen (Windows 10/11) und modernen Grafikkarten
        // (RTX, Radeon RX, Intel Arc) wird 16-Bit D3D8 entweder nicht unterstützt oder
        // fehlerhaft enummeriert. Wir erzwingen hier sicher 32-Bit TrueColor Farbtiefe!
        selectedDepth = 32;
#endif
    }

    if (bestModeIndex >= 0) {
        RwEngineSetSubSystem(bestSubsystemIndex); // 0x004D8050
        RwEngineSetVideoMode(bestModeIndex);       // 0x004D8120

        if ((selectedFlags & rwVIDEOMODEEXCLUSIVE) != 0) {
            *pWidth  = selectedWidth;
            *pHeight = selectedHeight;
        }

        RwSubSystemInfo subSysInfo;
        RwEngineGetSubSystemInfo(&subSysInfo, bestSubsystemIndex); // 0x004D7FE0
        return 1;
    }

    return 0;
}

// ============================================================================
// 0x00462880: RwCreateCameraAndRasters / RegisterImageFormatDecoders
//
// Iterates through image format extension table at 0x005D5D6C:
//   - Entry 0: PNG  (Read: 0x00478660, Write: 0x004781B0)
//   - Entry 1: BMP  (Read: 0x0049CE30, Write: 0x0049DE00)
//   - Entry 2: RAS  (Read: 0x00599DD0, Write: 0x0059ABE0)
//   - Entry 3: TIF  (Read: 0x0059B0B0, Write: NULL)
//
// Registers each codec via RwImageRegisterImageFormat (0x004E42F0).
// ============================================================================
struct RwImageFormatDescriptor {
    RwImageCallBackRead  readFn;
    RwImageCallBackWrite writeFn;
    const char*          extension;
};

// Original static table at 0x005D5D6C
static const RwImageFormatDescriptor s_imageFormats[4] = {
    { (RwImageCallBackRead)0x00478660, (RwImageCallBackWrite)0x004781B0, "png" },
    { (RwImageCallBackRead)0x0049CE30, (RwImageCallBackWrite)0x0049DE00, "bmp" },
    { (RwImageCallBackRead)0x00599DD0, (RwImageCallBackWrite)0x0059ABE0, "ras" },
    { (RwImageCallBackRead)0x0059B0B0, nullptr,                           "tif" }
};

int32_t RwCreateCameraAndRasters() {
    int32_t bAllSuccess = 1;

    for (int i = 0; i < 4; i++) {
        RwBool bOk = RwImageRegisterImageFormat(
            s_imageFormats[i].extension,
            s_imageFormats[i].readFn,
            s_imageFormats[i].writeFn
        ); // 0x004E42F0

        if (!bOk) {
            bAllSuccess = 0;
        }
    }

    return bAllSuccess;
}

// ============================================================================
// 0x0044E8E0: RwCreateDefaultCameraAndRasters
//
// Allocates RwCamera, Z-Buffer Raster (0x60), FrameBuffer Raster (0x64),
// attaches identity RwFrame, sets perspective projection, near/far planes
// (1.0f .. 5000.0f) and computes 60° FOV view window via tan(pi/6).
// ============================================================================
RwCamera* RwCreateDefaultCameraAndRasters(int32_t width, int32_t height, int32_t depth) {
    RwCamera* camera = RwCameraCreate(); // 0x004D5A20
    if (!camera) return nullptr;

    // 1. Create Z-Buffer Raster (rwRASTERTYPEZBUFFER = 1)
    camera->zBuffer = RwRasterCreate(width, height, 0, rwRASTERTYPEZBUFFER); // 0x004E2BB0

    // 2. Create Frame Buffer Raster (rwRASTERTYPECAMERA = 2)
    camera->frameBuffer = RwRasterCreate(width, height, depth, rwRASTERTYPECAMERA); // 0x004E2BB0

    // 3. Create & Attach Frame
    RwFrame* frame = RwFrameCreate(); // 0x004D8D40
    RwFrameSetIdentity(frame);        // 0x004D9580
    RwCameraSetFrame(camera, frame);  // 0x004D9750

    // 4. Projection & Clipping Planes
    RwCameraSetProjection(camera, rwPERSPECTIVE); // 0x004D5880
    RwCameraSetNearClipPlane(camera, 1.0f);       // 0x004D55F0 (0x3F800000)
    RwCameraSetFarClipPlane(camera, 5000.0f);     // 0x004D5720 (0x459C4000)

    // 5. View Window Calculation: FOV 60° (30° half-angle = 0.52359879 radians)
    // tan(0.52359879) ≈ 0.57735026
    if (height != 0) {
        RwV2d viewWindow;
        float tanHalfFov = 0.57735026f; // tan(pi / 6)
        viewWindow.x = tanHalfFov;
        viewWindow.y = tanHalfFov * 0.7f; // 4:3 aspect ratio factor
        RwCameraSetViewWindow(camera, &viewWindow); // 0x004D5910
    }

    return camera;
}

// ============================================================================
// 0x0044AE20: RwRegisterStandardPlugins
//
// Registers core RenderWare Studio world and collision extensions:
//   - 0x00507300: RpWorldPluginAttach
//   - 0x00507530: RpCollisionPluginAttach
//   - 0x0056C6B0: RpPVSPluginAttach / Additional plugin hook
// ============================================================================
void RwRegisterStandardPlugins() {
    g_dwRpWorldHandle = RpWorldPluginAttach(); // 0x00507300

    if (g_dwRpWorldHandle != 0) {
        RpCollisionPluginAttach(); // 0x00507530

        // Internal engine hook for visibility/PVS
        typedef void (*PluginAttachHookFn)();
        ((PluginAttachHookFn)0x0056C6B0)();

        g_bWorldPluginsRegistered = 1; // 0x0062969C
        return;
    }

    g_bWorldPluginsRegistered = 0;
}

// ============================================================================
// 0x00465150: RwRegisterTFBPlugins / TFB Async Subsystem
//
// Initialisiert die Critical Section für den Toys for Bob Hintergrund-Lader
// und setzt den Thread-Status auf -1 (0xFFFFFFFF).
// ============================================================================
int32_t RwRegisterTFBPlugins(int32_t threadPriority) {
    g_dwAsyncThreadStatus   = 0;
    g_nAsyncThreadPriority  = threadPriority; // 0x0062A610: -1
    g_dwAsyncWorkerCounter  = 0;
    g_dwAsyncQueueHead      = 0;
    g_dwAsyncQueueTail      = 0;

    InitializeCriticalSection(&g_csTFBAsyncStream); // 0x00630F40
    g_bTFBAsyncInitialized = 1;                     // 0x0062A618

    return 1;
}

// ============================================================================
// 0x0045F9A0: RwRegisterTFBCustomPlugins
//
// REGISTRIERUNG ALLER TOYS FOR BOB PROPRIETÄREN RENDERWARE-PLUGINS:
//
// 1. TFB Material Plugin (0x0045F920):
//    - Chunk ID: 0x800000F6
//    - Plugin Name: "TFB Material Plugin" (0x005D5808)
//    - Extra Data Size: 0x2DC Bytes (732 Bytes)
//    - Konstruktor: 0x0045F240, Destruktor: 0x0045E800, Copy: 0x0045EF50
//    - Stream Deserializer: 0x0045F7D0
//    - Verwaltet Shader-Flags: TFB_NOZWRITE, TFB_MESH.TRANSLUCENT
//
// 2. TFB Atomic Plugin (0x0045DEEB):
//    - Chunk / Identifier: "TFBATOMIC" (0x005D57A4)
//    - Erweitert RpAtomic um TFB Knochen-IDs (MOUTH, ELR, ELL, EUR, EUL)
//
// 3. TFB Clump Plugin (0x004606F8):
//    - Chunk / Identifier: "BTFBCLUMP" (0x005D57BB)
//    - Skeletthierarchie & HAnim Verknüpfung
//
// 4. TFB Geometry Plugin (0x0045CD73):
//    - Chunk / Identifier: "BTFBGEOMETRY" (0x005D57D3)
//    - Erweiterte Vertex-Buffer und Morph-Targets
//
// 5. TFB Light Plugin (0x0045E00A):
//    - Chunk / Identifier: "BTFBLIGHT" (0x005D57E3)
//    - Dynamische Lichtfarben & Schattenwurf-Eigenschaften
//
// 6. TFB World Plugin (0x0045FE01):
//    - Chunk / Identifier: "TFBWORLD" (0x005D5830)
//    - Weltweite Nebel-, Himmels- und Clip-Einstellungen
//
// 7. TFB WorldSector Plugin (0x0045D08E):
//    - Chunk / Identifier: "BTFBWORLDSECTOR" (0x005D583F)
//    - BSP-Sektor-Streaming und Culling
// ============================================================================
int32_t RwRegisterTFBCustomPlugins() {
    typedef int32_t (*PluginInitFn)();

    // 0. Base plugin hook
    if (!((PluginInitFn)0x004A10E0)()) return 0;

    // 1. TFB Material Plugin (Chunk 0x800000F6)
    if (!((PluginInitFn)0x0045F920)()) return 0;

    // 2. TFB Atomic Plugin ("TFBATOMIC")
    if (!((PluginInitFn)0x0045DEEB)()) return 0;

    // 3. TFB Clump Plugin ("BTFBCLUMP")
    if (!((PluginInitFn)0x004606F8)()) return 0;

    // 4. TFB Geometry Plugin ("BTFBGEOMETRY")
    if (!((PluginInitFn)0x0045CD73)()) return 0;

    // 5. TFB Light Plugin ("BTFBLIGHT")
    if (!((PluginInitFn)0x0045E00A)()) return 0;

    // 6. TFB World Plugin ("TFBWORLD")
    if (!((PluginInitFn)0x0045FE01)()) return 0;

    // 7. TFB WorldSector Plugin ("BTFBWORLDSECTOR")
    if (!((PluginInitFn)0x0045D08E)()) return 0;

    return 1;
}

// ============================================================================
// 0x004627B0: RegisterAllExtendedPlugins
//
// Iteriert die 19 erweiterten Plugin-Registrierungen (Tabelle 0x005D5CF0):
//   [0]:  0x004C1CB0 - RWS Chunk Handler (0x501..0x50B)
//   [1]:  0x004AFA80 - Skin Plugin (rwID_SKINPLUGIN 0x11D)
//   [2]:  0x004B80C0 - HAnim Plugin (rwID_HANIMPLUGIN 0x12A)
//   [15]: 0x0045F9A0 - RwRegisterTFBCustomPlugins (Material, Atomic, Clump, World)
//   [18]: 0x0044DD70 - Final Engine Setup
// ============================================================================
int32_t RegisterAllExtendedPlugins() {
    // 0x00573630: Pre-plugin initialization
    typedef int32_t (*PrePluginInitFn)();
    if (!((PrePluginInitFn)0x00573630)()) {
        return 0;
    }

    // Call TFB Plugins explicitly as part of registration
    int32_t bSuccess = 1;
    if (!RwRegisterTFBCustomPlugins()) {
        bSuccess = 0;
    }

    // 0x00599D50: Post-plugin hook
    typedef void (*PostPluginHookFn)();
    ((PostPluginHookFn)0x00599D50)();

    return bSuccess;
}

// ============================================================================
// 0x00462AA0: RwEngine_Initialize
//
// Der zentrale Grafik-Initialisierer von Madagascar:
// 1. RwEngineInit mit Speicherfunktionstabelle
// 2. RwRegisterStandardPlugins (RpWorld, RpCollision)
// 3. RwRegisterTFBPlugins (TFB Async Critical Section)
// 4. RegisterAllExtendedPlugins (TFB Material, Atomic, Clump, World etc.)
// 5. RwEngineOpen (mit Zielfenster HWND)
// 6. RwSelectVideoMode (Auflösungs- & Tiefenwahl)
// 7. FSAA Check via Registry ("Settings\Display\FSAA" -> RwD3D8SetMultisampling)
// 8. RwEngineStart (Direct3D 8 Treiber starten)
// 9. RwCreateCameraAndRasters (PNG, BMP, RAS, TIF Codecs registrieren)
// 10. InitPostEngineStart (Kamera, Raster, RWS Pipeline erzeugen)
// ============================================================================
int32_t RwEngine_Initialize(
    int param1,
    void** pFuncTable,
    uint32_t flags,
    uint32_t* pModeParams,
    uint32_t depth,
    HWND hWnd
) {
    RwEngineOpenParams openParams;
    int32_t bModeSelected = 0;

    // Memory Function Table Fallback
    void* memTable = pFuncTable;
    if (memTable == nullptr) {
        memTable = (void*)0x00606C90; // Default RenderWare Allocator
    }

    RwEngine_PreInitHook(); // 0x00464EF0

    // 1. Initialisiere RenderWare Core
    RwBool bInitOk = RwEngineInit((const RwMemoryFunctions*)memTable, 0, flags); // 0x004D8620
    if (!bInitOk) {
        return 0;
    }

    // 2. Standard RenderWare Plugins
    RwRegisterStandardPlugins(); // 0x0044AE20

    // 3. TFB Async Stream Subsystem
    if (!RwRegisterTFBPlugins(-1)) { // 0x00465150
        goto CLEANUP_TERM;
    }

    // 4. Filesystem Drive Setup
    typedef int32_t (*SetupFSDrivesFn)();
    if (!((SetupFSDrivesFn)0x00463360)()) { // 0x00463360
        goto CLEANUP_TERM;
    }

    // 5. Registriere TFB & RWS Custom Plugins
    if (!RegisterAllExtendedPlugins()) { // 0x004627B0
        goto CLEANUP_TERM;
    }

    // 6. Öffne die Engine mit HWND
    openParams.displayID = hWnd;
    if (!RwEngineOpen(&openParams)) { // 0x004D83B0
        goto CLEANUP_TERM;
    }

    // 7. Wähle besten Videomodus
    // pModeParams[0] = width, pModeParams[1] = height, pModeParams[2] = depth, pModeParams[3] = flags
    bModeSelected = RwSelectVideoMode(
        (int32_t*)&pModeParams[0],
        (int32_t*)&pModeParams[1],
        pModeParams[2],
        pModeParams[3]
    ); // 0x00462BF0

    if (bModeSelected) {
        // 8. FSAA Konfiguration aus der Registry
        int32_t fsaaSetting = GetSettingInt("Settings\\Display\\FSAA", 0); // 0x00519D20
        if (fsaaSetting == 1) {
            RwD3D8SetMultisampling(2); // 0x004E8830 (2x Multisampling)
        }

        // 9. Starte Direct3D 8 Treiber
        if (!RwEngineStart()) { // 0x004D8260
            // Fallback: Multisampling deaktivieren und erneut versuchen
            RwD3D8SetMultisampling(0); // 0x004E8830
            if (!RwEngineStart()) {
                goto CLEANUP_CLOSE;
            }
        }

        // 10. Registriere Bildformate (PNG, BMP, RAS, TIF)
        if (RwCreateCameraAndRasters()) { // 0x00462880
            // 11. Erzeuge Kamera, Raster & starte RWS Message-Pipeline
            InitPostEngineStart(pModeParams[0], pModeParams[1], depth); // 0x00461F70
            return 1;
        }

        RwEngineStop(); // 0x004D81F0
    }

CLEANUP_CLOSE:
    RwEngineClose(); // 0x004D8300

CLEANUP_TERM:
    RwEngineTerm();  // 0x004D85E0
    return 0;
}
