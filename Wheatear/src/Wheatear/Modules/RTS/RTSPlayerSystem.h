#pragma once

#include "Wheatear/Systems/ISystem.h"

#include <glm/glm.hpp>

namespace Wheatear {

    class RTSPlayerSystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;

    private:
        bool m_LeftDownLast = false;
        bool m_RightDownLast = false;
        bool m_F4DownLast = false;
        bool m_TrainSoldierLast = false;
        bool m_TrainHarvesterLast = false;
        bool m_BuildLast = false;
        bool m_Dragging = false;
        glm::vec2 m_DragStart = glm::vec2(0.0f);
        glm::vec2 m_DragEnd = glm::vec2(0.0f);
    };

} // namespace Wheatear
