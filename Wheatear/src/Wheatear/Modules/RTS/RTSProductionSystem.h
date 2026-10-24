#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear {

    // Ticks building construction and unit production queues.
    class RTSProductionSystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
    };

} // namespace Wheatear
