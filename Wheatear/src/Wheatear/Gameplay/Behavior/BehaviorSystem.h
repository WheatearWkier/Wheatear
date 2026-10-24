#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear::WAO {

    class BehaviorSystem final : public ISystem
    {
    public:
        void OnRuntimeStop(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
    };

} // namespace Wheatear::WAO
