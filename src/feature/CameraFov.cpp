#include "feature/CameraFov.hpp"

#include "client/ClientOffsets.hpp"
#include "client/Console.hpp"
#include "hook/Attach.hpp"
#include "hook/Registry.hpp"

#include "game/Camera.hpp"

#include <algorithm>
#include <cstdlib>

namespace awow::feature::cameraFov
{
    namespace off     = awow::client::offsets;
    namespace console = awow::client::console;

    namespace
    {
        constexpr const char* kVariableName = "cameraFov";

        /// The variable is a percentage, not an angle: 100 is the client's own 90-degree view, and
        /// the range is what the client stays stable across rather than what a projection allows.
        constexpr const char* kDefaultValue = "100";
        constexpr int         kMinPercent   = 1;
        constexpr int         kMaxPercent   = 200;

        /// Radians per percentage point, so that 100 maps to a right angle.
        constexpr double kRadiansPerPercent = 3.14159265358979323846 / 200.0;

        console::Variable*    g_variable = nullptr;
        off::CameraInitializeFn g_originalInitialize = nullptr;

        /// @return The angle @p value asks for, clamped to the supported range.
        float AngleFor(const char* value)
        {
            const int percent = std::clamp(value ? std::atoi(value) : 0, kMinPercent, kMaxPercent);
            return float(kRadiansPerPercent * double(percent));
        }

        /// @return The field of view of @p camera, which the SDK reads but has no writer for.
        float* FieldOf(void* camera)
        {
            return reinterpret_cast<float*>(static_cast<uint8_t*>(camera) + off::kCameraFovField);
        }

        /// Applies a new value to the camera that is already up, if there is one.
        int __cdecl OnChanged(console::Variable*, const char*, const char* value, void*)
        {
            if (void* camera = wxl::game::camera::GetActiveCamera())
                *FieldOf(camera) = AngleFor(value);
            return 1;
        }

        /**
         * @brief Substitutes the configured angle whenever a camera is initialised.
         *
         * The change handler alone is not enough: the client builds a fresh camera on every world
         * entry and initialises it with its own default, so without this the setting is lost at the
         * first loading screen.
         */
        void __fastcall InitializeDetour(void* self, void* edx, float a2, float a3, float fov)
        {
            if (g_variable) fov = AngleFor(g_variable->value);
            g_originalInitialize(self, edx, a2, a3, fov);
        }
    }

    bool Initialize()
    {
        hook::registry::AddVariable(&g_variable, kVariableName, nullptr,
                                    console::kFlagUserVariable, kDefaultValue, &OnChanged);

        return hook::Attach("Camera_Initialize", off::kCameraInitialize, &InitializeDetour,
                            &g_originalInitialize);
    }
}
