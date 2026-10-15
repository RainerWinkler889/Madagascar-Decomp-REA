/**
 * @file input_and_camera.cpp
 * @brief Madagascar (PC, 2005) - Input Subsystem, Controllers & Director Camera Pipeline
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 */

#include "input_and_camera.h"
#include "rws_core.h"
#include "audio_and_frame.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// Global State Variables in Game.exe .data
// ============================================================================

// 0x0062F13C: Input Module HINSTANCE
HINSTANCE g_hInputInstance = nullptr; // 0x0062F13C

// 0x0062ED30: Keystate buffer (255 DWORDs = 1020 bytes)
uint32_t g_dwKeystateBuffer[255] = { 0 }; // 0x0062ED30

// 0x0062F154: Original mouse speed setting
int32_t g_nOriginalMouseSpeed = 10; // 0x0062F154

// 0x0062F12C: Original sticky keys configuration
STICKYKEYS g_originalStickyKeys = { sizeof(STICKYKEYS), 0 }; // 0x0062F12C

// 0x0062F14C: Original screensaver timeout
uint32_t g_dwOriginalScreenSaverTimeout = 0; // 0x0062F14C

// 0x0061798C: Global GameController Singleton Pointer
GameController* g_pGameController = nullptr; // 0x0061798C

// 0x0063163C: Active Director Camera Pointer
RwCamera* g_pDirectorCamera = nullptr; // 0x0063163C

// Director Camera Raster & ViewWindow Metrics (0x00631568..0x00631574)
float g_fDirectorCamViewWindowX  = 1.0f;   // 0x00631568
float g_fDirectorCamViewWindowY  = 0.75f;  // 0x0063156C
float g_fDirectorCamRasterWidth  = 800.0f; // 0x00631570
float g_fDirectorCamRasterHeight = 600.0f; // 0x00631574

// ============================================================================
// 1. Windows & DirectInput 8 Input Subsystem (0x0051A070 .. 0x0051A230)
// ============================================================================

// 0x0051A070: DirectInput_Init
bool DirectInput_Init(HWND hWnd) {
    (void)hWnd;

    // 1. Get module instance
    g_hInputInstance = GetModuleHandleA(nullptr); // 0x0062F13C

    // 2. Clear keystate buffer (0xFF DWORDs = 255 * 4 = 1020 bytes)
    memset(g_dwKeystateBuffer, 0, sizeof(g_dwKeystateBuffer)); // 0x0062ED30

    // 3. Query mouse speed (SPI_GETMOUSESPEED = 0x70)
    SystemParametersInfoA(SPI_GETMOUSESPEED, 0, &g_nOriginalMouseSpeed, 0); // 0x0062F154

    // 4. Query sticky keys (SPI_GETSTICKYKEYS = 0x3A)
    g_originalStickyKeys.cbSize = sizeof(STICKYKEYS); // 0x0062F12C
    SystemParametersInfoA(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &g_originalStickyKeys, 0);

    // 5. Disable sticky keys shortcuts while game is active (clear bit 1 & bit 2: & 0xFFFFFFF9)
    STICKYKEYS disabledSticky = g_originalStickyKeys;
    disabledSticky.dwFlags &= 0xFFFFFFF9;
    SystemParametersInfoA(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &disabledSticky, 0); // SPI_SETSTICKYKEYS = 0x3B

    // 6. Query screensaver timeout (SPI_GETSCREENSAVETIMEOUT = 0x0E)
    BOOL bResult = SystemParametersInfoA(SPI_GETSCREENSAVETIMEOUT, 0, &g_dwOriginalScreenSaverTimeout, 0);
    if (!bResult) {
        g_dwOriginalScreenSaverTimeout = 0; // 0x0062F14C
    }

    return true;
}

// 0x0051A100: ProcessWindowsInputMessages
void ProcessWindowsInputMessages() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_bQuit = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    // Keep screensaver alive / reset idle timer
    if (g_dwOriginalScreenSaverTimeout != 0) {
        SystemParametersInfoA(SPI_SETSCREENSAVETIMEOUT, g_dwOriginalScreenSaverTimeout, nullptr, 0);
    }
}

// 0x0051A1A0: CleanupInput
void CleanupInput() {
    // 1. Reset input buffer / release acquisitions (0x0062ED30 .. 0x0062F12C)
    for (int i = 0; i < 255; ++i) {
        if (g_dwKeystateBuffer[i] != 0) {
            g_dwKeystateBuffer[i] = 0;
        }
    }

    // 2. Restore original sticky keys configuration (0x0062F12C)
    SystemParametersInfoA(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &g_originalStickyKeys, 0);

    // 3. Restore original screensaver timeout
    if (g_dwOriginalScreenSaverTimeout != 0) {
        SystemParametersInfoA(SPI_SETSCREENSAVETIMEOUT, g_dwOriginalScreenSaverTimeout, nullptr, 0);
    }

    // 4. Destroy main window if still alive (0x0062F138)
    if (g_hMainWindow != nullptr) {
        DestroyWindow(g_hMainWindow);
        g_hMainWindow = nullptr;
    }
}

// 0x0051A210: RestoreAccessibilityShortcuts
void RestoreAccessibilityShortcuts() {
    if (g_originalStickyKeys.cbSize == sizeof(STICKYKEYS)) {
        SystemParametersInfoA(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &g_originalStickyKeys, 0);
    }
}

// 0x0051A230: PollInputDevices
void PollInputDevices() {
    if (!g_bActive) return;

    // DirectInput / Win32 keystate polling
    BYTE keyState[256];
    if (GetKeyboardState(keyState)) {
        for (int i = 0; i < 255; ++i) {
            if (keyState[i] & 0x80) {
                g_dwKeystateBuffer[i] |= 1; // Key is currently down
            } else {
                g_dwKeystateBuffer[i] &= ~1; // Key is released
            }
        }
    }

    // Poll controller if available
    if (g_pGameController != nullptr) {
        g_pGameController->Poll();
    }
}

// ============================================================================
// 2. GameController Class Implementation (0x00402600)
// ============================================================================

static const char* const s_szInquiryActionNames[CTRL_INQ_COUNT] = {
    "INQ_ACTN_START",        // 0
    "INQ_ACTN_SELECT",       // 1
    "INQ_ACTN_TRICK",        // 2
    "INQ_ACTN_FIRE",         // 3
    "INQ_ACTN_JUMP",         // 4
    "INQ_ACTN_DODGE",        // 5
    "INQ_SHOULDER_LEFT",     // 6
    "INQ_SHOULDER_RIGHT",    // 7
    "INQ_ACTN_SHOULDER_L2",  // 8
    "INQ_ACTN_SHOULDER_R2",  // 9
    "INQ_ACTN_PAD_L",        // 10
    "INQ_ACTN_PAD_R",        // 11
    "INQ_ACTN_PAD_U",        // 12
    "INQ_ACTN_PAD_D",        // 13
    "INQ_ACTN_TURN",         // 14
    "INQ_ACTN_FWD",          // 15
    "INQ_LPAD_BUTTON",       // 16
    "INQ_CAMERA_ROTATE",     // 17
    "INQ_CAMERA_PITCH",      // 18
    "INQ_RPAD_BUTTON"        // 19
};

// 0x00402600: GameController Constructor
GameController::GameController() {
    m_pVTable          = (void*)0x005CA5A0; // VTable 0x005CA5A0
    m_dwActive         = 1;                 // +0x04: 1
    m_pSecondaryVTable = (void*)0x005CA590; // +0x08: 0x005CA590
    
    for (int i = 0; i < 6; ++i) {
        m_reserved0[i] = 0;
    }

    m_dwButtonMask = 0;
    m_fStickLeftX  = 0.0f;
    m_fStickLeftY  = 0.0f;
    m_fStickRightX = 0.0f;
    m_fStickRightY = 0.0f;

    // Register all 20 canonical action inquiries via RegisterRWSMessage (0x00550440)
    for (int i = 0; i < CTRL_INQ_COUNT; ++i) {
        m_pActionHandlers[i] = nullptr;
        m_fActionValues[i]   = 0.0f;
        RegisterRWSMessage(&m_pActionHandlers[i], s_szInquiryActionNames[i], 0);
    }

    // Bind singleton pointer (0x0061798C)
    g_pGameController = this;
}

GameController::~GameController() {
    if (g_pGameController == this) {
        g_pGameController = nullptr;
    }
}

// 0x00402230: Slot 1: Poll
void GameController::Poll() {
    // Read keyboard arrow keys / WASD as fallback
    m_fStickLeftX = 0.0f;
    m_fStickLeftY = 0.0f;

    if (g_dwKeystateBuffer['A'] & 1 || g_dwKeystateBuffer[VK_LEFT] & 1)  m_fStickLeftX -= 1.0f;
    if (g_dwKeystateBuffer['D'] & 1 || g_dwKeystateBuffer[VK_RIGHT] & 1) m_fStickLeftX += 1.0f;
    if (g_dwKeystateBuffer['W'] & 1 || g_dwKeystateBuffer[VK_UP] & 1)    m_fStickLeftY += 1.0f;
    if (g_dwKeystateBuffer['S'] & 1 || g_dwKeystateBuffer[VK_DOWN] & 1)  m_fStickLeftY -= 1.0f;

    m_fActionValues[CTRL_INQ_FWD]  = m_fStickLeftY;
    m_fActionValues[CTRL_INQ_TURN] = m_fStickLeftX;

    // Action buttons
    m_fActionValues[CTRL_INQ_JUMP]  = (g_dwKeystateBuffer[VK_SPACE] & 1) ? 1.0f : 0.0f;
    m_fActionValues[CTRL_INQ_FIRE]  = (g_dwKeystateBuffer['E'] & 1 || g_dwKeystateBuffer[VK_RETURN] & 1) ? 1.0f : 0.0f;
    m_fActionValues[CTRL_INQ_TRICK] = (g_dwKeystateBuffer['Q'] & 1) ? 1.0f : 0.0f;
    m_fActionValues[CTRL_INQ_DODGE] = (g_dwKeystateBuffer[VK_SHIFT] & 1) ? 1.0f : 0.0f;
    m_fActionValues[CTRL_INQ_START] = (g_dwKeystateBuffer[VK_ESCAPE] & 1) ? 1.0f : 0.0f;
}

// 0x00402550: Slot 2: Reset
void GameController::Reset() {
    for (int i = 0; i < CTRL_INQ_COUNT; ++i) {
        m_fActionValues[i] = 0.0f;
    }
    m_dwButtonMask = 0;
    m_fStickLeftX  = 0.0f;
    m_fStickLeftY  = 0.0f;
    m_fStickRightX = 0.0f;
    m_fStickRightY = 0.0f;
}

// 0x004022D0: Slot 3: GetDeviceState
void* GameController::GetDeviceState() {
    return &m_fActionValues[0];
}

// 0x00401000: Slot 4: HandleEvent
int32_t GameController::HandleEvent(int32_t eventId) {
    (void)eventId;
    return 1;
}

// 0x0043DC80: Slot 5: ProcessStep
int32_t GameController::ProcessStep(uint32_t* pToken) {
    if (pToken != nullptr) {
        *pToken = 0;
    }
    return 0;
}

float GameController::QueryAction(EControllerInquiryAction action) const {
    if (action >= 0 && action < CTRL_INQ_COUNT) {
        return m_fActionValues[action];
    }
    return 0.0f;
}

void GameController::SetActionValue(EControllerInquiryAction action, float val) {
    if (action >= 0 && action < CTRL_INQ_COUNT) {
        m_fActionValues[action] = val;
    }
}

// ============================================================================
// 3. CXboxStndController Class Implementation (0x00404E70)
// ============================================================================

CXboxStndController::CXboxStndController() {
    m_pVTable        = (void*)0x005CAD80; // VTable 0x005CAD80
    m_dwFlags        = 0;
    m_fStickDeadzone = 0.25f; // 0x005CA654 (0.25f default deadzone)
    m_fSensitivity   = 1.0f;
    m_dwButtons      = 0;
    m_fTriggerLeft   = 0.0f;
    m_fTriggerRight  = 0.0f;
    m_fAxisX         = 0.0f;
    m_fAxisY         = 0.0f;
    m_fRotateX       = 0.0f;
    m_fRotateY       = 0.0f;

    for (int i = 0; i < 22; ++i) {
        m_pAttributes[i] = nullptr;
    }
}

CXboxStndController::~CXboxStndController() {
}

// 0x00404E70: Stream Deserializer
void CXboxStndController::DeserializeStream(void* pStreamHandle) {
    if (pStreamHandle == nullptr) return;

    typedef char (*StreamAtEndFn)();
    typedef uint32_t (*ReadTagFn)();
    typedef void* (*ReadPayloadFn)();
    typedef void (*NextAttrFn)();
    typedef void (*BindAttrFn)(void*, void*, const char*);

    StreamAtEndFn  pfnStreamAtEnd  = (StreamAtEndFn)0x0054FE20;
    ReadTagFn      pfnReadTag      = (ReadTagFn)0x0054FDF0;
    ReadPayloadFn  pfnReadPayload  = (ReadPayloadFn)0x0054FEC0;
    NextAttrFn     pfnNextAttr     = (NextAttrFn)0x0054FEA0;
    BindAttrFn     pfnBindAttr     = (BindAttrFn)0x00550B20;

    // Stream validation hooks (0x00410380, 0x00550020)
    typedef void (*StreamHookFn)(void*);
    typedef void (*StreamClassHookFn)(void*, void*);
    ((StreamHookFn)0x00410380)(pStreamHandle);
    ((StreamClassHookFn)0x00550020)(pStreamHandle, (void*)0x00602868);

    while (pfnStreamAtEnd() == 0) {
        pfnReadPayload();
        uint32_t tag = pfnReadTag();

        if (tag >= 1 && tag <= 22) {
            uint8_t* pAttrDest = ((uint8_t*)this) + 0x1C + (tag - 1) * 0x18;
            void* pPayload = pfnReadPayload();
            void* pAttrSource = (void*)((uintptr_t)pPayload + 8);

            pfnBindAttr(pAttrDest, pAttrSource, "return RwReal");

            if (tag - 1 < 22) {
                m_pAttributes[tag - 1] = pAttrSource;
            }
        }

        pfnNextAttr();
    }
}

bool CXboxStndController::IsButtonPressed(uint32_t buttonMask) const {
    return (m_dwButtons & buttonMask) != 0;
}

float CXboxStndController::GetAxis(uint32_t axisIndex) const {
    switch (axisIndex) {
        case 0: return m_fAxisX;
        case 1: return m_fAxisY;
        case 2: return m_fRotateX;
        case 3: return m_fRotateY;
        case 4: return m_fTriggerLeft;
        case 5: return m_fTriggerRight;
        default: return 0.0f;
    }
}

// ============================================================================
// 4. Director Camera Pipeline Implementation (0x004676A0 & Chunk 0x71D)
// ============================================================================

// Global buffers and state in Game.exe .data (0x00631460 .. 0x0063166C)
static void* g_pDirectorCamViewportData2 = nullptr; // 0x00631460
static void* g_pDirectorCamViewportData1 = nullptr; // 0x00631464
static void* g_pDirectorCamFOVBuf1       = nullptr; // 0x00631468
static void* g_pDirectorCamFontData     = nullptr; // 0x0063146C
static uint32_t g_dwDirectorCamFontCount = 0;       // 0x00631470
static void* g_pDirectorCamFOVBuf2       = nullptr; // 0x00631474
static uint32_t g_dwDirectorCamMetric1   = 0;       // 0x00631478
static uint32_t g_dwDirectorCamMetric2   = 0;       // 0x0063147C
static uint32_t g_dwDirectorCamMetric3   = 0;       // 0x00631480
static void* g_pDirectorCamFrustumPlanes = nullptr; // 0x00631484
static void* g_pDirectorCamFrustumState  = nullptr; // 0x00631488
static void* g_pDirectorCamClipBuffer    = nullptr; // 0x0063148C
static void* g_pDirectorCamMatrixBuf1   = nullptr; // 0x00631494
static void* g_pDirectorCamMatrixBuf2   = nullptr; // 0x00631498
static uint16_t g_directorCamIndices[384] = { 0 }; // 0x00631160 (0x180 elements)
static void* g_pPostProcessSlots[32]     = { nullptr }; // 0x006314E4
static uint16_t g_postProcessIndexBuffer[256] = { 0 };  // 0x00630F60
static uint32_t g_dwPostProcessFlags     = 0;       // 0x006314B0

// 0x00467680: Set active Director Camera
int32_t DirectorCam_SetActive(RwCamera* pCamera) {
    g_pDirectorCamera = pCamera; // 0x0063163C
    return 1;
}

// 0x004719C0: Initialize view and projection matrix buffers (0x00631494, 0x00631498)
void DirectorCam_InitMatrices() {
    typedef void* (*AllocBufFn)(uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t);
    AllocBufFn pfnAlloc = (AllocBufFn)0x004E1880;

    uint32_t p1 = *(uint32_t*)0x00606E68;
    uint32_t p2 = *(uint32_t*)0x00606E6C;
    g_pDirectorCamMatrixBuf1 = pfnAlloc(8, p1, 4, p2, (void*)0x0062A70C, 0x40190);

    uint32_t p3 = *(uint32_t*)0x00606E60;
    uint32_t p4 = *(uint32_t*)0x00606E64;
    g_pDirectorCamMatrixBuf2 = pfnAlloc(0x14, p3, 4, p4, (void*)0x0062A6E8, 0x40190);
}

// 0x0046F7F0: Update view frustum boundary planes (0x00631484, 0x00631488)
void DirectorCam_UpdateFrustum() {
    typedef void* (*AllocBufFn)(uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t);
    AllocBufFn pfnAlloc = (AllocBufFn)0x004E1880;

    uint32_t p1 = *(uint32_t*)0x00606E50;
    uint32_t p2 = *(uint32_t*)0x00606E54;
    g_pDirectorCamFrustumPlanes = pfnAlloc(0x1C, p1, 4, p2, (void*)0x0062A690, 0x401A1);

    typedef void* (*CreateFrustumFn)();
    typedef void (*SetupFrustumFn)(void*);
    g_pDirectorCamFrustumState = ((CreateFrustumFn)0x0046F590)();
    ((SetupFrustumFn)0x0046F610)(g_pDirectorCamFrustumState);
}

// 0x0046C1F0: Setup viewports, subpixel raster offsets and aspect ratio (0x00631464)
void DirectorCam_SetupViewports() {
    uintptr_t devPtr = *(uintptr_t*)0x0062AC18;
    if (devPtr != 0) {
        typedef void* (*DevAllocFn)(uint32_t, uint32_t);
        DevAllocFn pfnDevAlloc = *(DevAllocFn*)(devPtr + 0x108);
        if (pfnDevAlloc != nullptr) {
            g_pDirectorCamViewportData1 = pfnDevAlloc(0x2400, 0x40190);
        }
    }

    typedef void* (*AllocBufFn)(uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t);
    AllocBufFn pfnAlloc = (AllocBufFn)0x004E1880;
    uint32_t p1 = *(uint32_t*)0x00606E48;
    uint32_t p2 = *(uint32_t*)0x00606E4C;
    g_pDirectorCamViewportData2 = pfnAlloc(0x78, p1, 0x10, p2, (void*)0x0062A66C, 0x401A2);
}

// 0x00469800: Initialize field-of-view parameters and perspective scaling (0x00631468)
void DirectorCam_InitFOV() {
    typedef void* (*AllocBufFn)(uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t);
    AllocBufFn pfnAlloc = (AllocBufFn)0x004E1880;

    uint32_t p1 = *(uint32_t*)0x00606E00;
    uint32_t p2 = *(uint32_t*)0x00606E04;
    g_pDirectorCamFOVBuf1 = pfnAlloc(0x244, p1, 4, p2, (void*)0x0062A648, 0x40199);
    if (!g_pDirectorCamFOVBuf1) return;

    uint32_t p3 = *(uint32_t*)0x00606E08;
    uint32_t p4 = *(uint32_t*)0x00606E0C;
    g_pDirectorCamFOVBuf2 = pfnAlloc(0x0C, p3, 4, p4, (void*)0x0062A624, 0x40190);
    if (!g_pDirectorCamFOVBuf2) return;

    uintptr_t devPtr = *(uintptr_t*)0x0062AC18;
    if (devPtr != 0) {
        typedef void* (*DevAllocFn)(uint32_t, uint32_t);
        DevAllocFn pfnDevAlloc = *(DevAllocFn*)(devPtr + 0x108);
        if (pfnDevAlloc != nullptr) {
            g_pDirectorCamFontData = pfnDevAlloc(0x51, 0x1040190);
        }
    }
    if (!g_pDirectorCamFontData) return;

    g_dwDirectorCamFontCount = 0x51;
    typedef void (*InitFontFn)(const char*);
    ((InitFontFn)0x004699B0)("fonts\\");

    int16_t baseVertex = 0;
    for (int i = 0; i < 0x180; i += 6) {
        g_directorCamIndices[i + 0] = baseVertex;
        g_directorCamIndices[i + 1] = (int16_t)(baseVertex + 2);
        g_directorCamIndices[i + 2] = (int16_t)(baseVertex + 3);
        g_directorCamIndices[i + 3] = baseVertex;
        g_directorCamIndices[i + 4] = (int16_t)(baseVertex + 3);
        g_directorCamIndices[i + 5] = (int16_t)(baseVertex + 1);
        baseVertex = (int16_t)(baseVertex + 4);
    }

    g_dwDirectorCamMetric1 = 0;
    typedef uint32_t (*GetTickFn)();
    g_dwDirectorCamMetric2 = ((GetTickFn)0x004DB9C0)();
    *(uint32_t*)0x00631CBC = 0;
    *(uint32_t*)0x00631CC0 = 0;
    g_dwDirectorCamMetric3 = g_dwDirectorCamMetric2;

    typedef void (*RegisterCallbackFn)(void*);
    ((RegisterCallbackFn)0x00469A50)((void*)0x0046A200);
}

// 0x00470300: Setup near/far clipping planes and depth ranges (0x0063148C)
int32_t DirectorCam_SetupClipping() {
    typedef void* (*AllocBufFn)(uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t);
    AllocBufFn pfnAlloc = (AllocBufFn)0x004E1880;

    uint32_t p1 = *(uint32_t*)0x00606E58;
    uint32_t p2 = *(uint32_t*)0x00606E5C;
    g_pDirectorCamClipBuffer = pfnAlloc(0x100, p1, 0x10, p2, (void*)0x0062A6B4, 0x401A3);
    return 1;
}

// 0x004677A0: Initialize camera post-processing, clear color & raster buffers (0x006314E4)
void DirectorCam_InitPostProcess() {
    typedef uint32_t (*PostInitFn)();
    typedef void (*SetParamFloatFn)(float);
    typedef void (*SubFn)();

    *(uint32_t*)0x00631564 = 0;
    g_pPostProcessSlots[0] = (void*)(uintptr_t)((PostInitFn)0x004DB700)();
    ((SubFn)0x00466BA0)();
    ((SetParamFloatFn)0x004671E0)(0.5f);
    ((SetParamFloatFn)0x00467500)(1.0f);

    *(uint32_t*)0x00631650 = 7;
    *(float*)0x006314CC = 1.0f;
    *(float*)0x006314B8 = 1.0f;
    *(float*)0x006314A4 = 1.0f;
    *(uint32_t*)0x006314B4 = 0;
    *(uint32_t*)0x006314AC = 0;
    *(uint32_t*)0x006314A8 = 0;
    *(uint32_t*)0x006314C8 = 0;
    *(uint32_t*)0x006314C4 = 0;
    *(uint32_t*)0x006314BC = 0;
    *(uint32_t*)0x006314DC = 0;
    *(uint32_t*)0x006314D8 = 0;
    *(uint32_t*)0x006314D4 = 0;
    g_dwPostProcessFlags |= 0x20003;

    for (int i = 0; i < 256; ++i) {
        g_postProcessIndexBuffer[i] = (uint16_t)i;
    }
    for (int i = 1; i < 32; ++i) {
        g_pPostProcessSlots[i] = nullptr;
    }

    *(uint32_t*)0x0063166C = 0;
    *(uint32_t*)0x00631658 = 0;
    *(uint32_t*)0x0063165C = 0;
    *(float*)0x00631660 = 1.0f;
    *(float*)0x00631664 = 1.0f;
    *(uint32_t*)0x00631C64 = 0;
    *(uint32_t*)0x00631C68 = 0;
}

// 0x00467530: Calculate ViewWindow and resolution metrics for Director Camera
void DirectorCam_UpdateMetrics(RwCamera* pCamera) {
    if (pCamera == nullptr) return;

    RwRaster* pFrameBuffer = pCamera->frameBuffer; // +0x60
    if (pFrameBuffer != nullptr) {
        g_fDirectorCamRasterWidth  = static_cast<float>(pFrameBuffer->width);  // 0x00631570
        g_fDirectorCamRasterHeight = static_cast<float>(pFrameBuffer->height); // 0x00631574
    }

    g_fDirectorCamViewWindowX = 2.0f * pCamera->viewWindow.x; // 0x00631568
    g_fDirectorCamViewWindowY = 2.0f * pCamera->viewWindow.y; // 0x0063156C
}

// 0x004676A0: Full Director Camera Pipeline Setup (Orchestrates the 8 steps above)
void DirectorCamSetup(RwCamera* pCamera) {
    if (pCamera == nullptr) return;

    DirectorCam_SetActive(pCamera);       // 0x00467680
    DirectorCam_InitMatrices();          // 0x004719C0
    DirectorCam_UpdateFrustum();         // 0x0046F7F0
    DirectorCam_SetupViewports();        // 0x0046C1F0
    DirectorCam_InitFOV();               // 0x00469800
    DirectorCam_SetupClipping();         // 0x00470300
    DirectorCam_InitPostProcess();       // 0x004677A0
    DirectorCam_UpdateMetrics(pCamera);  // 0x00467530
}

// 0x00551410: Camera & Target Transform Solver (Called in Chunk 0x71D)
void SolveCameraTarget() {
    struct CTargetNode {
        CTargetNode* pNext;            // +0x00
        CTargetNode* pPrev;            // +0x04
        uint32_t     pad[1];           // +0x08
        uint32_t     targetData[63];   // +0x0C
        uint32_t     targetState;      // +0x108
        uint32_t     bDisabled;        // +0x110
    };

    static CTargetNode* s_pTargetListHead = nullptr; // DAT_0062FEE4
    CTargetNode* pCurr = s_pTargetListHead;

    while (pCurr != nullptr) {
        if (pCurr->bDisabled == 0) {
            typedef int32_t (*TargetSolveFn)(void*, uint32_t);
            void* pTarget = &pCurr->targetData[0];
            void** pVTable = *(void***)pTarget;
            if (pVTable != nullptr && pVTable[4] != nullptr) {
                TargetSolveFn pfnSolve = (TargetSolveFn)pVTable[4]; // +0x10
                pfnSolve(pTarget, pCurr->targetState);
            }
        }
        pCurr = pCurr->pNext;
    }
}

// 0x0044E850: RenderWare Camera & Raster Cleanup
void RwDestroyCameraAndRasters(RwCamera* pCamera) {
    if (pCamera == nullptr) return;

    if (pCamera->frameBuffer != nullptr) {
        RwRasterDestroy(pCamera->frameBuffer);
        pCamera->frameBuffer = nullptr;
    }

    if (pCamera->zBuffer != nullptr) {
        RwRasterDestroy(pCamera->zBuffer);
        pCamera->zBuffer = nullptr;
    }

    RwCameraDestroy(pCamera);
}
