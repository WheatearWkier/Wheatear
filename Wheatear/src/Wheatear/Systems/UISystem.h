#pragma once

#include "ISystem.h"

#include <glm/glm.hpp>

namespace Wheatear {

    /// 
    class UISystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnUpdateEditor(Scene* scene, Timestep ts) override;

        void SetViewportOffset(float x, float y) { m_ViewportOffset = { x, y }; }

        void RenderUI(Scene* scene);

    private:
        glm::vec2 m_ViewportOffset = { 0.0f, 0.0f };
    };

} // namespace Wheatear
