#include "wtpch.h"
#include "Application.h"

#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Config/UserSettings.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/InputBindingService.h"
#include "Wheatear/Events/ApplicationEvent.h"
#include "Wheatear/ImGui/ImGuiLayer.h"
#include "Wheatear/Renderer/Renderer.h"
#include "Window.h"
#include "Log.h"

#include <GLFW/glfw3.h>
#include <system_error>

namespace Wheatear {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;



	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification)
	{
		WT_PROFILE_FUNCTION();

		WT_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		if (!m_Specification.WorkingDirectory.empty())
		{
			std::error_code error;
			std::filesystem::current_path(m_Specification.WorkingDirectory, error);
			WT_CORE_ASSERT(!error, "Failed to set working directory '{}': {}",
				m_Specification.WorkingDirectory.string(), error.message());
		}

		AssetPath::SetAssetDirectoryName(m_Specification.AssetDirectoryName);
		AssetPath::SetProjectRoot(m_Specification.ProjectRoot.empty()
			? AssetPath::DiscoverProjectRoot()
			: m_Specification.ProjectRoot);

		m_Window = std::unique_ptr<Window>(Window::Create(WindowProps(m_Specification.Name)));
		m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));
		UserSettings::Load();
		UserSettings::ApplyToRuntime();

		InstallApplicationEventHandlers();

		Renderer::Init();
		AudioEngine::Init();

		m_ImGuiLayer = new ImGuiLayer();
		// The layer stack takes ownership; m_ImGuiLayer stays a non-owning view
		// used by Run() (Begin/End) and GetImGuiLayer() until Clear() at shutdown.
		PushOverlay(std::unique_ptr<ImGuiLayer>(m_ImGuiLayer));
	}

	Application::~Application()
	{
		WT_PROFILE_FUNCTION();

		m_LayerEventSubscriptions.clear();
		m_ApplicationEventSubscriptions.clear();
		m_LayerStack.Clear();
		m_EventBus.Clear();

		Renderer::Shutdown();
		AudioEngine::Shutdown();
		s_Instance = nullptr;
	}

	void Application::PushLayer(std::unique_ptr<Layer> layer)
	{
		WT_PROFILE_FUNCTION();
		m_PendingLayersToPush.push_back(std::move(layer));
	}

	void Application::PopLayer(Layer* layer)
	{
		m_PendingLayersToPop.push_back(layer);
	}

	void Application::PushOverlay(std::unique_ptr<Layer> overlay)
	{
		WT_PROFILE_FUNCTION();
		m_PendingOverlaysToPush.push_back(std::move(overlay));
	}

	void Application::PopOverlay(Layer* overlay)
	{
		m_PendingOverlaysToPop.push_back(overlay);
	}

	void Application::Close()
	{
		m_Running = false;
	}

	void Application::OnEvent(Event& e)
	{
		WT_PROFILE_FUNCTION();

		m_EventBus.Queue(e);
	}

	void Application::DispatchEvent(Event& e)
	{
		WT_PROFILE_FUNCTION();
		m_EventBus.Dispatch(e);
	}

	void Application::FlushEventQueue()
	{
		WT_PROFILE_FUNCTION();
		m_EventBus.Flush();
	}

	void Application::Run()
	{
		WT_PROFILE_FUNCTION();

		PostUpdateLayers();

		while (m_Running)
		{
			WT_PROFILE_SCOPE("RunLoop");

			glfwPollEvents();
			FlushEventQueue();

			if (!m_Running)
				break;

			// Compute the frame delta in double and only narrow to float for the
			// Timestep handed to layers; float accumulation of glfwGetTime() loses
			// millisecond precision after ~77 s of runtime.
			const double nowTime = glfwGetTime();
			Timestep timestep = static_cast<float>(nowTime - m_LastFrameTime);
			m_LastFrameTime = nowTime;

			if (!m_Minimized)
			{
				{
					WT_PROFILE_SCOPE("LayerStack OnUpdate");

					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(timestep);
				}

				if (m_ImGuiLayer)
				{
					m_ImGuiLayer->Begin();
					{
						WT_PROFILE_SCOPE("LayerStack OnImGuiRender");

						for (Layer* layer : m_LayerStack)
							layer->OnImGuiRender();
					}
					m_ImGuiLayer->End();
				}
			}

			// Commit input state after every layer has had a chance to query it.
			// This keeps edge queries stable even for actions skipped by the
			// active gameplay mode this frame.
			InputBindingService::EndFrame();
			Input::EndFrame();

			m_Window->OnUpdate();
			PostUpdateLayers();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		WT_PROFILE_FUNCTION();

		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}

		m_Minimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
		return true;
	}

	void Application::InstallApplicationEventHandlers()
	{
		m_ApplicationEventSubscriptions.emplace_back(
			m_EventBus.Subscribe<WindowCloseEvent>(
				[this](WindowCloseEvent& e) { return OnWindowClose(e); },
				EventPriority::Application));

		m_ApplicationEventSubscriptions.emplace_back(
			m_EventBus.Subscribe<WindowResizeEvent>(
				[this](WindowResizeEvent& e) { return OnWindowResize(e); },
				EventPriority::Application));
	}

	void Application::RebuildLayerEventRoute()
	{
		m_LayerEventSubscriptions.clear();

		int priority = EventPriority::Layer;
		for (Layer* layer : m_LayerStack)
		{
			m_LayerEventSubscriptions.emplace_back(
				m_EventBus.SubscribeAll(
					[layer](Event& event) -> EventResult
					{
						layer->OnEvent(event);
						return event.Handled() ? EventResult::Consume : EventResult::Continue;
					},
					priority++));
		}
	}

	void Application::PostUpdateLayers()
	{
		const bool routeDirty =
			!m_PendingLayersToPop.empty() ||
			!m_PendingOverlaysToPop.empty() ||
			!m_PendingLayersToPush.empty() ||
			!m_PendingOverlaysToPush.empty();

		if (routeDirty)
			m_LayerEventSubscriptions.clear();

		// LayerStack owns layers: PopLayer/PopOverlay detach and delete.
		for (Layer* layer : m_PendingLayersToPop)
		{
			m_LayerStack.PopLayer(layer);
			if (layer == m_ImGuiLayer)
				m_ImGuiLayer = nullptr;
		}
		m_PendingLayersToPop.clear();

		for (Layer* overlay : m_PendingOverlaysToPop)
		{
			m_LayerStack.PopOverlay(overlay);
			if (overlay == m_ImGuiLayer)
				m_ImGuiLayer = nullptr;
		}
		m_PendingOverlaysToPop.clear();

		for (auto& layer : m_PendingLayersToPush)
		{
			Layer* raw = layer.get();
			m_LayerStack.PushLayer(std::move(layer));
			raw->OnAttach();
		}
		m_PendingLayersToPush.clear();

		for (auto& overlay : m_PendingOverlaysToPush)
		{
			Layer* raw = overlay.get();
			m_LayerStack.PushOverlay(std::move(overlay));
			raw->OnAttach();
		}
		m_PendingOverlaysToPush.clear();

		if (routeDirty)
			RebuildLayerEventRoute();
	}
}
