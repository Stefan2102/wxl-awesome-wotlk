#include "Module.hpp"

#include <cstdarg>
#include <cstdio>

namespace awow
{
    namespace
    {
        const WXL_Api* g_api = nullptr;

        /// Longest line this module emits. Anything past it is truncated rather than allocated for:
        /// a log call must not be able to fail or block on a path a detour runs from.
        constexpr size_t kLineBytes = 512;
    }

    void SetApi(const WXL_Api* api) { g_api = api; }

    const WXL_Api* Api() { return g_api; }

    void Log(int level, const char* format, ...)
    {
        if (!g_api || !format) return;

        char line[kLineBytes];
        va_list args;
        va_start(args, format);
        std::vsnprintf(line, sizeof line, format, args);
        va_end(args);

        g_api->Log(level, kModuleName, "%s", line);
    }
}
