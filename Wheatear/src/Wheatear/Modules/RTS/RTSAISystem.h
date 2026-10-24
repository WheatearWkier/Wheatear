#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear {

    // Simple economy + production + attack-wave AI for the greybox skirmish.
    class RTSAISystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
    };

} // namespace Wheatear
