/**
 * @file rws_core.h
 * @brief Madagascar (PC, 2005) - Criterion RenderWare Studio (RWS) Framework Core
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Engine: RenderWare Studio 3.x (RWS) / Criterion Software
 *
 * Mapped Subsystems & Functions:
 *  - 0x00461F70: InitPostEngineStart (Camera creation, double-buffer clear, RWS init)
 *  - 0x00552F70: ProcessRWSStream (RWS chunk dispatcher for Levels\*.stream)
 *  - 0x00552F00: RegisterRWSChunkHandler (std::map chunk registration)
 *  - 0x00461E80: BroadcastRunningTick (iMsgStopSystem / mode transition)
 *  - 0x00461BD0: BroadcastModeChange (iMsgStartSystem / mode transition)
 *  - 0x005502C0: DispatchRWSMessages (Event queue pump & message dispatch)
 *  - 0x00550290: ProcessMessageQueue (Queue consumer & listener callback invoke)
 *  - 0x0054FF60: UpdateRWSEntities (Active entity tick & garbage collection)
 *  - 0x00550440: RegisterRWSMessage (Name to message pointer hash/mapping)
 *  - 0x00550BB0: BroadcastRWSMessage (Synchronous listener broadcast)
 */

#pragma once

#include "common.h"
#include "rw_sdk.h"
#include <map>

// ============================================================================
// RWS Chunk Types & Protocol Constants
// ============================================================================

// Standard RenderWare Studio Stream Chunks (0x500 - 0x7FF)
#define rwsID_CHUNK_WORLD_SETUP         0x0700
#define rwsID_CHUNK_MODE_CHANGE         0x070b
#define rwsID_CHUNK_RUNNING_TICK        0x070c
#define rwsID_CHUNK_CAMERA_TARGET       0x071d

// RWS Chunk Handler Callback
typedef void (*RWSChunkHandlerFn)(void* pChunkHeader);

// ============================================================================
// RWS Entity & Message Queue Structures (Game.exe memory layout)
// ============================================================================

/**
 * @brief RWS Active Entity linked list node (0x0054FF60)
 */
struct CRWSEntityNode {
    void*               pVTable;       // Offset 0x00: Pointer to virtual function table
    uint32_t            flags;         // Offset 0x04: Bit 1 (0x2) indicates alive/active
    uint32_t            pad[4];        // Offset 0x08..0x14: Reserved / internal flags
    CRWSEntityNode*     pNext;         // Offset 0x18: Pointer to next entity in active list
};

/**
 * @brief RWS Message Queue Item (0x00550290 / 0x005502C0)
 */
struct CRWSMessageNode {
    CRWSMessageNode*    pNext;         // Offset 0x00: Next message in queue
    uint32_t            dwMsgId;       // Offset 0x04: Registered message ID / Hash
    void*               pListener;     // Offset 0x08: Target listener object (vtable invoke)
    void*               pData;         // Offset 0x0C: Payload / argument data
};

// ============================================================================
// Global State Variables in Game.exe .data
// ============================================================================

extern CRWSEntityNode*  g_pRWSEntityListHead;     // 0x0062FEA8: Active entity list
extern void*            g_pRWSMessageQueueHead;    // 0x0062FEC8: Event queue head
extern uint32_t         g_dwRWSMessageQueueSentinel; // 0x0062FEC0: Queue empty sentinel
extern uint8_t          g_bSystemRunning;          // 0x0062A49B: 1 = Running, 0 = Paused/Stopped
extern void*            g_pMsgStartSystem;         // 0x0062A510: "iMsgStartSystem"
extern void*            g_pMsgStopSystem;          // 0x0062A518: "iMsgStopSystem"

// ============================================================================
// RWS Core API Declarations
// ============================================================================

// 0x00461F70: Post-engine startup: camera creation, directors camera & RWS messages
void InitPostEngineStart(uint32_t width, uint32_t height, uint32_t depth);

// 0x00552F70: Parse RWS Level Stream Chunks
uint8_t ProcessRWSStream(RwStream* pStream);

// 0x00552F00: Register RWS Chunk Handler
void RegisterRWSChunkHandler(uint32_t chunkId, RWSChunkHandlerFn handler);

// 0x00461E80: Broadcast RWS "iMsgStopSystem" / Tick Mode
void BroadcastRunningTick(void* param1, int32_t param2);

// 0x00461BD0: Broadcast RWS "iMsgStartSystem" / Mode Change
void BroadcastModeChange(void* param1, int32_t param2);

// 0x005502C0: Dispatch RWS Events in Queue
void DispatchRWSMessages(void* pQueue);

// 0x00550290: Process Message Queue until empty
void ProcessMessageQueue(void* pQueue);

// 0x0054FF60: Tick Active RWS Entities
void UpdateRWSEntities();

// 0x00550440: Register RWS Message Name ("iMsgStartSystem", "iMsgStopSystem", etc.)
void RegisterRWSMessage(void** ppMsgOut, const char* msgName, uint32_t priority);

// 0x00550BB0: Broadcast RWS Message to registered listeners
void BroadcastRWSMessage(void** ppMsg);

// 0x0044AE80: Collision sector initialization
void RWS_InitCollisionSector();

// 0x005509E0: Event system and deletion message initialization
void RWS_InitEventSystem(int param1, int param2);

// 0x005535F0: Register RWS Camera Target Chunk 0x712
void RWS_RegisterCameraTargetChunk();

// 0x00551980: Register RWS Spline Chunk 0x71C
void RWS_RegisterSplineChunk();

// 0x00460DA0: Render loading progress bar / paw prints during stream load
void RenderLoadingProgressBar();
