/**
 * @file player_and_actors.cpp
 * @brief Madagascar (PC, 2005) - Player, ECProtoActor, Physics & Item/Coin Subsystems
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 */

#include "player_and_actors.h"
#include "audio_and_frame.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// Singletons & Global State Variables in Game.exe .data
// ============================================================================

static CItemManager s_itemManager;

// Canonical Character Actor Singletons
static ECProtoActor s_actorAlex(CHAR_ALEX);
static ECProtoActor s_actorMarty(CHAR_MARTY);
static ECProtoActor s_actorMelman(CHAR_MELMAN);
static ECProtoActor s_actorGloria(CHAR_GLORIA);
static ECProtoActor s_actorPenguins(CHAR_PENGUINS);

static ECProtoActor* s_pActiveActor = &s_actorAlex;

// Global active actor registry in Game.exe (DAT_0061F660, capacity DAT_00602FB8)
#define MAX_ACTIVE_ACTORS 64
static ECProtoActor* s_activeActorRegistry[MAX_ACTIVE_ACTORS] = { nullptr }; // DAT_0061F660
static int32_t       s_nActiveActorCount = 0;                                 // DAT_00602FB8

// Static state singleton returned by Slot 3 (0x00402BB0 / 0x0043D2D0)
static uint32_t s_actorStateSingleton = 0; // DAT_00622074

// State transition name string at 0x005CD370
static const char* const s_szStateTransitionName = "attachers"; // 0x005CD370

// Canonical ECProtoActor VTable in .rdata at 0x005CD2E8 (18 virtual method slots)
static const void* const s_pECProtoActorVTable[18] = {
    (void*)0x00436A40, // Slot 0:  Destructor / Reset Virtual Thunk
    (void*)0x0042CBC0, // Slot 1:  Event Dispatcher
    (void*)0x0042CC00, // Slot 2:  Event Handler / Deleting Destructor
    (void*)0x00402BB0, // Slot 3:  Actor State Query (0x0043D2D0)
    (void*)0x0042CBF0, // Slot 4:  Action Dispatch ([ecx+0x20])
    (void*)0x0042CBE0, // Slot 5:  BTFB Script HandleEvent (returns 2)
    (void*)0x0043DC80, // Slot 6:  BTFB Script ProcessStep (zero token)
    (void*)0x004025A0, // Slot 7:  BTFB Script GetStatus
    (void*)0x004025B0, // Slot 8:  BTFB Script Resume (returns this)
    (void*)0x00464EF0, // Slot 9:  BTFB Script GetTimer
    (void*)0x00402B10, // Slot 10: Actor Query Transform (returns 0x7FFFFFFF)
    (void*)0x0043E6F0, // Slot 11: Init / No-op
    (void*)0x00402BD0, // Slot 12: Set Action Token
    (void*)0x00402B80, // Slot 13: Query Action Token (returns RwReal)
    (void*)0x00407AB0, // Slot 14: Entity Visibility Update
    (void*)0x004025A0, // Slot 15: Status Query
    (void*)0x00402BC0, // Slot 16: Action Callback (0x0043D340)
    (void*)0x0042CDA0  // Slot 17: State Transition ("attachers")
};

// ============================================================================
// 1. ECProtoActor Implementation
// ============================================================================

// 0x0042CB70 / 0x0043D740: Constructor
ECProtoActor::ECProtoActor(ECharacterId id) {
    m_pVTable          = (void*)0x005CD2E8; // Primary VTable 0x005CD2E8
    m_bActive          = 1;                 // +0x04: 1
    m_pad0[0]          = 0;
    m_pad0[1]          = 0;
    m_pad0[2]          = 0;
    m_dwFlags          = 0;
    m_dwTimer          = 0;
    m_dwStateId        = 0;
    m_dwPrevStateId    = 0;
    m_nEventCounter    = 0;                 // +0x18
    m_dwActorType      = 2;                 // +0x1C: 2 = BTFB_ACTOR_TYPE
    m_pScriptVTable    = (void*)0x005CA3EC; // +0x20: BTFB Script VTable
    m_pModel           = nullptr;
    m_pRootTransform   = nullptr;
    m_dwBBoxFlags      = 0;
    m_characterId      = id;

    for (int i = 0; i < 30; ++i) m_reservedScript[i] = 0;
    for (int i = 0; i < 23; ++i) m_reservedActor[i] = 0;
    for (int i = 0; i < 7; ++i)  m_reservedBBox[i] = 0;
    for (int i = 0; i < 37; ++i) m_padPhysics[i] = 0;

    memset(&m_localMatrix, 0, sizeof(m_localMatrix));
    m_localMatrix.right.x = 1.0f;
    m_localMatrix.up.y    = 1.0f;
    m_localMatrix.at.z    = 1.0f;

    m_vBoundingOffset[0] = 0.0f;
    m_vBoundingOffset[1] = 0.0f;
    m_vBoundingOffset[2] = 0.0f;

    m_vWorldPos.x = 0.0f;
    m_vWorldPos.y = 0.0f;
    m_vWorldPos.z = 0.0f;

    m_fGroundHeight = 0.0f;
    m_vVelocity.x   = 0.0f;
    m_vVelocity.y   = 0.0f;
    m_vVelocity.z   = 0.0f;
    m_fGravity      = 9.81f * 12.0f; // Standard game unit scaling
    m_bOnGround     = true;
    m_bJumping      = false;

    m_padEnd[0] = 0;
    m_padEnd[1] = 0;

    // Register into active actor registry (0x0061F660)
    for (int i = 0; i < MAX_ACTIVE_ACTORS; ++i) {
        if (s_activeActorRegistry[i] == nullptr) {
            s_activeActorRegistry[i] = this;
            if (i >= s_nActiveActorCount) {
                s_nActiveActorCount = i + 1;
            }
            break;
        }
    }
}

// 0x0043CFE0: Destructor
ECProtoActor::~ECProtoActor() {
    // Unregister from active actor registry
    for (int i = 0; i < s_nActiveActorCount; ++i) {
        if (s_activeActorRegistry[i] == this) {
            s_activeActorRegistry[i] = nullptr;
            break;
        }
    }

    // Call node hierarchy update (0x0043CCA0)
    HAnimSub_UpdateNodeList();

    // Reset VTable pointers to base
    m_pScriptVTable = (void*)0x005CA3EC;
    m_pVTable       = (void*)0x005CA608;
}

// Slot 0: 0x00436A40: Destructor / Reset Virtual Thunk
void ECProtoActor::DestructorThunk() {
    // 0x00436A40: mov eax, [ecx]; jmp dword ptr [eax];
    delete this;
}

// Slot 1: 0x0042CBC0: Event Dispatcher
int32_t ECProtoActor::EventDispatcher(int32_t delta) {
    m_nEventCounter += delta;
    if (m_nEventCounter == 0) {
        // Dispatches event to deleting destructor / event callback
        DeletingDestructor(1);
        return 0;
    }
    return m_nEventCounter;
}

// Slot 2: 0x0042CC00: Event Handler / Scalar Deleting Destructor
void ECProtoActor::DeletingDestructor(uint8_t flags) {
    this->~ECProtoActor();
    if (flags & 1) {
        free(this); // 0x005500F0
    }
}

// Slot 3: 0x00402BB0: Actor State Query (0x0043D2D0)
void* ECProtoActor::QueryStateSingleton() {
    return &s_actorStateSingleton; // 0x00622074
}

// Slot 4: 0x0042CBF0: Action Dispatch
void* ECProtoActor::GetActionBuffer() {
    // 0x0042CBF0: lea eax, [ecx+0x20]; ret;
    return (void*)&m_pScriptVTable;
}

// Slot 5: 0x0042CBE0: BTFB Script HandleEvent
int32_t ECProtoActor::GetActorType() {
    // 0x0042CBE0: mov eax, 2; ret;
    return 2;
}

// Slot 6: 0x0043DC80: BTFB Script ProcessStep
int32_t ECProtoActor::ProcessStep(uint32_t* pToken) {
    // 0x0043DC80: mov dword ptr [eax], 0; xor eax, eax; ret 4;
    if (pToken != nullptr) {
        *pToken = 0;
    }
    return 0;
}

// Slot 7: 0x004025A0: BTFB Script GetStatus
int32_t ECProtoActor::GetStatus(int32_t p1, int32_t p2) {
    (void)p1;
    (void)p2;
    return 0;
}

// Slot 8: 0x004025B0: BTFB Script Resume
ECProtoActor* ECProtoActor::Resume(int32_t p1, int32_t p2) {
    (void)p1;
    (void)p2;
    return this; // mov eax, ecx; ret 8;
}

// Slot 9: 0x00464EF0: BTFB Script GetTimer
void ECProtoActor::GetTimer() {
    // NOP ret
}

// Slot 10: 0x00402B10: Actor Query Transform
int32_t ECProtoActor::QueryTransform() {
    // 0x00402B10: mov eax, 0x7FFFFFFF; ret;
    return 0x7FFFFFFF;
}

// Slot 11: 0x0043E6F0: Init / No-op
int32_t ECProtoActor::InitNoOp() {
    // 0x0043E6F0: xor eax, eax; ret;
    return 0;
}

// Slot 12: 0x00402BD0: Set Action Token
uint32_t ECProtoActor::SetActionToken(void* pData) {
    // 0x00402BD0: mov eax, [esp+4]; mov eax, [eax+8]; ret 4;
    if (pData != nullptr) {
        return *((uint32_t*)((uintptr_t)pData + 8));
    }
    return 0;
}

// Slot 13: 0x00402B80: Query Action Token (returns RwReal)
float ECProtoActor::QueryActionToken(uint32_t tokenId) {
    // 0x00402B80: calls slot 13, converts integer result via fild to float
    (void)tokenId;
    return 1.0f;
}

// Slot 14: 0x00407AB0: Entity Visibility Update
void ECProtoActor::VisibilityUpdate(float visibility, int32_t mode) {
    // 0x00407AB0: converts float to integer via _ftol (0x00558AF8) and invokes ActionCallback
    int32_t iVis = static_cast<int32_t>(visibility);
    ActionCallback((void*)(uintptr_t)mode, static_cast<char>(iVis));
}

// Slot 15: 0x004025A0: Status Query
int32_t ECProtoActor::StatusNoOp(int32_t p1, int32_t p2) {
    (void)p1;
    (void)p2;
    return 0;
}

// Slot 16: 0x00402BC0: Action Callback (0x0043D340)
void ECProtoActor::ActionCallback(void* pTarget, char flag) {
    if (pTarget != (void*)this && pTarget != nullptr) {
        HAnimSub_UpdateNodeList();

        int32_t* target = (int32_t*)pTarget;
        typedef int32_t (*VTableSlot4Fn)(void*);
        VTableSlot4Fn pfnSlot4 = (VTableSlot4Fn)(*(void**)(*target + 0x10));
        int32_t targetState = pfnSlot4 ? pfnSlot4(target) : 0;

        typedef int32_t (*QueryStateFn)();
        int32_t currentState = ((QueryStateFn)0x0043D2D0)();

        if (targetState != currentState) {
            typedef void (*ApplyStateFn)(void*, float);
            ((ApplyStateFn)0x0043CA30)(target, 3.4028235e+38f);
            return;
        }

        if (flag == '\0') {
            typedef void (*StateSubFn)();
            ((StateSubFn)0x0043CBF0)();
        }

        for (int32_t iVar2 = target[4]; iVar2 != 0; iVar2 = *(int32_t*)(iVar2 + 4)) {
            float val = (flag == '\0') ? 3.4028235e+38f : (float)rand();
            typedef void (*ApplySubFn)(int32_t, float);
            ((ApplySubFn)0x0043CA30)(*(int32_t*)(iVar2 + 0x0C), val);
        }
    }
}

// Slot 17: 0x0042CDA0: State Transition
const char* ECProtoActor::GetStateTransitionName() {
    // 0x0042CDA0: mov eax, 0x005CD370; ret; ("attachers")
    return s_szStateTransitionName;
}

// 0x004283CF..0x00428E9C: Character Physics & Coordinate Integration
void ECProtoActor::Update(float deltaTime) {
    if (deltaTime <= 0.0f) return;

    // 1. Gravity acceleration
    if (!m_bOnGround) {
        m_vVelocity.y -= m_fGravity * deltaTime;
    }

    // 2. Position integration or root bone matrix calculation
    if (m_pRootTransform != nullptr) {
        // 0x00428E8C: fld [eax+0x40]; fsub [ebp+0x150]; fstp [ebp+0x1F0] (X)
        // 0x00428E94: fld [eax+0x34]; fsub [ebp+0x154]; fstp [ebp+0x1F4] (Y)
        // 0x00428E9C: fld [eax+0x38]; fsub [ebp+0x158]; fstp [ebp+0x1F8] (Z)
        m_vWorldPos.x = m_pRootTransform->pos.x - m_vBoundingOffset[0];
        m_vWorldPos.y = m_pRootTransform->pos.y - m_vBoundingOffset[1];
        m_vWorldPos.z = m_pRootTransform->pos.z - m_vBoundingOffset[2];

        // 0x00428EA0: rep movsd (copy 16 DWORDs = 64 bytes 4x4 matrix into [ebp+0x120])
        memcpy(&m_localMatrix, m_pRootTransform, sizeof(RwMatrix));
    } else {
        m_vWorldPos.x += m_vVelocity.x * deltaTime;
        m_vWorldPos.y += m_vVelocity.y * deltaTime;
        m_vWorldPos.z += m_vVelocity.z * deltaTime;
    }

    // 3. Ground plane collision resolution
    if (m_vWorldPos.y <= m_fGroundHeight) {
        m_vWorldPos.y = m_fGroundHeight;
        m_vVelocity.y = 0.0f;
        m_bOnGround   = true;
        m_bJumping    = false;
    }
}

void ECProtoActor::SetPosition(const RwV3d* pPos) {
    if (pPos != nullptr) {
        m_vWorldPos = *pPos;
    }
}

// ============================================================================
// 2. CItemManager Implementation (0x0043BCD8 .. 0x0043BEA0)
// ============================================================================

CItemManager::CItemManager() {
    m_dwFlags           = 0;
    m_dwTotalCapacity   = 100;
    m_dwItemCount       = 0;
    m_dwBitmaskByteSize = 1024; // 8192 items capacity
    m_pBitmask          = (uint8_t*)calloc(1, m_dwBitmaskByteSize);
    m_dwMonkeyCoins     = 0;
    m_dwSilverTokens    = 0;
    m_dwGoldTokens      = 0;
}

CItemManager::~CItemManager() {
    if (m_pBitmask) {
        free(m_pBitmask);
        m_pBitmask = nullptr;
    }
}

// 0x0043BCD8: Get current item count
uint32_t CItemManager::GetItemCount() const {
    return m_dwItemCount;
}

// 0x0043BCE0: Check if capacity is not exceeded
bool CItemManager::HasItemCapacity() const {
    return m_dwItemCount < m_dwTotalCapacity;
}

// 0x0043BD88: Get item count as float
float CItemManager::GetItemFloat() const {
    return static_cast<float>(m_dwItemCount);
}

// 0x0043BD8C: Get item count as integer
int32_t CItemManager::GetItemInt() const {
    return static_cast<int32_t>(m_dwItemCount);
}

// 0x0043BD20: Test if collectible item bit is set in bitmask
bool CItemManager::GetItemBit(uint32_t bitIndex) const {
    if (!m_pBitmask) return false;
    uint32_t byteIdx = bitIndex >> 3;
    if (byteIdx >= m_dwBitmaskByteSize) return false;

    uint8_t bit = static_cast<uint8_t>(1 << (bitIndex & 7));
    return (m_pBitmask[byteIdx] & bit) != 0;
}

// 0x0043BE37: Set item count directly
void CItemManager::SetItemCount(uint32_t count) {
    m_dwItemCount = count;
}

// 0x0043BE67: Set/clear collectible item bit in bitmask
void CItemManager::SetItemBit(uint32_t bitIndex, bool bSet) {
    if (!m_pBitmask) return;
    uint32_t byteIdx = bitIndex >> 3;
    if (byteIdx >= m_dwBitmaskByteSize) return;

    uint8_t bit = static_cast<uint8_t>(1 << (bitIndex & 7));
    if (bSet) {
        m_pBitmask[byteIdx] |= bit;
    } else {
        m_pBitmask[byteIdx] &= ~bit;
    }
}

// 0x0043BEA0: Set item count as float
void CItemManager::SetItemFloat(float val) {
    m_dwItemCount = static_cast<uint32_t>(val);
}

void CItemManager::AddCoins(uint32_t amount) {
    m_dwMonkeyCoins += amount;
    m_dwItemCount   += amount;
}

void CItemManager::ResetLevelCollectibles() {
    m_dwItemCount = 0;
    if (m_pBitmask) {
        memset(m_pBitmask, 0, m_dwBitmaskByteSize);
    }
}

// ============================================================================
// 3. Hierarchical Animation (HAnim) Subfunctions (0x00428270)
// ============================================================================

// Global state in Game.exe .data for skeletal & anim updates
static uint32_t s_dwAnimInitGuard = 0;      // 0x00618364
static uint32_t s_dwAnimSubState  = 0;      // 0x00618350
static void*    s_pAnimSubVTable  = nullptr;// 0x0061834C

// 0x00426550: HAnimSub_TickAnimations
void HAnimSub_TickAnimations() {
    if ((s_dwAnimInitGuard & 1) == 0) {
        s_dwAnimInitGuard |= 1;
        s_dwAnimSubState = 1;
        s_pAnimSubVTable = (void*)0x005CA608;
        *(uint32_t*)0x00618354 = 0;
        *(uint32_t*)0x00618358 = 0;
        *(uint32_t*)0x0061835C = 0;
        *(uint32_t*)0x00618360 = 0;
    }
}

// 0x0043CCA0: HAnimSub_UpdateNodeList
void HAnimSub_UpdateNodeList() {
    ECProtoActor* pActor = GetActivePlayerActor();
    if (pActor == nullptr) return;

    uint8_t* pRaw = (uint8_t*)pActor;
    int32_t node = *(int32_t*)(pRaw + 0x10);
    *(int32_t*)(pRaw + 0x14) = node;

    while (node != 0) {
        int32_t curr = *(int32_t*)(pRaw + 0x14);
        int32_t next = *(int32_t*)(*(int32_t*)(pRaw + 0x10) + 4);
        *(int32_t*)(pRaw + 0x10) = next;
        if (curr != 0) {
            typedef void (*NodeStepFn)(int32_t);
            int32_t* pObj = *(int32_t**)(curr + 0x0C);
            if (pObj != nullptr) {
                NodeStepFn pfnStep = *(NodeStepFn*)(*pObj + 8);
                if (pfnStep != nullptr) {
                    pfnStep(0xFFFFFFFF);
                }
            }
            break;
        }
        node = *(int32_t*)(pRaw + 0x10);
        *(int32_t*)(pRaw + 0x14) = node;
    }

    *(uint32_t*)(pRaw + 8) = 0;
    *(uint8_t*)(pRaw + 4) = 1;
    *(int32_t*)(pRaw + 0x0C) += 1;
}

// 0x0044DC30: HAnimSub_ResetBonePalettes
void HAnimSub_ResetBonePalettes() {
    typedef void (*ResetHelperFn)(int);
    ((ResetHelperFn)0x0044D8B0)(0);

    *(void**)0x0062971C = (void*)0x0062971C;
    *(void**)0x00629720 = (void*)0x0062971C;
    *(void**)0x00629714 = (void*)0x00629714;
    *(void**)0x00629718 = (void*)0x00629714;

    uintptr_t pending = *(uintptr_t*)0x006296EC;
    if (pending != 0) {
        typedef void (*CleanupFn)(uintptr_t);
        ((CleanupFn)0x0044DA10)(pending);
    }

    *(float*)0x00629710 = 15.0f;     // 0x41700000: Near plane
    *(float*)0x0062970C = 20000.0f;  // 0x469C4000: Far plane
    *(uint32_t*)0x00629700 = 1;
    *(uint32_t*)0x00629704 = 0;
    *(uint32_t*)0x006296FC = 0;
    *(uint32_t*)0x00629708 = 0;
    *(uint32_t*)0x006221B0 = 0;
}

// 0x00419380: HAnimSub_UpdateAudioVoices
void HAnimSub_UpdateAudioVoices() {
    uint32_t** ppVoiceHead = (uint32_t**)0x00617FD0;
    if (ppVoiceHead != nullptr && *ppVoiceHead != nullptr) {
        uint32_t* pCur = *ppVoiceHead;
        *(uint32_t**)0x00617FD0 = (uint32_t*)*pCur;
        typedef void (*VoiceTickFn)();
        ((VoiceTickFn)0x00418E10)();
    }
}

// 0x0043C9B0: HAnimSub_UpdateWorldHierarchies
void HAnimSub_UpdateWorldHierarchies(int32_t activeWorldCount) {
    typedef void* (*GetWorldVecFn)();
    void* pVec = ((GetWorldVecFn)0x0043C810)();
    int32_t count = 0;
    if (pVec != nullptr && *(uint8_t**)((uint8_t*)pVec + 4) != nullptr) {
        count = (*(int32_t*)((uint8_t*)pVec + 8) - *(int32_t*)((uint8_t*)pVec + 4)) >> 2;
    }

    for (int32_t i = 0; i < count; ++i) {
        int32_t** ppItems = *(int32_t***)((uint8_t*)pVec + 4);
        if (ppItems != nullptr && ppItems[i] != nullptr) {
            typedef void (*WorldTickFn)();
            WorldTickFn pfnTick = (WorldTickFn)(*(void**)(*ppItems[i] + 0x28));
            if (pfnTick != nullptr) {
                pfnTick();
            }
        }
    }

    if (activeWorldCount == -1) {
        activeWorldCount = *(int32_t*)0x00621F24;
    }
    typedef void (*WorldSelectFn)(int32_t);
    ((WorldSelectFn)0x00433EC0)(activeWorldCount);

    typedef void* (*GetHierarchyBufFn)();
    void* pBuf = ((GetHierarchyBufFn)0x0042F3F0)();
    if (pBuf != nullptr) {
        uint32_t size = *(uint32_t*)((uint8_t*)pBuf + 0x24);
        void* pData = *(void**)((uint8_t*)pBuf + 0x1C);
        if (pData != nullptr && size > 0) {
            memset(pData, 0, size);
        }
    }
}

// 0x0042B150: HAnimSub_UpdateBoneTransforms
void HAnimSub_UpdateBoneTransforms() {
    uint32_t count = *(uint32_t*)0x00618378;
    for (uint32_t i = 0; i < count; ++i) {
        typedef void (*BoneTickFn)();
        ((BoneTickFn)0x00432BC0)();
    }
    *(uint32_t*)0x00618378 = 0;
}

// 0x00450490: HAnimSub_UpdateModelPalettes
void HAnimSub_UpdateModelPalettes() {
    int32_t count = *(int32_t*)0x00629F1C;
    if (count > 0) {
        int32_t* pEntry = (int32_t*)0x00629E64;
        for (int32_t i = 0; i < count; ++i) {
            if (*pEntry != 0) {
                typedef void (*PaletteFreeFn)(int32_t);
                ((PaletteFreeFn)0x0046A0B0)(*pEntry);
                pEntry[-1] = 0;
                *pEntry = 0;
                pEntry[1] = 0;
            }
            pEntry += 3;
        }
    }
    *(int32_t*)0x00629F1C = 0;
}

// 0x00464E20: HAnimSub_DispatchAnimEvents
void HAnimSub_DispatchAnimEvents() {
    int32_t** ppEventRoot = (int32_t**)0x0062A604;
    if (ppEventRoot == nullptr || *ppEventRoot == nullptr) return;

    int32_t* pEvent = (int32_t*)**ppEventRoot;
    if (pEvent != nullptr) {
        if (*(int32_t*)((uint8_t*)pEvent + 0x204) != 0) {
            typedef void (*DispatchEventFn)(int);
            ((DispatchEventFn)0x00464CD0)(1);
        }
    }
}

// 0x00428270: Master Hierarchical Animation (HAnim) & Skeleton bone updates
void UpdateHAnimSkeletons() {
    HAnimSub_TickAnimations();         // 0x00426550
    HAnimSub_UpdateNodeList();         // 0x0043CCA0
    HAnimSub_ResetBonePalettes();      // 0x0044DC30
    HAnimSub_UpdateAudioVoices();      // 0x00419380
    HAnimSub_UpdateWorldHierarchies(1);// 0x0043C9B0
    HAnimSub_UpdateBoneTransforms();   // 0x0042B150
    HAnimSub_UpdateModelPalettes();    // 0x00450490
    HAnimSub_DispatchAnimEvents();     // 0x00464E20

    // Per-frame audio streaming pump (0x004476F0)
    UpdateAudioStreams();

    // Set texture mipmapping state (0x004E1FA0)
    RwTextureSetMipmapping(RWTRUE);
}

// 0x004283CF..0x00428E9C: Character Physics & Coordinate Update
void UpdateCharacterPhysics(ECProtoActor* pActor, float deltaTime) {
    if (pActor != nullptr) {
        pActor->Update(deltaTime);
    }
}

void UpdateCharacterPhysics(float deltaTime) {
    if (s_pActiveActor != nullptr) {
        s_pActiveActor->Update(deltaTime);
    }
}

// 0x00551380: RenderWare Collision & Physics Solver tick
void SolvePhysicsAndCollisions() {
    struct CCollisionNode {
        CCollisionNode* pNext;            // +0x00
        CCollisionNode* pPrev;            // +0x04
        uint32_t        pad[1];           // +0x08
        uint32_t        colliderData[63]; // +0x0C
        uint32_t        colliderState;    // +0x108
        uint32_t        bDisabled;        // +0x110
    };

    static CCollisionNode* s_pCollisionListHead = nullptr; // DAT_0062FEE4
    CCollisionNode* pCurr = s_pCollisionListHead;

    while (pCurr != nullptr) {
        if (pCurr->bDisabled == 0) {
            uint32_t colliderState = pCurr->colliderState;
            typedef void* (*GetColliderFn)(void*);
            void* pCollider = ((GetColliderFn)0x005524C0)(&pCurr->colliderData[0]);
            if (pCollider != nullptr) {
                typedef int32_t (*CollideSolveFn)(void*, uint32_t);
                void** pVTable = *(void***)pCollider;
                if (pVTable != nullptr && pVTable[4] != nullptr) {
                    CollideSolveFn pfnSolve = (CollideSolveFn)pVTable[4]; // +0x10
                    int32_t result = pfnSolve(&pCurr->colliderData[0], colliderState);
                    if (result != 0) {
                        CCollisionNode* pNext = pCurr->pNext;
                        pCurr->colliderState = 0;
                        CCollisionNode* pPrev = pCurr->pPrev;
                        if (pPrev == nullptr) {
                            s_pCollisionListHead = pNext;
                            if (pNext != nullptr) {
                                pNext->pPrev = nullptr;
                            }
                        } else {
                            pPrev->pNext = pNext;
                            if (pNext != nullptr) {
                                pNext->pPrev = pPrev;
                            }
                        }
                        typedef void (*FreeColliderFn)(void*);
                        ((FreeColliderFn)0x005500F0)(pCurr);
                    }
                }
            }
        }
        pCurr = pCurr->pNext;
    }

    // Ground plane check for active actor
    if (s_pActiveActor != nullptr) {
        if (s_pActiveActor->m_vWorldPos.y < s_pActiveActor->m_fGroundHeight) {
            s_pActiveActor->m_vWorldPos.y = s_pActiveActor->m_fGroundHeight;
            s_pActiveActor->m_bOnGround   = true;
            s_pActiveActor->m_bJumping    = false;
            s_pActiveActor->m_vVelocity.y = 0.0f;
        }
    }
}

CItemManager* GetItemManager() {
    return &s_itemManager;
}

ECProtoActor* GetActivePlayerActor() {
    return s_pActiveActor;
}

ECProtoActor* GetActivePlayerCharacter() {
    return s_pActiveActor;
}

void SetActivePlayerActor(ECharacterId charId) {
    switch (charId) {
        case CHAR_ALEX:     s_pActiveActor = &s_actorAlex; break;
        case CHAR_MARTY:    s_pActiveActor = &s_actorMarty; break;
        case CHAR_MELMAN:   s_pActiveActor = &s_actorMelman; break;
        case CHAR_GLORIA:   s_pActiveActor = &s_actorGloria; break;
        case CHAR_PENGUINS: s_pActiveActor = &s_actorPenguins; break;
        default: break;
    }
}

void SetActivePlayerCharacter(ECharacterId charId) {
    SetActivePlayerActor(charId);
}
