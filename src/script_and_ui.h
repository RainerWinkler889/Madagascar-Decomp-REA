/**
 * @file script_and_ui.h
 * @brief Madagascar (PC, 2005) - Timer, Hash-Engine, BTFB Script Interpreter & UI Subsystem
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 *
 * Mapped Subsystems & Functions:
 *  - 0x00464F40: CTimer::Update (Dual-phase simulation/render timer)
 *  - 0x00464F00: CTimer::Reset
 *  - 0x0044D2C0: CTimer::GetTimeTicks (High-precision timeGetTime)
 *  - 0x00464D00: HashString (String-Interning & Hash-Table für Audio & Befehle)
 *  - 0x0043E6F0: HashAnimName (Animation Hash-Stub)
 *  - 0x00413500: ExecuteScriptCmd (BTFB Script-Befehls-Dispatcher)
 *  - 0x004112C0: InitUISubsystem (Skalierung & UI-Initialisierung)
 *  - 0x00416AA0: CreateUITextBox
 *  - 0x00416AE0: UpdateUITextBox
 *  - 0x004140D0: DestroyUITextBox
 *  - 0x00414020: FindUIElement (Suche nach UI-Element per Name)
 *  - 0x00461B40: IsUIElementVisible (Sichtbarkeitsprüfung inkl. Parent-Prüfung)
 */

#pragma once

#include "common.h"
#include <cstdint>

// ============================================================================
// 1. Dual-Phasen Engine Timer (CTimer)
//
// Madagascar verwendet in MainGameTick (0x004620D0) ein getaktetes Dual-Phasen-
// Protokoll:
//   - deltaTime > 0.0f:  Simulationsschritt (Physik, KI, RWS Messages)
//   - deltaTime == 0.0f: Simulation pausiert (Sleep(0))
//   - deltaTime < 0.0f:  -1.0f Token signals Rendering-Phase
// ============================================================================
class CTimer {
public:
    uint64_t m_perfFrequency;      // +0x00: QPC frequency
    uint64_t m_perfLastCounter;    // +0x08: QPC previous counter
    uint32_t m_reserved0;          // +0x10
    uint32_t m_reserved1;          // +0x14
    uint32_t m_lastTickCount;      // +0x18: GetTickCount previous
    uint32_t m_tickDelta;          // +0x1C: GetTickCount delta
    uint32_t m_bUseQPC;            // +0x24: 1 = Use QPC, 0 = Use GetTickCount
    uint32_t m_reserved2;          // +0x28
    uint32_t m_reserved3;          // +0x2C
    float    m_fSubstepDelta;      // +0x34: Fixed simulation delta time (default: 1/60s = ~0.01667f)
    uint8_t  m_bRenderPhaseDone;   // +0x38: Token toggle
    uint8_t  m_pad[3];             // +0x39
    float    m_fAccumulator;       // +0x3C: Frame accumulator
    uint32_t m_dwReserved4;        // +0x40
    uint32_t m_dwSimSubstepCount;  // +0x44: Total simulation steps taken
    uint32_t m_dwReserved5;        // +0x48
    uint32_t m_dwRenderFrameCount; // +0x4C: Total rendered frames

public:
    CTimer();

    // 0x00464F00: Reset timer state and synchronize baseline
    void Reset();

    // 0x00464F40: Dual-phase update tick
    float Update();

    // 0x0044D2C0: High-precision system time in milliseconds
    static uint32_t GetTimeTicks();
};

// Global Timer Singleton in Game.exe
extern CTimer g_gameTimer; // 0x0062A5E0

// ============================================================================
// 2. String-Interning & Hash Subsystem
// ============================================================================

// 0x00464D00: Zentraler String-Pool & Hasher für Audio-, Entity- und Script-Strings
uint32_t HashString(const char* str);

// 0x0043E6F0: Animation-Name Hasher (Gibt im Retail-Build 0 zurück)
uint32_t HashAnimName(const char* animName);

// ============================================================================
// 3. BTFB Script Interpreter
// ============================================================================

// 0x00413500: Führt ein BTFB-Script-Kommando anhand seines Befehls-Hashes aus
void ExecuteScriptCmd(uint32_t dwCommandHash);

// ============================================================================
// 4. UI-Subsystem
// ============================================================================

// 0x004112C0: Initialisiert UI-Skalierung und Geometrie-Metriken
bool InitUISubsystem();

// 0x00416AA0: Erzeugt eine UI-Text-Box
void* CreateUITextBox(int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6);
void* CreateUITextBox(const char* id, const char* text, float x, float y, float w, float h);

// 0x00416AE0: Aktualisiert eine UI-Text-Box
void UpdateUITextBox(int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6);
void UpdateUITextBox(const char* id, const char* text, float x, float y, float w, float h);

// 0x004140D0: Zerstört eine UI-Text-Box
void DestroyUITextBox(void* pTextBox);
void DestroyUITextBox(const char* id);

// 0x00414020: Sucht ein UI-Element im aktiven Menü/HUD-Baum nach Name (case-insensitive)
void* FindUIElement(const char* elementName);

// 0x00461B40: Prüft, ob ein UI-Element sichtbar ist (inkl. Parent-Status)
bool IsUIElementVisible(void* pElement);
bool IsUIElementVisible(int32_t handle);
