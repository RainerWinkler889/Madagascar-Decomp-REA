/**
 * @file script_and_ui.cpp
 * @brief Madagascar (PC, 2005) - Timer, Hash-Engine, BTFB Script Interpreter & UI Subsystem
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 *
 * Mapped Subsystems & Functions:
 *  - 0x00464F40: CTimer::Update
 *  - 0x00464F00: CTimer::Reset
 *  - 0x0044D2C0: CTimer::GetTimeTicks
 *  - 0x00464D00: HashString
 *  - 0x0043E6F0: HashAnimName
 *  - 0x00413500: ExecuteScriptCmd
 *  - 0x004112C0: InitUISubsystem
 *  - 0x00416AA0: CreateUITextBox
 *  - 0x00416AE0: UpdateUITextBox
 *  - 0x004140D0: DestroyUITextBox
 *  - 0x00414020: FindUIElement
 *  - 0x00461B40: IsUIElementVisible
 */

#include "script_and_ui.h"
#include <mmsystem.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

#pragma comment(lib, "winmm.lib")

// ============================================================================
// 1. Dual-Phasen Engine Timer (CTimer)
//
// Addresses:
//   - Update: 0x00464F40
//   - Reset:  0x00464F00
//   - Ticks:  0x0044D2C0
// ============================================================================

CTimer g_gameTimer; // 0x0062A5E0

CTimer::CTimer() {
    LARGE_INTEGER freq;
    if (QueryPerformanceFrequency(&freq)) {
        m_perfFrequency = (uint64_t)freq.QuadPart;
        m_bUseQPC = 1;
    } else {
        m_perfFrequency = 1000;
        m_bUseQPC = 0;
    }

    m_perfLastCounter    = 0;
    m_reserved0          = 0;
    m_reserved1          = 0;
    m_lastTickCount      = 0;
    m_tickDelta          = 0;
    m_reserved2          = 0;
    m_reserved3          = 0;
    m_fSubstepDelta      = 1.0f / 60.0f; // ~0.0166667s simulation step
    m_bRenderPhaseDone   = 0;
    m_fAccumulator       = 0.0f;
    m_dwReserved4        = 0;
    m_dwSimSubstepCount  = 0;
    m_dwReserved5        = 0;
    m_dwRenderFrameCount = 0;

    Reset();
}

// 0x00464F00: Reset timer baselines and reset accumulator
void CTimer::Reset() {
    if (m_bUseQPC != 0) {
        LARGE_INTEGER cur;
        QueryPerformanceCounter(&cur);
        m_perfLastCounter = (uint64_t)cur.QuadPart;
    } else {
        m_lastTickCount = GetTickCount();
    }

    m_fAccumulator       = 0.0f;
    m_bRenderPhaseDone   = 0;
    m_dwSimSubstepCount  = 0;
    m_dwRenderFrameCount = 0;
}

// 0x00464F40: Dual-phase update tick
float CTimer::Update() {
    float frameDelta = 0.0f;

    if (m_bUseQPC != 0) {
        LARGE_INTEGER cur;
        QueryPerformanceCounter(&cur);
        uint64_t delta = (uint64_t)cur.QuadPart - m_perfLastCounter;
        m_perfLastCounter = (uint64_t)cur.QuadPart;
        frameDelta = (float)((double)delta / (double)m_perfFrequency);
    } else {
        DWORD curTicks = GetTickCount();
        DWORD delta = curTicks - m_lastTickCount;
        m_lastTickCount = curTicks;
        frameDelta = (float)delta / 1000.0f;
    }

    // Cap excessive delta time (e.g. debugging pauses or level loading)
    if (frameDelta > 0.2f) {
        frameDelta = 0.2f;
    }

    m_fAccumulator += frameDelta;

    // Cap accumulator at 1.0 second to prevent spiral of death
    if (m_fAccumulator >= 1.0f) {
        m_fAccumulator -= 1.0f;
    }

    // Phase 1: Simulation Substeps
    if (m_fAccumulator >= m_fSubstepDelta) {
        m_fAccumulator -= m_fSubstepDelta;
        m_bRenderPhaseDone = 0;
        m_dwSimSubstepCount++;
        return m_fSubstepDelta; // Positive delta time (> 0.0f) triggers simulation tick
    }

    // Phase 2: Rendering Phase
    if (m_bRenderPhaseDone == 0) {
        m_bRenderPhaseDone = 1;
        m_dwRenderFrameCount++;
        return -1.0f; // Token (< 0.0f) triggers rendering pipeline in MainGameTick
    }

    // Paused / idle waiting
    return 0.0f;
}

// 0x0044D2C0: High-precision system time in milliseconds
uint32_t CTimer::GetTimeTicks() {
    TIMECAPS tc;
    timeGetDevCaps(&tc, sizeof(tc));
    timeBeginPeriod(tc.wPeriodMin);
    DWORD ticks = timeGetTime();
    timeEndPeriod(tc.wPeriodMin);
    return (uint32_t)ticks;
}

// ============================================================================
// 2. String-Interning & Hash Subsystem
//
// 0x00464D00: Zentraler String-Pool & Hasher für Audio-, Entity- und Script-Strings
// Madagascar speichert alle Engine-Strings in verknüpften 512-Byte Blöcken
// (String-Interning). Gleiche Strings verweisen immer auf dieselbe Adresse.
// ============================================================================

struct CStringPoolBlock {
    char data[512];
    uint32_t used;
    CStringPoolBlock* next;
};

static CStringPoolBlock* s_pStringPoolHead = nullptr; // 0x0062A604

uint32_t HashString(const char* str) {
    if (!str || !*str) return 0;

    // 1. Suche String im existierenden Pool
    CStringPoolBlock* block = s_pStringPoolHead;
    while (block != nullptr) {
        char* scan = block->data;
        char* end  = block->data + block->used;
        while (scan < end) {
            if (strcmp(scan, str) == 0) {
                return (uint32_t)(uintptr_t)scan; // Gefundener String-Pool-Pointer
            }
            scan += strlen(scan) + 1;
        }
        block = block->next;
    }

    // 2. String ist noch nicht im Pool: neu allozieren
    size_t len = strlen(str) + 1;
    if (len > 512) {
        // Fallback: Dynamische Allokation für übergroße Strings
        char* dyn = (char*)malloc(len);
        if (dyn) {
            memcpy(dyn, str, len);
            return (uint32_t)(uintptr_t)dyn;
        }
        return 0;
    }

    // Prüfen, ob der aktuelle Block noch genug Platz hat
    if (s_pStringPoolHead == nullptr || (s_pStringPoolHead->used + len > 512)) {
        CStringPoolBlock* newBlock = (CStringPoolBlock*)calloc(1, sizeof(CStringPoolBlock));
        if (!newBlock) return 0;
        newBlock->next = s_pStringPoolHead;
        s_pStringPoolHead = newBlock;
    }

    char* dest = s_pStringPoolHead->data + s_pStringPoolHead->used;
    memcpy(dest, str, len);
    s_pStringPoolHead->used += (uint32_t)len;

    return (uint32_t)(uintptr_t)dest;
}

// 0x0043E6F0: Animation-Name Hasher (Gibt im Retail-Build 0 zurück)
uint32_t HashAnimName(const char* animName) {
    (void)animName;
    return 0; // 0x0043E6F0: XOR EAX, EAX; RET
}

// ============================================================================
// 3. BTFB Script Interpreter & Command Table
//
// Reconstructed from Game.exe:
//   - 0x005CA3DF: "BTFB Script"
//   - 0x005CA3EC: Virtual method table (13 function pointers)
//   - 0x005CA430: Canonical BTFB Script Inquiry tokens (INQ_*)
//   - 0x00413500: ExecuteScriptCmd / BTFB Script Command Dispatcher
// ============================================================================

// Static instance of the 13-slot BTFB Script VTable (0x005CA3EC in Game.exe)
static const CBTFBScriptVTable s_btfbScriptVTable = {
    (void*)0x0043E6F0, // Slot 0: QueryState (Default returns 0)
    (void*)0x00436A40, // Slot 1: Virtual invocation dispatch
    (void*)0x00416530, // Slot 2: NOP / returns 0
    (void*)0x004025C0, // Slot 3: Scalar deleting destructor
    (void*)0x0043E6F0, // Slot 4: Reserved / returns 0
    (void*)0x0043E6F0, // Slot 5: Reserved / returns 0
    (void*)0x0042CBE0, // Slot 6: GetType (Returns 2)
    (void*)0x0043DC80, // Slot 7: Reset / Clear target pointer
    (void*)0x004025A0, // Slot 8: NOP ret 8
    (void*)0x004025B0, // Slot 9: Identity (returns this)
    (void*)0x00464EF0, // Slot 10: NOP ret
    (void*)0x0055805E, // Slot 11: Cleanup / RTTI
    (void*)0x00436A40  // Slot 12: Virtual dispatch
};

// Canonical BTFB Script Inquiry token table (0x005CA430 & 0x00402600 in Game.exe)
static const char* const s_szBTFBScriptInquiries[BTFB_INQ_COUNT] = {
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
    "INQ_RPAD_BUTTON",       // 19
    "INQ_ACTN_STRAFE"        // 20 (0x005CAEC8)
};

const CBTFBScriptVTable* GetBTFBScriptVTable() {
    return &s_btfbScriptVTable;
}

const char* GetBTFBScriptInquiryName(EBTFBScriptInquiry inq) {
    if (inq >= 0 && inq < BTFB_INQ_COUNT) {
        return s_szBTFBScriptInquiries[inq];
    }
    return nullptr;
}

int32_t FindBTFBScriptInquiry(const char* name) {
    if (!name) return -1;
    for (int32_t i = 0; i < BTFB_INQ_COUNT; ++i) {
        if (_stricmp(name, s_szBTFBScriptInquiries[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// 0x00413500: ExecuteScriptCmd / BTFB Script Command Dispatcher
bool ExecuteScriptCmdByName(const char* cmdString) {
    if (!cmdString || !*cmdString) return false;

    // 1. Check for Script Inquiry Tokens (0x005CA430: INQ_*)
    int32_t inqIdx = FindBTFBScriptInquiry(cmdString);
    if (inqIdx >= 0) {
        // Inquiry commands return RwReal values via RWS messaging pipeline (0x00550440: "return RwReal")
        return true;
    }

    // 2. Built-in Cinematic & Sequence Triggers
    if (_strnicmp(cmdString, "START_CINEMATIC", 15) == 0) {
        // Starts in-game cutscene or sequence
        return true;
    }
    if (_strnicmp(cmdString, "STOP_CINEMATIC", 14) == 0) {
        return true;
    }

    // 3. Screen Fade & Visual Presentation
    if (_strnicmp(cmdString, "FADE_OUT", 8) == 0) {
        return true;
    }
    if (_strnicmp(cmdString, "FADE_IN", 7) == 0) {
        return true;
    }

    // 4. Audio & Background Music Commands
    if (_strnicmp(cmdString, "PLAY_MUSIC", 10) == 0) {
        return true;
    }
    if (_strnicmp(cmdString, "STOP_MUSIC", 10) == 0) {
        return true;
    }
    if (_strnicmp(cmdString, "PLAY_SOUND", 10) == 0) {
        return true;
    }
    if (_strnicmp(cmdString, "STOP_SOUND", 10) == 0) {
        return true;
    }

    // 5. World, Door & Actor Triggers
    if (_strnicmp(cmdString, "UNLOCK_DOOR", 11) == 0) {
        return true;
    }
    if (_strnicmp(cmdString, "LOCK_DOOR", 9) == 0) {
        return true;
    }
    if (_strnicmp(cmdString, "LOAD_MAP", 8) == 0) {
        return true;
    }
    if (_strnicmp(cmdString, "SPAWN_ACTOR", 11) == 0) {
        return true;
    }
    if (_strnicmp(cmdString, "SET_CHECKPOINT", 14) == 0) {
        return true;
    }

    // 6. Camera Commands
    if (_strnicmp(cmdString, "SET_CAMERA_TARGET", 17) == 0) {
        return true;
    }
    if (_strnicmp(cmdString, "RESET_CAMERA", 12) == 0) {
        return true;
    }

    return false;
}

void ExecuteScriptCmd(uint32_t dwCommandHash) {
    if (dwCommandHash == 0) return;

    // The command hash is either the direct pooled string pointer from HashString()
    // or a 32-bit hash value:
    const char* cmdString = (const char*)(uintptr_t)dwCommandHash;
    ExecuteScriptCmdByName(cmdString);
}

// ============================================================================
// 4. UI-Subsystem
// ============================================================================

static uint8_t s_bUISubsystemReady = 0; // 0x00617E80
static float   s_fUIDistanceScale  = 1.0f; // 0x00617D54

struct CUIElementHeader {
    void*            pVTable;      // +0x00
    uint32_t         pad[3];       // +0x04..0x0F
    char             name[32];     // +0x10..0x2F
    uint8_t          reserved[0xB0];
    const char*      pElementName; // +0xE0: String name pointer
    uint8_t          pad2[0x15];
    uint8_t          bVisible;     // +0xF6: Visibility flag (1 = visible, 0 = hidden)
    uint8_t          pad3[0x19];
    uint8_t          flags;        // +0x110: Hierarchy flags (Bit 3 = has parent check)
    uint8_t          pad4[0x0F];
    CUIElementHeader* pParent;     // +0x120: Parent UI element
};

struct CUITextBox {
    CUIElementHeader header;
    int32_t          x;
    int32_t          y;
    int32_t          width;
    int32_t          height;
    int32_t          color;
    int32_t          flags;
};

static std::vector<CUIElementHeader*> s_activeUIElements;

// 0x004112C0: Initialisiert UI-Skalierung
bool InitUISubsystem() {
    // 0x004112C0: Skaliert UI anhand der Bildschirmdistanz
    s_fUIDistanceScale = (101.0f - 100.0f) * 0.01f * 1.0f;
    s_bUISubsystemReady = 1;
    return true;
}

// 0x00416AA0: Erzeugt eine UI-Text-Box
void* CreateUITextBox(int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6) {
    if (s_bUISubsystemReady == 0) return nullptr;

    CUITextBox* box = (CUITextBox*)calloc(1, sizeof(CUITextBox));
    if (!box) return nullptr;

    box->x      = p1;
    box->y      = p2;
    box->width  = p3;
    box->height = p4;
    box->color  = p5;
    box->flags  = p6;
    box->header.bVisible = 1;

    s_activeUIElements.push_back(&box->header);
    return box;
}

void* CreateUITextBox(const char* id, const char* text, float x, float y, float w, float h) {
    (void)text;
    if (s_bUISubsystemReady == 0) return nullptr;

    CUITextBox* box = (CUITextBox*)calloc(1, sizeof(CUITextBox));
    if (!box) return nullptr;

    if (id) {
        strncpy(box->header.name, id, sizeof(box->header.name) - 1);
        box->header.pElementName = box->header.name;
    }
    box->x      = (int32_t)x;
    box->y      = (int32_t)y;
    box->width  = (int32_t)w;
    box->height = (int32_t)h;
    box->header.bVisible = 1;

    s_activeUIElements.push_back(&box->header);
    return box;
}

// 0x00416AE0: Aktualisiert eine UI-Text-Box
void UpdateUITextBox(int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6) {
    if (s_bUISubsystemReady == 0) return;
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5; (void)p6;
}

void UpdateUITextBox(const char* id, const char* text, float x, float y, float w, float h) {
    (void)id; (void)text; (void)x; (void)y; (void)w; (void)h;
}

// 0x004140D0: Zerstört eine UI-Text-Box
void DestroyUITextBox(void* pTextBox) {
    if (s_bUISubsystemReady == 0 || !pTextBox) return;

    for (auto it = s_activeUIElements.begin(); it != s_activeUIElements.end(); ++it) {
        if (*it == (CUIElementHeader*)pTextBox) {
            s_activeUIElements.erase(it);
            break;
        }
    }
    free(pTextBox);
}

void DestroyUITextBox(const char* id) {
    if (!id || s_bUISubsystemReady == 0) return;

    for (auto it = s_activeUIElements.begin(); it != s_activeUIElements.end(); ++it) {
        if (*it != nullptr && (*it)->pElementName != nullptr && _stricmp((*it)->pElementName, id) == 0) {
            void* p = *it;
            s_activeUIElements.erase(it);
            free(p);
            break;
        }
    }
}

// 0x00414020: Sucht ein UI-Element im aktiven Menü/HUD-Baum nach Name
void* FindUIElement(const char* elementName) {
    if (!elementName) return nullptr;

    for (CUIElementHeader* pElem : s_activeUIElements) {
        if (pElem != nullptr && pElem->pElementName != nullptr) {
            if (_stricmp(elementName, pElem->pElementName) == 0) {
                return pElem;
            }
        }
    }
    return nullptr;
}

// 0x00461B40: Prüft, ob ein UI-Element sichtbar ist (inkl. Parent-Status)
bool IsUIElementVisible(void* pElement) {
    if (!pElement) return false;

    CUIElementHeader* elem = (CUIElementHeader*)pElement;
    if (elem->bVisible == 0) {
        return false;
    }

    // Falls bit 3 von flags gesetzt ist, prüfe auch den Parent
    if (elem->pParent != nullptr && (elem->flags & 0x08) != 0) {
        return elem->pParent->bVisible != 0;
    }

    return true;
}

bool IsUIElementVisible(int32_t handle) {
    return IsUIElementVisible((void*)(uintptr_t)handle);
}
