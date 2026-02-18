#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <cstdio>
#include <cmath>
#include <string>

typedef unsigned int UInt32;
typedef unsigned short UInt16;
typedef unsigned char UInt8;
typedef int SInt32;
typedef short SInt16;
typedef char SInt8;

struct NiPoint3 {
	float x, y, z;
	NiPoint3() : x(0), y(0), z(0) {}
	NiPoint3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct NiNode;
struct NiAVObject {
	UInt8 pad00[0x8C];
	float worldX;  //0x8C
	float worldY;  //0x90
	float worldZ;  //0x94
};

struct TESForm {
	void* vtable;
	UInt8 typeID;
	UInt8 pad05[3];
	UInt32 flags;
	UInt32 refID;
	UInt8 pad10[8];
};

struct TESObjectREFR : TESForm {
	UInt8 pad18[0x18];
	NiPoint3 pos;         //0x30
	NiPoint3 rot;         //0x3C
	UInt8 pad48[0x64 - 0x48];
	void* renderState;    //0x64
};

struct Actor : TESObjectREFR {};

struct PlayerCharacter : Actor {
	UInt8 padActor[0x64F - sizeof(Actor)];
	bool usingScope;
};

struct Tile;
struct Menu {
	void* vtable;
	Tile* tile;

	Tile* AddTileFromTemplate(Tile* destTile, const char* templateName);
};

struct HUDMainMenu : Menu {
	static HUDMainMenu* Get() { return *(HUDMainMenu**)0x11D96C0; }
};

struct TileValue {
	UInt32 id;
	void* parent;
	float num;
	char* str;
	void* action;
};

struct Tile {
	void* vtable;
	UInt8 pad04[0x14 - 4];
	const char* name;
	Tile* parent;
	void* children;

	TileValue* GetValue(UInt32 traitID);
	void SetFloat(UInt32 traitID, float val);
	void SetString(UInt32 traitID, const char* str);
	Tile* GetChild(const char* childName) {
		return ((Tile*(__thiscall*)(Tile*, const char*))0xA03DA0)(this, childName);
	}
};

struct PluginInfo {
	enum { kInfoVersion = 1 };
	UInt32 infoVersion;
	const char* name;
	UInt32 version;
};

struct NVSEInterface {
	UInt32 nvseVersion;
	UInt32 runtimeVersion;
	UInt32 editorVersion;
	UInt32 isEditor;
	void* RegisterCommand;
	void* SetOpcodeBase;
	void* (*QueryInterface)(UInt32 id);
	UInt32 (*GetPluginHandle)(void);
	void* RegisterTypedCommand;
	const char* (*GetRuntimeDirectory)(void);
	UInt32 isNogore;
};

struct NVSEMessagingInterface {
	UInt32 version;
	void (*RegisterListener)(UInt32 pluginHandle, const char* sender, void* callback);
	void* Dispatch;
};

struct NVSEMessage {
	const char* sender;
	UInt32 type;
	UInt32 dataLen;
	void* data;
};

enum { kMessage_PostLoad = 0, kMessage_ExitToMainMenu = 2, kMessage_PreLoadGame = 6, kMessage_PostLoadGame = 8, kMessage_PostPostLoad = 9, kMessage_NewGame = 14, kMessage_MainGameLoop = 20, kMessage_ReloadConfig = 25 };
enum { kInterface_Messaging = 2 };

#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)

template <typename T_Ret = void, typename ...Args>
__forceinline T_Ret ThisCall(UInt32 addr, void* _this, Args ...args) {
	return ((T_Ret(__thiscall*)(void*, Args...))addr)(_this, std::forward<Args>(args)...);
}

Tile* Menu::AddTileFromTemplate(Tile* destTile, const char* templateName) {
	return ThisCall<Tile*>(0xA1DDB0, this, destTile, templateName, 0);
}

TileValue* Tile::GetValue(UInt32 traitID) {
	return ThisCall<TileValue*>(0xA01000, this, traitID);
}

void Tile::SetFloat(UInt32 traitID, float val) {
	TileValue* value = GetValue(traitID);
	if (value) ThisCall<void>(0xA0A270, value, val, true);
}

void Tile::SetString(UInt32 traitID, const char* str) {
	TileValue* value = GetValue(traitID);
	if (value) ThisCall<void>(0xA0A300, value, str, true);
}

static bool GetLineOfSight(TESObjectREFR* from, TESObjectREFR* to) {
	if (!from || !to) return false;
	return ThisCall<bool>(0x88B880, from, 0, to, 1, 0, 0);
}

static const char* GetActorName(Actor* actor) {
	if (!actor) return nullptr;
	TESForm* baseForm = *(TESForm**)((UInt8*)actor + 0x20);
	if (!baseForm) return nullptr;
	//only NPC_ (42) and Creature (43) have TESFullName at this offset
	if (baseForm->typeID != 42 && baseForm->typeID != 43) return nullptr;
	char** namePtr = (char**)((UInt8*)baseForm + 0xD0 + 4);
	UInt16* lenPtr = (UInt16*)((UInt8*)baseForm + 0xD0 + 8);
	if (*namePtr && *lenPtr > 0) {
		const char* name = *namePtr;
		if (name[0] && strcmp(name, "<no name>") != 0) return name;
	}
	return nullptr;
}

static const char* GetFormEditorID(TESForm* form) {
	if (!form) return nullptr;
	//vtable index 0x4C (offset 0x130) = GetEditorID
	void** vtable = *(void***)form;
	return ((const char*(__thiscall*)(void*))vtable[0x4C])(form);
}

static bool StringContainsNoCase(const char* text, const char* token) {
	if (!text || !token || !token[0]) return false;
	size_t tokenLen = strlen(token);
	for (const char* p = text; *p; p++) {
		if (_strnicmp(p, token, tokenLen) == 0) return true;
	}
	return false;
}

static bool NameMatchesBobFromAccounting(const char* name) {
	if (!name || !name[0]) return false;
	// Be tolerant of small formatting differences (prefix/suffix/spacing).
	if (StringContainsNoCase(name, "bob from accounting")) return true;
	if (StringContainsNoCase(name, "bob  from accounting")) return true;
	if (StringContainsNoCase(name, "bob-from-accounting")) return true;
	return false;
}

typedef TESForm* (__cdecl* LookupByEditorID_t)(const char* editorID);
static LookupByEditorID_t g_lookupByEditorID = (LookupByEditorID_t)0x483A00;
static TESForm* g_cachedDLC02NarratorBase = nullptr;

static bool IsNarratorActor(Actor* actor) {
	if (!actor) return false;
	TESForm* baseForm = *(TESForm**)((UInt8*)actor + 0x20);
	if (!baseForm) return false;
	if (!g_cachedDLC02NarratorBase && g_lookupByEditorID) {
		g_cachedDLC02NarratorBase = g_lookupByEditorID("NVDLC02Narrator");
	}
	if (g_cachedDLC02NarratorBase) {
		if (baseForm == g_cachedDLC02NarratorBase) return true;
		if ((TESForm*)actor == g_cachedDLC02NarratorBase) return true;
		if (baseForm->refID == g_cachedDLC02NarratorBase->refID) return true;
	}
	const char* edid = GetFormEditorID(baseForm);
	if (StringContainsNoCase(edid, "narrator")) return true;
	if (StringContainsNoCase(edid, "bobfromaccounting")) return true;
	if (StringContainsNoCase(edid, "bob_from_accounting")) return true;
	const char* name = GetActorName(actor);
	if (StringContainsNoCase(name, "narrator")) return true;
	if (NameMatchesBobFromAccounting(name)) return true;
	return false;
}

static bool IsEmptyOrWhitespace(const char* str) {
	if (!str) return true;
	while (*str) {
		if (*str != ' ' && *str != '\t' && *str != '\n' && *str != '\r') return false;
		str++;
	}
	return true;
}

static void SafeWrite32(UInt32 addr, UInt32 data) {
	DWORD oldProtect;
	VirtualProtect((void*)addr, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
	*(UInt32*)addr = data;
	VirtualProtect((void*)addr, 4, oldProtect, &oldProtect);
}

static void SafeWriteBuf(UInt32 addr, void* data, UInt32 len) {
	DWORD oldProtect;
	VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy((void*)addr, data, len);
	VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
}

constexpr char PLUGIN_NAME[] = "FloatingSubtitlesNVSE";
constexpr UInt32 PLUGIN_VERSION = 100;

//forward declarations for itr-nvse types
class TESTopicInfo;
class TESTopic;

//callback type from itr-nvse (duration in seconds)
typedef void (*DTF_NativeCallback)(Actor* speaker, const char* text, float duration, TESTopicInfo* topicInfo, TESTopic* topic);
typedef bool (*DTF_RegisterNativeCallback_t)(DTF_NativeCallback callback);

namespace FloatingSubtitles {

static NVSEMessagingInterface* g_messagingInterface = nullptr;
static UInt32 g_pluginHandle = 0;
static PlayerCharacter** g_thePlayer = (PlayerCharacter**)0x11DEA3C;
static FILE* g_logFile = nullptr;
static DTF_RegisterNativeCallback_t g_registerCallback = nullptr;
static bool g_callbackRegistered = false;
static char g_iniPath[MAX_PATH] = {};
static volatile long g_narratorPending = 0;
static Actor* g_narratorSpeaker = nullptr;
static float g_narratorDuration = 5.0f;
static volatile long g_isLoading = 0;
static bool g_applyVisualSettingsPending = false;
static float g_runtimeFloatingWrapWidth = 400.0f;
static float g_runtimeCenterWrapWidth = 700.0f;

static bool IsLoadingState() { return g_isLoading != 0; }

static void SetLoadingState(bool loading) {
	InterlockedExchange(&g_isLoading, loading ? 1 : 0);
}

static bool IsNarratorPendingState() { return g_narratorPending != 0; }

static void SetNarratorPendingState(bool pending) {
	InterlockedExchange(&g_narratorPending, pending ? 1 : 0);
}

static void Log(const char* fmt, ...) {
	if (!g_logFile) return;
	va_list args;
	va_start(args, fmt);
	vfprintf(g_logFile, fmt, args);
	fprintf(g_logFile, "\n");
	fflush(g_logFile);
	va_end(args);
}

typedef bool (__cdecl* JG_WorldToScreen_t)(NiPoint3* posXYZ, NiPoint3* posOut, int iOffscreenHandling);
static JG_WorldToScreen_t g_JG_WorldToScreen = nullptr;
static bool g_jgInitAttempted = false;

static bool InitWorldToScreen() {
	if (g_JG_WorldToScreen) return true;
	if (g_jgInitAttempted) return false;
	g_jgInitAttempted = true;

	HMODULE jgModule = GetModuleHandleA("johnnyguitar.dll");
	if (!jgModule) {
		Log("JohnnyGuitar not found - floating subtitles disabled");
		return false;
	}

	g_JG_WorldToScreen = (JG_WorldToScreen_t)GetProcAddress(jgModule, "JG_WorldToScreen");
	if (g_JG_WorldToScreen) Log("JG_WorldToScreen initialized");
	return g_JG_WorldToScreen != nullptr;
}

static UInt32 GetTraitID(const char* name) {
	return ((UInt32(__cdecl*)(const char*, UInt32))0xA00940)(name, 0xFFFFFFFF);
}

static Tile* GetHUDMainMenuTile() {
	HUDMainMenu* hud = HUDMainMenu::Get();
	if (!hud) return nullptr;
	return hud->tile;
}

static const char* GetTileName(Tile* tile) {
	if (!tile) return nullptr;
	return *(const char**)((char*)tile + 0x20);
}

static Tile* GetTileChild(Tile* tile, const char* name) {
	if (!tile || !name) return nullptr;
	return ThisCall<Tile*>(0xA03DA0, tile, name);
}

static Tile* ReadXML(Tile* parentTile, const char* path) {
	if (!parentTile || !path) return nullptr;
	return ThisCall<Tile*>(0xA01B00, parentTile, path);
}

static bool IsFormValid(TESForm* form) {
	if (!form) return false;
	if (!form->refID) return false;
	if (form->flags & (0x20 | 0x800)) return false;
	return true;
}

static bool IsPlayerSpeaker(Actor* speaker, UInt32 knownSpeakerRefID = 0) {
	if (!speaker) return false;
	UInt32 refID = knownSpeakerRefID ? knownSpeakerRefID : speaker->refID;
	if (refID == 0x14) return true;
	PlayerCharacter* player = *g_thePlayer;
	return player && (Actor*)player == speaker;
}

static NiNode* GetRefNiNode(TESObjectREFR* ref) {
	if (!ref) return nullptr;
	void* renderState = *(void**)((UInt8*)ref + 0x64);
	if (!renderState) return nullptr;
	return *(NiNode**)((UInt8*)renderState + 0x14);
}

static const char* GetNiFixedString(const char* str) {
	return ((const char* (__cdecl*)(const char*))0xA5B690)(str);
}

static NiAVObject* GetBlockByNameInternal(NiNode* node, const char* namePtr) {
	if (!node) return nullptr;
	const char* nodeName = *(const char**)((UInt8*)node + 0x8);
	if (nodeName == namePtr) return (NiAVObject*)node;

	UInt16 childCount = *(UInt16*)((UInt8*)node + 0xA6);
	NiAVObject** children = *(NiAVObject***)((UInt8*)node + 0xA0);
	if (!children || childCount == 0) return nullptr;
	if (childCount > 512) return nullptr; //sanity check

	for (UInt16 i = 0; i < childCount; i++) {
		NiAVObject* child = children[i];
		if (!child) continue;

		const char* childName = *(const char**)((UInt8*)child + 0x8);
		if (childName == namePtr) return child;

		void** vtable = *(void***)child;
		if (!vtable) continue;

		typedef void* (__thiscall *IsNodeFn)(void*);
		if (((IsNodeFn)vtable[0xC / 4])(child)) {
			NiAVObject* result = GetBlockByNameInternal((NiNode*)child, namePtr);
			if (result) return result;
		}
	}
	return nullptr;
}

static NiAVObject* GetBlockByName(NiNode* node, const char* name) {
	if (!node || !name || !name[0]) return nullptr;
	const char* namePtr = GetNiFixedString(name);
	if (!namePtr) return nullptr;
	InterlockedDecrement((volatile long*)(namePtr - 8)); //balance AddString refcount
	return GetBlockByNameInternal(node, namePtr);
}

namespace Config {
	float fHeadOffset = 20.0f;
	float fMaxDistance = 3000.0f;
	float fFadeStartDistance = 2500.0f;
	int iOffScreenHandling = 0;
	int iFont = 3;
	float fFontSize = 100.0f;
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
		WritePrivateProfileStringA(NULL, NULL, NULL, path); //flush INI cache
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
}

//subtitle with actor tracking and its own tile
struct ActiveSubtitle {
	Actor* actor;        //actual speaker ref from itr-nvse callback
	UInt32 actorRefID;   //stable identity across transient actor pointers
	Tile* tile;          //dynamically created tile
	char text[512];
	DWORD timeAdded;     //GetTickCount() when added
	DWORD duration;      //duration in milliseconds
	DWORD transitionStart;
	DWORD transitionDuration;
	bool transitionActive;
	DWORD fadeInStart;
	DWORD fadeInDuration;
	bool fadeInActive;
	bool valid;
	bool isNarrator;
};

static const int MAX_SUBTITLES = 64;
static ActiveSubtitle g_activeSubtitles[MAX_SUBTITLES] = {};
static Tile* g_fsRootTile = nullptr;
static Tile* g_fsOffScreenTile = nullptr;
static Tile* g_fsOffScreenText = nullptr;
static bool g_initialized = false;
static bool g_disabled = false;
static void* g_lastPlayerCell = nullptr;

static void* GetParentCell(TESObjectREFR* ref) {
	if (!ref) return nullptr;
	return *(void**)((UInt8*)ref + 0x40);
}

static UInt32 g_traitX = 0;
static UInt32 g_traitY = 0;
static UInt32 g_traitVisible = 0;
static UInt32 g_traitAlpha = 0;
static UInt32 g_traitText = 0;
static UInt32 g_traitOffVisible = 0;
static UInt32 g_traitOffText = 0;
static UInt32 g_traitOffAlpha = 0;
static UInt32 g_traitStdVisible = 0;
static UInt32 g_traitStdAlpha = 0;
static UInt32 g_traitStdString = 0;
static UInt32 g_traitFont = 0;
static UInt32 g_traitZoom = 0;
static UInt32 g_traitStdY = 0;
static UInt32 g_traitOffY = 0;
static UInt32 g_traitFSWrapWidth = 0;
static UInt32 g_traitFSOffWrapWidth = 0;

static void InitTraits() {
	static bool done = false;
	if (done) return;
	g_traitX = GetTraitID("_X");
	g_traitY = GetTraitID("_Y");
	g_traitVisible = GetTraitID("_FSVisible");
	g_traitAlpha = GetTraitID("_FSAlpha");
	g_traitText = GetTraitID("_FSText");
	g_traitOffVisible = GetTraitID("_FSOffVisible");
	g_traitOffText = GetTraitID("_FSOffText");
	g_traitOffAlpha = GetTraitID("_FSOffAlpha");
	g_traitStdVisible = GetTraitID("visible");
	g_traitStdAlpha = GetTraitID("alpha");
	g_traitStdString = GetTraitID("string");
	g_traitFont = GetTraitID("font");
	g_traitZoom = GetTraitID("zoom");
	g_traitStdY = GetTraitID("y");
	g_traitOffY = GetTraitID("_FSOffY");
	g_traitFSWrapWidth = GetTraitID("_FSWrapWidth");
	g_traitFSOffWrapWidth = GetTraitID("_FSOffWrapWidth");
	done = true;
}

static int FindSubtitleForSpeaker(Actor* speaker) {
	if (!speaker) return -1;
	UInt32 refID = speaker->refID;
	if (!refID) return -1;
	for (int i = 0; i < MAX_SUBTITLES; i++) {
		if (!g_activeSubtitles[i].valid) continue;
		if (g_activeSubtitles[i].actorRefID == refID) return i;
	}
	return -1;
}

//pending subtitle queue (callback may be called from different context)
struct PendingSubtitle {
	Actor* speaker;
	char text[512];
	float duration;
	bool isNarrator;
	volatile long state; //0=free, 1=writing, 2=ready
};
static PendingSubtitle g_pending[16] = {};

static void ClearPendingSubtitleQueue() {
	for (int i = 0; i < 16; i++) {
		InterlockedExchange(&g_pending[i].state, 0);
	}
	SetNarratorPendingState(false);
	g_narratorSpeaker = nullptr;
	g_narratorDuration = 5.0f;
}

//strip {voice directions} from subtitle text
static void StripCurlyBraces(char* text) {
	char* src = text;
	char* dst = text;
	while (*src) {
		if (*src == '{') {
			while (*src && *src != '}') src++;
			if (*src == '}') src++;
			while (*src == ' ') src++; //skip trailing space
		} else {
			*dst++ = *src++;
		}
	}
	*dst = 0;
}

static void FormatSubtitleText(char* dest, int destSize, Actor* speaker, const char* text) {
	if (Config::bShowSpeakerName) {
		const char* name = GetActorName(speaker);
		if (name && name[0]) {
			snprintf(dest, destSize, "%s: %s", name, text);
			return;
		}
	}
	strncpy(dest, text, destSize - 1);
	dest[destSize - 1] = 0;
}

static bool IsTileVisibleNow(Tile* tile) {
	if (!tile) return false;
	TileValue* visible = tile->GetValue(g_traitVisible);
	return visible && visible->num > 0.5f;
}

//menu visibility array - index by menu type ID
static UInt8* g_menuVisibility = (UInt8*)0x11F308F;
static constexpr UInt32 kMenuType_Dialogue = 1009;
static constexpr int MAX_RECENT_DIALOGUE_TOPICS = 16;

struct RecentDialogueTopic {
	TESTopicInfo* topicInfo;
	DWORD seenAt;
};

static RecentDialogueTopic g_recentDialogueTopics[MAX_RECENT_DIALOGUE_TOPICS] = {};
static int g_recentDialogueTopicWrite = 0;
static DWORD g_lastDialogueMenuOpenTick = 0;
static bool g_prevDialogueMenuOpen = false;

static bool IsDialogueMenuOpen() {
	return g_menuVisibility[kMenuType_Dialogue] != 0;
}

static DWORD GetDialogueTailMs() {
	if (Config::fMenuDialogueTailSeconds <= 0.0f) return 0;
	return (DWORD)(Config::fMenuDialogueTailSeconds * 1000.0f);
}

static void ClearRecentDialogueTopics() {
	for (int i = 0; i < MAX_RECENT_DIALOGUE_TOPICS; i++) {
		g_recentDialogueTopics[i].topicInfo = nullptr;
		g_recentDialogueTopics[i].seenAt = 0;
	}
	g_recentDialogueTopicWrite = 0;
}

static void RememberDialogueTopic(TESTopicInfo* topicInfo, DWORD now) {
	if (!topicInfo) return;

	for (int i = 0; i < MAX_RECENT_DIALOGUE_TOPICS; i++) {
		if (g_recentDialogueTopics[i].topicInfo == topicInfo) {
			g_recentDialogueTopics[i].seenAt = now;
			return;
		}
	}

	g_recentDialogueTopics[g_recentDialogueTopicWrite].topicInfo = topicInfo;
	g_recentDialogueTopics[g_recentDialogueTopicWrite].seenAt = now;
	g_recentDialogueTopicWrite++;
	if (g_recentDialogueTopicWrite >= MAX_RECENT_DIALOGUE_TOPICS) g_recentDialogueTopicWrite = 0;
}

static bool IsRecentDialogueTopic(TESTopicInfo* topicInfo, DWORD now) {
	if (!topicInfo) return false;
	DWORD tailMs = GetDialogueTailMs();
	if (!tailMs) return false;

	for (int i = 0; i < MAX_RECENT_DIALOGUE_TOPICS; i++) {
		RecentDialogueTopic& recent = g_recentDialogueTopics[i];
		if (!recent.topicInfo) continue;
		if ((now - recent.seenAt) > tailMs) {
			recent.topicInfo = nullptr;
			continue;
		}
		if (recent.topicInfo == topicInfo) return true;
	}
	return false;
}

//callback from itr-nvse - queue speaker pointer for main thread processing
static void OnDialogueCallback(Actor* speaker, const char* text, float duration, TESTopicInfo* topicInfo, TESTopic* topic) {
	DWORD now = GetTickCount();

	if (IsLoadingState()) {
		return;
	}
	if (Config::bSuppressMenuDialogueTail) {
		bool dialogueMenuOpen = IsDialogueMenuOpen();
		if (dialogueMenuOpen) {
			g_lastDialogueMenuOpenTick = now;
			RememberDialogueTopic(topicInfo, now);
			return;
		}

		DWORD tailMs = GetDialogueTailMs();
		if (tailMs && (now - g_lastDialogueMenuOpenTick) <= tailMs && IsRecentDialogueTopic(topicInfo, now)) {
			return;
		}
	}

	if (!speaker || !text || !text[0]) {
		return;
	}

	for (int i = 0; i < 16; i++) {
		if (InterlockedCompareExchange(&g_pending[i].state, 1, 0) == 0) {
			g_pending[i].speaker = speaker;
			strncpy(g_pending[i].text, text, 511);
			g_pending[i].text[511] = 0;
			g_pending[i].duration = duration;
			g_pending[i].isNarrator = false;
			InterlockedExchange(&g_pending[i].state, 2);
			return;
		}
	}
}

static Tile* CreateSubtitleTile();
static void HideAllSubtitles();

static void ProcessPendingSubtitles() {
	DWORD now = GetTickCount();

	//skip all pending if dialogue menu opened (catches greeting lines)
	if (IsDialogueMenuOpen()) {
		for (int i = 0; i < 16; i++) {
			if (g_pending[i].state == 2) {
				InterlockedExchange(&g_pending[i].state, 0);
			}
		}
		return;
	}

		for (int i = 0; i < 16; i++) {
			if (InterlockedCompareExchange(&g_pending[i].state, 3, 2) == 2) {
				Actor* speaker = g_pending[i].speaker;
				if (!speaker || !IsFormValid((TESForm*)speaker)) {
					InterlockedExchange(&g_pending[i].state, 0);
					continue;
				}
				UInt32 speakerRefID = speaker->refID;
				if (IsPlayerSpeaker(speaker, speakerRefID)) {
					InterlockedExchange(&g_pending[i].state, 0);
					continue;
				}

			float durSec = g_pending[i].duration;
			if (durSec < 0.5f) durSec = 0.5f;
			if (durSec > 30.0f) durSec = 30.0f;
			DWORD durationMs = (DWORD)(durSec * 1000.0f);

			//strip voice directions from raw text before formatting
			StripCurlyBraces(g_pending[i].text);

			//skip if text is empty after stripping (don't show bare "Name:")
			if (IsEmptyOrWhitespace(g_pending[i].text)) {
				InterlockedExchange(&g_pending[i].state, 0);
				continue;
			}

				const char* text = g_pending[i].text;

					//check narrator on main thread
					bool narrator = g_pending[i].isNarrator;
					bool narratorDetected = false;
					if (!narrator) {
						narratorDetected = IsNarratorActor(speaker);
					}
					if (!narrator && narratorDetected) {
						g_narratorSpeaker = speaker;
						g_narratorDuration = durSec;
						SetNarratorPendingState(true);
						InterlockedExchange(&g_pending[i].state, 0);
						continue;
					}

				//check if speaker already has subtitle - replace it
				int existing = FindSubtitleForSpeaker(speaker);
				if (existing >= 0) {
					ActiveSubtitle& current = g_activeSubtitles[existing];
					bool wasVisible = current.valid && IsTileVisibleNow(current.tile) && ((now - current.timeAdded) < current.duration);
					current.actor = speaker;
					current.actorRefID = speakerRefID;
					if (narrator) {
						strncpy(g_activeSubtitles[existing].text, text, 511);
						g_activeSubtitles[existing].text[511] = 0;
					} else {
						FormatSubtitleText(g_activeSubtitles[existing].text, 512, speaker, text);
					}
					g_activeSubtitles[existing].timeAdded = now;
					g_activeSubtitles[existing].duration = durationMs;
				if (Config::bAnimateInterruptReplace && wasVisible) {
					current.transitionStart = now;
					current.transitionDuration = (DWORD)(Config::fInterruptAnimSeconds * 1000.0f);
					if (current.transitionDuration < 30) current.transitionDuration = 30;
					current.transitionActive = true;
					} else {
						current.transitionActive = false;
					}
					current.fadeInActive = false;
					current.fadeInStart = 0;
					current.fadeInDuration = 0;
					InterlockedExchange(&g_pending[i].state, 0);
					continue;
				}

			//find empty slot or oldest
			int slot = -1;
			DWORD oldestTime = now + 1;
			int oldestSlot = 0;

			for (int j = 0; j < MAX_SUBTITLES; j++) {
				if (!g_activeSubtitles[j].valid) {
					slot = j;
					break;
				}
				if (g_activeSubtitles[j].timeAdded < oldestTime) {
					oldestTime = g_activeSubtitles[j].timeAdded;
					oldestSlot = j;
				}
			}

			if (slot < 0) slot = oldestSlot;

			//create tile if needed
				if (!g_activeSubtitles[slot].tile) {
					g_activeSubtitles[slot].tile = CreateSubtitleTile();
				}

				// Reused tiles can still contain the previous speaker text for this frame.
				// Force hidden before assigning the next subtitle to avoid stale flicker.
				if (g_activeSubtitles[slot].tile) {
					g_activeSubtitles[slot].tile->SetFloat(g_traitVisible, 0.0f);
					g_activeSubtitles[slot].tile->SetFloat(g_traitAlpha, 0.0f);
				}

					g_activeSubtitles[slot].actor = speaker;
					g_activeSubtitles[slot].actorRefID = speakerRefID;
					if (narrator) {
						strncpy(g_activeSubtitles[slot].text, text, 511);
						g_activeSubtitles[slot].text[511] = 0;
					} else {
						FormatSubtitleText(g_activeSubtitles[slot].text, 512, speaker, text);
					}
					g_activeSubtitles[slot].timeAdded = now;
				g_activeSubtitles[slot].duration = durationMs;
				g_activeSubtitles[slot].transitionStart = 0;
				g_activeSubtitles[slot].transitionDuration = 0;
				g_activeSubtitles[slot].transitionActive = false;
				if (Config::bFadeInNewSubtitles) {
					g_activeSubtitles[slot].fadeInStart = now;
					g_activeSubtitles[slot].fadeInDuration = (DWORD)(Config::fNewSubtitleFadeSeconds * 1000.0f);
					g_activeSubtitles[slot].fadeInActive = g_activeSubtitles[slot].fadeInDuration > 0;
				} else {
					g_activeSubtitles[slot].fadeInStart = 0;
					g_activeSubtitles[slot].fadeInDuration = 0;
					g_activeSubtitles[slot].fadeInActive = false;
				}
				g_activeSubtitles[slot].valid = true;
				g_activeSubtitles[slot].isNarrator = narrator;

				InterlockedExchange(&g_pending[i].state, 0);
			}
		}
}

static void ResetState(bool hideTiles = true) {
	if (hideTiles) {
		HideAllSubtitles();
	}

	g_initialized = false;
	g_fsRootTile = nullptr;
	g_fsOffScreenTile = nullptr;
	g_fsOffScreenText = nullptr;
	g_lastPlayerCell = nullptr;
	g_lastDialogueMenuOpenTick = 0;
	g_prevDialogueMenuOpen = false;
	ClearRecentDialogueTopics();
	ClearPendingSubtitleQueue();
	for (int i = 0; i < MAX_SUBTITLES; i++) {
		g_activeSubtitles[i].tile = nullptr;
		g_activeSubtitles[i].valid = false;
		g_activeSubtitles[i].actor = nullptr;
		g_activeSubtitles[i].actorRefID = 0;
		g_activeSubtitles[i].transitionStart = 0;
		g_activeSubtitles[i].transitionDuration = 0;
		g_activeSubtitles[i].transitionActive = false;
		g_activeSubtitles[i].fadeInStart = 0;
		g_activeSubtitles[i].fadeInDuration = 0;
		g_activeSubtitles[i].fadeInActive = false;
		g_activeSubtitles[i].isNarrator = false;
	}
}

static Tile* CreateSubtitleTile() {
	HUDMainMenu* hud = HUDMainMenu::Get();
	if (!hud || !g_fsRootTile) return nullptr;
	Tile* tile = hud->AddTileFromTemplate(g_fsRootTile, "FSSubtitle");
	if (tile) {
		if (g_traitFSWrapWidth) {
			tile->SetFloat(g_traitFSWrapWidth, g_runtimeFloatingWrapWidth);
		}
		Tile* textTile = GetTileChild(tile, "FSText");
		if (textTile) {
			textTile->SetFloat(g_traitFont, (float)Config::iFont);
			textTile->SetFloat(g_traitZoom, Config::fFontSize * Config::fSubtitleScale);
		}
	}
	return tile;
}

static void InitFloatingSubtitles() {
	if (g_initialized || g_disabled) return;

	Tile* hudTile = GetHUDMainMenuTile();
	if (!hudTile) return;

	g_fsRootTile = GetTileChild(hudTile, "FloatingSubtitles");
	if (!g_fsRootTile) {
		g_fsRootTile = ReadXML(hudTile, "menus\\prefabs\\floatingsubtitles\\floatingsubtitles.xml");
	}

	if (!g_fsRootTile) {
		Log("Failed to inject FloatingSubtitles tile");
		g_disabled = true;
		return;
	}

	InitTraits();

	// Root can survive save/load transitions; ensure it is re-enabled on re-init.
	if (g_traitStdVisible) {
		g_fsRootTile->SetFloat(g_traitStdVisible, 1.0f);
	}

	g_fsOffScreenTile = GetTileChild(g_fsRootTile, "FSOffScreen");
	if (g_fsOffScreenTile) {
		g_fsOffScreenTile->SetFloat(g_traitOffY, Config::fOffScreenY);
		if (g_traitFSOffWrapWidth) {
			g_fsOffScreenTile->SetFloat(g_traitFSOffWrapWidth, g_runtimeCenterWrapWidth);
		}
		g_fsOffScreenText = GetTileChild(g_fsOffScreenTile, "FSOffText");
		if (g_fsOffScreenText) {
			g_fsOffScreenText->SetFloat(g_traitFont, (float)Config::iFont);
			g_fsOffScreenText->SetFloat(g_traitZoom, Config::fFontSize * Config::fSubtitleScale);
		}
	}

	g_initialized = true;
	Log("Floating subtitles initialized");
}


static float CalcFadeAlpha(float dist) {
	if (dist <= Config::fFadeStartDistance) return 255.0f;
	float fadeRange = Config::fMaxDistance - Config::fFadeStartDistance;
	if (fadeRange <= 0.0f) return 255.0f;
	float fadePct = (dist - Config::fFadeStartDistance) / fadeRange;
	if (fadePct > 1.0f) fadePct = 1.0f;
	return 255.0f * (1.0f - fadePct);
}

struct ScreenPlacement {
	int slot;
	float baseX;
	float x;
	float y;
	float alpha;
	UInt32 actorRefID;
};

static float ClampFloat(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static bool BoxesOverlap(const ScreenPlacement& a, const ScreenPlacement& b, float halfW, float halfH) {
	return fabsf(a.x - b.x) < (halfW * 2.0f) && fabsf(a.y - b.y) < (halfH * 2.0f);
}

static void ResolveSubtitleOverlaps(ScreenPlacement* placements, int count) {
	if (!Config::bResolveSubtitleOverlap || !placements || count < 2) return;

	const float halfW = Config::fOverlapBoxWidthNorm * 0.5f;
	const float halfH = Config::fOverlapBoxHeightNorm * 0.5f;
	const float desiredCenterDelta = Config::fOverlapBoxWidthNorm + Config::fOverlapPaddingNorm;
	const float maxShift = Config::fOverlapMaxShiftNorm;
	if (desiredCenterDelta <= 0.0f || halfW <= 0.0f || halfH <= 0.0f) return;

	for (int pass = 0; pass < 4; pass++) {
		bool any = false;

		for (int i = 0; i < count; i++) {
			for (int j = i + 1; j < count; j++) {
				if (!BoxesOverlap(placements[i], placements[j], halfW, halfH)) continue;
				any = true;

				// Stable side assignment to avoid jitter when actors are in-line.
				bool iLeft = true;
				float baseDX = placements[i].baseX - placements[j].baseX;
				if (fabsf(baseDX) > 0.003f) {
					iLeft = baseDX < 0.0f;
				} else {
					iLeft = placements[i].actorRefID <= placements[j].actorRefID;
				}

				int leftIdx = iLeft ? i : j;
				int rightIdx = iLeft ? j : i;
				ScreenPlacement& left = placements[leftIdx];
				ScreenPlacement& right = placements[rightIdx];

				float center = (left.x + right.x) * 0.5f;
				float leftTarget = center - (desiredCenterDelta * 0.5f);
				float rightTarget = center + (desiredCenterDelta * 0.5f);

				float leftMin = left.baseX - maxShift;
				float leftMax = left.baseX + maxShift;
				float rightMin = right.baseX - maxShift;
				float rightMax = right.baseX + maxShift;

				left.x = ClampFloat(leftTarget, leftMin, leftMax);
				right.x = ClampFloat(rightTarget, rightMin, rightMax);

				left.x = ClampFloat(left.x, halfW, 1.0f - halfW);
				right.x = ClampFloat(right.x, halfW, 1.0f - halfW);
			}
		}

		if (!any) break;
	}
}

static void UpdateSubtitlePositions() {
	if (!g_initialized || !g_fsRootTile || g_disabled) return;
	if (!g_JG_WorldToScreen && !InitWorldToScreen()) return;

	PlayerCharacter* player = *g_thePlayer;
	if (!player) return;
	DWORD now = GetTickCount();

	//track closest off-screen speaker
	int offScreenIdx = -1;
	float offScreenDist = 999999.0f;
	ScreenPlacement placements[MAX_SUBTITLES];
	int placementCount = 0;

	for (int i = 0; i < MAX_SUBTITLES; i++) {
		ActiveSubtitle& sub = g_activeSubtitles[i];
		if (!sub.tile) continue;

			if (!sub.valid || !sub.actor) {
				sub.tile->SetFloat(g_traitX, -1.0f);
				sub.tile->SetFloat(g_traitY, -1.0f);
				sub.tile->SetFloat(g_traitVisible, 0.0f);
				continue;
			}

			//check actor still valid
			if (!IsFormValid((TESForm*)sub.actor)) {
				sub.valid = false;
				sub.actor = nullptr;
				sub.actorRefID = 0;
				sub.transitionActive = false;
				sub.fadeInActive = false;
				sub.tile->SetFloat(g_traitVisible, 0.0f);
				continue;
			}
			if (sub.actorRefID && sub.actor->refID != sub.actorRefID) {
				sub.valid = false;
				sub.actor = nullptr;
				sub.actorRefID = 0;
				sub.transitionActive = false;
				sub.fadeInActive = false;
				sub.tile->SetFloat(g_traitVisible, 0.0f);
				continue;
			}

		//narrators always go to off-screen display, skip distance/LOS
		if (sub.isNarrator) {
			sub.tile->SetFloat(g_traitX, -10.0f);
			sub.tile->SetFloat(g_traitY, -10.0f);
			sub.tile->SetFloat(g_traitVisible, 0.0f);
			offScreenIdx = i;
			offScreenDist = 0.0f;
			continue;
		}

		//get actor head position - track in real-time
		NiPoint3 worldPos;
		NiNode* niNode = GetRefNiNode((TESObjectREFR*)sub.actor);
		if (niNode) {
			NiAVObject* headBone = GetBlockByName(niNode, "Bip01 Head");
			if (headBone) {
				worldPos.x = headBone->worldX;
				worldPos.y = headBone->worldY;
				worldPos.z = headBone->worldZ + Config::fHeadOffset;
			} else {
				worldPos.x = sub.actor->pos.x;
				worldPos.y = sub.actor->pos.y;
				worldPos.z = sub.actor->pos.z + 100.0f + Config::fHeadOffset;
			}
		} else {
			worldPos.x = sub.actor->pos.x;
			worldPos.y = sub.actor->pos.y;
			worldPos.z = sub.actor->pos.z + 100.0f + Config::fHeadOffset;
		}

		//distance check
		float dx = worldPos.x - player->pos.x;
		float dy = worldPos.y - player->pos.y;
		float dz = worldPos.z - player->pos.z;
		float dist = sqrtf(dx*dx + dy*dy + dz*dz);

		if (dist > Config::fMaxDistance) {
			sub.tile->SetFloat(g_traitVisible, 0.0f);
			continue;
		}

		//check if player has line of sight to actor
		bool hasLOS = GetLineOfSight((TESObjectREFR*)player, (TESObjectREFR*)sub.actor);

		//project to screen with configured off-screen handling
		NiPoint3 screenPos;
		g_JG_WorldToScreen(&worldPos, &screenPos, Config::iOffScreenHandling);

		if (!hasLOS) {
			if (Config::bRequireLOS) {
				//LOS required - hide subtitle entirely when no LOS
				sub.tile->SetFloat(g_traitVisible, 0.0f);
			} else {
				//no LOS - show in off-screen tile instead
				sub.tile->SetFloat(g_traitX, -10.0f);
				sub.tile->SetFloat(g_traitY, -10.0f);
				sub.tile->SetFloat(g_traitVisible, 0.0f);
				if (dist < offScreenDist) {
					offScreenDist = dist;
					offScreenIdx = i;
				}
			}
			continue;
		}

		float alpha = CalcFadeAlpha(dist);
			if (sub.transitionActive) {
				DWORD elapsed = now - sub.transitionStart;
				if (elapsed >= sub.transitionDuration || sub.transitionDuration == 0) {
					sub.transitionActive = false;
				} else {
					float t = (float)elapsed / (float)sub.transitionDuration;
					alpha *= Config::fInterruptFadeFloor + (1.0f - Config::fInterruptFadeFloor) * t;
					screenPos.y -= Config::fInterruptSlideNorm * (1.0f - t);
				}
			}
			if (sub.fadeInActive) {
				DWORD elapsed = now - sub.fadeInStart;
				if (elapsed >= sub.fadeInDuration || sub.fadeInDuration == 0) {
					sub.fadeInActive = false;
				} else {
					float t = (float)elapsed / (float)sub.fadeInDuration;
					alpha *= t;
				}
			}
			if (placementCount < MAX_SUBTITLES) {
				ScreenPlacement& placement = placements[placementCount++];
			placement.slot = i;
			placement.baseX = screenPos.x;
			placement.x = screenPos.x;
			placement.y = screenPos.y;
			placement.alpha = alpha;
			placement.actorRefID = sub.actor ? sub.actor->refID : 0;
		}
	}

	ResolveSubtitleOverlaps(placements, placementCount);

	for (int i = 0; i < placementCount; i++) {
		const ScreenPlacement& placement = placements[i];
		ActiveSubtitle& sub = g_activeSubtitles[placement.slot];
		if (!sub.tile) continue;

		sub.tile->SetString(g_traitText, sub.text);
		sub.tile->SetFloat(g_traitX, placement.x);
		sub.tile->SetFloat(g_traitY, placement.y);
		sub.tile->SetFloat(g_traitAlpha, placement.alpha);
		sub.tile->SetFloat(g_traitVisible, 1.0f);
	}

	//show closest off-screen speaker in bottom tile
	if (offScreenIdx >= 0 && g_fsOffScreenTile && g_fsOffScreenText) {
		ActiveSubtitle& offSub = g_activeSubtitles[offScreenIdx];
		float alpha = CalcFadeAlpha(offScreenDist);
		g_fsOffScreenTile->SetFloat(g_traitStdVisible, 1.0f);
		g_fsOffScreenTile->SetFloat(g_traitStdAlpha, alpha);
		g_fsOffScreenText->SetString(g_traitStdString, offSub.text);
		g_fsOffScreenText->SetFloat(g_traitStdAlpha, alpha);
	} else if (g_fsOffScreenTile) {
		g_fsOffScreenTile->SetFloat(g_traitStdVisible, 0.0f);
	}
}

static void HideAllSubtitles() {
	for (int i = 0; i < MAX_SUBTITLES; i++) {
		g_activeSubtitles[i].valid = false;
		g_activeSubtitles[i].actor = nullptr;
		g_activeSubtitles[i].actorRefID = 0;
		g_activeSubtitles[i].transitionActive = false;
		g_activeSubtitles[i].transitionStart = 0;
		g_activeSubtitles[i].transitionDuration = 0;
		g_activeSubtitles[i].fadeInActive = false;
		g_activeSubtitles[i].fadeInStart = 0;
		g_activeSubtitles[i].fadeInDuration = 0;
		if (g_activeSubtitles[i].tile) {
			g_activeSubtitles[i].tile->SetFloat(g_traitVisible, 0.0f);
		}
	}
	if (g_fsOffScreenTile) {
		g_fsOffScreenTile->SetFloat(g_traitStdVisible, 0.0f);
	}
}

static void ApplyFontSettings() {
	float effectiveZoom = Config::fFontSize * Config::fSubtitleScale;
	for (int i = 0; i < MAX_SUBTITLES; i++) {
		if (!g_activeSubtitles[i].tile) continue;
		if (g_traitFSWrapWidth)
			g_activeSubtitles[i].tile->SetFloat(g_traitFSWrapWidth, g_runtimeFloatingWrapWidth);
		Tile* textTile = GetTileChild(g_activeSubtitles[i].tile, "FSText");
		if (textTile) {
			textTile->SetFloat(g_traitFont, (float)Config::iFont);
			textTile->SetFloat(g_traitZoom, effectiveZoom);
		}
	}
	if (g_fsOffScreenTile) {
		g_fsOffScreenTile->SetFloat(g_traitOffY, Config::fOffScreenY);
		if (g_traitFSOffWrapWidth)
			g_fsOffScreenTile->SetFloat(g_traitFSOffWrapWidth, g_runtimeCenterWrapWidth);
	}
	if (g_fsOffScreenText) {
		g_fsOffScreenText->SetFloat(g_traitFont, (float)Config::iFont);
		g_fsOffScreenText->SetFloat(g_traitZoom, effectiveZoom);
	}
}

static void OnHUDUpdate() {
	if (g_disabled || !g_initialized) return;

	Tile* hudTile = GetHUDMainMenuTile();
	if (!hudTile || !g_fsRootTile) {
		ResetState(false);
		return;
	}

	PlayerCharacter* player = *g_thePlayer;
	if (!player || !GetRefNiNode((TESObjectREFR*)player)) {
		HideAllSubtitles();
		ClearPendingSubtitleQueue();
		g_lastPlayerCell = nullptr;
		return;
	}

	if (g_applyVisualSettingsPending) {
		ApplyFontSettings();
		g_applyVisualSettingsPending = false;
	}

	bool dialogueMenuOpen = IsDialogueMenuOpen();
	if (Config::bSuppressMenuDialogueTail) {
		DWORD now = GetTickCount();
		if (dialogueMenuOpen) {
			g_lastDialogueMenuOpenTick = now;
		} else if (g_prevDialogueMenuOpen) {
			//drop leftovers queued right as dialogue menu closes
			for (int i = 0; i < 16; i++) {
				if (g_pending[i].state == 2) {
					InterlockedExchange(&g_pending[i].state, 0);
				}
			}
		}
	}
	g_prevDialogueMenuOpen = dialogueMenuOpen;

	//clear subtitles on cell change
	void* currentCell = GetParentCell((TESObjectREFR*)player);
	if (currentCell != g_lastPlayerCell) {
		if (g_lastPlayerCell) HideAllSubtitles();
		g_lastPlayerCell = currentCell;
	}

	//hide floating subtitles during dialogue menu
	if (dialogueMenuOpen) {
		HideAllSubtitles();
		return;
	}

	ProcessPendingSubtitles();
	UpdateSubtitlePositions();

	//expire subtitles based on their duration
	DWORD now = GetTickCount();
		for (int i = 0; i < MAX_SUBTITLES; i++) {
			if (g_activeSubtitles[i].valid && (now - g_activeSubtitles[i].timeAdded) > g_activeSubtitles[i].duration) {
				g_activeSubtitles[i].valid = false;
				g_activeSubtitles[i].actor = nullptr;
				g_activeSubtitles[i].actorRefID = 0;
				g_activeSubtitles[i].transitionActive = false;
				g_activeSubtitles[i].transitionStart = 0;
				g_activeSubtitles[i].transitionDuration = 0;
				g_activeSubtitles[i].fadeInActive = false;
				g_activeSubtitles[i].fadeInStart = 0;
				g_activeSubtitles[i].fadeInDuration = 0;
				if (g_activeSubtitles[i].tile) {
					g_activeSubtitles[i].tile->SetFloat(g_traitVisible, 0.0f);
				}
		}
	}
}

typedef void (__thiscall* HUDUpdate_t)(void* hud);
static HUDUpdate_t g_originalHUDUpdate = nullptr;

static void __fastcall HUDUpdateHook(void* hud, void* edx) {
	if (g_originalHUDUpdate) g_originalHUDUpdate(hud);
	OnHUDUpdate();
}

static void InstallHUDHook() {
	UInt32 vtblAddr = 0x1072DF4 + 11 * 4;
	g_originalHUDUpdate = *(HUDUpdate_t*)vtblAddr;
	SafeWrite32(vtblAddr, (UInt32)HUDUpdateHook);
	Log("HUD update hook installed");
}

//intercept AppendSubtitleData: capture narrator text for off-screen display, suppress everything else
//0x774FD0 __thiscall(text, BSSoundHandle[12], NiPoint3, speaker, instant) ret 0x24
static void __cdecl OnVanillaSubtitle(const char* text, TESObjectREFR* speaker) {
	if (IsLoadingState()) return;
	if (!text || !text[0]) return;
	if (!IsNarratorPendingState()) {
		return;
	}
	if (IsEmptyOrWhitespace(text)) return;

	for (int i = 0; i < 16; i++) {
		if (InterlockedCompareExchange(&g_pending[i].state, 1, 0) == 0) {
			g_pending[i].speaker = g_narratorSpeaker;
			strncpy(g_pending[i].text, text, 511);
				g_pending[i].text[511] = 0;
				g_pending[i].duration = g_narratorDuration;
				g_pending[i].isNarrator = true;
				InterlockedExchange(&g_pending[i].state, 2);
				SetNarratorPendingState(false);
				return;
			}
		}
}

static __declspec(naked) void AppendSubtitleHook() {
	__asm {
		mov eax, [esp+0x04]  //apText
		mov edx, [esp+0x20]  //apSpeaker
		push edx
		push eax
		call OnVanillaSubtitle
		add esp, 8
		xor al, al
		ret 0x24
	}
}

static void SuppressVanillaSubtitles() {
	UInt8 jmpPatch[5];
	jmpPatch[0] = 0xE9;
	*(UInt32*)(jmpPatch + 1) = (UInt32)AppendSubtitleHook - (0x774FD0 + 5);
	SafeWriteBuf(0x774FD0, jmpPatch, 5);
	Log("Vanilla subtitle hook installed");
}

static bool InitITRCallback() {
	if (g_callbackRegistered) return true;

	HMODULE itrModule = GetModuleHandleA("itr-nvse.dll");
	if (!itrModule) {
		Log("itr-nvse.dll not found - floating subtitles disabled");
		return false;
	}

	g_registerCallback = (DTF_RegisterNativeCallback_t)GetProcAddress(itrModule, "DTF_RegisterNativeCallback");
	if (!g_registerCallback) {
		Log("DTF_RegisterNativeCallback not found in itr-nvse.dll");
		return false;
	}

	if (g_registerCallback(OnDialogueCallback)) {
		g_callbackRegistered = true;
		Log("Registered dialogue callback with itr-nvse");
		return true;
	}

	Log("Failed to register dialogue callback");
	return false;
}

static void OnMainGameLoop() {
	if (g_disabled) return;

	if (!g_callbackRegistered) {
		InitITRCallback();
	}

	if (!g_initialized) {
		Tile* hudTile = GetHUDMainMenuTile();
		if (!hudTile) return;

		PlayerCharacter* player = *g_thePlayer;
		if (!player || !GetRefNiNode((TESObjectREFR*)player)) return;

		InitFloatingSubtitles();
	}
}

static void MessageHandler(NVSEMessage* msg) {
	switch (msg->type) {
		case kMessage_PostPostLoad:
			InstallHUDHook();
			SuppressVanillaSubtitles();
			break;
		case kMessage_MainGameLoop:
			OnMainGameLoop();
			break;
		case kMessage_PreLoadGame:
			SetLoadingState(true);
			g_callbackRegistered = false;
			g_registerCallback = nullptr;
			ResetState(true);
			g_disabled = false;
			break;
		case kMessage_ExitToMainMenu:
			SetLoadingState(true);
			g_callbackRegistered = false;
			g_registerCallback = nullptr;
			ResetState(true);
			g_disabled = false;
			break;
		case kMessage_PostLoadGame:
			g_callbackRegistered = false;
			g_registerCallback = nullptr;
			ResetState(true);
			g_disabled = false;
			SetLoadingState(false);
			break;
		case kMessage_NewGame:
			SetLoadingState(false);
			break;
			case kMessage_ReloadConfig:
				// ReloadPluginConfig dispatches with target plugin name in msg->data.
				// Ignore reload requests for other plugins to avoid unnecessary churn.
			if (msg->data && msg->dataLen > 0) {
				const char* pluginName = (const char*)msg->data;
				if (_stricmp(pluginName, PLUGIN_NAME) != 0) {
					break;
				}
			}
				Config::Load(g_iniPath);
				g_runtimeFloatingWrapWidth = Config::fFloatingWrapWidth;
				g_runtimeCenterWrapWidth = Config::fCenterWrapWidth;
				if (!Config::bSuppressMenuDialogueTail) {
					g_lastDialogueMenuOpenTick = 0;
					g_prevDialogueMenuOpen = false;
					ClearRecentDialogueTopics();
				}
				// MCM can fire reload during menu teardown; defer UI writes to HUD update.
				g_applyVisualSettingsPending = true;
				break;
		}
	}

void Init(const NVSEInterface* nvse) {
	char logPath[MAX_PATH];
	sprintf(logPath, "%sFloatingSubtitlesNVSE.log", nvse->GetRuntimeDirectory());
	g_logFile = fopen(logPath, "w");
	Log("FloatingSubtitlesNVSE v%d initializing", PLUGIN_VERSION);

	sprintf(g_iniPath, "%sData\\config\\FloatingSubtitlesNVSE.ini", nvse->GetRuntimeDirectory());
	Config::Load(g_iniPath);
	g_runtimeFloatingWrapWidth = Config::fFloatingWrapWidth;
	g_runtimeCenterWrapWidth = Config::fCenterWrapWidth;

	g_pluginHandle = nvse->GetPluginHandle();
	g_messagingInterface = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);

	if (g_messagingInterface) {
		g_messagingInterface->RegisterListener(g_pluginHandle, "NVSE", (void*)MessageHandler);
	}
}

} //namespace FloatingSubtitles

constexpr UInt32 RUNTIME_VERSION_1_4_0_525 = 0x040020D0;

EXTERN_DLL_EXPORT bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = PLUGIN_NAME;
	info->version = PLUGIN_VERSION;

	if (nvse->isEditor) return false;

	if (nvse->runtimeVersion != RUNTIME_VERSION_1_4_0_525) {
		MessageBoxA(NULL, "FloatingSubtitlesNVSE requires Fallout New Vegas v1.4.0.525 (Steam). Other versions are not supported.", "Version Mismatch", MB_OK | MB_ICONWARNING);
		return false;
	}

	return true;
}

EXTERN_DLL_EXPORT bool NVSEPlugin_Load(const NVSEInterface* nvse) {
	FloatingSubtitles::Init(nvse);
	return true;
}

BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {
	return TRUE;
}
