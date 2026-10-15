/**
 * @file rws_core.cpp
 * @brief Madagascar (PC, 2005) - Criterion RenderWare Studio (RWS) Framework Core
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Engine: RenderWare Studio 3.x (RWS) / Criterion Software
 *
 * Mapped Subsystems & Functions:
 *  - 0x00461F70: InitPostEngineStart
 *  - 0x00552F70: ProcessRWSStream
 *  - 0x00552F00: RegisterRWSChunkHandler
 *  - 0x00461E80: BroadcastRunningTick
 *  - 0x00461BD0: BroadcastModeChange
 *  - 0x005502C0: DispatchRWSMessages
 *  - 0x00550290: ProcessMessageQueue
 *  - 0x0054FF60: UpdateRWSEntities
 *  - 0x00550440: RegisterRWSMessage
 *  - 0x00550BB0: BroadcastRWSMessage
 */

#include "rws_core.h"
#include "rw_sdk.h"
#include "script_and_ui.h"
#include "audio_and_frame.h"

// ============================================================================
// Global State Variables in Game.exe .data
// ============================================================================

// 0x0062FEA8: Active RWS Entity List Head
CRWSEntityNode* g_pRWSEntityListHead = nullptr; // 0x0062FEA8

// 0x0062FEC8: Event Queue Head
void* g_pRWSMessageQueueHead = nullptr; // 0x0062FEC8

// 0x0062FEC0: Queue Empty Sentinel Value
uint32_t g_dwRWSMessageQueueSentinel = 0; // 0x0062FEC0

// 0x0062A49B: System Running Mode (1 = Running, 0 = Stopped/Paused)
uint8_t g_bSystemRunning = 0; // 0x0062A49B

// 0x0062A510: "iMsgStartSystem" Message Pointer
void* g_pMsgStartSystem = nullptr; // 0x0062A510

// 0x0062A518: "iMsgStopSystem" Message Pointer
void* g_pMsgStopSystem = nullptr; // 0x0062A518

// Internal RWS chunk handler dispatch registry (0x00552E70)
static std::map<uint32_t, RWSChunkHandlerFn> s_chunkHandlers;

// Time accumulator variables
static uint32_t s_dwTimeAccum1 = 0; // 0x0062A380
static uint32_t s_dwTimeAccum2 = 0; // 0x0062A384
static uint32_t s_dwTimeAccum3 = 0; // 0x0062A388

// ============================================================================
// Internal Engine Helper Stubs / Imports
// ============================================================================

// 0x004D5810: RenderWare Camera Clear
static void ClearCamera(RwCamera* camera, RwRGBA* color, int32_t clearFlags) {
    RwCameraClear(camera, color, clearFlags); // 0x004D5810
}

// ============================================================================
// 0x00552F00: RegisterRWSChunkHandler
//
// Registers a stream chunk deserializer callback for the given chunk ID.
// Reconstructed from std::map insertion logic at 0x00552F00.
// ============================================================================
void RegisterRWSChunkHandler(uint32_t chunkId, RWSChunkHandlerFn handler) {
    if (s_chunkHandlers.find(chunkId) == s_chunkHandlers.end()) {
        s_chunkHandlers[chunkId] = handler;
    }
}

// ============================================================================
// 0x00550440: RegisterRWSMessage
//
// Maps a text event name (e.g. "iMsgStartSystem", "iMsgStopSystem") to an
// internal 32-bit hashed event handle and stores it into *ppMsgOut.
// ============================================================================
void RegisterRWSMessage(void** ppMsgOut, const char* msgName, uint32_t priority) {
    typedef void (*RegisterRWSMsgFn)(void**, const char*, uint32_t);
    ((RegisterRWSMsgFn)0x00550440)(ppMsgOut, msgName, priority);
}

// ============================================================================
// 0x00550BB0: BroadcastRWSMessage
//
// Iterates through all entities subscribed to the message and dispatches it
// synchronously.
// ============================================================================
void BroadcastRWSMessage(void** ppMsg) {
    if (ppMsg == nullptr || *ppMsg == nullptr) {
        return;
    }
    typedef void (*BroadcastMsgFn)(void**);
    ((BroadcastMsgFn)0x00550BB0)(ppMsg);
}

// ============================================================================
// 0x00461BD0: BroadcastModeChange
//
// Broadcasts "iMsgStartSystem" to all RWS entities and transitions engine
// to running mode.
// ============================================================================
void BroadcastModeChange(void* param1, int32_t param2) {
    (void)param1;
    (void)param2;

    if (g_bSystemRunning == 0) {
        g_bSystemRunning = 1; // 0x0062A49B

        // Broadcast "iMsgStartSystem" (0x0062A510)
        if (g_pMsgStartSystem != nullptr) {
            void* msgPtr = g_pMsgStartSystem;
            BroadcastRWSMessage(&msgPtr);
        }

        // Set running flags in sub-managers
        typedef void (*SetStateFn)(int32_t);
        ((SetStateFn)0x004626F0)(1);
        ((SetStateFn)0x00461860)(1);
    }

    // Reset delta timer (0x00464F00)
    g_gameTimer.Reset();
}

// ============================================================================
// 0x00461E80: BroadcastRunningTick
//
// Broadcasts "iMsgStopSystem" to all RWS entities and transitions engine
// to stopped/paused mode.
// ============================================================================
void BroadcastRunningTick(void* param1, int32_t param2) {
    (void)param1;
    (void)param2;

    if (g_bSystemRunning != 0) {
        g_bSystemRunning = 0; // 0x0062A49B

        // Broadcast "iMsgStopSystem" (0x0062A518)
        if (g_pMsgStopSystem != nullptr) {
            void* msgPtr = g_pMsgStopSystem;
            BroadcastRWSMessage(&msgPtr);
        }

        // Clear running flags in sub-managers
        typedef void (*SetStateFn)(int32_t);
        ((SetStateFn)0x004626F0)(0);
        ((SetStateFn)0x00461860)(0);

        // Update high-resolution timer accumulators
        if (s_dwTimeAccum1 == 0) {
            typedef int32_t* (*GetTimerTicksFn)();
            int32_t* pTicks = ((GetTimerTicksFn)0x0043FB40)();
            if (pTicks != nullptr) {
                s_dwTimeAccum1 = *pTicks;
            }
        }
        s_dwTimeAccum2 += (s_dwTimeAccum1 - s_dwTimeAccum3);
        s_dwTimeAccum1 = 0;
        s_dwTimeAccum3 = 0;
    }
}

// ============================================================================
// 0x00550290: ProcessMessageQueue
//
// Iterates over all pending events in the queue until empty.
// ============================================================================
void ProcessMessageQueue(void* pQueue) {
    if (pQueue == nullptr) return;

    CRWSMessageNode** ppCurrent = (CRWSMessageNode**)pQueue;
    if (*ppCurrent == nullptr || *ppCurrent == (CRWSMessageNode*)(uintptr_t)g_dwRWSMessageQueueSentinel) {
        return;
    }

    while (*ppCurrent != nullptr && *ppCurrent != (CRWSMessageNode*)(uintptr_t)g_dwRWSMessageQueueSentinel) {
        CRWSMessageNode* pNode = *ppCurrent;
        if (pNode == nullptr) break;

        if (pNode->pListener != nullptr) {
            // Invoke virtual callback on listener object: listener->OnMessage(1)
            typedef void (__thiscall *ListenerCallbackFn)(void*, int32_t);
            void** pVTable = *(void***)pNode->pListener;
            if (pVTable != nullptr && *pVTable != nullptr) {
                ((ListenerCallbackFn)(*pVTable))(pNode->pListener, 1);
            }
        }

        // Falls der Listener-Aufruf den Knoten nicht selbst ausklinkt,
        // schalten wir die Warteschlange voran, um Endlosschleifen zu verhindern:
        if (*ppCurrent == pNode) {
            *ppCurrent = pNode->pNext;
        }
    }
}

// ============================================================================
// 0x005502C0: DispatchRWSMessages
//
// Entry point for the per-frame event dispatch pump.
// ============================================================================
void DispatchRWSMessages(void* pQueue) {
    if (pQueue == nullptr) return;

    void* queuePtr = *(void**)pQueue;
    if (queuePtr != (void*)(uintptr_t)g_dwRWSMessageQueueSentinel) {
        ProcessMessageQueue(pQueue); // 0x00550290
    }
}

// ============================================================================
// 0x0054FF60: UpdateRWSEntities
//
// Iterates through all registered RWS game entities:
//  - If bit 1 of flags is clear, the entity has been destroyed/marked dead:
//    unlinks node and calls virtual destructor.
//  - If bit 1 of flags is set, the entity remains active.
// ============================================================================
void UpdateRWSEntities() {
    CRWSEntityNode** ppPrev = &g_pRWSEntityListHead;
    CRWSEntityNode*  pCurr = g_pRWSEntityListHead;

    while (pCurr != nullptr) {
        if ((pCurr->flags & 0x02) == 0) {
            // Entity is dead: unlink from active list
            *ppPrev = pCurr->pNext;

            // Call virtual destructor / cleanup method: node->Destroy(1)
            typedef void (__thiscall *EntityDestructorFn)(void*, int32_t);
            void** pVTable = (void**)pCurr->pVTable;
            if (pVTable != nullptr && *pVTable != nullptr) {
                ((EntityDestructorFn)(*pVTable))(pCurr, 1);
            }
        } else {
            // Entity remains alive: advance previous pointer
            ppPrev = &pCurr->pNext;
        }

        pCurr = *ppPrev;
    }
}

// ============================================================================
// 0x00552F70: ProcessRWSStream
//
// Master deserialization loop for `.stream` files:
// Iterates through Criterion chunks, reads headers via RwStreamReadChunkHeaderInfo,
// dispatches known chunks to registered entity handlers, or skips unknown chunks.
// ============================================================================
uint8_t ProcessRWSStream(RwStream* pStream) {
    if (pStream == nullptr) return 0;

    struct RwChunkHeaderInfo {
        uint32_t type;
        uint32_t length;
        uint32_t version;
        uint32_t buildNum;
    };

    typedef RwStream* (*RwStreamReadChunkHeaderInfoFn)(RwStream*, RwChunkHeaderInfo*);
    typedef RwStream* (*RwStreamSkipFn)(RwStream*, uint32_t);
    typedef void (*PostChunkHookFn)();

    RwChunkHeaderInfo chunkHeader;

    while (true) {
        // Read RenderWare Chunk Header Info (0x004EE6D0)
        RwStream* pOk = ((RwStreamReadChunkHeaderInfoFn)0x004EE6D0)(pStream, &chunkHeader);
        if (pOk == nullptr) {
            break; // End of stream or error
        }

        // Look up registered chunk deserializer
        auto it = s_chunkHandlers.find(chunkHeader.type);
        if (it != s_chunkHandlers.end() && it->second != nullptr) {
            // Chunk is registered: invoke handler
            it->second(&chunkHeader);
        } else {
            // Unregistered chunk: skip payload bytes (0x004EFA90)
            ((RwStreamSkipFn)0x004EFA90)(pStream, chunkHeader.length);
        }

        // Post-chunk notification hook (0x00460DA0)
        ((PostChunkHookFn)0x00460DA0)();
    }

    return 1;
}

// ============================================================================
// 0x00461F70: InitPostEngineStart
//
// Initialisiert RenderWare Kamera, Raster, Direktorenkamera und registriert
// alle RWS System-Events und Chunk-Handler für die Level-Streams.
// ============================================================================
void InitPostEngineStart(uint32_t width, uint32_t height, uint32_t depth) {
    // 1. Random seed initialisieren
    int32_t t = (int32_t)CTimer::GetTimeTicks();
    srand((unsigned)(t + rand()));

    // 2. Erzeuge Haupt-Kamera & Z-Buffer / FrameBuffer Raster (0x0044E8E0)
    RwCamera* pCamera = RwCreateDefaultCameraAndRasters(width, height, depth);
    SetActiveCamera(pCamera); // 0x00462670

    // 3. Direktorenkamera & erweiterte Setups
    DirectorCamSetup(pCamera);

    typedef void (*CameraSubsystemFn)(RwCamera*);
    ((CameraSubsystemFn)0x00462A50)(pCamera);

    // 4. RWS Framework Initialisierungen
    typedef void (*RWSInitFn)();
    ((RWSInitFn)0x0044AE80)();
    
    typedef void (*RWSSetupFn)(int, int);
    ((RWSSetupFn)0x005509E0)(0, 0);

    ((RWSInitFn)0x005535F0)();
    ((RWSInitFn)0x00551980)();

    // 5. System Messages registrieren
    RegisterRWSMessage(&g_pMsgStartSystem, "iMsgStartSystem", 0); // 0x00550440
    RegisterRWSMessage(&g_pMsgStopSystem,  "iMsgStopSystem",  0); // 0x00550440

    // 6. Double-Buffer Clear (Clear front buffer and back buffer)
    RwRGBA blackColor = { 0, 0, 0, 255 };
    ClearCamera(pCamera, &blackColor, rwCAMERACLEARZ | rwCAMERACLEARIMAGE); // 0x004D5810 (rwCAMERACLEARZ | rwCAMERACLEARIMAGE)
    PresentCameraRaster();                // 0x00462720

    ClearCamera(pCamera, &blackColor, rwCAMERACLEARZ | rwCAMERACLEARIMAGE); // 0x004D5810
    PresentCameraRaster();                // 0x00462720

    // 7. Standard RWS Chunk Handlers registrieren (0x00552F00)
    RegisterRWSChunkHandler(rwsID_CHUNK_MODE_CHANGE,   (RWSChunkHandlerFn)BroadcastModeChange);  // 0x70b
    RegisterRWSChunkHandler(rwsID_CHUNK_RUNNING_TICK,  (RWSChunkHandlerFn)BroadcastRunningTick); // 0x70c
    RegisterRWSChunkHandler(rwsID_CHUNK_WORLD_SETUP,   (RWSChunkHandlerFn)Chunk700_WorldSetup);  // 0x700
    RegisterRWSChunkHandler(rwsID_CHUNK_CAMERA_TARGET, (RWSChunkHandlerFn)Chunk71D_CameraTarget);// 0x71d

    // 8. Sound & UI Initialisierungen
    SoundInitHook();

    (void)pCamera;
    InitUISubsystem();
}
