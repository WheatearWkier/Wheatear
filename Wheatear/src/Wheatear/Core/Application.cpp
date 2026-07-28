#include "wtpch.h"
#include "Application.h"

#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Events/ApplicationEvent.h"
#include "Wheatear/ImGui/ImGuiLayer.h"
#include "Wheatear/Renderer/Renderer.h"
#include "Wheatear/Scripting/ScriptEngine.h"
#include "Window.h"
#include "Log.h"

#include <GLFW/glfw3.h>
#include <system_error>

namespace Wheatear {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	namespace {

		static ApplicationSpecification CreateSpecification(const std::string& name,
			ApplicationCommandLineArgs args,
			bool enableScripting)
		{
			ApplicationSpecification specification;
			specification.Name = name;
			specification.CommandLineArgs = args;
			specification.EnableScripting = enableScripting;
			return specification;
		}

	} // namespace

	Application::Application(const std::string& name, ApplicationCommandLineArgs args, bool enableScripting)
		: Application(CreateSpecification(name, args, enableScripting))
	{
	}

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

		InstallApplicationEventHandlers();

		Renderer::Init();
		if (m_Specification.EnableScripting)
			ScriptEngine::Init();
		AudioEngine::Init();

		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);
	}

	Application::~Application()
	{
		WT_PROFILE_FUNCTION();

		// 先断开事件路由，再释放 Layer。这样 Layer::OnDetach() 内部即使触发事件，
		// 也不会再进入已经准备销毁的 LayerStack。
		m_LayerEventSubscriptions.clear();
		m_ApplicationEventSubscriptions.clear();
		m_LayerStack.Clear();
		m_EventBus.Clear();

		// Renderer 持有 OpenGL 资源，必须在 Window/GL context 析构前释放。
		Renderer::Shutdown();
		if (m_Specification.EnableScripting)
			ScriptEngine::Shutdown();
		AudioEngine::Shutdown();
		s_Instance = nullptr;
	}

	void Application::PushLayer(Layer* layer)
	{
		WT_PROFILE_FUNCTION();
		m_PendingLayersToPush.push_back(layer);
	}

	void Application::PopLayer(Layer* layer)
	{
		m_PendingLayersToPop.push_back(layer);
	}

	void Application::PushOverlay(Layer* overlay)
	{
		WT_PROFILE_FUNCTION();
		m_PendingOverlaysToPush.push_back(overlay);
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

		// 平台回调只提交事件，不直接深入业务层。
		// 统一在主循环 Flush，可以避免 GLFW 回调里发生复杂的重入调用。
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
			// 所有窗口/输入事件在帧开始统一派发，保持 FIFO 顺序。
			FlushEventQueue();

			if (!m_Running)
				break;

			float nowTime = (float)glfwGetTime();
			Timestep timestep = nowTime - m_LastFrameTime;
			m_LastFrameTime = nowTime;

			if (!m_Minized)
			{
				{
					WT_PROFILE_SCOPE("LayerStack OnUpdate");

					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(timestep);
				}

				m_ImGuiLayer->Begin();
				{
					WT_PROFILE_SCOPE("LayerStack OnImGuiRender");

					for (Layer* layer : m_LayerStack)
						layer->OnImGuiRender();
				}
				m_ImGuiLayer->End();
			}

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
			m_Minized = true;
			return false;
		}

		m_Minized = false;
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

		// LayerStack 仍保留“后加入的层优先处理事件”的语义；
		// 只是实现方式从手写逆序遍历，变成 EventBus 的优先级订阅。
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

		for (Layer* layer : m_PendingLayersToPop)
		{
			layer->OnDetach();
			m_LayerStack.PopLayer(layer);
			delete layer;
		}
		m_PendingLayersToPop.clear();

		for (Layer* overlay : m_PendingOverlaysToPop)
		{
			overlay->OnDetach();
			m_LayerStack.PopOverlay(overlay);
			delete overlay;
		}
		m_PendingOverlaysToPop.clear();

		for (Layer* layer : m_PendingLayersToPush)
		{
			m_LayerStack.PushLayer(layer);
			layer->OnAttach();
		}
		m_PendingLayersToPush.clear();

		for (Layer* overlay : m_PendingOverlaysToPush)
		{
			m_LayerStack.PushOverlay(overlay);
			overlay->OnAttach();
		}
		m_PendingOverlaysToPush.clear();

		if (routeDirty)
			RebuildLayerEventRoute();
	}
}
