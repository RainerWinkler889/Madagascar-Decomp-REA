/**
 * @file player_and_actors.h
 * @brief Madagascar (PC, 2005) - Player, ECProtoActor, Physics & Item/Coin Subsystems
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 *
 * Mapped Subsystems & Functions:
 *  - ECProtoActor:
 *    * Class Descriptor ASCII: "ECProtoActor" at 0x005CD28F
 *    * VTable (18 virtual slots): 0x005CD2E8 (Base at 0x005CD2E0)
 *    * Slots:
 *      - Slot 0:  0x00436A40 (Destructor / Reset Virtual Thunk)
 *      - Slot 1:  0x0042CBC0 (Event Dispatcher)
 *      - Slot 2:  0x0042CC00 (Event Handler / Deleting Destructor)
 *      - Slot 3:  0x00402BB0 (Actor State Query -> 0x0043D2D0)
 *      - Slot 4:  0x0042CBF0 (Action Dispatch -> [ecx+0x20])
 *      - Slot 5:  0x0042CBE0 (BTFB Script HandleEvent -> returns 2)
 *      - Slot 6:  0x0043DC80 (BTFB Script ProcessStep -> zero token)
 *      - Slot 7:  0x004025A0 (BTFB Script GetStatus -> ret 8)
 *      - Slot 8:  0x004025B0 (BTFB Script Resume -> returns this)
 *      - Slot 9:  0x00464EF0 (BTFB Script GetTimer -> ret)
 *      - Slot 10: 0x00402B10 (Actor Query Transform -> returns 0x7FFFFFFF)
 *      - Slot 11: 0x0043E6F0 (Init / No-op -> returns 0)
 *      - Slot 12: 0x00402BD0 (Set Action Token -> reads [data+8])
 *      - Slot 13: 0x00402B80 (Query Action Token -> FPU ST(0) RwReal)
 *      - Slot 14: 0x00407AB0 (Entity Visibility Update -> dispatches float to int)
 *      - Slot 15: 0x004025A0 (Status -> ret 8)
 *      - Slot 16: 0x00402BC0 (Action Callback -> 0x0043D340)
 *      - Slot 17: 0x0042CDA0 (State Transition -> returns "attachers" 0x005CD370)
 *
 * Physics & Skeleton Subsystems:
 *  - 0x004283CF..0x00428E9C: Character Physics & Coordinate Update (writes [ebp+0x1F0..0x1F8])
 *  - 0x00428270: UpdateHAnimSkeletons (Master skeletal & pose update tick with 8 subfunctions)
 *    * 0x00426550: HAnimSub_TickAnimations
 *    * 0x0043CCA0: HAnimSub_UpdateNodeList
 *    * 0x0044DC30: HAnimSub_ResetBonePalettes
 *    * 0x00419380: HAnimSub_UpdateAudioVoices
 *    * 0x0043C9B0: HAnimSub_UpdateWorldHierarchies
 *    * 0x0042B150: HAnimSub_UpdateBoneTransforms
 *    * 0x00450490: HAnimSub_UpdateModelPalettes
 *    * 0x00464E20: HAnimSub_DispatchAnimEvents
 *  - 0x00551380: SolvePhysicsAndCollisions (Collision resolution pump)
 *  - 0x0043BCD8..0x0043BEA0: Item / Coin / Counter System (CItemManager)
 */

#pragma once

#include "common.h"
#include "rw_sdk.h"
#include "tfb_entities.h"
#include <cstdint>

// ============================================================================
// 1. Playable Character Identifiers
// ============================================================================

enum ECharacterId {
    CHAR_ALEX = 0,     // Alex the Lion
    CHAR_MARTY,        // Marty the Zebra
    CHAR_MELMAN,       // Melman the Giraffe
    CHAR_GLORIA,       // Gloria the Hippo
    CHAR_PENGUINS,     // Penguins (Skipper, Kowalski, Rico, Private)
    CHAR_COUNT
};

// ============================================================================
// 2. ECProtoActor Class (Descriptor 0x005CD28F, VTable 0x005CD2E8)
//
// Canonical Toys for Bob / Criterion RWS Actor entity.
// Reconstructed directly from Game.exe binary layout and VTable.
// ============================================================================

class ECProtoActor {
public:
    void*        m_pVTable;                   // +0x00: VTable 0x005CD2E8 (Base at 0x005CD2E0)
    uint8_t      m_bActive;                   // +0x04: 1 = Active, 0 = Inactive
    uint8_t      m_pad0[3];                   // +0x05
    uint32_t     m_dwFlags;                   // +0x08: Entity status flags
    uint32_t     m_dwTimer;                   // +0x0C: Tick / state timer
    uint32_t     m_dwStateId;                 // +0x10: Active state ID
    uint32_t     m_dwPrevStateId;             // +0x14: Previous state ID
    int32_t      m_nEventCounter;             // +0x18: Event reference counter (0x0042CBC0)
    uint32_t     m_dwActorType;               // +0x1C: Actor type ID (2)
    void*        m_pScriptVTable;             // +0x20: BTFB Script VTable (0x005CA3EC)
    uint32_t     m_reservedScript[30];        // +0x24..0x9B: Script node & handler state
    CTFBModel*   m_pModel;                    // +0x9C: Attached skeletal model
    uint32_t     m_reservedActor[23];         // +0xA0..0xFB: Internal actor state
    RwMatrix*    m_pRootTransform;            // +0xFC: Transformed root bone matrix
    uint32_t     m_dwBBoxFlags;               // +0x100
    uint32_t     m_reservedBBox[7];           // +0x104..0x11F
    RwMatrix     m_localMatrix;               // +0x120..0x15F: 4x4 Transformation Matrix (64 bytes)
    float        m_vBoundingOffset[3];        // +0x150, 0x154, 0x158: Local origin offset
    uint32_t     m_padPhysics[37];            // +0x15C..0x1EF
    RwV3d        m_vWorldPos;                 // +0x1F0, 0x1F4, 0x1F8: World Position (X, Y, Z)
    float        m_fGroundHeight;             // +0x1FC: Current ground plane elevation
    RwV3d        m_vVelocity;                 // +0x200..0x20B: Velocity vector (Vx, Vy, Vz)
    float        m_fGravity;                  // +0x20C: Gravity fall acceleration
    bool         m_bOnGround;                 // +0x210: Ground contact flag
    bool         m_bJumping;                  // +0x211: Airborne jump flag
    uint8_t      m_padEnd[2];                 // +0x212
    ECharacterId m_characterId;               // +0x214: Active character profile ID

public:
    // 0x0042CB70 / 0x0043D740: ECProtoActor Constructor
    ECProtoActor(ECharacterId id = CHAR_ALEX);
    virtual ~ECProtoActor();

    // ========================================================================
    // Authentic ECProtoActor VTable Methods (0x005CD2E8)
    // ========================================================================

    // Slot 0: 0x00436A40 - Virtual Destructor / Reset Thunk
    virtual void DestructorThunk();

    // Slot 1: 0x0042CBC0 - Event Dispatcher
    virtual int32_t EventDispatcher(int32_t delta);

    // Slot 2: 0x0042CC00 - Event Handler / Scalar Deleting Destructor
    virtual void DeletingDestructor(uint8_t flags);

    // Slot 3: 0x00402BB0 - Actor State Query (Jumps to 0x0043D2D0)
    virtual void* QueryStateSingleton();

    // Slot 4: 0x0042CBF0 - Action Dispatch (Returns pointer to [ecx+0x20])
    virtual void* GetActionBuffer();

    // Slot 5: 0x0042CBE0 - BTFB Script HandleEvent (Returns 2 = BTFB_ACTOR_TYPE)
    virtual int32_t GetActorType();

    // Slot 6: 0x0043DC80 - BTFB Script ProcessStep (Zeroes token & returns 0)
    virtual int32_t ProcessStep(uint32_t* pToken);

    // Slot 7: 0x004025A0 - BTFB Script GetStatus (NOP ret 8)
    virtual int32_t GetStatus(int32_t p1, int32_t p2);

    // Slot 8: 0x004025B0 - BTFB Script Resume (Returns this)
    virtual ECProtoActor* Resume(int32_t p1, int32_t p2);

    // Slot 9: 0x00464EF0 - BTFB Script GetTimer (NOP ret)
    virtual void GetTimer();

    // Slot 10: 0x00402B10 - Actor Query Transform (Returns 0x7FFFFFFF)
    virtual int32_t QueryTransform();

    // Slot 11: 0x0043E6F0 - Init / No-op (Returns 0)
    virtual int32_t InitNoOp();

    // Slot 12: 0x00402BD0 - Set Action Token (Reads [data+8])
    virtual uint32_t SetActionToken(void* pData);

    // Slot 13: 0x00402B80 - Query Action Token (Returns RwReal on FPU ST(0))
    virtual float QueryActionToken(uint32_t tokenId);

    // Slot 14: 0x00407AB0 - Entity Visibility Update (Dispatches float to int)
    virtual void VisibilityUpdate(float visibility, int32_t mode);

    // Slot 15: 0x004025A0 - Status Query (NOP ret 8)
    virtual int32_t StatusNoOp(int32_t p1, int32_t p2);

    // Slot 16: 0x00402BC0 - Action Callback (Jumps to 0x0043D340)
    virtual void ActionCallback(void* pTarget, char flag);

    // Slot 17: 0x0042CDA0 - State Transition (Returns "attachers" 0x005CD370)
    virtual const char* GetStateTransitionName();

    // ========================================================================
    // Engine Subsystem Helpers
    // ========================================================================

    // 0x004283CF..0x00428E9C: Character Physics & Coordinate Integration
    void Update(float deltaTime);

    void SetPosition(const RwV3d* pPos);
    const RwV3d* GetPosition() const { return &m_vWorldPos; }
    ECharacterId GetCharacterId() const { return m_characterId; }
};

// Typedef for seamless backwards compatibility
typedef ECProtoActor CCharacterActor;

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

// Subfunctions called inside UpdateHAnimSkeletons (0x00428270):
void HAnimSub_TickAnimations();         // 0x00426550
void HAnimSub_UpdateNodeList();         // 0x0043CCA0
void HAnimSub_ResetBonePalettes();      // 0x0044DC30
void HAnimSub_UpdateAudioVoices();      // 0x00419380
void HAnimSub_UpdateWorldHierarchies(int32_t activeWorldCount); // 0x0043C9B0
void HAnimSub_UpdateBoneTransforms();   // 0x0042B150
void HAnimSub_UpdateModelPalettes();    // 0x00450490
void HAnimSub_DispatchAnimEvents();     // 0x00464E20

// 0x004283CF..0x00428E9C: Character Physics & Coordinate Update
void UpdateCharacterPhysics(ECProtoActor* pActor, float deltaTime);
void UpdateCharacterPhysics(float deltaTime);

// 0x00551380: RenderWare Collision & Physics Solver tick
void SolvePhysicsAndCollisions();

// Get the global item & coin manager instance
CItemManager* GetItemManager();

// Get active player actor instance
ECProtoActor* GetActivePlayerActor();
ECProtoActor* GetActivePlayerCharacter();

// Set the active player actor (Alex, Marty, Melman, Gloria, Penguins)
void SetActivePlayerActor(ECharacterId charId);
void SetActivePlayerCharacter(ECharacterId charId);
