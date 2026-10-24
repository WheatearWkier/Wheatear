#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear {

    class RTSCameraSystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
    };

} // namespace Wheatear
