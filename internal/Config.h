#pragma once
#include <Windows.h>
#include <cstdlib>

namespace FloatingSubtitles { namespace Config {

extern float fHeadOffset;
extern float fMaxDistance;
extern float fFadeStartDistance;
extern int iOffScreenHandling;
extern int iFont;
extern float fFontSize;
extern bool bShowSpeakerName;
extern float fOffScreenY;
extern float fFloatingWrapWidth;
extern float fCenterWrapWidth;
extern bool bRequireLOS;
extern float fSubtitleScale;
extern bool bSuppressMenuDialogueTail;
extern float fMenuDialogueTailSeconds;
extern bool bAnimateInterruptReplace;
extern float fInterruptAnimSeconds;
extern float fInterruptFadeFloor;
extern float fInterruptSlideNorm;
extern bool bFadeInNewSubtitles;
extern float fNewSubtitleFadeSeconds;
extern bool bResolveSubtitleOverlap;
extern float fOverlapBoxWidthNorm;
extern float fOverlapBoxHeightNorm;
extern float fOverlapPaddingNorm;
extern float fOverlapMaxShiftNorm;

void Load(const char* path);

}}
