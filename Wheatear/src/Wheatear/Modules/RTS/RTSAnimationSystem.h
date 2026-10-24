#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear {

    // Maps RTS unit states (Idle/Move/Attack/Dying) to either skeletal clips
    // on an Animator3DComponent or a greybox scale fallback for units that
    // only have a procedural mesh.
    class RTSAnimationSystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
    };

} // namespace Wheatear
