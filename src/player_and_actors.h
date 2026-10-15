/**
 * @file player_and_actors.h
 * @brief Madagascar (PC, 2005) - Player, Character, Physics & Item/Coin Subsystems
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 *
 * Mapped Subsystems & Functions:
 *  - 0x00428270: UpdateHAnimSkeletons (Master skeletal & pose update tick)
 *  - 0x004283E0 / 0x00428E9C: Character Physics & Coordinate Update (x,y,z at 0x1F0, 0x1F4, 0x1F8)
 *  - 0x00551380: SolvePhysicsAndCollisions (Collision resolution pump)
 *  - 0x0043BCD8..0x0043BEA0: Item / Coin / Counter System (CItemManager)
 *    * 0x0043BCD8: GetItemCount
 *    * 0x0043BCE0: HasItemCapacity
 *    * 0x0043BD20: GetItemBit (Bitset test for collected level items)
 *    * 0x0043BD88 / 0x0043BD8C: GetItemFloat / GetItemInt
 *    * 0x0043BE37: SetItemCount
 *    * 0x0043BE67: SetItemBit (Bitset mark for collected level items)
 *    * 0x0043BEA0: SetItemFloat
 *  - Character Entities (Alex, Marty, Melman, Gloria, Penguins)
 */

#pragma once

#include "common.h"
#include "rw_sdk.h"
#include "tfb_entities.h"
#include <cstdint>

// ============================================================================
// 1. Playable Character Identifiers & Ability Profiles
// ============================================================================

enum ECharacterId {
    CHAR_ALEX = 0,     // Alex the Lion: Roar attack, double jump, claw swipe
    CHAR_MARTY,        // Marty the Zebra: Zebra long jump, rear kick, gallop dash
    CHAR_MELMAN,       // Melman the Giraffe: Helicopter twirl, head swing, ground pound
    CHAR_GLORIA,       // Gloria the Hippo: Butt bounce, chili pepper sprint, charge
    CHAR_PENGUINS,     // Penguins (Skipper/Kowalski/Rico/Private): Slide, bowling ball, stealth
    CHAR_COUNT
};

// Character movement and physics state
struct CActorPhysicsState {
    RwV3d   vPosition;       // Offsets 0x1F0, 0x1F4, 0x1F8: World position
    RwV3d   vVelocity;       // Velocity vector
    RwV3d   vRotation;       // Orientation Euler angles (Pitch, Yaw, Roll)
    float   fGroundHeight;   // Current ground floor plane Y
    bool    bOnGround;       // Ground contact flag
    bool    bJumping;        // Airborne jump flag
    float   fMoveSpeed;      // Forward movement speed
    float   fJumpVelocity;   // Vertical jump impulse
    float   fGravity;        // Gravity acceleration (default: 9.81f * 12.0f)
};

// ============================================================================
// 2. Character Actor Class (CCharacterActor)
// ============================================================================

class CCharacterActor {
public:
    ECharacterId        m_characterId;
    CActorPhysicsState  m_physics;
    CTFBModel*          m_pModel;
    uint32_t            m_dwHealth;
    uint32_t            m_dwMaxHealth;
    uint32_t            m_dwSpecialEnergy;
    bool                m_bActive;

public:
    CCharacterActor(ECharacterId id = CHAR_ALEX);
    ~CCharacterActor();

    void Update(float deltaTime);
    void PerformAttack();
    void PerformSpecialAction();
    void Jump();
    void SetPosition(const RwV3d* pPos);
    const RwV3d* GetPosition() const;
    ECharacterId GetCharacterId() const { return m_characterId; }
};

// ============================================================================
// 3. Item & Coin Counter Subsystem (CItemManager)
//
// Reconstructed from 0x0043BCD8 .. 0x0043BEA0:
// Manages Monkey Coins, Collectible Paw Prints, Totems, and Level Collectibles
// using bitsets to prevent re-collection of acquired items.
// ============================================================================

class CItemManager {
public:
    uint32_t m_dwFlags;           // Offset 0x14
    uint32_t m_dwTotalCapacity;   // Offset 0x18: Total capacity / level target
    uint32_t m_dwItemCount;       // Offset 0x1C: Current collected count
    uint8_t* m_pBitmask;          // Offset 0x20: Bitset array for collected level items
    uint32_t m_dwBitmaskByteSize; // Size of bitset in bytes
    uint32_t m_dwMonkeyCoins;     // Total persistent monkey coins for shop
    uint32_t m_dwSilverTokens;    // Silver paw print tokens
    uint32_t m_dwGoldTokens;      // Gold paw print tokens

public:
    CItemManager();
    ~CItemManager();

    // 0x0043BCD8: Get current item count
    uint32_t GetItemCount() const;

    // 0x0043BCE0: Check if capacity is not exceeded
    bool HasItemCapacity() const;

    // 0x0043BD88: Get item count as float
    float GetItemFloat() const;

    // 0x0043BD8C: Get item count as integer
    int32_t GetItemInt() const;

    // 0x0043BD20: Test if collectible item bit is set in bitmask
    bool GetItemBit(uint32_t bitIndex) const;

    // 0x0043BE37: Set item count directly
    void SetItemCount(uint32_t count);

    // 0x0043BE67: Set/clear collectible item bit in bitmask
    void SetItemBit(uint32_t bitIndex, bool bSet);

    // 0x0043BEA0: Set item count as float
    void SetItemFloat(float val);

    // High-level coin and inventory helpers
    void AddCoins(uint32_t amount);
    uint32_t GetCoins() const { return m_dwMonkeyCoins; }
    void ResetLevelCollectibles();
};

// ============================================================================
// 4. Global Subsystem Prototypes
// ============================================================================

// 0x00428270: Master Hierarchical Animation (HAnim) & Skeleton bone updates
void UpdateHAnimSkeletons();

// 0x004283E0 / 0x00428E9C: Character Physics & Coordinate Update
void UpdateCharacterPhysics(float deltaTime);

// 0x00551380: RenderWare Collision & Physics Solver tick
void SolvePhysicsAndCollisions();

// Get the global item & coin manager instance
CItemManager* GetItemManager();

// Get the active player character instance
CCharacterActor* GetActivePlayerCharacter();

// Set the active player character (Alex, Marty, Melman, Gloria, Penguins)
void SetActivePlayerCharacter(ECharacterId charId);
