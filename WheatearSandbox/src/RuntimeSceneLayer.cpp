#include "RuntimeSceneLayer.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Core/MouseButtonCodes.h"
#include "Wheatear/Core/PlayerConfig.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Events/Event.h"
#include "Wheatear/Events/MouseEvent.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Scene/Components.h"
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

    static std::string PayloadAfter(const std::string& value, const std::string& prefix)
    {
        return StartsWith(value, prefix) ? value.substr(prefix.size()) : std::string{};
    }

    static int ParseSlot(const std::string& value, int fallback)
    {
        try
        {
            return std::max(1, std::stoi(value));
        }
        catch (...)
        {
            return fallback;
        }
    }

} // namespace

RuntimeSceneLayer::RuntimeSceneLayer()
    : Layer("RuntimeSceneLayer")
{
}

void RuntimeSceneLayer::OnAttach()
{
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
}

std::filesystem::path RuntimeSceneLayer::ResolveScenePath(
    const std::filesystem::path& requestedPath) const
{
    return Wheatear::AssetPath::Resolve(requestedPath);
}

void RuntimeSceneLayer::LoadScene(const std::filesystem::path& requestedPath)
{
    const std::filesystem::path sceneRequest = requestedPath.empty()
        ? m_DefaultScenePath
        : requestedPath;

    if (m_ActiveScene && m_RuntimeStarted)
    {
        m_ActiveScene->OnRuntimeStop();
        m_RuntimeStarted = false;
    }

    m_ScenePath = ResolveScenePath(sceneRequest);
    m_ActiveScene = Wheatear::CreateRef<Wheatear::Scene>();

    Wheatear::SceneSerializer serializer(m_ActiveScene);
    if (!serializer.DeserializeYaml(m_ScenePath))
    {
        WT_CORE_ERROR("RuntimeSceneLayer: failed to load scene '{}'", m_ScenePath.string());
        m_ActiveScene = nullptr;
        return;
    }

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

    auto& registry = m_ActiveScene->GetRegistry();
    for (auto e : registry.view<Wheatear::VisualNovelComponent>())
        registry.get<Wheatear::VisualNovelComponent>(e).AutoLoadSlot = m_PendingVisualNovelLoadSlot;

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
    auto& registry = m_ActiveScene->GetRegistry();
    for (auto e : registry.view<Wheatear::VisualNovelComponent>())
    {
        auto& component = registry.get<Wheatear::VisualNovelComponent>(e);
        if (!component.RuntimeRequestedCommand.empty())
        {
            commands.push_back(component.RuntimeRequestedCommand);
            component.RuntimeRequestedCommand.clear();
        }
    }
    for (auto e : registry.view<Wheatear::ArcadeCombatLevelComponent>())
    {
        auto& component = registry.get<Wheatear::ArcadeCombatLevelComponent>(e);
        if (!component.RuntimeRequestedCommand.empty())
        {
            commands.push_back(component.RuntimeRequestedCommand);
            component.RuntimeRequestedCommand.clear();
        }
    }
    for (auto e : registry.view<Wheatear::SideCombatLevelComponent>())
    {
        auto& component = registry.get<Wheatear::SideCombatLevelComponent>(e);
        if (!component.RuntimeRequestedCommand.empty())
        {
            commands.push_back(component.RuntimeRequestedCommand);
            component.RuntimeRequestedCommand.clear();
        }
    }

    bool consumed = false;
    for (const std::string& command : commands)
    {
        consumed |= ExecuteButtonCommand(command);
        if (!m_ActiveScene)
            break;
    }

    return consumed;
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

    std::vector<std::string> commands;
    auto view = m_ActiveScene->GetRegistry().view<Wheatear::UIWidgetComponent, Wheatear::UIButtonComponent>();
    for (auto e : view)
    {
        auto [widget, button] = view.get<Wheatear::UIWidgetComponent, Wheatear::UIButtonComponent>(e);
        if (widget.Visible && button.IsHovered && !button.OnClickFunction.empty())
            commands.push_back(button.OnClickFunction);
    }

    Wheatear::UIInputSystem::OnMouseReleased(m_ActiveScene.get());

    bool consumed = false;
    for (const std::string& command : commands)
        consumed |= ExecuteButtonCommand(command);

    return consumed;
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

    if (StartsWith(command, "scene:"))
    {
        LoadScene(PayloadAfter(command, "scene:"));
        return true;
    }

    if (StartsWith(command, "newgame:"))
    {
        Wheatear::GameProgress::ResetForNewGame();
        m_PendingVisualNovelLoadSlot = 0;
        LoadScene(PayloadAfter(command, "newgame:"));
        return true;
    }

    if (StartsWith(command, "loadgame:"))
    {
        std::string payload = PayloadAfter(command, "loadgame:");
        int slot = 1;

        const size_t separator = payload.rfind(':');
        if (separator != std::string::npos)
        {
            slot = ParseSlot(payload.substr(separator + 1), 1);
            payload = payload.substr(0, separator);
        }

        m_PendingVisualNovelLoadSlot = slot;
        LoadScene(payload);
        return true;
    }

    if (StartsWith(command, "progression:"))
    {
        return Wheatear::GameProgress::ExecuteCommand(command).Handled;
    }

    return false;
}
