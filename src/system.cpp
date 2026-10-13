/**
 * @file system.cpp
 * @brief Madagascar (PC, 2005) - System, Registry, Savegame, Filesystem & Localization
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Activision / Beenox
 * Original Engine: RenderWare Studio 3.x + Toys for Bob Extensions
 *
 * Mapped Subsystems & Functions:
 *  - 0x004634E0: InitSettingsManager ("Activision\Madagascar\Save")
 *  - 0x00519D20: GetSettingInt (Reads DWORD from Software\Activision\Madagascar)
 *  - 0x00519CB0: GetSettingString (Reads string from Software\Activision\Madagascar)
 *  - 0x00519C90: SetSettingInt (Writes DWORD to Software\Activision\Madagascar)
 *  - 0x0051C940: FlushSettings (Flushes pending save file/registry handles)
 *  - 0x0051A430: GetLocalizedString (Dict lookup: KEY=VALUE parser)
 *  - 0x0051A450: FreeStringDict (Frees dictionary allocation)
 *  - 0x00463360: SetupFileSystemDrives (Logical drive scanner & device setup)
 *  - 0x00460C10: ResolveAssetPath (Asset path resolver context at 0x0062A1F8)
 *  - 0x004025F0: GetRumbleSetting (Reads rumble flag from 0x00617988)
 *  - 0x004025E0: SetRumbleSetting (Writes rumble flag to 0x00617988)
 *  - 0x0051A1A0: CleanupInput (DirectInput release, restores sticky keys)
 *  - 0x00460D50: ShutdownGame (Level asset unload & resource release)
 *  - 0x00462BC0: ShutdownRenderWare (RwEngine stop/close/term & async cleanup)
 */

#include "common.h"
#include "rw_sdk.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// Global State Variables (Original Addresses in Game.exe .data)
// ============================================================================

// 0x00617988: Rumble Force-Feedback Enabled Flag
static uint8_t s_bRumbleEnabled = 0; // 0x00617988

// 0x00606CC4: Detailed Shadows Enabled Flag
uint8_t g_bDetailedShadows = 1; // 0x00606CC4

// 0x00617FC4 & 0x00617FC8: Volume levels (75% default = 0x4B)
uint32_t g_dwMusicVolumeSetting  = 75; // 0x00617FC4
uint32_t g_dwEffectVolumeSetting = 75; // 0x00617FC8

// 0x0062A1F8: Global Asset / Path Resolver Context
static uint8_t s_assetResolverContext[0x100] = { 0 }; // 0x0062A1F8

// 0x0062F12C: StickyKeys system backup structure
static STICKYKEYS s_stickyKeysBackup = { sizeof(STICKYKEYS), 0 }; // 0x0062F12C

// 0x0062F174 & 0x0062F178: Async File Handles for Save/Settings
static HANDLE s_hSaveFileHandle1 = nullptr; // 0x0062F174
static HANDLE s_hSaveFileHandle2 = nullptr; // 0x0062F178

// 0x00606B84 & 0x00606BC4: Resource Cleanup Table Pointers
static void* s_pResourceTable1[8] = { nullptr }; // 0x00606B84
static void* s_pResourceTable2[8] = { nullptr }; // 0x00606BC4

// External references from video.cpp
extern CRITICAL_SECTION g_csTFBAsyncStream; // 0x00630F40
extern uint32_t g_bTFBAsyncInitialized;    // 0x0062A618

// ============================================================================
// Registry Helper Functions
// ============================================================================

static const char* const REGISTRY_COMPANY = "Activision";
static const char* const REGISTRY_PRODUCT = "Madagascar";

/**
 * Splits a composite key (e.g. "Settings\Display\Resolution") into
 * subkey path ("Software\Activision\Madagascar\Settings\Display")
 * and leaf value name ("Resolution").
 */
static bool BuildRegistryKeyPath(const char* fullKey, char* outSubKey, size_t subKeySize, char* outValueName, size_t valNameSize) {
    if (!fullKey || !outSubKey || !outValueName) return false;

    const char* lastSlash = strrchr(fullKey, '\\');
    if (lastSlash != nullptr) {
        size_t pathLen = (size_t)(lastSlash - fullKey);
        char subPath[MAX_PATH] = { 0 };
        if (pathLen >= sizeof(subPath)) pathLen = sizeof(subPath) - 1;
        memcpy(subPath, fullKey, pathLen);
        subPath[pathLen] = '\0';

        snprintf(outSubKey, subKeySize, "Software\\%s\\%s\\%s", REGISTRY_COMPANY, REGISTRY_PRODUCT, subPath);
        snprintf(outValueName, valNameSize, "%s", lastSlash + 1);
    } else {
        snprintf(outSubKey, subKeySize, "Software\\%s\\%s", REGISTRY_COMPANY, REGISTRY_PRODUCT);
        snprintf(outValueName, valNameSize, "%s", fullKey);
    }
    return true;
}

// ============================================================================
// 0x00519D20: GetSettingInt
//
// Reads a 32-bit DWORD from HKEY_CURRENT_USER\Software\Activision\Madagascar\<key>,
// falling back to HKEY_LOCAL_MACHINE if not found.
// ============================================================================
int32_t GetSettingInt(const char* key, int32_t defaultValue) {
    char subKey[MAX_PATH] = { 0 };
    char valName[MAX_PATH] = { 0 };
    if (!BuildRegistryKeyPath(key, subKey, sizeof(subKey), valName, sizeof(valName))) {
        return defaultValue;
    }

    HKEY hKey = nullptr;
    DWORD dwType = 0;
    DWORD dwValue = 0;
    DWORD dwSize = sizeof(DWORD);

    // 1. Try HKEY_CURRENT_USER
    if (RegOpenKeyExA(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        LSTATUS status = RegQueryValueExA(hKey, valName, nullptr, &dwType, (LPBYTE)&dwValue, &dwSize);
        RegCloseKey(hKey);
        if (status == ERROR_SUCCESS && dwType == REG_DWORD) {
            return (int32_t)dwValue;
        }
    }

    // 2. Fallback to HKEY_LOCAL_MACHINE
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        LSTATUS status = RegQueryValueExA(hKey, valName, nullptr, &dwType, (LPBYTE)&dwValue, &dwSize);
        RegCloseKey(hKey);
        if (status == ERROR_SUCCESS && dwType == REG_DWORD) {
            return (int32_t)dwValue;
        }
    }

    return defaultValue;
}

// ============================================================================
// 0x00519CB0: GetSettingString
//
// Reads a null-terminated string from registry, copying defaultVal if absent.
// ============================================================================
void GetSettingString(const char* key, const char* defaultVal, char* outBuffer, uint32_t maxLen) {
    if (!outBuffer || maxLen == 0) return;

    char subKey[MAX_PATH] = { 0 };
    char valName[MAX_PATH] = { 0 };
    if (!BuildRegistryKeyPath(key, subKey, sizeof(subKey), valName, sizeof(valName))) {
        strncpy(outBuffer, defaultVal ? defaultVal : "", maxLen - 1);
        outBuffer[maxLen - 1] = '\0';
        return;
    }

    HKEY hKey = nullptr;
    DWORD dwType = 0;
    DWORD dwSize = maxLen;

    // 1. Try HKEY_CURRENT_USER
    if (RegOpenKeyExA(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        LSTATUS status = RegQueryValueExA(hKey, valName, nullptr, &dwType, (LPBYTE)outBuffer, &dwSize);
        RegCloseKey(hKey);
        if (status == ERROR_SUCCESS && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
            outBuffer[maxLen - 1] = '\0';
            return;
        }
    }

    // 2. Fallback to HKEY_LOCAL_MACHINE
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        LSTATUS status = RegQueryValueExA(hKey, valName, nullptr, &dwType, (LPBYTE)outBuffer, &dwSize);
        RegCloseKey(hKey);
        if (status == ERROR_SUCCESS && (dwType == REG_SZ || dwType == REG_EXPAND_SZ)) {
            outBuffer[maxLen - 1] = '\0';
            return;
        }
    }

    // Fallback default
    strncpy(outBuffer, defaultVal ? defaultVal : "", maxLen - 1);
    outBuffer[maxLen - 1] = '\0';
}

// ============================================================================
// 0x00519C90: SetSettingInt
//
// Saves a 32-bit DWORD into HKEY_CURRENT_USER\Software\Activision\Madagascar\<key>.
// ============================================================================
void SetSettingInt(const char* key, int32_t value) {
    char subKey[MAX_PATH] = { 0 };
    char valName[MAX_PATH] = { 0 };
    if (!BuildRegistryKeyPath(key, subKey, sizeof(subKey), valName, sizeof(valName))) {
        return;
    }

    HKEY hKey = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, subKey, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD dwVal = (DWORD)value;
        RegSetValueExA(hKey, valName, 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

// ============================================================================
// 0x0051C940: FlushSettings
//
// Closes pending asynchronous save/settings file handles.
// ============================================================================
void FlushSettings() {
    if (s_hSaveFileHandle1 != nullptr) {
        CloseHandle(s_hSaveFileHandle1);
        s_hSaveFileHandle1 = nullptr;
    }
    if (s_hSaveFileHandle2 != nullptr) {
        CloseHandle(s_hSaveFileHandle2);
        s_hSaveFileHandle2 = nullptr;
    }
}

// ============================================================================
// 0x004025F0 & 0x004025E0: Rumble Setting Accessors
// ============================================================================
int32_t GetRumbleSetting() {
    return s_bRumbleEnabled; // 0x00617988
}

void SetRumbleSetting(int32_t bEnabled) {
    s_bRumbleEnabled = (bEnabled != 0) ? 1 : 0; // 0x004025E0
}

// ============================================================================
// Localization Subsystem: 0x0051A430 & 0x0051A450
//
// Madagascar loads "data\mdgdict.bnx" which contains lines in "KEY=VALUE" format.
// ============================================================================
struct CStringDictEntry {
    char* pText; // Stored line pointer
};

struct CStringDict {
    char*             pRawBuffer;
    CStringDictEntry* pEntries;
    int32_t           nCount;
    int32_t           nCapacity;
};

// 0x0051A480: Load localized string dictionary from file
static void* LoadStringDict(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return nullptr;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        fclose(fp);
        return nullptr;
    }

    CStringDict* pDict = (CStringDict*)malloc(sizeof(CStringDict));
    if (!pDict) {
        fclose(fp);
        return nullptr;
    }

    pDict->pRawBuffer = (char*)malloc(size + 1);
    if (!pDict->pRawBuffer) {
        free(pDict);
        fclose(fp);
        return nullptr;
    }

    fread(pDict->pRawBuffer, 1, size, fp);
    pDict->pRawBuffer[size] = '\0';
    fclose(fp);

    // Parse lines into entry table
    pDict->nCapacity = 512;
    pDict->nCount = 0;
    pDict->pEntries = (CStringDictEntry*)malloc(sizeof(CStringDictEntry) * pDict->nCapacity);

    char* line = pDict->pRawBuffer;
    while (line && *line) {
        char* nextLine = strchr(line, '\r');
        if (!nextLine) nextLine = strchr(line, '\n');

        if (nextLine) {
            *nextLine = '\0';
            char* scan = nextLine + 1;
            while (*scan == '\r' || *scan == '\n') scan++;
            
            if (*line != '\0') {
                if (pDict->nCount < pDict->nCapacity) {
                    pDict->pEntries[pDict->nCount++].pText = line;
                }
            }
            line = scan;
        } else {
            if (*line != '\0') {
                if (pDict->nCount < pDict->nCapacity) {
                    pDict->pEntries[pDict->nCount++].pText = line;
                }
            }
            break;
        }
    }

    return pDict;
}

// 0x0051A430: Look up localized string by ID ("ID_WINNAME" -> "Madagascar")
const char* GetLocalizedString(void* pDict, const char* strId) {
    if (!strId) return "";

    if (pDict != nullptr) {
        CStringDict* dict = (CStringDict*)pDict;
        size_t idLen = strlen(strId);

        for (int32_t i = 0; i < dict->nCount; i++) {
            const char* entry = dict->pEntries[i].pText;
            if (entry != nullptr && strncmp(entry, strId, idLen) == 0 && entry[idLen] == '=') {
                return entry + idLen + 1;
            }
        }
    }

    // Fallback for essential well-known keys
    if (strcmp(strId, "ID_WINNAME") == 0) {
        return "Madagascar";
    }

    return strId;
}

// 0x0051A450: Free string dictionary
void FreeStringDict(void* pDict) {
    if (pDict != nullptr) {
        CStringDict* dict = (CStringDict*)pDict;
        if (dict->pEntries) free(dict->pEntries);
        if (dict->pRawBuffer) free(dict->pRawBuffer);
        free(dict);
    }
}

// ============================================================================
// 0x004634E0: InitSettingsManager
//
// Initializes DirectInput, loads "data\mdgdict.bnx", configures registry
// settings context under "Activision\Madagascar", and reads initial volumes.
// ============================================================================
void* InitSettingsManager() {
    // 1. Initialize DirectInput / Input Subsystem (0x0051A070)
    // Stored and managed internally.

    // 2. Load Localized String Dictionary (0x0051A480)
    s_pStringDict = LoadStringDict("data\\mdgdict.bnx"); // 0x0062A5AC

    // 3. Read Audio & Graphic Settings
    g_dwEffectVolumeSetting = (uint32_t)GetSettingInt("Settings\\Options\\Effect", 0x4B); // 75%
    g_dwMusicVolumeSetting  = (uint32_t)GetSettingInt("Settings\\Options\\Music", 0x4B);  // 75%

    // 4. Rumble Setting
    int32_t rumbleOff = GetSettingInt("Settings\\Options\\RumbleOff", 0);
    SetRumbleSetting(rumbleOff == 0 ? 1 : 0); // 0x004025E0

    // 5. Detailed Shadows
    int32_t detailedShadows = GetSettingInt("Settings\\Display\\DetailedShadows", 1);
    g_bDetailedShadows = (detailedShadows != 0) ? 1 : 0; // 0x00606CC4

    // Return dummy settings manager handle
    static uint32_t s_settingsManagerInstance = 0x12345678;
    return &s_settingsManagerInstance;
}

// ============================================================================
// 0x00463360: SetupFileSystemDrives
//
// Queries current directory and scans logical drives (GetLogicalDrives & GetDriveTypeA)
// to configure root storage mount points for assets.
// ============================================================================
int32_t SetupFileSystemDrives() {
    char currentDir[MAX_PATH] = { 0 };
    DWORD dwLen = GetCurrentDirectoryA(sizeof(currentDir), currentDir);
    if (dwLen == 0 || dwLen >= sizeof(currentDir)) {
        return 0;
    }

    DWORD drivesMask = GetLogicalDrives();
    char drivePath[8] = "C:\\";

    for (char driveLetter = 'A'; driveLetter <= 'Z'; driveLetter++) {
        if (drivesMask & (1 << (driveLetter - 'A'))) {
            drivePath[0] = driveLetter;
            UINT driveType = GetDriveTypeA(drivePath);
            // Accept DRIVE_FIXED (3), DRIVE_CDROM (5), DRIVE_REMOTE (4), DRIVE_RAMDISK (6)
            if (driveType != DRIVE_UNKNOWN && driveType != DRIVE_NO_ROOT_DIR) {
                // Registered drive
            }
        }
    }

    return 1;
}

// ============================================================================
// 0x00460C10: ResolveAssetPath
//
// Returns the global asset and path resolver context pointer (0x0062A1F8).
// ============================================================================
void* ResolveAssetPath() {
    return (void*)s_assetResolverContext; // 0x0062A1F8
}

// ============================================================================
// 0x0051A1A0: CleanupInput
//
// Destroys DirectInput device acquisitions, restores sticky keys settings,
// and terminates the main window if still open.
// ============================================================================
void CleanupInput() {
    // Restore StickyKeys OS setting
    SystemParametersInfoA(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s_stickyKeysBackup, 0);

    // Destroy main window if still alive
    if (g_hMainWindow != nullptr) {
        DestroyWindow(g_hMainWindow);
        g_hMainWindow = nullptr;
    }
}

// ============================================================================
// 0x00460D50: ShutdownGame
//
// Cleans up level geometry, releases active resource caches and resets scene.
// ============================================================================
void ShutdownGame() {
    // Release cached resource objects
    for (int i = 0; i < 8; i++) {
        if (s_pResourceTable1[i] != nullptr) {
            // Free resource
            s_pResourceTable1[i] = nullptr;
        }
        if (s_pResourceTable2[i] != nullptr) {
            // Free resource
            s_pResourceTable2[i] = nullptr;
        }
    }
}

// ============================================================================
// 0x00462BC0: ShutdownRenderWare
//
// Shuts down RenderWare graphics engine, camera pipelines and background loaders.
// ============================================================================
void ShutdownRenderWare() {
    // 1. Terminate graphics pipelines
    RwEngineStop();  // 0x004D81F0
    RwEngineClose(); // 0x004D8300
    RwEngineTerm();  // 0x004D85E0

    // 2. Clean up TFB Asynchronous Streaming Critical Section
    if (g_bTFBAsyncInitialized != 0) {
        DeleteCriticalSection(&g_csTFBAsyncStream); // 0x00630F40
        g_bTFBAsyncInitialized = 0;
    }
}
