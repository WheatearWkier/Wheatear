#pragma once

#include "Wheatear/Core/Timestep.h"

namespace Wheatear {

    class Entity;
    class Scene;

    class ISystem
    {
    public:
        virtual ~ISystem() = default;

        virtual void OnRuntimeStart(Scene* scene) {}
        virtual void OnRuntimeStop(Scene* scene) {}

        virtual void OnEditorStart(Scene* scene) { OnRuntimeStart(scene); }
        virtual void OnEditorStop(Scene* scene) { OnRuntimeStop(scene); }

        virtual void OnUpdateRuntime(Scene* scene, Timestep ts) {}
        virtual void OnUpdateEditor(Scene* scene, Timestep ts) {}

        virtual void OnEntityCreated(Scene* scene, Entity& entity) {}
        virtual void OnEntityDestroy(Scene* scene, Entity& entity) {}
    };

} // namespace Wheatear