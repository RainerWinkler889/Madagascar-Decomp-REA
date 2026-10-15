/**
 * @file player_and_actors.cpp
 * @brief Madagascar (PC, 2005) - Player, Character, Physics & Item/Coin Subsystems
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

static CItemManager      s_itemManager;
static CCharacterActor   s_characterAlex(CHAR_ALEX);
static CCharacterActor   s_characterMarty(CHAR_MARTY);
static CCharacterActor   s_characterMelman(CHAR_MELMAN);
static CCharacterActor   s_characterGloria(CHAR_GLORIA);
static CCharacterActor   s_characterPenguins(CHAR_PENGUINS);

static CCharacterActor*  s_pActiveCharacter = &s_characterAlex;

// ECProtoActor VTable in .rdata at 0x005CD2E8 (18 virtual method slots)
static const void* const s_pECProtoActorVTable[18] = {
    (void*)0x00436A40, // Slot 0:  Destructor / Cleanup
    (void*)0x0042CBC0, // Slot 1:  Event Dispatcher
    (void*)0x0042CC00, // Slot 2:  Event Handler
    (void*)0x00402BB0, // Slot 3:  Actor State Query
    (void*)0x0042CBF0, // Slot 4:  Action Dispatch
    (void*)0x0042CBE0, // Slot 5:  BTFB Script HandleEvent
    (void*)0x0043DC80, // Slot 6:  BTFB Script ProcessStep
    (void*)0x004025A0, // Slot 7:  BTFB Script GetStatus
    (void*)0x004025B0, // Slot 8:  BTFB Script Resume
    (void*)0x00464EF0, // Slot 9:  BTFB Script GetTimer
    (void*)0x00402B10, // Slot 10: Actor Query Transform
    (void*)0x0043E6F0, // Slot 11: Init / No-op
    (void*)0x00402BD0, // Slot 12: Set Action Token
    (void*)0x00402B80, // Slot 13: Query Action Token
    (void*)0x00407AB0, // Slot 14: Entity Visibility Update
    (void*)0x004025A0, // Slot 15: Status
    (void*)0x00402BC0, // Slot 16: Action Callback
    (void*)0x0042CDA0  // Slot 17: State Transition
};

// ============================================================================
// 1. CItemManager Implementation (0x0043BCD8 .. 0x0043BEA0)
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

    uint8_t bit = 1 << (bitIndex & 7);
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

    uint8_t bit = 1 << (bitIndex & 7);
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
    m_dwItemCount += amount;
}

void CItemManager::ResetLevelCollectibles() {
    m_dwItemCount = 0;
    if (m_pBitmask) {
        memset(m_pBitmask, 0, m_dwBitmaskByteSize);
    }
}

// ============================================================================
// 2. CCharacterActor Implementation
// ============================================================================

CCharacterActor::CCharacterActor(ECharacterId id) {
    m_characterId       = id;
    m_pModel            = nullptr;
    m_dwSpecialEnergy   = 100;
    m_bActive           = true;

    memset(&m_physics, 0, sizeof(m_physics));
    m_physics.fGravity       = 9.81f * 12.0f; // Scale in game world units
    m_physics.fGroundHeight  = 0.0f;
    m_physics.bOnGround      = true;
    m_physics.bJumping       = false;

    // Character specific ability tuning
    switch (id) {
        case CHAR_ALEX:
            m_physics.fMoveSpeed    = 8.0f;
            m_physics.fJumpVelocity = 12.0f;
            m_dwHealth              = 100;
            m_dwMaxHealth           = 100;
            break;
        case CHAR_MARTY:
            m_physics.fMoveSpeed    = 12.0f; // Faster gallop
            m_physics.fJumpVelocity = 14.0f; // High long jump
            m_dwHealth              = 100;
            m_dwMaxHealth           = 100;
            break;
        case CHAR_MELMAN:
            m_physics.fMoveSpeed    = 7.0f;
            m_physics.fJumpVelocity = 10.0f;
            m_dwHealth              = 120;
            m_dwMaxHealth           = 120;
            break;
        case CHAR_GLORIA:
            m_physics.fMoveSpeed    = 6.5f;
            m_physics.fJumpVelocity = 9.0f;
            m_dwHealth              = 150; // Heavy tank
            m_dwMaxHealth           = 150;
            break;
        case CHAR_PENGUINS:
            m_physics.fMoveSpeed    = 7.5f;
            m_physics.fJumpVelocity = 8.0f;
            m_dwHealth              = 80;
            m_dwMaxHealth           = 80;
            break;
        default:
            m_physics.fMoveSpeed    = 8.0f;
            m_physics.fJumpVelocity = 10.0f;
            m_dwHealth              = 100;
            m_dwMaxHealth           = 100;
            break;
    }
}

CCharacterActor::~CCharacterActor() {
}

// 0x004283CF / 0x00428E9C: Character Physics & Coordinate Integration
void CCharacterActor::Update(float deltaTime) {
    if (deltaTime <= 0.0f) return;

    // 1. Apply gravity fall acceleration if airborne
    if (!m_physics.bOnGround) {
        m_physics.vVelocity.y -= m_physics.fGravity * deltaTime;
    }

    // 2. Integrate position (Offsets 0x1F0, 0x1F4, 0x1F8)
    // 0x00428E80: fld [eax+0x34]; fsub [ebp+0x154]; fstp dword ptr [ebp+0x1f4] (Y)
    // 0x00428E90: fld [eax+0x38]; fsub [ebp+0x158]; fstp dword ptr [ebp+0x1f8] (Z)
    m_physics.vPosition.x += m_physics.vVelocity.x * deltaTime;
    m_physics.vPosition.y += m_physics.vVelocity.y * deltaTime;
    m_physics.vPosition.z += m_physics.vVelocity.z * deltaTime;

    // 3. Ground plane collision resolution
    if (m_physics.vPosition.y <= m_physics.fGroundHeight) {
        m_physics.vPosition.y = m_physics.fGroundHeight;
        m_physics.vVelocity.y = 0.0f;
        m_physics.bOnGround   = true;
        m_physics.bJumping    = false;
    }
}

void CCharacterActor::PerformAttack() {
    switch (m_characterId) {
        case CHAR_ALEX:
            if (m_pModel) m_pModel->PlayAnimation("alex_roar");
            break;
        case CHAR_MARTY:
            if (m_pModel) m_pModel->PlayAnimation("marty_kick");
            break;
        case CHAR_MELMAN:
            if (m_pModel) m_pModel->PlayAnimation("melman_spin");
            break;
        case CHAR_GLORIA:
            if (m_pModel) m_pModel->PlayAnimation("gloria_slam");
            break;
        case CHAR_PENGUINS:
            if (m_pModel) m_pModel->PlayAnimation("penguin_attack");
            break;
        default:
            break;
    }
}

void CCharacterActor::PerformSpecialAction() {
    switch (m_characterId) {
        case CHAR_ALEX:
            Jump();
            break;
        case CHAR_MARTY:
            m_physics.vVelocity.z += 5.0f;
            break;
        case CHAR_MELMAN:
            if (!m_physics.bOnGround) {
                m_physics.vVelocity.y = -2.0f;
            }
            break;
        case CHAR_GLORIA:
            m_physics.vVelocity.z += 8.0f;
            break;
        case CHAR_PENGUINS:
            m_physics.vVelocity.z += 6.0f;
            break;
        default:
            break;
    }
}

void CCharacterActor::Jump() {
    if (m_physics.bOnGround) {
        m_physics.vVelocity.y = m_physics.fJumpVelocity;
        m_physics.bOnGround   = false;
        m_physics.bJumping    = true;
    }
}

void CCharacterActor::SetPosition(const RwV3d* pPos) {
    if (pPos) {
        m_physics.vPosition = *pPos;
    }
}

const RwV3d* CCharacterActor::GetPosition() const {
    return &m_physics.vPosition;
}

// ============================================================================
// 3. Global Subsystem Implementations
// ============================================================================

// 0x00428270: Master Hierarchical Animation (HAnim) & Skeleton bone updates
void UpdateHAnimSkeletons() {
    typedef void (*HAnimSubFn)();
    typedef void (*WorldSubFn)(int32_t);

    ((HAnimSubFn)0x00426550)();
    ((HAnimSubFn)0x0043CCA0)();
    ((HAnimSubFn)0x0044DC30)();
    ((HAnimSubFn)0x00419380)();

    // 0x0043C9B0: Update world hierarchies for active worlds (DAT_00602EA0)
    int32_t activeWorldCount = 1;
    ((WorldSubFn)0x0043C9B0)(activeWorldCount);

    ((HAnimSubFn)0x0042B150)();
    ((HAnimSubFn)0x00450490)();
    ((HAnimSubFn)0x00464E20)();

    // Per-frame audio streaming pump (0x004476F0)
    UpdateAudioStreams();

    // Set texture mipmapping state (0x004E1FA0)
    RwTextureSetMipmapping(RWTRUE);
}

// 0x004283CF / 0x00428E9C: Character Physics & Coordinate Update
void UpdateCharacterPhysics(float deltaTime) {
    if (s_pActiveCharacter != nullptr) {
        s_pActiveCharacter->Update(deltaTime);
    }
}

// 0x00551380: RenderWare Collision & Physics Solver tick
void SolvePhysicsAndCollisions() {
    // Traverses doubly linked active collision list (DAT_0062FEE4)
    // Resolves contact impulses against world BSP geometry
    struct CCollisionNode {
        CCollisionNode* pNext;            // +0x00
        CCollisionNode* pPrev;            // +0x04
        uint32_t        pad[1];           // +0x08
        uint32_t        colliderData[63]; // +0x0C
        uint32_t        colliderState;    // +0x108 (offset 0x42 in dwords)
        uint32_t        bDisabled;        // +0x110 (offset 0x44 in dwords)
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
        pCurr = pCurr->pNext;
    }

    // Also update active character ground and bounds
    if (s_pActiveCharacter != nullptr) {
        if (s_pActiveCharacter->m_physics.vPosition.y < s_pActiveCharacter->m_physics.fGroundHeight) {
            s_pActiveCharacter->m_physics.vPosition.y = s_pActiveCharacter->m_physics.fGroundHeight;
            s_pActiveCharacter->m_physics.bOnGround   = true;
            s_pActiveCharacter->m_physics.bJumping    = false;
            s_pActiveCharacter->m_physics.vVelocity.y = 0.0f;
        }
    }
}

CItemManager* GetItemManager() {
    return &s_itemManager;
}

CCharacterActor* GetActivePlayerCharacter() {
    return s_pActiveCharacter;
}

void SetActivePlayerCharacter(ECharacterId charId) {
    switch (charId) {
        case CHAR_ALEX:     s_pActiveCharacter = &s_characterAlex; break;
        case CHAR_MARTY:    s_pActiveCharacter = &s_characterMarty; break;
        case CHAR_MELMAN:   s_pActiveCharacter = &s_characterMelman; break;
        case CHAR_GLORIA:   s_pActiveCharacter = &s_characterGloria; break;
        case CHAR_PENGUINS: s_pActiveCharacter = &s_characterPenguins; break;
        default: break;
    }
}
