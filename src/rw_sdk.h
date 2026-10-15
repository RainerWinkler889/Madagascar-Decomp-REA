/**
 * @file rw_sdk.h
 * @brief Madagascar (PC, 2005) - Criterion RenderWare 3.x SDK Interface Declarations
 *
 * ============================================================================
 * STRIKTE PROJEKT-REGEL ("RENDERWARE-FIREWALL"):
 * ============================================================================
 * Die eigentliche RenderWare 3.x Engine (Bereich ca. 0x004D0000 bis 0x00559FFF)
 * sowie die MSVC 7.1 CRT (ab 0x0055A000) sind statisch in Game.exe gelinkt.
 * Als offizielles Criterion SDK 3.7 / RWS 3.x sind deren interne Implementierungen
 * bereits standardisiert und öffentlich bekannt.
 *
 * -> Interne RenderWare-Core-Funktionen im Bereich 0x004D0000–0x005C9000 werden
 *    daher NIEMALS dekompiliert, sondern als externe SDK-API behandelt!
 * -> Dieses Header deklariert alle kanonischen RenderWare 3.x Typen, Datenstrukturen
 *    und Funktionssignaturen, die vom Toys for Bob Spiel- und Framework-Code
 *    (0x00401000–0x004CFFFF) aufgerufen werden.
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <cstddef>

// ============================================================================
// RenderWare 3.x Primitive Types
// ============================================================================
typedef int32_t             RwInt32;
typedef uint32_t            RwUInt32;
typedef int16_t             RwInt16;
typedef uint16_t            RwUInt16;
typedef int8_t              RwInt8;
typedef uint8_t             RwUInt8;
typedef char                RwChar;
typedef float               RwReal;
typedef int32_t             RwBool;

#define RWTRUE              1
#define RWFALSE             0

// ============================================================================
// Mathematics & Vectors
// ============================================================================
struct RwV2d {
    RwReal x;
    RwReal y;
};

struct RwRect {
    RwInt32 x;
    RwInt32 y;
    RwInt32 w;
    RwInt32 h;
};

struct RwV3d {
    RwReal x;
    RwReal y;
    RwReal z;
};

struct RwRGBA {
    RwUInt8 red;
    RwUInt8 green;
    RwUInt8 blue;
    RwUInt8 alpha;
};

struct RwRGBAReal {
    RwReal red;
    RwReal green;
    RwReal blue;
    RwReal alpha;
};

struct RwMatrix {
    RwV3d   right;
    RwUInt32 flags;
    RwV3d   up;
    RwUInt32 pad1;
    RwV3d   at;
    RwUInt32 pad2;
    RwV3d   pos;
    RwUInt32 pad3;
};

// ============================================================================
// RenderWare Memory Management (0x00606C90)
// ============================================================================
typedef void* (*RwMemoryAllocFn)(size_t size, RwUInt32 hint);
typedef void  (*RwMemoryFreeFn)(void* mem);
typedef void* (*RwMemoryReallocFn)(void* mem, size_t newSize, RwUInt32 hint);
typedef void* (*RwMemoryCallocFn)(size_t numObj, size_t sizeObj, RwUInt32 hint);

struct RwMemoryFunctions {
    RwMemoryAllocFn   rwmalloc;
    RwMemoryFreeFn    rwfree;
    RwMemoryReallocFn rwrealloc;
    RwMemoryCallocFn  rwcalloc;
};

// ============================================================================
// Video Modes & Subsystem Setup
// ============================================================================
enum RwVideoModeFlag : RwUInt32 {
    rwVIDEOMODEEXCLUSIVE   = 0x0001,
    rwVIDEOMODEINTERLACE   = 0x0002,
    rwVIDEOMODEFFINTERLACE = 0x0004,
    rwVIDEOMODEFSAA0       = 0x0008,
    rwVIDEOMODEFSAA1       = 0x0010
};

struct RwVideoMode {
    RwInt32         width;
    RwInt32         height;
    RwInt32         depth;
    RwVideoModeFlag flags;
};

struct RwSubSystemInfo {
    RwChar name[80];
};

struct RwEngineOpenParams {
    void* displayID; // HWND under Win32 Direct3D 8
};

// ============================================================================
// Raster & Textures
// ============================================================================
enum RwRasterType {
    rwRASTERTYPENORMAL          = 0x00,
    rwRASTERTYPEZBUFFER         = 0x01,
    rwRASTERTYPECAMERA          = 0x02,
    rwRASTERTYPETEXTURE         = 0x04,
    rwRASTERTYPECAMERATEXTURE   = 0x05,
    rwRASTERTYPEMASK            = 0x07
};

enum RwRasterFormat {
    rwRASTERFORMATDEFAULT       = 0x0000,
    rwRASTERFORMAT1555          = 0x0100,
    rwRASTERFORMAT565           = 0x0200,
    rwRASTERFORMAT4444          = 0x0300,
    rwRASTERFORMATLUM8          = 0x0400,
    rwRASTERFORMAT8888          = 0x0500,
    rwRASTERFORMAT888           = 0x0600,
    rwRASTERFORMAT16            = 0x0700,
    rwRASTERFORMAT24            = 0x0800,
    rwRASTERFORMAT32            = 0x0900,
    rwRASTERFORMAT555           = 0x0a00
};

struct RwRaster {
    struct RwRaster* parent;
    RwUInt8*         cpPixels;
    RwUInt8*         palette;
    RwInt32          width;
    RwInt32          height;
    RwInt32          depth;
    RwInt32          stride;
    RwInt16          nOffsetX;
    RwInt16          nOffsetY;
    RwUInt8          cType;
    RwUInt8          cFlags;
    RwUInt8          privateFlags;
    RwUInt8          cFormat;
    RwUInt8*         originalPixels;
    RwInt32          originalWidth;
    RwInt32          originalHeight;
    RwInt32          originalStride;
};

struct RwTexture {
    RwRaster*   raster;
    void*       dict;
    void*       lInDictionary;
    RwChar      name[32];
    RwChar      mask[32];
    RwUInt32    filterAddressing;
    RwInt32     refCount;
};

struct RwImage {
    RwInt32     flags;
    RwInt32     width;
    RwInt32     height;
    RwInt32     depth;
    RwInt32     stride;
    RwUInt8*    cpPixels;
    RwRGBA*     palette;
};

typedef RwImage* (*RwImageCallBackRead)(const RwChar* imageName);
typedef RwImage* (*RwImageCallBackWrite)(RwImage* image, const RwChar* imageName);

// ============================================================================
// Camera & Frame
// ============================================================================
enum RwCameraProjection {
    rwNACAMERAPROJECTION = 0x00,
    rwPERSPECTIVE        = 0x01,
    rwPARALLEL           = 0x02
};

struct RwFrame {
    void*       object;
    void*       lChild;
    void*       lNext;
    void*       root;
    RwMatrix    modelling;
    RwMatrix    ltm;
    void*       pluginData;
};

struct RwCamera {
    void*               objectDevice;
    RwCameraProjection  projectionType;
    void*               beginUpdate;
    void*               endUpdate;
    void*               viewMatrix;
    RwRaster*           frameBuffer;    // Offset 0x64 (or 100)
    RwRaster*           zBuffer;        // Offset 0x60
    RwV2d               viewWindow;
    RwV2d               recipViewWindow;
    RwV2d               viewOffset;
    RwReal              nearPlane;
    RwReal              farPlane;
    RwReal              fogPlane;
    RwReal              zScale;
    RwReal              zShift;
    RwFrame*            frame;
};

enum RwCameraClearModes {
    rwCAMERACLEARIMAGE    = 0x1,
    rwCAMERACLEARZ        = 0x2,
    rwCAMERACLEARSTENCIL  = 0x4
};

// ============================================================================
// Stream Subsystem
// ============================================================================
enum RwStreamType {
    rwSTREAMNASTREAM  = 0x00,
    rwSTREAMFILE      = 0x01,
    rwSTREAMFILENAME  = 0x02,
    rwSTREAMBUFFER    = 0x03,
    rwSTREAMCUSTOM    = 0x04
};

enum RwStreamAccessType {
    rwSTREAMNASTREAMACCESS = 0x00,
    rwSTREAMREAD           = 0x01,
    rwSTREAMWRITE          = 0x02,
    rwSTREAMAPPEND         = 0x03
};

struct RwStream;

// ============================================================================
// Plugins & Chunks
// ============================================================================
typedef void* (*RwPluginObjectConstructor)(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef void* (*RwPluginObjectDestructor)(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef void* (*RwPluginObjectCopy)(void* dstObject, const void* srcObject, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef RwStream* (*RwPluginDataChunkReadCallBack)(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef RwStream* (*RwPluginDataChunkWriteCallBack)(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef RwInt32 (*RwPluginDataChunkGetSizeCallBack)(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);

// Standard Plugin / Chunk IDs
#define rwID_WORLD              0x000B
#define rwID_SKINPLUGIN         0x011D
#define rwID_HANIMPLUGIN        0x012A

// Toys for Bob Custom Chunk IDs
#define rwID_TFB_MATERIAL       0x800000F6
#define rwID_TFB_ATOMIC         0x800000F0

// ============================================================================
// Core RenderWare API Function Declarations (Static Link in Game.exe)
// ============================================================================
#ifdef __cplusplus
extern "C" {
#endif

// 0x004D7FA0: Get number of available graphics subsystems (Direct3D adapters)
RwInt32 RwEngineGetNumSubSystems(void);

// 0x004D7FE0: Get information string / adapter name for subsystem index
RwSubSystemInfo* RwEngineGetSubSystemInfo(RwSubSystemInfo* subSystemInfo, RwInt32 subSystemIndex);

// 0x004D8050: Set active graphics subsystem
RwBool RwEngineSetSubSystem(RwInt32 subSystemIndex);

// 0x004D8080: Get number of display video modes for currently active subsystem
RwInt32 RwEngineGetNumVideoModes(void);

// 0x004D80B0: Query width, height, color depth, and flags for video mode index
RwVideoMode* RwEngineGetVideoModeInfo(RwVideoMode* modeinfo, RwInt32 modeIndex);

// 0x004D8120: Set active video mode
RwBool RwEngineSetVideoMode(RwInt32 videoMode);

// 0x004D81F0: Stop RenderWare graphics engine
RwBool RwEngineStop(void);

// 0x004D8260: Start RenderWare graphics engine (Initializes D3D8 device)
RwBool RwEngineStart(void);

// 0x004D8300: Close RenderWare engine
RwBool RwEngineClose(void);

// 0x004D83B0: Open RenderWare engine with display target window handle
RwBool RwEngineOpen(RwEngineOpenParams* initParams);

// 0x004D85E0: Terminate RenderWare core and free memory arenas
RwBool RwEngineTerm(void);

// 0x004D8620: Initialize RenderWare core function table & memory allocator
RwBool RwEngineInit(const RwMemoryFunctions* memFuncs, RwUInt32 initFlags, RwUInt32 resArenaSize);

// 0x004E8830: Set Direct3D 8 Multisampling (FSAA: 0 = Off, 2 = 2x FSAA)
void RwD3D8SetMultisampling(RwInt32 multisampling);

// 0x004D7F50: Register generic plugin extension into RenderWare engine
RwInt32 RwEngineRegisterPlugin(
    RwInt32 size,
    RwUInt32 pluginID,
    RwPluginObjectConstructor initCB,
    RwPluginObjectDestructor termCB
);

// 0x004E42F0: Register custom image format decoder/encoder (.png, .bmp, .ras, .tif)
RwBool RwImageRegisterImageFormat(
    const RwChar* extension,
    RwImageCallBackRead readCB,
    RwImageCallBackWrite writeCB
);

// 0x004D5A20: Create a new RenderWare Camera
RwCamera* RwCameraCreate(void);

// 0x004D55F0: Set camera near clipping plane
RwCamera* RwCameraSetNearClipPlane(RwCamera* camera, RwReal nearClip);

// 0x004D5720: Set camera far clipping plane
RwCamera* RwCameraSetFarClipPlane(RwCamera* camera, RwReal farClip);

// 0x004D5880: Set camera projection type (rwPERSPECTIVE / rwPARALLEL)
RwCamera* RwCameraSetProjection(RwCamera* camera, RwCameraProjection projection);

// 0x004D5910: Set camera view window / aspect ratio vector
RwCamera* RwCameraSetViewWindow(RwCamera* camera, const RwV2d* viewWindow);

// 0x004D9750: Attach coordinate Frame to Camera
RwCamera* RwCameraSetFrame(RwCamera* camera, RwFrame* frame);

// 0x004D5810: Clear camera frame and/or z-buffer
RwCamera* RwCameraClear(RwCamera* camera, RwRGBA* colour, RwInt32 clearFlags);

// 0x004D6000: Present/Display camera raster
RwCamera* RwCameraShowRaster(RwCamera* camera, void* pDev, RwUInt32 flags);

// 0x004D5590: Begin camera update
RwCamera* RwCameraBeginUpdate(RwCamera* camera);

// 0x004D5570: End camera update
RwCamera* RwCameraEndUpdate(RwCamera* camera);

// 0x004D59D0: Destroy camera instance
RwBool RwCameraDestroy(RwCamera* camera);

// 0x004D8D40: Create a new transformation Frame
RwFrame* RwFrameCreate(void);

// 0x004D9580: Set Frame transformation matrix to Identity
RwFrame* RwFrameSetIdentity(RwFrame* frame);

// 0x004E2BB0: Allocate a FrameBuffer or ZBuffer Raster
RwRaster* RwRasterCreate(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags);

// 0x004E28B0: Destroy raster instance
RwBool RwRasterDestroy(RwRaster* raster);

// 0x004E2910: Push raster rendering context
RwRaster* RwRasterPushContext(RwRaster* raster);

// 0x004E29E0: Pop raster rendering context
RwRaster* RwRasterPopContext(void);

// 0x004E2700: Clear active raster context
RwRaster* RwRasterClear(RwInt32 colour);

// 0x004E2790: Render raster at screen coordinates (x, y)
RwRaster* RwRasterRender(RwRaster* raster, RwInt32 x, RwInt32 y);

// 0x004E2680: Render raster scaled to destination rectangle
RwRaster* RwRasterRenderScaled(RwRaster* raster, RwRect* rect);

// 0x004E3EA0: Load image file from disk (.png, .bmp, etc.)
RwImage* RwImageRead(const RwChar* imageName);

// 0x004E4FA0: Copy image pixels into target raster
RwRaster* RwRasterSetFromImage(RwRaster* raster, RwImage* image);

// 0x004E3530: Destroy image buffer
RwBool RwImageDestroy(RwImage* image);

// 0x004E1660: Create RenderWare freelist memory pool
void* RwFreeListCreate(RwInt32 entrySize, RwInt32 entriesPerBlock, RwInt32 alignment, RwUInt32 hint);

// 0x004EFD00: Open RenderWare Stream (rwSTREAMFILENAME, rwSTREAMREAD, path)
RwStream* RwStreamOpen(RwStreamType type, RwStreamAccessType accessType, const void* pData);

// 0x004EFBF0: Close RenderWare Stream
RwBool RwStreamClose(RwStream* stream, void* pData);

// 0x004E1FA0: Set texture mipmapping state / flush cache
RwBool RwTextureSetMipmapping(RwBool enable);

// 0x00507300: Attach standard RenderWare World plugin
RwInt32 RpWorldPluginAttach(void);

// 0x00507530: Attach standard RenderWare Collision plugin
RwInt32 RpCollisionPluginAttach(void);

// 0x004C7BF0: Register Material plugin extension
RwInt32 RpMaterialRegisterPlugin(
    RwInt32 size,
    RwUInt32 pluginID,
    RwPluginObjectConstructor constructCB,
    RwPluginObjectDestructor destructCB,
    RwPluginObjectCopy copyCB
);

// 0x004C7C20: Register Material stream serialization callbacks
RwInt32 RpMaterialRegisterPluginStream(
    RwUInt32 pluginID,
    RwPluginDataChunkReadCallBack readCB,
    RwPluginDataChunkWriteCallBack writeCB,
    RwPluginDataChunkGetSizeCallBack getSizeCB
);

#ifdef __cplusplus
}
#endif
