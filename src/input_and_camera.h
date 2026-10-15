/**
 * @file input_and_camera.h
 * @brief Madagascar (PC, 2005) - Input Subsystem, Controllers & Director Camera Pipeline
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 *
 * Mapped Subsystems & Functions:
 *  - 0x0051A070: DirectInput_Init (Windows & DirectInput8 Subsystem Initializer)
 *  - 0x0051A100: ProcessWindowsInputMessages (Input Message Pump & Screensaver Keepalive)
 *  - 0x0051A1A0: CleanupInput (Restores System Accessibility & Screensaver Settings)
 *  - 0x0051A210: RestoreAccessibilityShortcuts
 *  - 0x0051A230: PollInputDevices (Keyboard, Mouse, Gamepad Poller)
 *
 * Controllers:
 *  - GameController:
 *    * Descriptor ASCII: "GameController" at 0x005CA3C8 (Singleton at 0x00602440)
 *    * VTable: 0x005CA5A0 / 0x005CA3A8
 *    * Constructor & Action Inquiry Registration: 0x00402600 (20 INQ_ACTN_* tokens)
 *    * Global Singleton Instance: DAT_0061798c
 *  - CXboxStndController:
 *    * Descriptor ASCII: "CXboxStndController" at 0x005CAD5C (Singleton at 0x00602868)
 *    * VTable: 0x005CAD80
 *    * Stream Deserializer: 0x00404E70 (22 TLV Controller Attributes)
 *
 * Director Camera Pipeline:
 *  - 0x00467680: DirectorCam_SetActive (Active Director Camera pointer at 0x0063163C)
 *  - 0x004719C0: DirectorCam_InitMatrices (View and projection matrix buffers at 0x00631494)
 *  - 0x0046F7F0: DirectorCam_UpdateFrustum (Frustum planes at 0x00631484, 0x00631488)
 *  - 0x0046C1F0: DirectorCam_SetupViewports (Viewport offsets and subpixel offsets at 0x00631464)
 *  - 0x00469800: DirectorCam_InitFOV (Horizontal/vertical FOV parameters at 0x00631468)
 *  - 0x00470300: DirectorCam_SetupClipping (Near and far clipping planes at 0x0063148C)
 *  - 0x004677A0: DirectorCam_InitPostProcess (Post-process parameters and clear color at 0x006314E4)
 *  - 0x00467530: DirectorCam_UpdateMetrics (ViewWindow & raster resolution at 0x00631568..0x00631574)
 *  - 0x004676A0: DirectorCamSetup (Director camera pipeline orchestration)
 *  - 0x00551410: SolveCameraTarget (Camera target solver called in Chunk 0x71D)
 *  - 0x0044E850: RwDestroyCameraAndRasters (Camera & raster resource cleanup)
 */

#pragma once

#include "common.h"
#include "rw_sdk.h"
#include <cstdint>

// ============================================================================
// 1. Windows & DirectInput 8 Input Subsystem (0x0051A070 .. 0x0051A230)
// ============================================================================

// 0x0051A070: Initializes Windows input state, disables sticky keys & gets screensaver timeout
bool DirectInput_Init(HWND hWnd);

// 0x0051A100: Pumps Windows messages and keeps screensaver alive
void ProcessWindowsInputMessages();

// 0x0051A1A0: Restores original accessibility parameters & screensaver settings
void CleanupInput();

// 0x0051A210: Restores sticky keys accessibility shortcuts
void RestoreAccessibilityShortcuts();

// 0x0051A230: Polls keyboard, mouse and gamepad input buffers
void PollInputDevices();

// Global DirectInput / Windows input state addresses in Game.exe
extern HINSTANCE g_hInputInstance;       // 0x0062F13C
extern uint32_t  g_dwKeystateBuffer[255]; // 0x0062ED30
extern int32_t   g_nOriginalMouseSpeed;   // 0x0062F154
extern STICKYKEYS g_originalStickyKeys;  // 0x0062F12C
extern uint32_t  g_dwOriginalScreenSaverTimeout; // 0x0062F14C

// ============================================================================
// 2. GameController Class (Descriptor 0x005CA3C8, VTable 0x005CA5A0)
// ============================================================================

// The 20 canonical inquiry action identifiers registered at 0x00402600
enum EControllerInquiryAction {
    CTRL_INQ_START = 0,        // "INQ_ACTN_START"
    CTRL_INQ_SELECT,           // "INQ_ACTN_SELECT"
    CTRL_INQ_TRICK,            // "INQ_ACTN_TRICK"
    CTRL_INQ_FIRE,             // "INQ_ACTN_FIRE"
    CTRL_INQ_JUMP,             // "INQ_ACTN_JUMP"
    CTRL_INQ_DODGE,            // "INQ_ACTN_DODGE"
    CTRL_INQ_SHOULDER_LEFT,    // "INQ_SHOULDER_LEFT"
    CTRL_INQ_SHOULDER_RIGHT,   // "INQ_SHOULDER_RIGHT"
    CTRL_INQ_SHOULDER_L2,      // "INQ_ACTN_SHOULDER_L2"
    CTRL_INQ_SHOULDER_R2,      // "INQ_ACTN_SHOULDER_R2"
    CTRL_INQ_PAD_L,            // "INQ_ACTN_PAD_L"
    CTRL_INQ_PAD_R,            // "INQ_ACTN_PAD_R"
    CTRL_INQ_PAD_U,            // "INQ_ACTN_PAD_U"
    CTRL_INQ_PAD_D,            // "INQ_ACTN_PAD_D"
    CTRL_INQ_TURN,             // "INQ_ACTN_TURN"
    CTRL_INQ_FWD,              // "INQ_ACTN_FWD"
    CTRL_INQ_LPAD_BUTTON,      // "INQ_LPAD_BUTTON"
    CTRL_INQ_CAMERA_ROTATE,    // "INQ_CAMERA_ROTATE"
    CTRL_INQ_CAMERA_PITCH,     // "INQ_CAMERA_PITCH"
    CTRL_INQ_RPAD_BUTTON,      // "INQ_RPAD_BUTTON"
    CTRL_INQ_COUNT             // 20 Actions
};

class GameController {
public:
    void*     m_pVTable;                   // +0x00: VTable 0x005CA5A0
    uint32_t  m_dwActive;                  // +0x04: 1 = Active
    void*     m_pSecondaryVTable;          // +0x08: 0x005CA590
    uint32_t  m_reserved0[6];              // +0x0C..0x23
    void*     m_pActionHandlers[20];       // +0x24..0x73: 20 Registered RWS Action Handlers
    float     m_fActionValues[20];         // Cached analog/digital values
    uint32_t  m_dwButtonMask;              // Active button bitfield
    float     m_fStickLeftX;               // Left thumbstick X (-1.0f .. +1.0f)
    float     m_fStickLeftY;               // Left thumbstick Y (-1.0f .. +1.0f)
    float     m_fStickRightX;              // Right thumbstick X (-1.0f .. +1.0f)
    float     m_fStickRightY;              // Right thumbstick Y (-1.0f .. +1.0f)

public:
    // 0x00402600: GameController Constructor & Action-Inquiry Registration
    GameController();
    virtual ~GameController();

    // VTable Methods (0x005CA5A0)
    virtual void Poll();                          // Slot 1 (0x00402230)
    virtual void Reset();                         // Slot 2 (0x00402550)
    virtual void* GetDeviceState();               // Slot 3 (0x004022D0)
    virtual int32_t HandleEvent(int32_t eventId); // Slot 4 (0x00401000)
    virtual int32_t ProcessStep(uint32_t* pToken);// Slot 5 (0x0043DC80)

    // Action Inquiry Query (returns RwReal)
    float QueryAction(EControllerInquiryAction action) const;
    void SetActionValue(EControllerInquiryAction action, float val);
};

// Global GameController singleton pointer in Game.exe (0x0061798C)
extern GameController* g_pGameController;

// ============================================================================
// 3. CXboxStndController Class (Descriptor 0x005CAD5C, VTable 0x005CAD80)
// ============================================================================

enum EXboxControllerButtons {
    XBOX_BTN_DPAD_UP    = 0x0001,
    XBOX_BTN_DPAD_DOWN  = 0x0002,
    XBOX_BTN_DPAD_LEFT  = 0x0004,
    XBOX_BTN_DPAD_RIGHT = 0x0008,
    XBOX_BTN_START      = 0x0010,
    XBOX_BTN_BACK       = 0x0020,
    XBOX_BTN_LTHUMB     = 0x0040,
    XBOX_BTN_RTHUMB     = 0x0080,
    XBOX_BTN_A          = 0x0100,
    XBOX_BTN_B          = 0x0200,
    XBOX_BTN_X          = 0x0400,
    XBOX_BTN_Y          = 0x0800,
    XBOX_BTN_BLACK      = 0x1000,
    XBOX_BTN_WHITE      = 0x2000,
    XBOX_BTN_LTRIGGER   = 0x4000,
    XBOX_BTN_RTRIGGER   = 0x8000
};

class CXboxStndController {
public:
    void*     m_pVTable;                   // +0x00: VTable 0x005CAD80
    uint32_t  m_dwFlags;                   // +0x04
    void*     m_pAttributes[22];           // +0x08..0x5F: 22 Controller Action Bindings
    float     m_fStickDeadzone;            // +0x60: Default deadzone threshold
    float     m_fSensitivity;              // +0x64: Analog sensitivity
    uint32_t  m_dwButtons;                 // +0x68: Raw digital button flags
    float     m_fTriggerLeft;              // +0x6C: Left trigger (0.0f .. 1.0f)
    float     m_fTriggerRight;             // +0x70: Right trigger (0.0f .. 1.0f)
    float     m_fAxisX;                    // +0x74: Main stick X
    float     m_fAxisY;                    // +0x78: Main stick Y
    float     m_fRotateX;                  // +0x7C: Camera rotate X
    float     m_fRotateY;                  // +0x80: Camera pitch Y

public:
    CXboxStndController();
    virtual ~CXboxStndController();

    // 0x00404E70: Stream Deserializer (Parses TLV attributes 1..22 from .stream chunk)
    void DeserializeStream(void* pStreamHandle);

    // Button and Axis Queries
    bool IsButtonPressed(uint32_t buttonMask) const;
    float GetAxis(uint32_t axisIndex) const;
};

// ============================================================================
// 4. Director Camera Pipeline (0x004676A0 & Chunk 0x71D)
// ============================================================================

// 0x00467680: Set active Director Camera (Stores pointer at 0x0063163C)
int32_t DirectorCam_SetActive(RwCamera* pCamera);

// 0x004719C0: Initialize view and projection matrix buffers (0x00631494)
void DirectorCam_InitMatrices();

// 0x0046F7F0: Update view frustum boundary planes (0x00631484, 0x00631488)
void DirectorCam_UpdateFrustum();

// 0x0046C1F0: Setup viewports, subpixel raster offsets and aspect ratio (0x00631464)
void DirectorCam_SetupViewports();

// 0x00469800: Initialize field-of-view parameters and perspective scaling (0x00631468)
void DirectorCam_InitFOV();

// 0x00470300: Setup near/far clipping planes and depth ranges (0x0063148C)
int32_t DirectorCam_SetupClipping();

// 0x004677A0: Initialize camera post-processing, clear color & raster buffers (0x006314E4)
void DirectorCam_InitPostProcess();

// 0x00467530: Calculate ViewWindow and resolution metrics for Director Camera
void DirectorCam_UpdateMetrics(RwCamera* pCamera);

// 0x004676A0: Full Director Camera Pipeline Setup (Orchestrates the 8 steps above)
void DirectorCamSetup(RwCamera* pCamera);

// 0x00551410: Camera & Target Transform Solver (Called in Chunk 0x71D)
void SolveCameraTarget();

// 0x0044E850: RenderWare Camera & Raster Cleanup
void RwDestroyCameraAndRasters(RwCamera* pCamera);

// Global Director Camera State Variables in Game.exe .data
extern RwCamera* g_pDirectorCamera;        // 0x0063163C
extern float     g_fDirectorCamRasterWidth; // 0x00631570
extern float     g_fDirectorCamRasterHeight;// 0x00631574
extern float     g_fDirectorCamViewWindowX; // 0x00631568
extern float     g_fDirectorCamViewWindowY; // 0x0063156C
