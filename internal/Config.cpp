#include "Config.h"

namespace FloatingSubtitles { namespace Config {

float fHeadOffset = 15.0f;
float fMaxDistance = 2048.0f;
float fFadeStartDistance = 1536.0f;
int iOffScreenHandling = 0;
int iFont = 3;
float fFontSize = 80.0f;
bool bShowSpeakerName = true;
float fOffScreenY = 0.85f;
float fFloatingWrapWidth = 400.0f;
float fCenterWrapWidth = 700.0f;
bool bRequireLOS = false;
float fSubtitleScale = 1.0f;
bool bSuppressMenuDialogueTail = true;
float fMenuDialogueTailSeconds = 1.2f;
bool bAnimateInterruptReplace = true;
float fInterruptAnimSeconds = 0.16f;
float fInterruptFadeFloor = 0.35f;
float fInterruptSlideNorm = 0.010f;
bool bFadeInNewSubtitles = true;
float fNewSubtitleFadeSeconds = 0.08f;
bool bResolveSubtitleOverlap = true;
float fOverlapBoxWidthNorm = 0.22f;
float fOverlapBoxHeightNorm = 0.08f;
float fOverlapPaddingNorm = 0.02f;
float fOverlapMaxShiftNorm = 0.20f;

static float GetFloat(const char* section, const char* key, float def, const char* path) {
	char buf[32];
	GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), path);
	return buf[0] ? (float)atof(buf) : def;
}

static int GetInt(const char* section, const char* key, int def, const char* path) {
	return GetPrivateProfileIntA(section, key, def, path);
}

void Load(const char* path) {
	WritePrivateProfileStringA(NULL, NULL, NULL, path);
	fHeadOffset = GetFloat("Settings", "fHeadOffset", fHeadOffset, path);
	fMaxDistance = GetFloat("Settings", "fMaxDistance", fMaxDistance, path);
	fFadeStartDistance = GetFloat("Settings", "fFadeStartDistance", fFadeStartDistance, path);
	iOffScreenHandling = GetInt("Settings", "iOffScreenHandling", iOffScreenHandling, path);
	iFont = GetInt("Settings", "iFont", iFont, path);
	fFontSize = GetFloat("Settings", "fFontSize", fFontSize, path);
	bShowSpeakerName = GetInt("Settings", "bShowSpeakerName", bShowSpeakerName ? 1 : 0, path) != 0;
	fOffScreenY = GetFloat("Settings", "fOffScreenY", fOffScreenY, path);
	fFloatingWrapWidth = GetFloat("Settings", "fFloatingWrapWidth", fFloatingWrapWidth, path);
	fCenterWrapWidth = GetFloat("Settings", "fCenterWrapWidth", fCenterWrapWidth, path);
	if (fFloatingWrapWidth < 120.0f) fFloatingWrapWidth = 120.0f;
	if (fFloatingWrapWidth > 1800.0f) fFloatingWrapWidth = 1800.0f;
	if (fCenterWrapWidth < 120.0f) fCenterWrapWidth = 120.0f;
	if (fCenterWrapWidth > 2400.0f) fCenterWrapWidth = 2400.0f;
	bRequireLOS = GetInt("Settings", "bRequireLOS", bRequireLOS ? 1 : 0, path) != 0;
	fSubtitleScale = GetFloat("Settings", "fSubtitleScale", fSubtitleScale, path);
	bSuppressMenuDialogueTail = GetInt("Settings", "bSuppressMenuDialogueTail", bSuppressMenuDialogueTail ? 1 : 0, path) != 0;
	fMenuDialogueTailSeconds = GetFloat("Settings", "fMenuDialogueTailSeconds", fMenuDialogueTailSeconds, path);
	if (fMenuDialogueTailSeconds < 0.0f) fMenuDialogueTailSeconds = 0.0f;
	if (fMenuDialogueTailSeconds > 5.0f) fMenuDialogueTailSeconds = 5.0f;
	bAnimateInterruptReplace = GetInt("Settings", "bAnimateInterruptReplace", bAnimateInterruptReplace ? 1 : 0, path) != 0;
	fInterruptAnimSeconds = GetFloat("Settings", "fInterruptAnimSeconds", fInterruptAnimSeconds, path);
	fInterruptFadeFloor = GetFloat("Settings", "fInterruptFadeFloor", fInterruptFadeFloor, path);
	fInterruptSlideNorm = GetFloat("Settings", "fInterruptSlideNorm", fInterruptSlideNorm, path);
	if (fInterruptAnimSeconds < 0.05f) fInterruptAnimSeconds = 0.05f;
	if (fInterruptAnimSeconds > 1.0f) fInterruptAnimSeconds = 1.0f;
	if (fInterruptFadeFloor < 0.0f) fInterruptFadeFloor = 0.0f;
	if (fInterruptFadeFloor > 0.95f) fInterruptFadeFloor = 0.95f;
	if (fInterruptSlideNorm < 0.0f) fInterruptSlideNorm = 0.0f;
	if (fInterruptSlideNorm > 0.08f) fInterruptSlideNorm = 0.08f;
	bFadeInNewSubtitles = GetInt("Settings", "bFadeInNewSubtitles", bFadeInNewSubtitles ? 1 : 0, path) != 0;
	fNewSubtitleFadeSeconds = GetFloat("Settings", "fNewSubtitleFadeSeconds", fNewSubtitleFadeSeconds, path);
	if (fNewSubtitleFadeSeconds < 0.0f) fNewSubtitleFadeSeconds = 0.0f;
	if (fNewSubtitleFadeSeconds > 1.0f) fNewSubtitleFadeSeconds = 1.0f;
	bResolveSubtitleOverlap = GetInt("Settings", "bResolveSubtitleOverlap", bResolveSubtitleOverlap ? 1 : 0, path) != 0;
	fOverlapBoxWidthNorm = GetFloat("Settings", "fOverlapBoxWidthNorm", fOverlapBoxWidthNorm, path);
	fOverlapBoxHeightNorm = GetFloat("Settings", "fOverlapBoxHeightNorm", fOverlapBoxHeightNorm, path);
	fOverlapPaddingNorm = GetFloat("Settings", "fOverlapPaddingNorm", fOverlapPaddingNorm, path);
	fOverlapMaxShiftNorm = GetFloat("Settings", "fOverlapMaxShiftNorm", fOverlapMaxShiftNorm, path);
	if (fOverlapBoxWidthNorm < 0.05f) fOverlapBoxWidthNorm = 0.05f;
	if (fOverlapBoxWidthNorm > 0.60f) fOverlapBoxWidthNorm = 0.60f;
	if (fOverlapBoxHeightNorm < 0.03f) fOverlapBoxHeightNorm = 0.03f;
	if (fOverlapBoxHeightNorm > 0.30f) fOverlapBoxHeightNorm = 0.30f;
	if (fOverlapPaddingNorm < 0.0f) fOverlapPaddingNorm = 0.0f;
	if (fOverlapPaddingNorm > 0.20f) fOverlapPaddingNorm = 0.20f;
	if (fOverlapMaxShiftNorm < 0.02f) fOverlapMaxShiftNorm = 0.02f;
	if (fOverlapMaxShiftNorm > 0.45f) fOverlapMaxShiftNorm = 0.45f;
}

}}
