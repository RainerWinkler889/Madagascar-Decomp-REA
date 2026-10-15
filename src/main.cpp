/**
 * @file main.cpp
 * @brief Madagascar (PC, 2005) - Application Entry Point & Main Loop
 * 
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Activision
 * Original Engine: RenderWare Studio 3.x (DirectX 8.1)
 *
 * Mapped Functions:
 *  - 0x0055A0B2: entry / WinMainCRTStartup
 *  - 0x004646F0: WinMain
 *  - 0x00464370: MainWndProc
 *  - 0x00519F00: CreateGameWindow ("RWSConsoleD3D8")
 *  - 0x0051A100: ProcessWindowsMessages
 *  - 0x004620D0: MainGameTick
 */

#include "common.h"

// ============================================================================
// Global State Variables (Original Addresses in .data)
// ============================================================================

HWND g_hMainWindow        = nullptr; // 0x0062F138: Main Game Window Handle
HWND g_hDeviceWindow      = nullptr; // 0x0062A5DC: Duplicate / Render Target HWND
bool g_bQuit              = false;   // 0x0062F140: Flag set to 1 on WM_CLOSE / WM_DESTROY
bool g_bActive            = false;   // 0x0062F141: Flag set on WM_SETFOCUS (1) / WM_KILLFOCUS (0)
bool g_bFullscreen        = false;   // 0x0062A5B0: 0 = Fullscreen, 1 = Windowed
uint32_t g_dwScreenWidth  = 800;     // 0x0062A5C8: Render width (default: 800)
uint32_t g_dwScreenHeight = 600;     // 0x0062A5CC: Render height (default: 600)

static uint8_t  s_bDistanceClip   = 0;       // 0x0062A59C: "Settings\Display\DistanceClipping"
static uint32_t s_dwViewDistance  = 100;     // 0x0062A5A0: "Settings\Display\Distance"
static void*    s_pSaveManager    = nullptr; // 0x0062A5A8: Savegame / Registry Handler
void*           s_pStringDict     = nullptr; // 0x0062A5AC: Localized String Dictionary ("ID_WINNAME")
static uint8_t  s_bUnlockLevels   = 0;       // 0x0062A5C0: "Settings\Options\UnlockLevels"
static int32_t  s_nCursorVisible  = 0;       // 0x0062A5D0: Cursor visibility state
static uint32_t s_dwMusicVolume   = 100;     // 0x00617FC4: "Settings\Options\Music"
static uint32_t s_dwEffectVolume  = 100;     // 0x00617FC8: "Settings\Options\Effect"
static uint32_t s_dwScreenSaverCounter = 0;  // 0x0062F150: Counter to prevent screensaver

// ============================================================================
// 0x00464370: MainWndProc
// Window Procedure handling OS events, focus switches, and window lifecycle
// ============================================================================
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
        case WM_CLOSE: // 0x0046440B
            g_bQuit = true;
            PostQuitMessage(0);
            return 0;

        case WM_SETFOCUS: // 0x004643A1
            g_bActive = true;
            return 0;

        case WM_KILLFOCUS: // 0x004643D7
            g_bActive = false;
            return 0;

        case WM_PAINT: // 0x00464421
            ValidateRect(hWnd, nullptr);
            return 0;

        case WM_SYSCOMMAND: // 0x00464498
            // Filter Alt key menu activation to prevent pausing the RenderWare loop
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;
            }
            // Filter screensaver & monitor power down while game is running
            if ((wParam & 0xFFF0) == SC_SCREENSAVE || (wParam & 0xFFF0) == SC_MONITORPOWER) {
                return 0;
            }
            break;

        case WM_KEYDOWN: // 0x0046442E
            if (wParam == VK_ESCAPE) {
                // In debug/editor builds this opened the console; handled in game tick
            }
            break;

        case WM_ERASEBKGND:
            return 1; // Prevent flickering since RenderWare clears backbuffer

        default:
            break;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

// ============================================================================
// 0x0051A100: ProcessWindowsMessages
// Non-blocking Windows message pump invoked each frame
// ============================================================================
void ProcessWindowsMessages() {
    // Keep screensaver disabled during gameplay
    if (++s_dwScreenSaverCounter > 200) {
        s_dwScreenSaverCounter = 0;
        SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE, FALSE, nullptr, 0);
    }

    MSG msg;
    while (PeekMessageA(&msg, g_hMainWindow, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

// ============================================================================
// 0x00519F00: CreateGameWindow
// Registers "RWSConsoleD3D8" window class and creates the application window
// ============================================================================
void CreateGameWindow(
    LPCSTR lpClassName,
    LPCSTR lpWindowName,
    int x, int y,
    int width, int height,
    WNDPROC lpfnWndProc,
    HINSTANCE hInstance,
    UINT uIconId,
    bool bFullscreenMode
) {
    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEXA);
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = lpfnWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = lpClassName;

    if (uIconId != 0) {
        wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(uIconId));
    }

    RegisterClassExA(&wc);

    DWORD dwStyle;
    DWORD dwExStyle = 0;
    int finalWidth  = width;
    int finalHeight = height;

    if (!bFullscreenMode) {
        // Windowed mode: WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX (0x80C80000)
        dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT rect = { 0, 0, width, height };
        AdjustWindowRect(&rect, dwStyle, FALSE);
        finalWidth  = rect.right - rect.left;
        finalHeight = rect.bottom - rect.top;

        if (x == -1) {
            x = (GetSystemMetrics(SM_CXSCREEN) - finalWidth) / 2;
        }
        if (y == -1) {
            y = (GetSystemMetrics(SM_CYSCREEN) - finalHeight) / 2;
        }
    } else {
        // Fullscreen exclusive: WS_POPUP (0x80000000) with WS_EX_TOPMOST (0x8)
        dwExStyle   = WS_EX_TOPMOST;
        dwStyle     = WS_POPUP;
        x           = 0;
        y           = 0;
        finalWidth  = GetSystemMetrics(SM_CXSCREEN);
        finalHeight = GetSystemMetrics(SM_CYSCREEN);
    }

    SetLastError(0);
    g_hMainWindow = CreateWindowExA(
        dwExStyle,
        lpClassName,
        lpWindowName,
        dwStyle,
        x, y,
        finalWidth, finalHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );
}

// ============================================================================
// 0x004646F0: WinMain
// The true Win32 entry point for Madagascar
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // 1. Single Instance Check via named mutex
    HANDLE hMutex = CreateMutexA(nullptr, FALSE, "Madagascar Running");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex != nullptr && hMutex != INVALID_HANDLE_VALUE) {
            CloseHandle(hMutex);
        }
        return 0; // Instance already running
    }

    // 2. Initialize Settings & Registry Manager
    s_pSaveManager = InitSettingsManager();

    // 3. Read Display & Gameplay Options
    s_bUnlockLevels = (GetSettingInt("Settings\\Options\\UnlockLevels", 0) == 1) ? 1 : 0;

    // Parse resolution string (e.g., "800x600")
    char resBuffer[16] = { 0 };
    GetSettingString("Settings\\Display\\Resolution", "800x600", resBuffer, sizeof(resBuffer));
    
    char* xPtr = strchr(resBuffer, 'x');
    if (xPtr != nullptr) {
        *xPtr = '\0';
        g_dwScreenWidth  = static_cast<uint32_t>(atol(resBuffer));
        g_dwScreenHeight = static_cast<uint32_t>(atol(xPtr + 1));
    } else {
        g_dwScreenWidth  = 800;
        g_dwScreenHeight = 600;
    }

    s_bDistanceClip  = (GetSettingInt("Settings\\Display\\DistanceClipping", 0) == 1) ? 1 : 0;
    s_dwViewDistance = GetSettingInt("Settings\\Display\\Distance", 100);
    g_bFullscreen    = false; // 0 = Fullscreen in retail game

    // 4. Check Current Desktop Resolution & Attempt Display Change
    DEVMODEA currentDevMode;
    ZeroMemory(&currentDevMode, sizeof(currentDevMode));
    if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &currentDevMode)) {
        if (currentDevMode.dmPelsWidth < g_dwScreenWidth || currentDevMode.dmPelsHeight < g_dwScreenHeight) {
            DEVMODEA targetDevMode;
            ZeroMemory(&targetDevMode, sizeof(targetDevMode));
            targetDevMode.dmSize       = sizeof(targetDevMode);
            targetDevMode.dmPelsWidth  = g_dwScreenWidth;
            targetDevMode.dmPelsHeight = g_dwScreenHeight;
            targetDevMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

            if (ChangeDisplaySettingsA(&targetDevMode, 0) != DISP_CHANGE_SUCCESSFUL) {
                MessageBoxA(nullptr, "Display change failed", "Error", MB_ICONERROR | MB_OK);
            }
        }
    }

    // 5. Create Game Window
    const char* windowTitle = GetLocalizedString(s_pStringDict, "ID_WINNAME");
    if (!windowTitle || !*windowTitle) {
        windowTitle = "Madagascar";
    }

    CreateGameWindow(
        "RWSConsoleD3D8",
        windowTitle,
        0, 0,
        g_dwScreenWidth, g_dwScreenHeight,
        MainWndProc,
        hInstance,
        103, // Icon Resource ID (RT_GROUP_ICON 103)
        (g_bFullscreen == false) // true = fullscreen
    );

    g_hDeviceWindow = g_hMainWindow;

    if (g_hMainWindow != nullptr) {
        RECT clientRect;
        GetClientRect(g_hMainWindow, &clientRect);

        uint32_t renderParams[4];
        if (g_bFullscreen == false) {
            renderParams[0] = g_dwScreenWidth;
            renderParams[1] = g_dwScreenHeight;
        } else {
            renderParams[0] = clientRect.right - clientRect.left;
            renderParams[1] = clientRect.bottom - clientRect.top;
        }
        renderParams[2] = 32;                   // 32 bpp requested color depth
        renderParams[3] = rwVIDEOMODEEXCLUSIVE; // rwVIDEOMODEEXCLUSIVE (1)

        // 6. Initialize RenderWare Studio & Direct3D 8 Engine
        int32_t rwSuccess = RwEngine_Initialize(0, nullptr, 0x01000000, renderParams, 0x10, g_hDeviceWindow);
        if (rwSuccess != 0) {
            // 7. Load Global Stream & Game World
            LoadLevelStream("levels\\global.stream", 0);
            InitGameWorldManager();

            // Center Cursor
            GetClientRect(g_hDeviceWindow, &clientRect);
            POINT centerPt = {
                (clientRect.right - clientRect.left) / 2,
                (clientRect.bottom - clientRect.top) / 2
            };
            ClientToScreen(g_hDeviceWindow, &centerPt);
            SetCursorPos(centerPt.x, centerPt.y);

            s_nCursorVisible = 1;
            if (g_bFullscreen) {
                ShowWindow(g_hMainWindow, SW_SHOWNORMAL);
            }
            ShowCursor(FALSE);
            UpdateWindow(g_hDeviceWindow);

            // ================================================================
            // 8. MAIN GAME LOOP
            // ================================================================
            while (!g_bQuit) {
                ProcessWindowsMessages();

                // Idle loop when window is inactive / minimized
                while (!g_bActive && !g_bQuit) {
                    ProcessWindowsMessages();
                    Sleep(10);
                }

                // Execute Frame Update & Render (RenderWare PowerPipe Tick)
                MainGameTick();
            }

            // 9. Shutdown & Cleanup
            ShutdownGame();
            ShutdownRenderWare();
        }
    }

    // 10. Save Persistent Audio & Controller Settings
    SetSettingInt("Settings\\Options\\Effect", s_dwEffectVolume);
    SetSettingInt("Settings\\Options\\Music", s_dwMusicVolume);
    SetSettingInt("Settings\\Options\\RumbleOff", GetRumbleSetting());
    FlushSettings();

    if (s_pStringDict != nullptr) {
        FreeStringDict(s_pStringDict);
        s_pStringDict = nullptr;
    }
    CleanupInput();

    // 11. Restore Display Resolution
    if (ChangeDisplaySettingsA(nullptr, 0) != DISP_CHANGE_SUCCESSFUL) {
        MessageBoxA(nullptr, "Display reset failed", "Error", MB_ICONERROR | MB_OK);
    }

    if (hMutex != nullptr && hMutex != INVALID_HANDLE_VALUE) {
        CloseHandle(hMutex);
    }

    return 1;
}

// ============================================================================
// 0x0055A0B2: CRT Entry Point
// Standard Microsoft Visual C++ 2003 Runtime startup wrapper
// ============================================================================
int main() {
    HINSTANCE hInstance = GetModuleHandleA(nullptr);
    return WinMain(hInstance, nullptr, GetCommandLineA(), SW_SHOWDEFAULT);
}
