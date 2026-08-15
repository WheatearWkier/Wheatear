#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Core/Layer.h"
#include "Wheatear/Core/Timestep.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace Wheatear {
    class Event;
    class MouseButtonPressedEvent;
    class MouseButtonReleasedEvent;
    class MouseScrolledEvent;
    class Scene;
    struct SceneTransitionRequest;
}

class RuntimeSceneLayer : public Wheatear::Layer
{
public:
    RuntimeSceneLayer();
    ~RuntimeSceneLayer() override = default;

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(Wheatear::Timestep ts) override;
    void OnEvent(Wheatear::Event& event) override;

private:
    std::filesystem::path ResolveScenePath(const std::filesystem::path& requestedPath) const;
    void LoadScene(const std::filesystem::path& requestedPath = {});
    void ApplyPendingSceneAutoLoadSlot();
    void UpdateViewport();
    bool ConsumeRuntimeSceneCommands();
    bool ConsumeSceneTransitionRequests();
    void ExecuteSceneTransitionRequest(const Wheatear::SceneTransitionRequest& request);

    bool OnMouseButtonPressed(Wheatear::MouseButtonPressedEvent& event);
    bool OnMouseButtonReleased(Wheatear::MouseButtonReleasedEvent& event);
    bool OnMouseScrolled(Wheatear::MouseScrolledEvent& event);
    bool ExecuteButtonCommand(const std::string& command);

private:
    Wheatear::Ref<Wheatear::Scene> m_ActiveScene;
    std::filesystem::path m_DefaultScenePath = Wheatear::EngineInfo::DefaultStartupScene;
    std::filesystem::path m_ScenePath;
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;
    bool m_RuntimeStarted = false;
    int m_PendingSceneAutoLoadSlot = 0;
};
