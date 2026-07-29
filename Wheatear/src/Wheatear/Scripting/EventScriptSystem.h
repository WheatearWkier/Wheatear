#pragma once

#include "Wheatear/Systems/ISystem.h"

#include "entt.hpp"
#include <string>

namespace Wheatear {

    class EventScriptSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;

    private:
        void StartEvent(Scene* scene, entt::entity entity, const std::string& eventName);
        void UpdateScript(Scene* scene, entt::entity entity, float deltaSeconds);
    };

} // namespace Wheatear
