#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/Layer.h"
#include "Wheatear/Scene/Scene.h"

#include <cstdint>
#include <filesystem>

namespace Wheatear {

    // Milestone 1 RTS host: loads the authored 3D scene, starts the RTS
    // systems, and exposes the baseline performance HUD.
    class RTSGameLayer : public Layer
    {
    public:
        RTSGameLayer();

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(Timestep ts) override;
        void OnImGuiRender() override;
        void OnEvent(Event& event) override;

    private:
        void LoadScene();
        void Restart();
        void UpdateViewport(bool force = false);

        Ref<Scene> m_Scene;
        std::filesystem::path m_ScenePath;

        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;
        double m_UpdateMilliseconds = 0.0;

        bool m_RuntimeStarted = false;
        bool m_RestartRequested = false;
    };

} // namespace Wheatear
