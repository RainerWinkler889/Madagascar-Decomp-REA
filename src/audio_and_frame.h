/**
 * @file audio_and_frame.h
 * @brief Madagascar (PC, 2005) - Audio Backend (.krn Streamer) & Frame Pipeline
 *
 * Reconstructed from: Game.exe (Image Base 0x00400000, MSVC 7.1)
 * Original Developer: Toys for Bob / Beenox / Activision
 *
 * Mapped Subsystems & Functions:
 *  - 0x00451470: SoundInitHook (Audio engine startup & mixer init)
 *  - 0x004476F0: UpdateAudioStreams (Per-frame streaming audio pump)
 *  - 0x0044F580: GetSoundBank (Audio bank & string ID lookup)
 *  - 0x0044AFB0: OpenAudioStream (Wave dictionary lookup & .krn stream opener)
 *  - 0x0044B200: PlaySoundStream (Voice playback with volume & loop)
 *  - 0x0044B250: StopSoundStream (Stop playback & buffer reset)
 *  - 0x0044B2A0: SetSoundVolume (0.0f .. 1.0f volume scaling)
 *  - 0x0044B300: SetSound3DPosition (3D world position falloff)
 *
 * Frame & State Pipeline:
 *  - 0x004610B0: PreTickUpdate (Level loader, demo mode, Bink sound)
 *  - 0x0051A070: DirectInput_Init (DirectInput 8 device & gamepad init)
 *  - 0x00463030: BeginFrameRender (Frame target setup & clear)
 *  - 0x00463070: EndFrameRender (Frame target flush)
 *  - 0x00462720: PresentCameraRaster (RwCameraShowRaster)
 *  - 0x00462660: GetActiveCamera (0x0062A524)
 *  - 0x00462670: SetActiveCamera (0x0062A524)
 *  - 0x004614D0: CheckStateChangeRequest (Level transitions & loading screens)
 *  - 0x00461880: PostTickFrameStep (Post-tick timing)
 *  - 0x00462700: IsEnginePaused (0x00606C68)
 *  - 0x00461AE0: IsDebugHUDEnabled (FPS overlay timer tick)
 *  - 0x00461F10: Chunk700_WorldSetup (RWS Chunk 0x700 Handler)
 *  - 0x00461F40: Chunk71D_CameraTarget (RWS Chunk 0x71D Handler)
 *  - 0x004676A0: DirectorCamSetup (Director camera matrices)
 */

#pragma once

#include "common.h"
#include "rw_sdk.h"
#include <cstdint>

// ============================================================================
// 1. Audio Engine (.krn Streamer & DirectSound Mixer)
// ============================================================================

// 0x00451470: Initializes audio subsystem & global mixer (0x0046C240)
void SoundInitHook();

// 0x004476F0: Per-frame audio streaming pump
void UpdateAudioStreams();

// 0x0044F580: Lookup audio channel group / bank by ID
void* GetSoundBank(uint32_t bankId);

// 0x0044AFB0: Open streaming audio instance (.krn track or wave dictionary entry)
void* OpenAudioStream(uint32_t soundNameHash);

// 0x0044B200: Start sound stream playback
void PlaySoundStream(void* pStreamHandle, float volume, bool loop);

// 0x0044B250: Stop sound stream playback
void StopSoundStream(void* pStreamHandle);

// 0x0044B2A0: Set sound stream volume (0.0f .. 1.0f)
void SetSoundVolume(void* pStreamHandle, float volume);

// 0x0044B300: Set 3D positional audio coordinates
void SetSound3DPosition(void* pStreamHandle, const RwV3d* pPos);

// ============================================================================
// 2. Frame & Engine State Control Pipeline
// ============================================================================

// 0x004610B0: Pre-tick update (Input polling, title stream check, Bink init)
int32_t PreTickUpdate();

// 0x0051A070: DirectInput 8 gamepad & device initializer
bool DirectInput_Init(HWND hWnd);

// 0x00463030: Begin frame rendering pipeline
void BeginFrameRender();

// 0x00463070: End frame rendering pipeline
void EndFrameRender();

// 0x00462720: Present camera backbuffer raster (RwCameraShowRaster)
void PresentCameraRaster();

// 0x00462660: Get current active camera instance (0x0062A524)
RwCamera* GetActiveCamera();

// 0x00462670: Set active camera instance (0x0062A524)
void SetActiveCamera(RwCamera* camera);

// 0x004614D0: Check if level transition or pause state change is pending
char CheckStateChangeRequest();

// 0x00461880: Post-tick frame timing step
void PostTickFrameStep();

// 0x00462700: Check if game engine is paused (0x00606C68)
int32_t IsEnginePaused();

// 0x00461AE0: Check if debug HUD / FPS overlay is enabled & update FPS timer
char IsDebugHUDEnabled();

// 0x00461F10: RWS Chunk 0x700 Handler (World Setup)
void Chunk700_WorldSetup(void* pChunkHeader);

// 0x00461F40: RWS Chunk 0x71D Handler (Camera Target)
void Chunk71D_CameraTarget(void* pChunkHeader);

// 0x004676A0: Director camera matrix & projection setup
void DirectorCamSetup(RwCamera* pCamera);
