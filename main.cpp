#include "EngineTypes.h"
#include "FloatingSubtitles.h"

const char PLUGIN_NAME[] = "FloatingSubtitlesNVSE";
const UInt32 PLUGIN_VERSION = 100;
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
