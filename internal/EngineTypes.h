#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <cstdio>
#include <cmath>

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

struct PlayerCharacter : Actor {};

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
enum { kInterface_Messaging = 2, kInterface_EventManager = 8 };

struct NVSEEventManagerInterface {
	typedef void (*NativeEventHandler)(TESObjectREFR* thisObj, void* parameters);

	enum ParamType : UInt8 {
		eParamType_Float = 0, eParamType_Int, eParamType_String, eParamType_Array,
		eParamType_RefVar, eParamType_AnyForm = eParamType_RefVar,
		eParamType_Reference, eParamType_BaseForm,
		eParamType_Invalid, eParamType_Anything
	};

	enum EventFlags : UInt32 { kFlags_None = 0, kFlag_FlushOnLoad = 1 << 0 };

	bool (*RegisterEvent)(const char* name, UInt8 numParams, ParamType* paramTypes, EventFlags flags);
	bool (*DispatchEvent)(const char* eventName, TESObjectREFR* thisObj, ...);

	enum DispatchReturn : signed char { kRetn_UnknownEvent = -2, kRetn_GenericError = -1, kRetn_Normal = 0, kRetn_EarlyBreak, kRetn_Deferred };
	typedef bool (*DispatchCallback)(void* result, void* anyData);

	DispatchReturn (*DispatchEventAlt)(const char* eventName, DispatchCallback resultCallback, void* anyData, TESObjectREFR* thisObj, ...);
	bool (*SetNativeEventHandler)(const char* eventName, NativeEventHandler func);
	bool (*RemoveNativeEventHandler)(const char* eventName, NativeEventHandler func);
};

#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)

template <typename T_Ret = void, typename ...Args>
__forceinline T_Ret ThisCall(UInt32 addr, void* _this, Args ...args) {
	return ((T_Ret(__thiscall*)(void*, Args...))addr)(_this, args...);
}

inline Tile* Menu::AddTileFromTemplate(Tile* destTile, const char* templateName) {
	return ThisCall<Tile*>(0xA1DDB0, this, destTile, templateName, 0);
}

inline TileValue* Tile::GetValue(UInt32 traitID) {
	return ThisCall<TileValue*>(0xA01000, this, traitID);
}

inline void Tile::SetFloat(UInt32 traitID, float val) {
	TileValue* value = GetValue(traitID);
	if (value) ThisCall<void>(0xA0A270, value, val, true);
}

inline void Tile::SetString(UInt32 traitID, const char* str) {
	TileValue* value = GetValue(traitID);
	if (value) ThisCall<void>(0xA0A300, value, str, true);
}
