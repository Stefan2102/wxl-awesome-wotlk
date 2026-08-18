// The two entry points the core resolves out of this DLL, and the order the module comes up in.

#include "wxl/PluginApi.h"

#include "Module.hpp"
#include "feature/CameraFov.hpp"
#include "feature/ClipboardFix.hpp"
#include "feature/Inventory.hpp"
#include "feature/NamePlates.hpp"
#include "feature/UnitApi.hpp"
#include "feature/Window.hpp"
#include "hook/Registry.hpp"
#include "hook/ScriptGate.hpp"

// WXL_EXTENSION, which is what makes PluginApi.h declare WXL_Query and WXL_Load as exports, comes
// from the build rather than from here: an extension that forgets it still compiles and fails only
// at load, so it belongs somewhere the whole target inherits it.

namespace
{
    const WXL_PluginInfo g_info = {
        sizeof(WXL_PluginInfo),
        WXL_API_VERSION,
        awow::kModuleName,
        awow::kModuleVersion,
        WXL_CLIENT_BUILD,
    };
}

const WXL_PluginInfo* __cdecl WXL_Query(void)
{
    return &g_info;
}

int __cdecl WXL_Load(const WXL_Api* api)
{
    if (!api || api->apiVersion != WXL_API_VERSION) return 0;
    awow::SetApi(api);

    // The callback gate first: every script function below is refused at its first call without it,
    // and it is the one piece whose absence makes the rest pointless rather than merely reduced.
    bool complete = awow::hook::scriptgate::Initialize();

    // Then the features, which only declare what they want. Nothing here touches the client, so the
    // order is the order awesome_wotlk brought the same features up in and carries no other meaning.
    complete &= awow::feature::clipboardFix::Initialize();
    awow::feature::inventory::Initialize();
    complete &= awow::feature::namePlates::Initialize();
    complete &= awow::feature::cameraFov::Initialize();
    awow::feature::window::Initialize();
    awow::feature::unitApi::Initialize();

    // Last, the seams those declarations need. Doing it here rather than per feature is what keeps
    // one detour per client function no matter how many features asked for a piece of it.
    complete &= awow::hook::registry::Initialize();

    if (complete)
        awow::Log(WXL_LOG_INFO, "loaded");
    else
        awow::Log(WXL_LOG_WARN, "loaded with errors; the features named above are inactive");

    // Loaded either way: a seam that failed disables the feature that wanted it and nothing else,
    // and the errors have already been logged individually.
    return 1;
}
