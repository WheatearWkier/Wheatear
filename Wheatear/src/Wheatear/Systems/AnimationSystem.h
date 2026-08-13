#pragma once

#include "ISystem.h"
#include "Wheatear/Animation/AnimationClip.h"

#include "entt.hpp"
#include <unordered_map>

namespace Wheatear {

    /// 
    class AnimationSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnUpdateEditor(Scene* scene, Timestep ts) override;
        void OnEntityDestroy(Scene* scene, Entity& entity) override;

        void SetEditorPreviewActive(bool active) { m_EditorPreviewActive = active; }

    private:
        void UpdateAnimations(Scene* scene, Timestep ts);
        void SyncEditorPreviewFrame(Scene* scene);

        bool m_EditorPreviewActive = false;

        // Property-track writers are bound per (entity, clip) because clips are
        // shared assets but the writer must target this entity's components.
        // Rebuilding them every frame churns std::function allocations, so cache
        // the last bound clip per entity and only rebind on clip switch.
        std::unordered_map<entt::entity, const AnimationClip*> m_BoundClipForEntity;
    };

} // namespace Wheatear
