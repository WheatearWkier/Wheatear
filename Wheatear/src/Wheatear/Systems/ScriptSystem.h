#pragma once

#include "ISystem.h"

namespace Wheatear {

    /// 
    class ScriptSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnEntityCreated(Scene* scene, Entity& entity) override;
        void OnEntityDestroy(Scene* scene, Entity& entity) override;

    };

} // namespace Wheatear
