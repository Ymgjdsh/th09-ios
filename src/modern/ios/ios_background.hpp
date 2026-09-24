#pragma once
#include "Background.hpp"
#if 0

#include "AnmManager.hpp"
#include "AnmVmId.hpp"

namespace th095
{
struct BackgroundStageHeader;
struct BackgroundStageObject;
struct BackgroundStageObjectInstance;
struct BackgroundStageInstruction;

struct BackgroundPhotoBlend
{
    f32 x, y;
    ZunColor color;
};

// Native owner shared by construction, stage loading, drawing and destruction.
// Disk offsets remain 32 bit; pointers and embedded VMs use their native sizes.
struct Background
{
    BackgroundStageHeader *stageData;
    BackgroundStageObject **stageObjects;
    BackgroundStageObjectInstance *stageObjectInstances;
    u8 *stageScript;
    ZunTimer stageScriptTimer;
    BackgroundStageInstruction *stageInstruction;
    ZunTimer interpolationCurrentTimers[4];
    ZunTimer interpolationEndTimers[4];
    u16 interpolationModes[4];
    Float3 cameraLookAtFinal, cameraLookAtInitial;
    Float3 cameraLookAtTangentFinal, cameraLookAtTangentInitial;
    Float3 cameraPositionFinal, cameraPositionInitial;
    Float3 cameraPositionTangentFinal, cameraPositionTangentInitial;
    u8 cameraMotionMode;
    u8 unknown00e9[7];
    AnmLoaded *anm;
    AnmVm *stageObjectVms;
    AnmVm stageVms[8];
    f32 cullingDistanceSq;
    i32 spellBackgroundFrameCounter;
    ZunColor photoColor;
    i32 photoAreaActive;
    Float3 photoAreaPosition, photoAreaSize;
    AnmVm photoAreaVms[3];
    AnmVmId spellBackgroundVms[2];
    BackgroundPhotoBlend photoBlendCurrent, photoBlendInitial, photoBlendFinal;
    ChainElem *calcChain, *drawHighChain, *drawLowChain;

    Background();
    ~Background();
    i32 Initialize();
    i32 Update();
    i32 UpdateStageObjectVms();
    i32 RunStageScript();
    i32 DrawHighPrio();
    i32 DrawLowPrio();
    i32 RenderObjects(i32 mode);
    i32 LoadStageData(const char *path);
    i32 LoadStageDataInner(const char *path);
    void SetPhotoArea(const Float3 *position, const Float3 *size);
    void StartSpellBackground();
    void StopSpellBackground();
    static i32 __fastcall OnUpdate(Background *background);
    static i32 __fastcall OnDrawHighPrio(Background *background);
    static i32 __fastcall OnDrawLowPrio(Background *background);
    static Background *Create();
};
}

#endif
