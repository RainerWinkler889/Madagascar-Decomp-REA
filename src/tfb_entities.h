/**
 * @file tfb_entities.h
 * @brief Madagascar (PC, 2005) - Toys for Bob Custom Entity Classes (CTFB*)
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 * Original Engine: RenderWare Studio 3.x (RWS) + Toys for Bob Extensions
 *
 * Mapped Classes:
 *  - CTFBWorld   (Descriptor 0x00602EC8, Deserializer 0x0041B640)
 *  - CTFBModel   (Descriptor 0x00603548, Deserializer 0x00444FB0)
 *  - CTFBSound   (Descriptor 0x00602A10, Deserializer 0x00419450)
 *  - CTFBCommand (Descriptor 0x006029BC, Deserializer 0x00413370)
 */

#pragma once

#include "common.h"
#include "rw_sdk.h"
#include "rws_core.h"

// ============================================================================
// Toys for Bob Static Class Descriptors (Addresses in Game.exe .data)
// ============================================================================
#define TFB_DESCRIPTOR_WORLD    0x00602EC8 // "CTFBWorld"
#define TFB_DESCRIPTOR_MODEL    0x00603548 // "CTFBModel"
#define TFB_DESCRIPTOR_SOUND    0x00602A10 // "CTFBSound"
#define TFB_DESCRIPTOR_COMMAND  0x006029BC // "CTFBCommand"

// ============================================================================
// 1. CTFBWorld
//
// Manages world BSP geometry (RpWorld), camera linkage, clipping planes,
// dynamic shadow modes (0..3), and distance fog parameters.
// ============================================================================
class CTFBWorld {
public:
    void*       m_pVTable;            // Offset 0x00: Virtual table
    uint32_t    m_dwFlags;            // Offset 0x04: Entity flags (active/alive)
    uint32_t    m_reserved0[6];       // Offset 0x08..0x1F: Base entity members
    void*       m_pBSPRenderFn;       // Offset 0x20: BSP sector render function
    uint32_t    m_dwRenderFlags;      // Offset 0x24: Rendering flags
    uint32_t    m_reserved1;          // Offset 0x28
    char        m_szCameraLink1[8];   // Offset 0x2C: Primary "RwCamera*" linkage
    char        m_szCameraLink2[8];   // Offset 0x34: Secondary camera linkage
    char        m_szCameraLink3[8];   // Offset 0x3C: Tertiary camera linkage
    void*       m_pSkyDome;           // Offset 0x44: Sky / horizon geometry pointer

    // Global engine state managed by CTFBWorld in Game.exe .data
    static void*&    s_pMainWorld;         // 0x00629724: Active RpWorld pointer
    static float&    s_fNearClip;          // 0x00629710: Near clipping plane (0.1..6.0)
    static float&    s_fFarClip;           // 0x0062970C: Far clipping plane (5000.0)
    static uint32_t& s_dwShadowMode;       // 0x00629700: Shadow mode (0=off, 1=blob, 2=proj, 3=detailed)
    static float&    s_fFogStart;          // 0x006296F8: Fog start distance
    static float&    s_fFogEnd;            // 0x006296F4: Fog end distance
    static uint32_t& s_dwFogLinear;        // 0x006296F0: Fog range linear flag
    static uint8_t&  s_bFogEnabled;        // 0x00617D5C: Master fog enable
    static uint8_t&  s_bFogVolumetric;     // 0x00617D95: Volumetric fog enable
    static uint8_t&  s_bFogColorR;         // 0x00617D60: Fog Red
    static uint8_t&  s_bFogColorG;         // 0x00617D61: Fog Green
    static uint8_t&  s_bFogColorB;         // 0x00617D62: Fog Blue
    static uint8_t&  s_bFogColorA;         // 0x00617D63: Fog Alpha
    static float&    s_fFogDensity;        // 0x00617D98: Fog Density float

public:
    CTFBWorld();
    ~CTFBWorld();

    // 0x0041B640: Stream Deserializer
    void StreamRead(void* pStream);

    void SetClippingPlanes(float nearClip, float farClip);
    void SetFog(bool enable, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float startDist, float endDist);
    void SetShadowMode(uint32_t mode);
};

// ============================================================================
// 2. CTFBModel
//
// Manages hierarchical skeletal meshes (RpClump / RpAtomic), HAnim animation
// streams, bone transformations, and facial animation bones:
//   - TFB_MESH.MOUTH_BONE_ID (Mouth bone)
//   - TFB_MESH.EUL_BONE_ID   (Eye Upper Left)
//   - TFB_MESH.EUR_BONE_ID   (Eye Upper Right)
//   - TFB_MESH.ELL_BONE_ID   (Eye Lower Left)
//   - TFB_MESH.ELR_BONE_ID   (Eye Lower Right)
// ============================================================================

/**
 * @brief 64-byte bone record allocated per bone in CTFBModel (0x00444FB0)
 */
struct CTFBBoneNode {
    uint32_t dwBoneId;          // Offset 0x00
    uint32_t dwFlags;           // Offset 0x04
    uint32_t dwParentIndex;     // Offset 0x08
    uint32_t dwBoneIndex;       // Offset 0x0C
    uint32_t dwLimitFlags;      // Offset 0x14
    uint32_t dwState;           // Offset 0x1C
    float    fPitchLimit;       // Offset 0x34: Pitch angle limit (default 90.0f)
    float    fYawLimit;         // Offset 0x38: Yaw angle limit (default 30.0f)
    float    fRollLimit;        // Offset 0x3C: Roll angle limit (default 45.0f)
    uint32_t pad[6];
};

class CTFBModel {
public:
    void*           m_pVTable;            // Offset 0x00: Virtual table
    uint32_t        m_dwFlags;            // Offset 0x04: Entity flags
    uint32_t        m_reserved0[5];       // Offset 0x08..0x1B
    uint32_t        m_nBoneCount;         // Offset 0x1C: Total number of bones
    CTFBBoneNode*   m_pBones;             // Offset 0x20: Allocated array of CTFBBoneNode (64B each)
    void*           m_pHAnimAnims[9];     // Offset 0x24..0x47: Primary HAnim stream handles
    uint8_t         m_bAnimValid;         // Offset 0x48: Primary animation valid flag
    uint8_t         m_pad0[3];
    int32_t         m_nMouthBoneId;       // Offset 0x4C: TFB_MESH.MOUTH_BONE_ID
    void*           m_pSecondaryAnims[4]; // Offset 0x50..0x5F: Secondary animation streams
    uint8_t         m_bSecondaryValid;    // Offset 0x60: Secondary animation valid flag
    uint8_t         m_pad1[3];
    int32_t         m_nEyeUpperLeftBoneId;  // Offset 0x64 (100): TFB_MESH.EUL_BONE_ID
    int32_t         m_nEyeUpperRightBoneId; // Offset 0x68 (104): TFB_MESH.EUR_BONE_ID
    int32_t         m_nEyeLowerLeftBoneId;  // Offset 0x6C (108): TFB_MESH.ELL_BONE_ID
    int32_t         m_nEyeLowerRightBoneId; // Offset 0x70 (112): TFB_MESH.ELR_BONE_ID
    uint8_t         m_reserved1[164];     // Offset 0x74..0x117
    void*           m_pClumpResource;     // Offset 0x118: RpClump resource pointer
    uint8_t         m_reserved2[252];     // Offset 0x11C..0x217
    uint32_t        m_bonePalette[53];    // Offset 0x218..0x2EB: Bone default transform matrix palette
    uint32_t        m_dwAnimNameHash;     // Offset 0x2EC: Animation string name hash (0x0043E6F0)

public:
    CTFBModel();
    ~CTFBModel();

    // 0x00444FB0: Stream Deserializer
    void StreamRead(void* pStream);

    // 0x00444B60: Bind Facial Bone IDs (EUL, EUR, ELL, ELR)
    void BindFacialBones(void* pClump);

    // 0x00444AD0: Bind Mouth Bone ID
    void BindMouthBone(void* pClump);

    void PlayAnimation(const char* animName);
};

// ============================================================================
// 3. CTFBSound
//
// Manages audio playback, .krn dynamic background music, positional 3D sound
// falloff, volume control and memory vs streamed sound buffers.
// ============================================================================
class CTFBSound {
public:
    void*       m_pVTable;            // Offset 0x00: Virtual table
    uint32_t    m_dwFlags;            // Offset 0x04: Entity flags
    uint32_t    m_reserved0[5];       // Offset 0x08..0x1B
    void*       m_pSoundRegistry;     // Offset 0x1C: Sound resource registry pointer
    uint32_t    m_dwSoundNameHash;    // Offset 0x20: Hashed sound name (0x00464D00)
    void*       m_pSoundBank;         // Offset 0x24: Audio bank / group channel handle
    void*       m_pStreamHandle;      // Offset 0x28: Opened .krn stream handle (0x0044AFB0)
    uint8_t     m_bLoaded;            // Offset 0x30: Sound loaded flag
    uint8_t     m_pad0[3];
    uint32_t    m_dwChannelId;        // Offset 0x34: Voice channel ID
    uint32_t    m_dwCategoryId;       // Offset 0x38: Audio category ID (SFX vs BGM)
    float       m_fMaxDistance;       // Offset 0x3C: 3D falloff radius (distance * 12.0f)
    float       m_fVolume;            // Offset 0x40: Volume (volume * 0.01f, 0.0 .. 1.0)
    bool        m_bLooped;            // Offset 0x44: Loop flag
    bool        m_b3DPositional;      // Offset 0x45: 3D positional audio flag
    bool        m_bMuteOnFocusLoss;   // Offset 0x46: Mute when window loses focus
    bool        m_bAutoPlay;          // Offset 0x47: Auto play on level load
    bool        m_bStreamed;          // Offset 0x48: Streamed (.krn) vs memory sample
    bool        m_bMusicTrack;        // Offset 0x49: 1 = Background Music, 0 = Sound Effect
    bool        m_bPausable;          // Offset 0x4A: Pause when game pauses
    uint8_t     m_pad1;
    float       m_fPitch;             // Offset 0x4C: Frequency / pitch multiplier
    float       m_fPan;               // Offset 0x50: Stereo balance (-1.0 .. +1.0)
    void*       m_pTriggerVolume;     // Offset 0x54: Trigger volume handle

public:
    CTFBSound();
    ~CTFBSound();

    // 0x00419450: Stream Deserializer
    void StreamRead(void* pStream);

    void Play();
    void Stop();
    void SetVolume(float volume);
    void Set3DPosition(const RwV3d* pPos);
};

// ============================================================================
// 4. CTFBCommand
//
// Spatial trigger & scripting command executor:
// Evaluates bounding planes (0x004133E0) and runs "BTFB Script" commands.
// ============================================================================
class CTFBCommand {
public:
    uint32_t    m_dwCommandHash;      // Offset 0x00: Hashed script command string (0x00464D00)
    uint32_t    m_dwTriggerState;     // Offset 0x04: Trigger state / mode
    uint8_t     m_reserved[0x4C];     // Offset 0x08..0x53
    float*      m_pBoundPlanes;       // Offset 0x54: 4-plane geometric frustum/box for trigger volume

public:
    CTFBCommand();
    ~CTFBCommand();

    // 0x00413370: Stream Deserializer
    void StreamRead(void* pStream);

    // 0x004133E0: Geometric trigger volume test
    bool TestTriggerVolume(const RwV3d* pActorPos);

    // Execute script command
    void Execute();
};
