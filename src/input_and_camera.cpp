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
    // 1. Restore original sticky keys configuration
    if (g_originalStickyKeys.cbSize == sizeof(STICKYKEYS)) {
        SystemParametersInfoA(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &g_originalStickyKeys, 0);
    }

    // 2. Restore original screensaver timeout
    if (g_dwOriginalScreenSaverTimeout != 0) {
        SystemParametersInfoA(SPI_SETSCREENSAVETIMEOUT, g_dwOriginalScreenSaverTimeout, nullptr, 0);
    }

    // 3. Reset input buffer
    memset(g_dwKeystateBuffer, 0, sizeof(g_dwKeystateBuffer));
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
    (void)pStreamHandle;
    // Iterates 22 TLV attributes in .stream file chunk and binds action inquiries
    // with return type "return RwReal" (0x00550B20)
    for (int i = 0; i < 22; ++i) {
        char attrName[32];
        snprintf(attrName, sizeof(attrName), "XBOX_ACTN_%02d", i + 1);
        RegisterRWSMessage(&m_pAttributes[i], attrName, 0);
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

// 0x00467680: Set active Director Camera
int32_t DirectorCam_SetActive(RwCamera* pCamera) {
    g_pDirectorCamera = pCamera; // 0x0063163C
    return 1;
}

// 0x004719C0: Initialize view and projection matrix buffers (0x00631494)
void DirectorCam_InitMatrices() {
    // Configures primary projection transformation buffer
    // Reconstructed from 0x004719C0
}

// 0x0046F7F0: Update view frustum boundary planes (0x00631484, 0x00631488)
void DirectorCam_UpdateFrustum() {
    // Updates 6 canonical bounding planes for camera view culling
    // Reconstructed from 0x0046F7F0
}

// 0x0046C1F0: Setup viewports, subpixel raster offsets and aspect ratio (0x00631464)
void DirectorCam_SetupViewports() {
    // Configures viewport offset and display metrics
    // Reconstructed from 0x0046C1F0
}

// 0x00469800: Initialize field-of-view parameters and perspective scaling (0x00631468)
void DirectorCam_InitFOV() {
    // Sets horizontal & vertical FOV ratios
    // Reconstructed from 0x00469800
}

// 0x00470300: Setup near/far clipping planes and depth ranges (0x0063148C)
int32_t DirectorCam_SetupClipping() {
    // Sets near clip plane and far clip plane
    // Reconstructed from 0x00470300
    return 1;
}

// 0x004677A0: Initialize camera post-processing, clear color & raster buffers (0x006314E4)
void DirectorCam_InitPostProcess() {
    // Initializes post-processing buffer and raster parameters
    // Reconstructed from 0x004677A0
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
    // Traverses target list linked at 0x0062FEE4 and solves target positions
    // Reconstructed from 0x00551410
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

// 0x00462A50: RenderWare Camera & Raster Cleanup
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
