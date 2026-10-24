#include "wtpch.h"
#include "RTSModule.h"

#include "RTSCameraSystem.h"
#include "RTSPlayerSystem.h"
#include "RTSUnitSystem.h"
#include "RTSAnimationSystem.h"
#include "RTSProductionSystem.h"
#include "RTSAISystem.h"

#include "Wheatear/Scene/SceneSystemRegistry.h"

namespace Wheatear {

    void RegisterRTSModule()
    {
        // SceneSystemRegistry dedupes by name, but this keeps repeated calls
        // cheap when the sandbox reloads the vertical slice.
        static bool registered = false;
        if (registered)
            return;
        registered = true;

        // Order matters: camera first, then player input/selection, then unit
        // simulation. RenderSystem is appended by Scene and always runs last.
        SceneSystemRegistry::RegisterRuntimeSystem(
            "RTSCamera",
            []() -> Scope<ISystem> { return CreateScope<RTSCameraSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "RTSPlayer",
            []() -> Scope<ISystem> { return CreateScope<RTSPlayerSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "RTSUnit",
            []() -> Scope<ISystem> { return CreateScope<RTSUnitSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "RTSAnimation",
            []() -> Scope<ISystem> { return CreateScope<RTSAnimationSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "RTSProduction",
            []() -> Scope<ISystem> { return CreateScope<RTSProductionSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "RTSAI",
            []() -> Scope<ISystem> { return CreateScope<RTSAISystem>(); });
    }

} // namespace Wheatear
