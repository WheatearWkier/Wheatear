#pragma once

#include "Wheatear/Core/Log.h"

#include "Wheatear/Core/LayerStack.h"
#include "Wheatear/Core/Timestep.h"
#include "Wheatear/Events/Event.h"
#include "Wheatear/Events/EventBus.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Wheatear {

	class ImGuiLayer;
	class Window;
	class WindowCloseEvent;
	class WindowResizeEvent;

	struct ApplicationCommandLineArgs
	{
		int Count = 0;
		char** Args = nullptr;

		const char* operator[](int index) const
		{
			WT_CORE_ASSERT(index < Count);
			return Args[index];
		}
	};

	struct ApplicationSpecification
	{
		std::string Name = "Wheatear App";
		std::filesystem::path WorkingDirectory;
		std::filesystem::path ProjectRoot;
		std::filesystem::path AssetDirectoryName = "assets";
		bool EnableScripting = true;
		ApplicationCommandLineArgs CommandLineArgs;
	};

	class Application
	{
	public:
		explicit Application(const ApplicationSpecification& specification = ApplicationSpecification());
		Application(const std::string& name = "Wheatear App",
			ApplicationCommandLineArgs args = ApplicationCommandLineArgs(),
			bool enableScripting = true);
		virtual ~Application();

		void Run();

		// Window callbacks enqueue platform events. Use DispatchEvent only when an
		// event must be delivered immediately and the caller owns its lifetime.
		void OnEvent(Event& e);
		void DispatchEvent(Event& e);
		void FlushEventQueue();

		void PushLayer(Layer* layer);
		void PopLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopOverlay(Layer* overlay);

		inline Window& GetWindow() { return *m_Window; }
		EventBus& GetEventBus() { return m_EventBus; }
		const EventBus& GetEventBus() const { return m_EventBus; }

		void Close();

		ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		inline static Application& Get() { return *s_Instance; }
		inline static bool Exists() { return s_Instance != nullptr; }

		const ApplicationSpecification& GetSpecification() const { return m_Specification; }
		ApplicationCommandLineArgs GetCommandLineArgs() const { return m_Specification.CommandLineArgs; }
		bool IsScriptingEnabled() const { return m_Specification.EnableScripting; }

	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

		void InstallApplicationEventHandlers();
		void RebuildLayerEventRoute();
		void PostUpdateLayers();

	private:
		ApplicationSpecification m_Specification;
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer = nullptr;

		EventBus m_EventBus;
		std::vector<EventSubscription> m_ApplicationEventSubscriptions;
		std::vector<EventSubscription> m_LayerEventSubscriptions;

		bool m_Running = true;
		bool m_Minimized = false;

		LayerStack m_LayerStack;
		std::vector<Layer*> m_PendingLayersToPush;
		std::vector<Layer*> m_PendingLayersToPop;
		std::vector<Layer*> m_PendingOverlaysToPush;
		std::vector<Layer*> m_PendingOverlaysToPop;

		float m_LastFrameTime = 0.0f;

		static Application* s_Instance;
	};

	// To be defined by the client application.
	Application* CreateApplication(ApplicationCommandLineArgs args);

}
