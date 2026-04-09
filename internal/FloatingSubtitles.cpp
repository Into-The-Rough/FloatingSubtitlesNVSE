#include "FloatingSubtitles.h"
#include "EngineTypes.h"
#include "Config.h"

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
	if (StringContainsNoCase(name, "bob from accounting")) return true;
	if (StringContainsNoCase(name, "bob  from accounting")) return true;
	if (StringContainsNoCase(name, "bob-from-accounting")) return true;
	return false;
}

typedef TESForm* (__cdecl* LookupByEditorID_t)(const char* editorID);
static LookupByEditorID_t g_lookupByEditorID = (LookupByEditorID_t)0x483A00;

typedef TESForm* (__cdecl* _LookupFormByID)(UInt32 refID);
static _LookupFormByID LookupFormByID = (_LookupFormByID)0x4839C0;
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

static void SafeWriteBuf(UInt32 addr, void* data, UInt32 len) {
	DWORD oldProtect;
	VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy((void*)addr, data, len);
	VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), (void*)addr, len);
}

class TESTopicInfo;
class TESTopic;

namespace FloatingSubtitles {

static NVSEMessagingInterface* g_messagingInterface = nullptr;
static NVSEEventManagerInterface* g_eventInterface = nullptr;
static UInt32 g_pluginHandle = 0;
static PlayerCharacter** g_thePlayer = (PlayerCharacter**)0x11DEA3C;
static bool g_callbackRegistered = false;
static char g_iniPath[MAX_PATH] = {};
static volatile long g_narratorPending = 0;
static UInt32 g_narratorSpeakerRefID = 0;
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

typedef bool (__cdecl* JG_WorldToScreen_t)(NiPoint3* posXYZ, NiPoint3* posOut, int iOffscreenHandling);
static JG_WorldToScreen_t g_JG_WorldToScreen = nullptr;
static bool g_jgInitAttempted = false;

static bool InitWorldToScreen() {
	if (g_JG_WorldToScreen) return true;
	if (g_jgInitAttempted) return false;
	g_jgInitAttempted = true;

	HMODULE jgModule = GetModuleHandleA("johnnyguitar.dll");
	if (!jgModule) {
		return false;
	}

	g_JG_WorldToScreen = (JG_WorldToScreen_t)GetProcAddress(jgModule, "JG_WorldToScreen");
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
	if (childCount > 512) return nullptr;

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

static const char* g_cachedHeadBoneStr = nullptr;

static NiAVObject* GetBlockByName(NiNode* node, const char* name) {
	if (!node || !name || !name[0]) return nullptr;
	const char* namePtr = GetNiFixedString(name);
	if (!namePtr) return nullptr;
	InterlockedDecrement((volatile long*)(namePtr - 8));
	return GetBlockByNameInternal(node, namePtr);
}

struct QueuedLine {
	char text[512];
	float duration;
};

static const int MAX_QUEUED_LINES = 16;

struct ActiveSubtitle {
	Actor* actor;
	UInt32 actorRefID;
	UInt32 topicInfoRefID;
	Tile* tile;
	char text[512];
	DWORD timeAdded;
	DWORD duration;
	DWORD transitionStart;
	DWORD transitionDuration;
	bool transitionActive;
	DWORD fadeInStart;
	DWORD fadeInDuration;
	bool fadeInActive;
	bool valid;
	bool isNarrator;
	QueuedLine queue[MAX_QUEUED_LINES];
	int queueCount;
	int queueHead;
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

struct PendingSubtitle {
	UInt32 speakerRefID;
	UInt32 topicInfoRefID;
	char text[512];
	float duration;
	bool isNarrator;
	volatile long state;
};
static PendingSubtitle g_pending[16] = {};

static void ClearPendingSubtitleQueue() {
	for (int i = 0; i < 16; i++) {
		InterlockedExchange(&g_pending[i].state, 0);
	}
	SetNarratorPendingState(false);
	g_narratorSpeakerRefID = 0;
	g_narratorDuration = 5.0f;
}

static void StripCurlyBraces(char* text) {
	char* src = text;
	char* dst = text;
	while (*src) {
		if (*src == '{') {
			while (*src && *src != '}') src++;
			if (*src == '}') src++;
			while (*src == ' ') src++;
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

static void OnDialogueEvent(TESObjectREFR* thisObj, void* parameters) {
	void** p = (void**)parameters;
	Actor* speaker = (Actor*)p[0];
	TESTopicInfo* topicInfo = (TESTopicInfo*)p[2];
	const char* text = (const char*)p[3];

	DWORD now = GetTickCount();

	if (IsLoadingState()) return;

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

	if (!speaker || !text || !text[0]) return;

	const char* voicePath = (const char*)p[4];
	UInt32 infoRefID = topicInfo ? ((TESForm*)topicInfo)->refID : 0;
	float timePerChar = *(float*)(0x11D2178 + 0x04);
	if (timePerChar <= 0.0f) timePerChar = 0.08f;
	float duration = (float)strlen(text) * timePerChar;
	if (duration < 2.0f) duration = 2.0f;

	bool narrator = IsNarratorActor(speaker);

	for (int i = 0; i < 16; i++) {
		if (InterlockedCompareExchange(&g_pending[i].state, 1, 0) == 0) {
			g_pending[i].speakerRefID = speaker->refID;
			g_pending[i].topicInfoRefID = infoRefID;
			strncpy(g_pending[i].text, text, 511);
			g_pending[i].text[511] = 0;
			g_pending[i].duration = duration;
			g_pending[i].isNarrator = narrator;
			InterlockedExchange(&g_pending[i].state, 2);
			return;
		}
	}
}

static Tile* CreateSubtitleTile();
static void HideAllSubtitles();

static void ProcessPendingSubtitles() {
	DWORD now = GetTickCount();

	if (IsDialogueMenuOpen()) {
		for (int i = 0; i < 16; i++) {
			if (g_pending[i].state == 2) {
				InterlockedExchange(&g_pending[i].state, 0);
			}
		}
		return;
	}

	for (int i = 0; i < 16; i++) {
		if (InterlockedCompareExchange(&g_pending[i].state, 3, 2) != 2) continue;

		UInt32 speakerRefID = g_pending[i].speakerRefID;
		Actor* speaker = (Actor*)LookupFormByID(speakerRefID);
		if (!speaker || !IsFormValid((TESForm*)speaker)) {
			InterlockedExchange(&g_pending[i].state, 0);
			continue;
		}
		if (!g_pending[i].isNarrator && IsPlayerSpeaker(speaker, speakerRefID)) {
			InterlockedExchange(&g_pending[i].state, 0);
			continue;
		}

		float durSec = g_pending[i].duration;
		if (durSec < 0.5f) durSec = 0.5f;
		if (durSec > 30.0f) durSec = 30.0f;
		DWORD durationMs = (DWORD)(durSec * 1000.0f);

		StripCurlyBraces(g_pending[i].text);
		if (IsEmptyOrWhitespace(g_pending[i].text)) {
			InterlockedExchange(&g_pending[i].state, 0);
			continue;
		}

		const char* text = g_pending[i].text;
		bool narrator = g_pending[i].isNarrator;

		int existing = FindSubtitleForSpeaker(speaker);
		if (existing >= 0) {
			ActiveSubtitle& current = g_activeSubtitles[existing];
			UInt32 pendingInfoRefID = g_pending[i].topicInfoRefID;

			if (pendingInfoRefID && current.topicInfoRefID && pendingInfoRefID != current.topicInfoRefID) {
				char formatted[512];
				if (narrator) {
					strncpy(formatted, text, 511);
					formatted[511] = 0;
				} else {
					FormatSubtitleText(formatted, 512, speaker, text);
				}
				strncpy(current.text, formatted, 511);
				current.text[511] = 0;
				current.topicInfoRefID = pendingInfoRefID;
				current.timeAdded = now;
				current.duration = durationMs;
				current.queueCount = 0;
				current.queueHead = 0;
				if (Config::bAnimateInterruptReplace) {
					current.transitionStart = now;
					current.transitionDuration = (DWORD)(Config::fInterruptAnimSeconds * 1000.0f);
					current.transitionActive = current.transitionDuration > 0;
					current.fadeInActive = false;
				} else if (Config::bFadeInNewSubtitles) {
					current.transitionActive = false;
					current.fadeInStart = now;
					current.fadeInDuration = (DWORD)(Config::fNewSubtitleFadeSeconds * 1000.0f);
					current.fadeInActive = current.fadeInDuration > 0;
				} else {
					current.transitionActive = false;
					current.fadeInActive = false;
				}
				InterlockedExchange(&g_pending[i].state, 0);
				continue;
			}

			char formatted[512];
			if (narrator) {
				strncpy(formatted, text, 511);
				formatted[511] = 0;
			} else {
				FormatSubtitleText(formatted, 512, speaker, text);
			}

			bool duplicate = (strcmp(current.text, formatted) == 0);
			if (!duplicate) {
				for (int q = 0; q < current.queueCount && !duplicate; q++) {
					int idx = (current.queueHead + q) % MAX_QUEUED_LINES;
					if (strcmp(current.queue[idx].text, formatted) == 0)
						duplicate = true;
				}
			}
			if (duplicate) {
				InterlockedExchange(&g_pending[i].state, 0);
				continue;
			}

			if (current.queueCount < MAX_QUEUED_LINES) {
				int writeIdx = (current.queueHead + current.queueCount) % MAX_QUEUED_LINES;
				QueuedLine& ql = current.queue[writeIdx];
				strncpy(ql.text, formatted, 511);
				ql.text[511] = 0;
				ql.duration = durSec;
				current.queueCount++;
			}
			InterlockedExchange(&g_pending[i].state, 0);
			continue;
		}

		int slot = -1;
		DWORD oldestTime = now + 1;
		int oldestSlot = 0;
		for (int j = 0; j < MAX_SUBTITLES; j++) {
			if (!g_activeSubtitles[j].valid) { slot = j; break; }
			if (g_activeSubtitles[j].timeAdded < oldestTime) {
				oldestTime = g_activeSubtitles[j].timeAdded;
				oldestSlot = j;
			}
		}
		if (slot < 0) slot = oldestSlot;

		if (!g_activeSubtitles[slot].tile)
			g_activeSubtitles[slot].tile = CreateSubtitleTile();

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
		g_activeSubtitles[slot].topicInfoRefID = g_pending[i].topicInfoRefID;
		g_activeSubtitles[slot].queueCount = 0;
		g_activeSubtitles[slot].queueHead = 0;

		InterlockedExchange(&g_pending[i].state, 0);
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
	g_cachedDLC02NarratorBase = nullptr;
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
		g_activeSubtitles[i].topicInfoRefID = 0;
		g_activeSubtitles[i].queueCount = 0;
		g_activeSubtitles[i].queueHead = 0;
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
		g_disabled = true;
		return;
	}

	InitTraits();

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

	int offScreenIdx = -1;
	float offScreenDist = 999999.0f;
	ScreenPlacement placements[MAX_SUBTITLES];
	int placementCount = 0;

	for (int i = 0; i < MAX_SUBTITLES; i++) {
		ActiveSubtitle& sub = g_activeSubtitles[i];
		if (!sub.tile) continue;

		if (!sub.valid || !sub.actorRefID) {
			sub.tile->SetFloat(g_traitX, -1.0f);
			sub.tile->SetFloat(g_traitY, -1.0f);
			sub.tile->SetFloat(g_traitVisible, 0.0f);
			continue;
		}

		Actor* actor = (Actor*)LookupFormByID(sub.actorRefID);
		if (!actor || !IsFormValid((TESForm*)actor)) {
			sub.valid = false;
			sub.actor = nullptr;
			sub.actorRefID = 0;
			sub.transitionActive = false;
			sub.fadeInActive = false;
			sub.tile->SetFloat(g_traitVisible, 0.0f);
			continue;
		}
		sub.actor = actor;

		if (sub.isNarrator) {
			sub.tile->SetFloat(g_traitX, -10.0f);
			sub.tile->SetFloat(g_traitY, -10.0f);
			sub.tile->SetFloat(g_traitVisible, 0.0f);
			offScreenIdx = i;
			offScreenDist = 0.0f;
			continue;
		}

		NiPoint3 worldPos;
		NiNode* niNode = GetRefNiNode((TESObjectREFR*)sub.actor);
		if (niNode) {
			if (!g_cachedHeadBoneStr) {
				g_cachedHeadBoneStr = GetNiFixedString("Bip01 Head");
			}
			NiAVObject* headBone = g_cachedHeadBoneStr ? GetBlockByNameInternal(niNode, g_cachedHeadBoneStr) : nullptr;
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

		float dx = worldPos.x - player->pos.x;
		float dy = worldPos.y - player->pos.y;
		float dz = worldPos.z - player->pos.z;
		float dist = sqrtf(dx*dx + dy*dy + dz*dz);

		if (dist > Config::fMaxDistance) {
			sub.tile->SetFloat(g_traitVisible, 0.0f);
			continue;
		}

		bool hasLOS = GetLineOfSight((TESObjectREFR*)player, (TESObjectREFR*)sub.actor);
		NiPoint3 screenPos;
		g_JG_WorldToScreen(&worldPos, &screenPos, Config::iOffScreenHandling);

		if (!hasLOS) {
			if (Config::bRequireLOS) {
				sub.tile->SetFloat(g_traitVisible, 0.0f);
			} else {
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
			placement.actorRefID = sub.actorRefID;
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
		g_activeSubtitles[i].queueCount = 0;
		g_activeSubtitles[i].queueHead = 0;
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
			for (int i = 0; i < 16; i++) {
				if (g_pending[i].state == 2) {
					InterlockedExchange(&g_pending[i].state, 0);
				}
			}
		}
	}
	g_prevDialogueMenuOpen = dialogueMenuOpen;

	void* currentCell = GetParentCell((TESObjectREFR*)player);
	if (currentCell != g_lastPlayerCell) {
		if (g_lastPlayerCell) HideAllSubtitles();
		g_lastPlayerCell = currentCell;
	}

	if (dialogueMenuOpen) {
		HideAllSubtitles();
		return;
	}

	ProcessPendingSubtitles();
	UpdateSubtitlePositions();

	DWORD now = GetTickCount();
	for (int i = 0; i < MAX_SUBTITLES; i++) {
		ActiveSubtitle& sub = g_activeSubtitles[i];
		if (!sub.valid) continue;
		if ((now - sub.timeAdded) <= sub.duration) continue;

		if (sub.queueCount > 0) {
			QueuedLine& ql = sub.queue[sub.queueHead];
			strncpy(sub.text, ql.text, 511);
			sub.text[511] = 0;
			float durSec = ql.duration;
			if (durSec < 0.5f) durSec = 0.5f;
			if (durSec > 30.0f) durSec = 30.0f;
			sub.timeAdded = now;
			sub.duration = (DWORD)(durSec * 1000.0f);
			sub.queueHead = (sub.queueHead + 1) % MAX_QUEUED_LINES;
			sub.queueCount--;
			sub.transitionActive = false;
			sub.fadeInActive = false;
			continue;
		}

		sub.valid = false;
		sub.actor = nullptr;
		sub.actorRefID = 0;
		sub.transitionActive = false;
		sub.transitionStart = 0;
		sub.transitionDuration = 0;
		sub.fadeInActive = false;
		sub.fadeInStart = 0;
		sub.fadeInDuration = 0;
		sub.queueCount = 0;
		sub.queueHead = 0;
		if (sub.tile) {
			sub.tile->SetFloat(g_traitVisible, 0.0f);
		}
	}
}

static void __cdecl OnVanillaSubtitle(const char* text, TESObjectREFR* speaker) {
	if (IsLoadingState()) return;
	if (!text || !text[0]) return;
	if (IsEmptyOrWhitespace(text)) return;

	if (IsNarratorPendingState()) {
		for (int i = 0; i < 16; i++) {
			if (InterlockedCompareExchange(&g_pending[i].state, 1, 0) == 0) {
				g_pending[i].speakerRefID = g_narratorSpeakerRefID;
				g_pending[i].topicInfoRefID = 0;
				strncpy(g_pending[i].text, text, 511);
				g_pending[i].text[511] = 0;
				g_pending[i].duration = g_narratorDuration;
				g_pending[i].isNarrator = true;
				InterlockedExchange(&g_pending[i].state, 2);
				SetNarratorPendingState(false);
				return;
			}
		}
		SetNarratorPendingState(false);
		return;
	}

	if (!IsPlayerSpeaker((Actor*)speaker)) return;
	if (!*(UInt8*)0x11DCFA4) return;
	if (IsDialogueMenuOpen()) return;

	float duration = (float)strlen(text) * 0.08f;
	if (duration < 2.0f) duration = 2.0f;

	for (int i = 0; i < 16; i++) {
		if (InterlockedCompareExchange(&g_pending[i].state, 1, 0) == 0) {
			g_pending[i].speakerRefID = speaker->refID;
			g_pending[i].topicInfoRefID = 0;
			strncpy(g_pending[i].text, text, 511);
			g_pending[i].text[511] = 0;
			g_pending[i].duration = duration;
			g_pending[i].isNarrator = true;
			InterlockedExchange(&g_pending[i].state, 2);
			return;
		}
	}
}

static __declspec(naked) void AppendSubtitleHook() {
	__asm {
		mov eax, [esp+0x04]
		mov edx, [esp+0x20]
		push edx
		push eax
		call OnVanillaSubtitle
		add esp, 8
		xor al, al
		ret 0x24
	}
}

static void SuppressVanillaSubtitles() {
	UInt8 expected[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
	if (memcmp((void*)0x774FD0, expected, 5) != 0) {
		return;
	}
	UInt8 jmpPatch[5];
	jmpPatch[0] = 0xE9;
	*(UInt32*)(jmpPatch + 1) = (UInt32)AppendSubtitleHook - (0x774FD0 + 5);
	SafeWriteBuf(0x774FD0, jmpPatch, 5);
}

static bool InitITRCallback() {
	if (g_callbackRegistered) return true;
	if (!g_eventInterface) return false;

	if (g_eventInterface->SetNativeEventHandler("ITR:OnDialogueText", OnDialogueEvent)) {
		g_callbackRegistered = true;
		return true;
	}

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

	OnHUDUpdate();
}

static void MessageHandler(NVSEMessage* msg) {
	switch (msg->type) {
		case kMessage_PostPostLoad:
			SuppressVanillaSubtitles();
			break;
		case kMessage_MainGameLoop:
			OnMainGameLoop();
			break;
		case kMessage_PreLoadGame:
			SetLoadingState(true);
			g_callbackRegistered = false;
			ResetState(true);
			g_disabled = false;
			break;
		case kMessage_ExitToMainMenu:
			SetLoadingState(true);
			g_callbackRegistered = false;
			ResetState(true);
			g_disabled = false;
			break;
		case kMessage_PostLoadGame:
			g_callbackRegistered = false;
			ResetState(true);
			g_disabled = false;
			SetLoadingState(false);
			break;
		case kMessage_NewGame:
			SetLoadingState(false);
			break;
		case kMessage_ReloadConfig:
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
			g_applyVisualSettingsPending = true;
			break;
	}
}

void Init(const NVSEInterface* nvse) {
	sprintf(g_iniPath, "%sData\\config\\FloatingSubtitlesNVSE.ini", nvse->GetRuntimeDirectory());
	Config::Load(g_iniPath);
	g_runtimeFloatingWrapWidth = Config::fFloatingWrapWidth;
	g_runtimeCenterWrapWidth = Config::fCenterWrapWidth;

	g_pluginHandle = nvse->GetPluginHandle();
	g_messagingInterface = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);
	g_eventInterface = (NVSEEventManagerInterface*)nvse->QueryInterface(kInterface_EventManager);

	if (g_messagingInterface) {
		g_messagingInterface->RegisterListener(g_pluginHandle, "NVSE", (void*)MessageHandler);
	}
}

} //namespace FloatingSubtitles
