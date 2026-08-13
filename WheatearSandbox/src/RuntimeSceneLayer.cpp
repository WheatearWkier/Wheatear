#include "RuntimeSceneLayer.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Config/PlayerConfig.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Events/Event.h"
#include "Wheatear/Events/MouseEvent.h"
#include "Wheatear/Gameplay/Action/ActionDebugHistory.h"
#include "Wheatear/Modules/GameplayModuleRuntime.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Runtime/SceneTransitionService.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneSerializer.h"
#include "Wheatear/UI/UIInputSystem.h"

#include <algorithm>
#include <vector>

namespace {

    static bool StartsWith(const std::string& value, const std::string& prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

} // namespace

RuntimeSceneLayer::RuntimeSceneLayer()
    : Layer("RuntimeSceneLayer")
{
}

void RuntimeSceneLayer::OnAttach()
{
    Wheatear::WAO::ActionDebugHistory::Clear();

    Wheatear::RuntimePlayerConfig config = Wheatear::LoadRuntimePlayerConfig();
    std::filesystem::path requestedPath = config.StartupScene.empty()
        ? m_DefaultScenePath
        : config.StartupScene;
    Wheatear::ApplicationCommandLineArgs args = Wheatear::Application::Get().GetCommandLineArgs();
    for (int i = 1; i < args.Count; ++i)
    {
        const std::string argument = args[i];
        if (!argument.empty() && argument.front() == '-')
            continue;

        requestedPath = argument;
        break;
    }

    LoadScene(requestedPath);
}

void RuntimeSceneLayer::OnDetach()
{
    if (m_ActiveScene && m_RuntimeStarted)
    {
        m_ActiveScene->OnRuntimeStop();
        m_RuntimeStarted = false;
    }
}

void RuntimeSceneLayer::OnUpdate(Wheatear::Timestep ts)
{
    if (!m_ActiveScene)
        return;

    UpdateViewport();

    Wheatear::RenderCommand::SetClearColor({ 0.08f, 0.09f, 0.10f, 1.0f });
    Wheatear::RenderCommand::Clear();

    m_ActiveScene->OnUpdateRuntime(ts);
    ConsumeRuntimeSceneCommands();
}

void RuntimeSceneLayer::OnEvent(Wheatear::Event& event)
{
    Wheatear::EventDispatcher dispatcher(event);
    dispatcher.Dispatch<Wheatear::MouseButtonPressedEvent>(
        [this](Wheatear::MouseButtonPressedEvent& e) { return OnMouseButtonPressed(e); });
    dispatcher.Dispatch<Wheatear::MouseButtonReleasedEvent>(
        [this](Wheatear::MouseButtonReleasedEvent& e) { return OnMouseButtonReleased(e); });
    dispatcher.Dispatch<Wheatear::MouseScrolledEvent>(
        [this](Wheatear::MouseScrolledEvent& e) { return OnMouseScrolled(e); });
}

std::filesystem::path RuntimeSceneLayer::ResolveScenePath(
    const std::filesystem::path& requestedPath) const
{
    return Wheatear::AssetPath::ResolveRuntimeData(requestedPath);
}

void RuntimeSceneLayer::LoadScene(const std::filesystem::path& requestedPath)
{
    const std::filesystem::path sceneRequest = requestedPath.empty()
        ? m_DefaultScenePath
        : requestedPath;

    Wheatear::UIInputSystem::Reset();
    Wheatear::CommandBus::ClearQueuedCommands();

    if (m_ActiveScene && m_RuntimeStarted)
    {
        m_ActiveScene->OnRuntimeStop();
        m_RuntimeStarted = false;
    }

    const std::filesystem::path previousScenePath = m_ScenePath;
    m_ScenePath = ResolveScenePath(sceneRequest);
    m_ActiveScene = Wheatear::CreateRef<Wheatear::Scene>();

    Wheatear::SceneSerializer serializer(m_ActiveScene);
    if (!serializer.DeserializeYaml(m_ScenePath))
    {
        WT_CORE_ERROR("RuntimeSceneLayer: failed to load scene '{}'", m_ScenePath.string());
        m_ActiveScene = nullptr;
        return;
    }

    Wheatear::GameProgress::SetSceneTransitionContext(previousScenePath, sceneRequest);

    ApplyPendingVisualNovelLoad();

    WT_CORE_INFO("RuntimeSceneLayer: loaded scene '{}'", m_ScenePath.string());

    // A scene loaded at runtime has fresh cameras. Force a viewport push even
    // when the window size is unchanged from the previous scene.
    m_ViewportWidth = 0;
    m_ViewportHeight = 0;
    UpdateViewport();
    m_ActiveScene->OnRuntimeStart();
    m_RuntimeStarted = true;
}

void RuntimeSceneLayer::ApplyPendingVisualNovelLoad()
{
    if (!m_ActiveScene || m_PendingVisualNovelLoadSlot <= 0)
        return;

    Wheatear::ApplyVisualNovelAutoLoadSlot(m_ActiveScene.get(), m_PendingVisualNovelLoadSlot);
    m_PendingVisualNovelLoadSlot = 0;
}

void RuntimeSceneLayer::UpdateViewport()
{
    Wheatear::Window& window = Wheatear::Application::Get().GetWindow();
    const uint32_t width = window.GetWidth();
    const uint32_t height = window.GetHeight();

    if (width == 0 || height == 0)
        return;

    if (m_ViewportWidth == width && m_ViewportHeight == height)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;

    Wheatear::RenderCommand::SetViewport(0, 0, width, height);
    if (m_ActiveScene)
        m_ActiveScene->OnViewportResize(width, height);
}

bool RuntimeSceneLayer::ConsumeRuntimeSceneCommands()
{
    if (!m_ActiveScene)
        return false;

    std::vector<std::string> commands;
    Wheatear::DrainGameplayRuntimeCommands(m_ActiveScene.get(), commands);
    for (const std::string& command : Wheatear::CommandBus::DrainRuntimeCommands())
        commands.push_back(command);

    bool consumed = false;
    for (const std::string& command : commands)
    {
        consumed |= ExecuteButtonCommand(command);
        if (!m_ActiveScene)
            break;
    }

    consumed |= ConsumeSceneTransitionRequests();
    return consumed;
}

bool RuntimeSceneLayer::ConsumeSceneTransitionRequests()
{
    std::vector<Wheatear::SceneTransitionRequest> requests = Wheatear::SceneTransitionService::DrainRequests();
    if (requests.empty())
        return false;

    // Multiple scene requests in one frame are resolved by the final request.
    ExecuteSceneTransitionRequest(requests.back());
    return true;
}

void RuntimeSceneLayer::ExecuteSceneTransitionRequest(const Wheatear::SceneTransitionRequest& request)
{
    switch (request.Mode)
    {
    case Wheatear::SceneTransitionMode::LoadScene:
        LoadScene(request.ScenePath);
        break;
    case Wheatear::SceneTransitionMode::NewGame:
        Wheatear::GameProgress::ResetForNewGame();
        m_PendingVisualNovelLoadSlot = 0;
        LoadScene(request.ScenePath);
        break;
    case Wheatear::SceneTransitionMode::LoadGame:
        m_PendingVisualNovelLoadSlot = request.Slot;
        Wheatear::GameProgress::LoadSlot(request.Slot);
        LoadScene(request.ScenePath);
        break;
    }
}

bool RuntimeSceneLayer::OnMouseButtonPressed(Wheatear::MouseButtonPressedEvent& event)
{
    if (!m_ActiveScene || event.GetMouseButton() != WT_MOUSE_BUTTON_LEFT)
        return false;

    Wheatear::UIInputSystem::OnMousePressed(m_ActiveScene.get());
    return false;
}

bool RuntimeSceneLayer::OnMouseButtonReleased(Wheatear::MouseButtonReleasedEvent& event)
{
    if (!m_ActiveScene || event.GetMouseButton() != WT_MOUSE_BUTTON_LEFT)
        return false;

    Wheatear::UIInputSystem::OnMouseReleased(m_ActiveScene.get());
    return ConsumeRuntimeSceneCommands();
}

bool RuntimeSceneLayer::OnMouseScrolled(Wheatear::MouseScrolledEvent& event)
{
    if (!m_ActiveScene)
        return false;

    Wheatear::Window& window = Wheatear::Application::Get().GetWindow();
    return Wheatear::UIInputSystem::OnMouseScrolled(
        m_ActiveScene.get(),
        event.GetYOffset(),
        Wheatear::Input::GetMouseX(),
        Wheatear::Input::GetMouseY(),
        window.GetWidth(),
        window.GetHeight());
}

bool RuntimeSceneLayer::ExecuteButtonCommand(const std::string& command)
{
    if (command.empty() || StartsWith(command, "script:"))
        return false;

    if (command == "quit")
    {
        Wheatear::Application::Get().Close();
        return true;
    }

    return Wheatear::CommandBus::Execute(m_ActiveScene.get(), command).Handled;
}
