#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear {

    ///
    class ArcadeCombatSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;

    private:

    private:
    };

} // namespace Wheatear
