// Module identity, and the core service table every part of it reaches the host through.

#pragma once

#include <cstdint>

#include "wxl/PluginApi.h"

namespace awow
{
    /// Name the core logs this module under, and the folder it is deployed into.
    inline constexpr const char* kModuleName = "wxl-awesome-wotlk";

    /// This module's own version, opaque to the core and only ever shown in a log line. Encoded as
    /// major * 10000 + minor * 100 + patch, so releases stay distinguishable and ordered.
    inline constexpr uint32_t kModuleVersion = 10000;

    /**
     * @brief Records the service table for the process lifetime.
     * @param api  The table WXL_Load received.
     *
     * Called once, first thing in WXL_Load. Everything below is undefined before it.
     */
    void SetApi(const WXL_Api* api);

    /// @return The service table, or null before SetApi.
    const WXL_Api* Api();

    /**
     * @brief Writes one line to the core's log under this module's name.
     * @param level  One of the WXL_LOG_* values.
     * @param format printf-style format, formatted here.
     *
     * The formatting is done on this side because the ABI's Log is itself variadic and there is no
     * va_list entry to forward to.
     */
    void Log(int level, const char* format, ...);
}
