// The cameraFov console variable: how wide a view the camera renders.

#pragma once

namespace awow::feature::cameraFov
{
    /**
     * @brief Declares the cameraFov console variable and keeps it applied.
     * @return true when the camera seam was installed.
     */
    bool Initialize();
}
