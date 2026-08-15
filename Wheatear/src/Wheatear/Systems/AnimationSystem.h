#pragma once

#include "ISystem.h"
#include "Wheatear/Animation/AnimationClip.h"

namespace Wheatear {

    /// 
    class AnimationSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnUpdateEditor(Scene* scene, Timestep ts) override;

        void SetEditorPreviewActive(bool active) { m_EditorPreviewActive = active; }

    private:
        void UpdateAnimations(Scene* scene, Timestep ts);
        void SyncEditorPreviewFrame(Scene* scene);

        bool m_EditorPreviewActive = false;
    };

} // namespace Wheatear
