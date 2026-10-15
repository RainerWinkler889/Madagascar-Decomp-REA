/**
 * @file tfb_entities.cpp
 * @brief Madagascar (PC, 2005) - Toys for Bob Custom Entity Implementations (CTFB*)
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Activision / Beenox
 * Original Engine: RenderWare Studio 3.x (RWS) + Toys for Bob Extensions
 *
 * Mapped Functions:
 *  - 0x0041B640: CTFBWorld::StreamRead (World geometry, fog, clip, shadow modes)
 *  - 0x00444FB0: CTFBModel::StreamRead (Mesh hierarchy, bones, HAnim animations)
 *  - 0x00444B60: CTFBModel::BindFacialBones (EUL, EUR, ELL, ELR)
 *  - 0x00444AD0: CTFBModel::BindMouthBone (MOUTH)
 *  - 0x00419450: CTFBSound::StreamRead (.krn dynamic BGM, 3D falloff, volume)
 *  - 0x00413370: CTFBCommand::StreamRead (BTFB script hash deserializer)
 *  - 0x004133E0: CTFBCommand::TestTriggerVolume (Spatial bounding plane test)
 */

#include "tfb_entities.h"
#include "script_and_ui.h"
#include "audio_and_frame.h"
#include <cstring>

// ============================================================================
// Static Member Variable Bindings for CTFBWorld (Addresses in Game.exe .data)
// ============================================================================

void*&    CTFBWorld::s_pMainWorld     = *(void**)0x00629724;
float&    CTFBWorld::s_fNearClip      = *(float*)0x00629710;
float&    CTFBWorld::s_fFarClip       = *(float*)0x0062970C;
uint32_t& CTFBWorld::s_dwShadowMode   = *(uint32_t*)0x00629700;
float&    CTFBWorld::s_fFogStart      = *(float*)0x006296F8;
float&    CTFBWorld::s_fFogEnd        = *(float*)0x006296F4;
uint32_t& CTFBWorld::s_dwFogLinear    = *(uint32_t*)0x006296F0;
uint8_t&  CTFBWorld::s_bFogEnabled    = *(uint8_t*)0x00617D5C;
uint8_t&  CTFBWorld::s_bFogVolumetric = *(uint8_t*)0x00617D95;
uint8_t&  CTFBWorld::s_bFogColorR     = *(uint8_t*)0x00617D60;
uint8_t&  CTFBWorld::s_bFogColorG     = *(uint8_t*)0x00617D61;
uint8_t&  CTFBWorld::s_bFogColorB     = *(uint8_t*)0x00617D62;
uint8_t&  CTFBWorld::s_bFogColorA     = *(uint8_t*)0x00617D63;
float&    CTFBWorld::s_fFogDensity    = *(float*)0x00617D98;

// ============================================================================
// Internal RWS Stream Iteration Helpers (Addresses in Game.exe)
// ============================================================================
typedef void     (*RWSInitEntityFn)(void* pObj);
typedef void     (*RWSRegisterEntityFn)(void* pObj, void* pClassDesc);
typedef char     (*RWSStreamAtEndFn)();
typedef void*    (*RWSStreamGetAttrFn)();
typedef uint32_t (*RWSStreamGetAttrTypeFn)();
typedef void     (*RWSStreamNextAttrFn)();
typedef uint32_t (*RWSStringHashFn)(const char* str);

// ============================================================================
// 1. CTFBWorld Implementation
// ============================================================================

CTFBWorld::CTFBWorld() {
    m_pVTable       = (void*)0x005D56A4; // Original CTFBWorld vtable
    m_dwFlags       = 2;                 // Active entity flag
    m_pBSPRenderFn  = nullptr;
    m_dwRenderFlags = 0;
    m_reserved1     = 0;
    m_pSkyDome      = nullptr;
    memset(m_szCameraLink1, 0, sizeof(m_szCameraLink1));
    memset(m_szCameraLink2, 0, sizeof(m_szCameraLink2));
    memset(m_szCameraLink3, 0, sizeof(m_szCameraLink3));
}

CTFBWorld::~CTFBWorld() {
}

// 0x0041B640: CTFBWorld Stream Deserializer
void CTFBWorld::StreamRead(void* pStream) {
    // 1. Base Entity Constructor (0x00410380)
    ((RWSInitEntityFn)0x00410380)(this);

    // 2. Register Entity Class ("CTFBWorld", 0x00602EC8)
    ((RWSRegisterEntityFn)0x00550020)(this, (void*)TFB_DESCRIPTOR_WORLD);

    // 3. Deserialize Property Chunks Loop
    while (((RWSStreamAtEndFn)0x0054FE20)() == '\0') {
        ((RWSStreamGetAttrFn)0x0054FEC0)();
        uint32_t attrType = ((RWSStreamGetAttrTypeFn)0x0054FDF0)();
        void* pAttrData = ((RWSStreamGetAttrFn)0x0054FEC0)();
        uint8_t* pRaw = (uint8_t*)pAttrData + 8;

        switch (attrType) {
            case 0: { // Main World Geometry & Sub-Worlds (BSP)
                uint32_t subWorldType = *(uint32_t*)pRaw;
                // 0x00551000: Resolve Geometry Resource
                typedef void* (*ResolveResFn)(void*, void*, int, int);
                void* pRes = nullptr;
                void* pWorld = ((ResolveResFn)0x00551000)(pRaw, &pRes, 0, 0);

                if (subWorldType == 0 && pWorld != nullptr) {
                    s_pMainWorld = pWorld; // 0x00629724
                    // Check if world has BSP tree (0x004B8570)
                    typedef int32_t (*RpWorldGetNumSectorsFn)(void*);
                    if (((RpWorldGetNumSectorsFn)0x004B8570)(pWorld) == 0) {
                        m_pBSPRenderFn = (void*)0x0041B5F0; // Standard BSP renderer
                    } else {
                        m_pBSPRenderFn = (void*)0x0041B150; // Optimized sector renderer
                    }
                }
                break;
            }

            case 2: { // Near Clip Plane (0x00629710)
                float nearClip = *(float*)pRaw;
                if (nearClip > 6.0f) nearClip = 6.0f;
                if (nearClip >= s_fFarClip) nearClip = s_fFarClip - 1.0f;
                if (nearClip < 0.1f) nearClip = 0.1f;
                s_fNearClip = nearClip;
                break;
            }

            case 3: { // Far Clip Plane (0x0062970C)
                float farClip = *(float*)pRaw;
                if (farClip <= s_fNearClip) farClip = s_fNearClip + 1.0f;
                s_fFarClip = farClip;
                break;
            }

            case 4: { // Primary Camera Link Name ("RwCamera*")
                // Register message target at m_szCameraLink1
                typedef void (*LinkCamFn)(void*, void*, const char*);
                ((LinkCamFn)0x00550440)(&m_szCameraLink1, pRaw, "RwCamera*");
                break;
            }

            case 5: { // Secondary Camera Link
                typedef void (*LinkCamFn)(void*, void*, const char*);
                ((LinkCamFn)0x005508E0)(&m_szCameraLink2, pRaw, "RwCamera*");
                break;
            }

            case 6: { // Tertiary Camera Link
                typedef void (*LinkCamFn)(void*, void*, const char*);
                ((LinkCamFn)0x005508E0)(&m_szCameraLink3, pRaw, "RwCamera*");
                break;
            }

            case 8: { // Shadow Rendering Mode (0 = Off, 1 = Blob, 2 = Projective, 3 = Detailed)
                s_dwShadowMode = *(uint32_t*)pRaw;
                break;
            }

            case 12: { // Sky Dome pointer
                m_pSkyDome = *(void**)pRaw;
                break;
            }

            case 13: { // Fog Start Distance (0x006296F8)
                s_fFogStart = *(float*)pRaw;
                if (s_fFogStart <= s_fNearClip) s_fFogStart = s_fNearClip + 1.0f;
                break;
            }

            case 14: { // Fog End Distance & Range Mode (0x006296F4 / 0x006296F0)
                s_fFogEnd = *(float*)pRaw;
                s_dwFogLinear = (s_fFogEnd >= 0.0f) ? 1 : 0;
                break;
            }

            case 16: { // Fog Master Enable Flag (0x00617D5C)
                s_bFogEnabled = (*(int32_t*)pRaw != 0) ? 1 : 0;
                break;
            }

            case 17: { // Fog Density (0x00617D98)
                s_fFogDensity = *(float*)pRaw * 12.0f;
                break;
            }

            case 19: { // Fog ARGB Color (0x00617D60..63)
                uint32_t argb = *(uint32_t*)pRaw;
                s_bFogColorA = static_cast<uint8_t>((argb >> 24) & 0xFF);
                s_bFogColorR = static_cast<uint8_t>((argb >> 16) & 0xFF);
                s_bFogColorG = static_cast<uint8_t>((argb >> 8)  & 0xFF);
                s_bFogColorB = static_cast<uint8_t>(argb & 0xFF);
                break;
            }

            case 20: { // Fog Volumetric Enable Flag (0x00617D95)
                s_bFogVolumetric = (*(int32_t*)pRaw != 0) ? 1 : 0;
                break;
            }

            default:
                break;
        }

        ((RWSStreamNextAttrFn)0x0054FEA0)();
    }
}

void CTFBWorld::SetClippingPlanes(float nearClip, float farClip) {
    if (nearClip < 0.1f) nearClip = 0.1f;
    if (nearClip > 6.0f) nearClip = 6.0f;
    if (farClip <= nearClip) farClip = nearClip + 1.0f;
    s_fNearClip = nearClip;
    s_fFarClip  = farClip;
}

void CTFBWorld::SetFog(bool enable, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float startDist, float endDist) {
    s_bFogEnabled = enable ? 1 : 0;
    s_bFogColorR  = r;
    s_bFogColorG  = g;
    s_bFogColorB  = b;
    s_bFogColorA  = a;
    s_fFogStart   = startDist;
    s_fFogEnd     = endDist;
}

void CTFBWorld::SetShadowMode(uint32_t mode) {
    s_dwShadowMode = (mode <= 3) ? mode : 0;
}

// ============================================================================
// 2. CTFBModel Implementation
// ============================================================================

CTFBModel::CTFBModel() {
    m_pVTable             = (void*)0x005D5714;
    m_dwFlags             = 2;
    m_nBoneCount          = 0;
    m_pBones              = nullptr;
    m_bAnimValid          = 0;
    m_bSecondaryValid     = 0;
    m_nMouthBoneId        = -1;
    m_nEyeUpperLeftBoneId = -1;
    m_nEyeUpperRightBoneId= -1;
    m_nEyeLowerLeftBoneId = -1;
    m_nEyeLowerRightBoneId= -1;
    m_pClumpResource      = nullptr;
    m_dwAnimNameHash      = 0;
    memset(m_pHAnimAnims, 0, sizeof(m_pHAnimAnims));
    memset(m_pSecondaryAnims, 0, sizeof(m_pSecondaryAnims));
    memset(m_bonePalette, 0, sizeof(m_bonePalette));
}

CTFBModel::~CTFBModel() {
    if (m_pBones != nullptr) {
        // Free bone array via RenderWare allocator (0x00606C90)
        free(m_pBones);
        m_pBones = nullptr;
    }
}

// 0x00444B60: Bind Facial Eye Bones
void CTFBModel::BindFacialBones(void* pClump) {
    typedef int32_t (*ClumpGetNumAtomicsFn)(void*);
    typedef void*   (*ClumpGetAtomicFn)(void*, int32_t);
    typedef char*   (*AtomicGetUserDataStringFn)(void*);
    typedef int32_t (*AtomicGetBoneIdFn)(void*, int32_t);

    if (pClump == nullptr) return;

    // Iterates Clump Atomics and searches for bone identifier strings
    int32_t numAtomics = ((ClumpGetNumAtomicsFn)0x00574F10)(pClump);
    for (int32_t i = 0; i < numAtomics; i++) {
        void* pAtomic = ((ClumpGetAtomicFn)0x00574F90)(pClump, i);
        char* pName = ((AtomicGetUserDataStringFn)0x005754A0)(pAtomic);
        if (pName == nullptr) continue;

        if (strstr(pName, "TFB_MESH.EUL_BONE_ID") != nullptr) {
            m_nEyeUpperLeftBoneId = ((AtomicGetBoneIdFn)0x005754D0)(pAtomic, 0);
        }
        if (strstr(pName, "TFB_MESH.EUR_BONE_ID") != nullptr) {
            m_nEyeUpperRightBoneId = ((AtomicGetBoneIdFn)0x005754D0)(pAtomic, 0);
        }
        if (strstr(pName, "TFB_MESH.ELL_BONE_ID") != nullptr) {
            m_nEyeLowerLeftBoneId = ((AtomicGetBoneIdFn)0x005754D0)(pAtomic, 0);
        }
        if (strstr(pName, "TFB_MESH.ELR_BONE_ID") != nullptr) {
            m_nEyeLowerRightBoneId = ((AtomicGetBoneIdFn)0x005754D0)(pAtomic, 0);
        }
    }
}

// 0x00444AD0: Bind Mouth Bone ID
void CTFBModel::BindMouthBone(void* pClump) {
    typedef char*   (*AtomicGetUserDataStringFn)(void*);
    typedef int32_t (*AtomicGetBoneIdFn)(void*, int32_t);

    if (pClump == nullptr) return;
    char* pName = ((AtomicGetUserDataStringFn)0x005754A0)(pClump);
    if (pName != nullptr && strstr(pName, "TFB_MESH.MOUTH_BONE_ID") != nullptr) {
        m_nMouthBoneId = ((AtomicGetBoneIdFn)0x005754D0)(pClump, 0);
    }
}

// 0x00444FB0: CTFBModel Stream Deserializer
void CTFBModel::StreamRead(void* pStream) {
    // 1. Register Class ("CTFBModel", 0x00603548)
    ((RWSRegisterEntityFn)0x00550020)(this, (void*)TFB_DESCRIPTOR_MODEL);

    char szAnimName[1024] = { 0 };

    // 2. Deserialize Property Chunks Loop
    while (((RWSStreamAtEndFn)0x0054FE20)() == '\0') {
        ((RWSStreamGetAttrFn)0x0054FEC0)();
        uint32_t attrType = ((RWSStreamGetAttrTypeFn)0x0054FDF0)();
        void* pAttrData = ((RWSStreamGetAttrFn)0x0054FEC0)();
        uint8_t* pRaw = (uint8_t*)pAttrData + 8;

        switch (attrType) {
            case 1: { // 53 DWORDS: Default Bone Matrix Palette (0x218)
                memcpy(m_bonePalette, pRaw, 53 * sizeof(uint32_t));
                break;
            }

            case 2: { // Animation Name String -> Hash into 0x2EC
                strncpy(szAnimName, (const char*)pRaw, sizeof(szAnimName) - 1);
                // Compute hash via 0x0043E6F0
                typedef uint32_t (*HashAnimNameFn)(const char*);
                m_dwAnimNameHash = ((HashAnimNameFn)0x0043E6F0)(szAnimName);
                break;
            }

            case 4: { // Total Bone Count -> Allocate 64-byte Bone Array (0x20)
                m_nBoneCount = *(uint32_t*)pRaw;
                if (m_nBoneCount > 0) {
                    m_pBones = (CTFBBoneNode*)malloc(m_nBoneCount * sizeof(CTFBBoneNode));
                    if (m_pBones != nullptr) {
                        memset(m_pBones, 0, m_nBoneCount * sizeof(CTFBBoneNode));
                    }
                }
                break;
            }

            case 5: { // Per-Bone Properties (16 dwords = 64 bytes)
                uint32_t boneIndex = *(uint32_t*)(pRaw + 12);
                if (m_pBones != nullptr && boneIndex < m_nBoneCount) {
                    CTFBBoneNode* pNode = &m_pBones[boneIndex];
                    memcpy(pNode, pRaw, sizeof(CTFBBoneNode));
                    pNode->dwBoneIndex = boneIndex;

                    // Set default joint angle limits if unspecified
                    if (pNode->dwBoneId < 2) {
                        pNode->fPitchLimit = 90.0f; // 0x42B40000
                        pNode->dwLimitFlags = 0xF;
                        pNode->fYawLimit   = 30.0f; // 0x41F00000
                        pNode->fRollLimit  = 45.0f; // 0x42340000
                    }
                }
                break;
            }

            case 6: { // Primary HAnim Animation Linkage ("rwID_HANIMANIMATION")
                uint32_t slot = *(uint32_t*)pRaw;
                if (slot < 9) {
                    typedef void* (*ResolveResFn)(void*, void*, int, int);
                    void* pAnim = ((ResolveResFn)0x00551000)(pRaw, nullptr, 0, 0);
                    if (pAnim != nullptr) {
                        m_pHAnimAnims[slot] = pAnim;
                        m_bAnimValid = 1;
                    }
                }
                break;
            }

            case 7: { // Secondary HAnim Animation Linkage
                uint32_t slot = *(uint32_t*)pRaw;
                if (slot < 4) {
                    typedef void* (*ResolveResFn)(void*, void*, int, int);
                    void* pAnim = ((ResolveResFn)0x00551000)(pRaw, nullptr, 0, 0);
                    if (pAnim != nullptr) {
                        m_pSecondaryAnims[slot] = pAnim;
                        m_bSecondaryValid = 1;
                    }
                }
                break;
            }

            default:
                break;
        }

        ((RWSStreamNextAttrFn)0x0054FEA0)();
    }

    // 3. Post-Stream: Bind Skeleton & Facial Bones (0x00444B60 / 0x00444AD0)
    if (m_pClumpResource != nullptr) {
        BindMouthBone(m_pClumpResource);
        BindFacialBones(m_pClumpResource);
    }
}

void CTFBModel::PlayAnimation(const char* animName) {
    if (animName == nullptr) return;
    m_dwAnimNameHash = HashAnimName(animName); // 0x0043E6F0
}

// ============================================================================
// 3. CTFBSound Implementation
// ============================================================================

CTFBSound::CTFBSound() {
    m_pVTable          = (void*)0x005D56E4;
    m_dwFlags          = 2;
    m_pSoundRegistry   = nullptr;
    m_dwSoundNameHash  = 0;
    m_pSoundBank       = nullptr;
    m_pStreamHandle    = nullptr;
    m_bLoaded          = 0;
    m_dwChannelId      = 0;
    m_dwCategoryId     = 0;
    m_fMaxDistance     = 100.0f;
    m_fVolume          = 1.0f;
    m_bLooped          = false;
    m_b3DPositional    = false;
    m_bMuteOnFocusLoss = false;
    m_bAutoPlay        = false;
    m_bStreamed        = false;
    m_bMusicTrack      = false;
    m_bPausable        = true;
    m_fPitch           = 1.0f;
    m_fPan             = 0.0f;
    m_pTriggerVolume   = nullptr;
}

CTFBSound::~CTFBSound() {
    Stop();
}

// 0x00419450: CTFBSound Stream Deserializer
void CTFBSound::StreamRead(void* pStream) {
    // 1. Base Entity Constructor (0x00410380)
    ((RWSInitEntityFn)0x00410380)(this);

    // 2. Register Entity Class ("CTFBSound", 0x00602A10)
    ((RWSRegisterEntityFn)0x00550020)(this, (void*)TFB_DESCRIPTOR_SOUND);

    char szSoundFile[1024] = { 0 };

    // 3. Deserialize Property Chunks Loop
    while (((RWSStreamAtEndFn)0x0054FE20)() == '\0') {
        ((RWSStreamGetAttrFn)0x0054FEC0)();
        uint32_t attrType = ((RWSStreamGetAttrTypeFn)0x0054FDF0)();
        void* pAttrData = ((RWSStreamGetAttrFn)0x0054FEC0)();
        uint8_t* pRaw = (uint8_t*)pAttrData + 8;

        switch (attrType) {
            case 0: { // Sound File Path String (e.g. "sound\01.krn")
                strncpy(szSoundFile, (const char*)pRaw, sizeof(szSoundFile) - 1);
                // Convert to lowercase
                for (char* p = szSoundFile; *p; p++) {
                    if (*p >= 'A' && *p <= 'Z') *p = (*p + ('a' - 'A'));
                }
                break;
            }

            case 4: { // 3D Falloff Max Distance (distance * 12.0f)
                m_fMaxDistance = *(float*)pRaw * 12.0f;
                break;
            }

            case 5: { // Volume (0..100 -> 0.0 .. 1.0)
                m_fVolume = static_cast<float>(*(int32_t*)pRaw) * 0.01f;
                break;
            }

            case 6: { // Looping Flag
                m_bLooped = (*(int32_t*)pRaw != 0);
                break;
            }

            case 7: { // 3D Positional Audio Flag
                m_b3DPositional = (*(int32_t*)pRaw != 0);
                break;
            }

            case 8: { // Auto Play Flag
                m_bAutoPlay = (*(int32_t*)pRaw != 0);
                break;
            }

            case 9: { // Streamed Audio (.krn track) vs Memory Sample
                m_bStreamed = (*(int32_t*)pRaw != 0);
                break;
            }

            case 10: { // Category: 1 = Background Music (BGM), 0 = SFX
                m_bMusicTrack = (*(int32_t*)pRaw != 0);
                break;
            }

            case 11: { // Pitch Multiplier
                m_fPitch = *(float*)pRaw;
                break;
            }

            case 12: { // Stereo Pan
                m_fPan = *(float*)pRaw;
                break;
            }

            case 13: { // Pausable Flag
                m_bPausable = (*(int32_t*)pRaw != 0);
                break;
            }

            case 14: { // Mute On Focus Loss Flag
                m_bMuteOnFocusLoss = (*(int32_t*)pRaw != 0);
                break;
            }

            case 16: { // Audio Channel Group / Bank
                m_pSoundBank = GetSoundBank(*(uint32_t*)pRaw);
                break;
            }

            default:
                break;
        }

        ((RWSStreamNextAttrFn)0x0054FEA0)();
    }

    // 4. Resolve Sound Resource in Registry & Open File Stream (0x0044AFB0)
    if (szSoundFile[0] != '\0') {
        // Hash sound name via 0x00464D00
        m_dwSoundNameHash = HashString(szSoundFile); // 0x00464D00

        // Open audio stream handle
        m_pStreamHandle = OpenAudioStream(m_dwSoundNameHash);
        m_bLoaded = 1;
    }
}

void CTFBSound::Play() {
    if (!m_bLoaded || m_pStreamHandle == nullptr) return;
    PlaySoundStream(m_pStreamHandle, m_fVolume, m_bLooped);
}

void CTFBSound::Stop() {
    if (m_pStreamHandle != nullptr) {
        StopSoundStream(m_pStreamHandle);
    }
}

void CTFBSound::SetVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    m_fVolume = volume;
    if (m_pStreamHandle != nullptr) {
        SetSoundVolume(m_pStreamHandle, m_fVolume);
    }
}

void CTFBSound::Set3DPosition(const RwV3d* pPos) {
    if (pPos != nullptr && m_pStreamHandle != nullptr) {
        SetSound3DPosition(m_pStreamHandle, pPos);
    }
}

// ============================================================================
// 4. CTFBCommand Implementation
// ============================================================================

CTFBCommand::CTFBCommand() {
    m_dwCommandHash   = 0;
    m_dwTriggerState  = 0;
    m_pBoundPlanes    = nullptr;
    memset(m_reserved, 0, sizeof(m_reserved));
}

CTFBCommand::~CTFBCommand() {
}

// 0x00413370: CTFBCommand Stream Deserializer
void CTFBCommand::StreamRead(void* pStream) {
    // 1. Register Entity Class ("CTFBCommand", 0x006029BC)
    ((RWSRegisterEntityFn)0x00550020)(this, (void*)TFB_DESCRIPTOR_COMMAND);

    // 2. Read Attribute Chunks Loop
    while (((RWSStreamAtEndFn)0x0054FE20)() == '\0') {
        ((RWSStreamGetAttrFn)0x0054FEC0)();
        ((RWSStreamGetAttrTypeFn)0x0054FDF0)();
        void* pAttrData = ((RWSStreamGetAttrFn)0x0054FEC0)();
        const char* szCommandStr = (const char*)pAttrData + 8;

        // Hash script command string via 0x00464D00
        m_dwCommandHash = HashString(szCommandStr); // 0x00464D00

        ((RWSStreamNextAttrFn)0x0054FEA0)();
    }
}

// 0x004133E0: Geometric Trigger Volume Test
//
// Evaluates dot products of actor coordinate against 4 bounding planes:
//   Dist = (Nx * x + Ny * y + Nz * z) - D
// Point is strictly inside if all 4 signed distances are non-negative.
// ============================================================================
bool CTFBCommand::TestTriggerVolume(const RwV3d* pActorPos) {
    if (pActorPos == nullptr || m_pBoundPlanes == nullptr) {
        return false;
    }

    float* pPlane = m_pBoundPlanes; // 4 planes of 4 floats each (Nx, Ny, Nz, D)
    for (int i = 0; i < 4; i++) {
        float dotDist = (pPlane[0] * pActorPos->x +
                         pPlane[1] * pActorPos->y +
                         pPlane[2] * pActorPos->z) - pPlane[3];

        if (dotDist < 0.0f) {
            return false; // Point is outside this bounding half-space
        }
        pPlane += 4;
    }

    return true; // Point is inside trigger volume
}

void CTFBCommand::Execute() {
    if (m_dwCommandHash != 0) {
        // Dispatches "BTFB Script" command by hash to the engine script interpreter (0x00413500)
        ExecuteScriptCmd(m_dwCommandHash);
    }
}
