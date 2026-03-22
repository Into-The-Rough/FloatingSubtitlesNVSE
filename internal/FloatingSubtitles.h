#pragma once

struct NVSEInterface;

extern const char PLUGIN_NAME[];
extern const unsigned int PLUGIN_VERSION;

namespace FloatingSubtitles {
	void Init(const NVSEInterface* nvse);
}
