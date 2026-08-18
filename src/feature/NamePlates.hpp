// Nameplates: the retail C_NamePlate API, its events, its unit tokens, and depth ordering.

#pragma once

namespace awow::feature::namePlates
{
    /**
     * @brief Declares everything the nameplate feature adds, and patches the engine's level pass.
     *
     * Adds the C_NamePlate table, the three NAME_PLATE_* events, the nameplate<N> unit token, the
     * nameplateDistance console variable, and a per-frame scan that keeps them all in step.
     * @return true when every seam it needs was installed.
     */
    bool Initialize();
}
