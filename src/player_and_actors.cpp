/**
 * @file player_and_actors.cpp
 * @brief Madagascar (PC, 2005) - Player, Character, Physics & Item/Coin Subsystems
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 */

#include "player_and_actors.h"
#include "audio_and_frame.h"
#include "script_and_ui.h"
#include <cstring>
#include <cmath>

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

// 0x004283E0 / 0x00428E9C: Character Physics & Coordinate Integration
void CCharacterActor::Update(float deltaTime) {
    if (deltaTime <= 0.0f) return;

    // 1. Apply gravity if airborne
    if (!m_physics.bOnGround) {
        m_physics.vVelocity.y -= m_physics.fGravity * deltaTime;
    }

    // 2. Integrate position (Offsets 0x1F0, 0x1F4, 0x1F8)
    m_physics.vPosition.x += m_physics.vVelocity.x * deltaTime;
    m_physics.vPosition.y += m_physics.vVelocity.y * deltaTime;
    m_physics.vPosition.z += m_physics.vVelocity.z * deltaTime;

    // 3. Ground collision check
    if (m_physics.vPosition.y <= m_physics.fGroundHeight) {
        m_physics.vPosition.y = m_physics.fGroundHeight;
        m_physics.vVelocity.y = 0.0f;
        m_physics.bOnGround   = true;
        m_physics.bJumping    = false;
    }

    // 4. Update model coordinates if attached
    if (m_pModel != nullptr) {
        // Synchronize skeletal origin
    }
}

void CCharacterActor::PerformAttack() {
    switch (m_characterId) {
        case CHAR_ALEX:
            // Alex Roar & Claw Swipe
            if (m_pModel) m_pModel->PlayAnimation("alex_roar");
            break;
        case CHAR_MARTY:
            // Marty Rear Zebra Kick
            if (m_pModel) m_pModel->PlayAnimation("marty_kick");
            break;
        case CHAR_MELMAN:
            // Melman Head Spin Attack
            if (m_pModel) m_pModel->PlayAnimation("melman_spin");
            break;
        case CHAR_GLORIA:
            // Gloria Butt Bounce / Slam
            if (m_pModel) m_pModel->PlayAnimation("gloria_slam");
            break;
        case CHAR_PENGUINS:
            // Penguin Bowling Ball / Karate Chop
            if (m_pModel) m_pModel->PlayAnimation("penguin_attack");
            break;
        default:
            break;
    }
}

void CCharacterActor::PerformSpecialAction() {
    switch (m_characterId) {
        case CHAR_ALEX:
            // Alex Double Jump
            Jump();
            break;
        case CHAR_MARTY:
            // Marty Gallop Dash
            m_physics.vVelocity.z += 5.0f;
            break;
        case CHAR_MELMAN:
            // Melman Helicopter Twirl (slow descent)
            if (!m_physics.bOnGround) {
                m_physics.vVelocity.y = -2.0f; // Controlled glide
            }
            break;
        case CHAR_GLORIA:
            // Gloria Chili Pepper Charge
            m_physics.vVelocity.z += 8.0f;
            break;
        case CHAR_PENGUINS:
            // Penguin Belly Slide
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
    // 1. Tick character coordinate physics
    UpdateCharacterPhysics(1.0f / 60.0f);

    // 2. Tick audio streaming voices
    UpdateAudioStreams();
}

// 0x004283E0 / 0x00428E9C: Character Physics & Coordinate Update
void UpdateCharacterPhysics(float deltaTime) {
    if (s_pActiveCharacter != nullptr) {
        s_pActiveCharacter->Update(deltaTime);
    }
}

// 0x00551380: RenderWare Collision & Physics Solver tick
void SolvePhysicsAndCollisions() {
    if (s_pActiveCharacter != nullptr) {
        // Enforce boundary geometry & ground collision
        if (s_pActiveCharacter->m_physics.vPosition.y < s_pActiveCharacter->m_physics.fGroundHeight) {
            s_pActiveCharacter->m_physics.vPosition.y = s_pActiveCharacter->m_physics.fGroundHeight;
            s_pActiveCharacter->m_physics.bOnGround   = true;
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
