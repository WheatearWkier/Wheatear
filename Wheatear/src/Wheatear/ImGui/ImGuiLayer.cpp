#include "wtpch.h"
#include "ImGuiLayer.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_glfw.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Events/Event.h"

#include "GLFW/glfw3.h"
#include "glad/glad.h"

#include "ImGuizmo.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace Wheatear {

	namespace {

		std::filesystem::path ResolveImGuiFontPath()
		{
			std::vector<std::filesystem::path> candidates;

			if (const char* windowsDir = std::getenv("WINDIR"))
			{
				const std::filesystem::path fonts = std::filesystem::path(windowsDir) / "Fonts";
				candidates.push_back(fonts / "msyh.ttc");
				candidates.push_back(fonts / "msyhbd.ttc");
				candidates.push_back(fonts / "msyhl.ttc");
				candidates.push_back(fonts / "arial.ttf");
			}

            candidates.push_back(AssetPath::Resolve(AssetAliasRegistry::Path("font.ui_default", "assets/fonts/wqy-microhei.ttc")));
            candidates.push_back(AssetPath::Resolve(AssetAliasRegistry::Path("font.ui_fallback_sc", "assets/fonts/NotoSansSC-VF.ttf")));
            candidates.push_back(AssetPath::Resolve(AssetAliasRegistry::Path("font.latin", "assets/fonts/Open-Sans-2.ttf")));

			for (const auto& candidate : candidates)
			{
				if (std::filesystem::exists(candidate))
					return candidate;
			}

			return {};
		}

		const char* ResolveImGuiIniPath()
		{
			static std::string s_IniPath;

			std::filesystem::path basePath;
			if (const char* localAppData = std::getenv("LOCALAPPDATA"))
				basePath = localAppData;
			else
				basePath = std::filesystem::current_path();

			const std::filesystem::path settingsDirectory =
				basePath / "Wheatear" / Application::Get().GetSpecification().Name;

			std::error_code error;
			std::filesystem::create_directories(settingsDirectory, error);
			s_IniPath = (settingsDirectory / "imgui.ini").string();
			return s_IniPath.c_str();
		}

	}

	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer")
	{
	}

	ImGuiLayer::~ImGuiLayer()
	{
	}

	void ImGuiLayer::OnAttach()
	{
		WT_PROFILE_FUNCTION();

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.IniFilename = ResolveImGuiIniPath();

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard controls
		//io.configFlags |=ImGuiconfigFlags_NavEnableGamepad;  // Enable Gamepad controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable Multi-Viewport/ Platform windows
		io.ConfigViewportsNoDecoration = false;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge,
		
		float fontSize = 30.0f;
		std::filesystem::path fontPath = ResolveImGuiFontPath();
		if (!fontPath.empty())
			io.FontDefault = io.Fonts->AddFontFromFileTTF(
				fontPath.string().c_str(),
				fontSize,
				nullptr,
				io.Fonts->GetGlyphRangesChineseFull());
		else
			io.FontDefault = io.Fonts->AddFontDefault();

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();
		//ImGui::StyleColorsLight();
		
		/*When viewports are enabled we tweak WindowRounding/WindowBg 
		so platform windows can look identical to regular ones.*/
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) 
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		SetDarkThemeColors();

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForOpenGL(window, true); 
		ImGui_ImplOpenGL3_Init("#version 410");

	}

	void ImGuiLayer::OnDetach()
	{
		WT_PROFILE_FUNCTION();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if(m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.SetHandled(e.Handled() || (e.IsInCategory(EventCategoryMouse) && io.WantCaptureMouse));
			e.SetHandled(e.Handled() || (e.IsInCategory(EventCategoryKeyboard) && io.WantCaptureKeyboard));
		}
	}

	void ImGuiLayer::Begin() 
	{
		WT_PROFILE_FUNCTION();

		ImGui_ImplOpenGL3_NewFrame(); 
		ImGui_ImplGlfw_NewFrame(); 
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}
		
	void ImGuiLayer::End() 
	{
		WT_PROFILE_FUNCTION();

		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());
		
		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) 
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext(); 
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}


	void ImGuiLayer::SetDarkThemeColors()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(11.0f, 9.0f);
		style.FramePadding = ImVec2(8.0f, 5.0f);
		style.CellPadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 7.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
		style.IndentSpacing = 15.0f;
		style.ScrollbarSize = 14.0f;
		style.GrabMinSize = 11.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.TabBorderSize = 0.0f;
		style.WindowRounding = 7.0f;
		style.ChildRounding = 7.0f;
		style.FrameRounding = 6.0f;
		style.PopupRounding = 7.0f;
		style.ScrollbarRounding = 7.0f;
		style.GrabRounding = 6.0f;
		style.TabRounding = 6.0f;

		auto& colors = style.Colors;

		const ImVec4 ink = ImVec4{ 0.105f, 0.135f, 0.145f, 1.0f };
		const ImVec4 disabled = ImVec4{ 0.405f, 0.465f, 0.475f, 1.0f };
		const ImVec4 paper = ImVec4{ 0.785f, 0.815f, 0.810f, 1.0f };
		const ImVec4 panel = ImVec4{ 0.835f, 0.865f, 0.860f, 1.0f };
		const ImVec4 mist = ImVec4{ 0.875f, 0.900f, 0.895f, 1.0f };
		const ImVec4 line = ImVec4{ 0.575f, 0.655f, 0.650f, 1.0f };
		const ImVec4 teal = ImVec4{ 0.140f, 0.540f, 0.565f, 1.0f };
		const ImVec4 tealHot = ImVec4{ 0.090f, 0.640f, 0.670f, 1.0f };
		const ImVec4 wheat = ImVec4{ 0.760f, 0.565f, 0.245f, 1.0f };
		const ImVec4 coral = ImVec4{ 0.805f, 0.380f, 0.330f, 1.0f };

		colors[ImGuiCol_Text] = ink;
		colors[ImGuiCol_TextDisabled] = disabled;
		colors[ImGuiCol_WindowBg] = paper;
		colors[ImGuiCol_ChildBg] = panel;
		colors[ImGuiCol_PopupBg] = ImVec4{ 0.890f, 0.915f, 0.910f, 0.98f };
		colors[ImGuiCol_Border] = line;
		colors[ImGuiCol_BorderShadow] = ImVec4{ 0.000f, 0.000f, 0.000f, 0.0f };

		colors[ImGuiCol_FrameBg] = ImVec4{ 0.875f, 0.900f, 0.895f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.805f, 0.870f, 0.860f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.735f, 0.815f, 0.805f, 1.0f };
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.745f, 0.785f, 0.780f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.815f, 0.850f, 0.845f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = panel;
		colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.760f, 0.800f, 0.795f, 1.0f };

		colors[ImGuiCol_Button] = ImVec4{ 0.860f, 0.890f, 0.885f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.755f, 0.845f, 0.835f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.670f, 0.775f, 0.765f, 1.0f };
		colors[ImGuiCol_Header] = ImVec4{ 0.820f, 0.875f, 0.865f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.725f, 0.830f, 0.815f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.640f, 0.760f, 0.745f, 1.0f };
		colors[ImGuiCol_CheckMark] = tealHot;
		colors[ImGuiCol_SliderGrab] = teal;
		colors[ImGuiCol_SliderGrabActive] = tealHot;

		colors[ImGuiCol_Tab] = ImVec4{ 0.800f, 0.845f, 0.838f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.710f, 0.825f, 0.810f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.885f, 0.915f, 0.910f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.750f, 0.790f, 0.785f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.830f, 0.865f, 0.860f, 1.0f };

		colors[ImGuiCol_Separator] = line;
		colors[ImGuiCol_SeparatorHovered] = teal;
		colors[ImGuiCol_SeparatorActive] = tealHot;
		colors[ImGuiCol_ResizeGrip] = ImVec4{ teal.x, teal.y, teal.z, 0.25f };
		colors[ImGuiCol_ResizeGripHovered] = ImVec4{ teal.x, teal.y, teal.z, 0.55f };
		colors[ImGuiCol_ResizeGripActive] = tealHot;
		colors[ImGuiCol_ScrollbarBg] = ImVec4{ 0.770f, 0.810f, 0.805f, 1.0f };
		colors[ImGuiCol_ScrollbarGrab] = ImVec4{ 0.610f, 0.690f, 0.680f, 1.0f };
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{ 0.520f, 0.635f, 0.620f, 1.0f };
		colors[ImGuiCol_ScrollbarGrabActive] = teal;

		colors[ImGuiCol_DockingPreview] = ImVec4{ tealHot.x, tealHot.y, tealHot.z, 0.42f };
		colors[ImGuiCol_DockingEmptyBg] = ImVec4{ 0.720f, 0.765f, 0.760f, 1.0f };
		colors[ImGuiCol_TableHeaderBg] = ImVec4{ 0.775f, 0.830f, 0.820f, 1.0f };
		colors[ImGuiCol_TableBorderStrong] = line;
		colors[ImGuiCol_TableBorderLight] = ImVec4{ 0.665f, 0.745f, 0.735f, 1.0f };
		colors[ImGuiCol_TableRowBg] = ImVec4{ 1.000f, 1.000f, 1.000f, 0.0f };
		colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 0.900f, 0.930f, 0.925f, 0.38f };
		colors[ImGuiCol_TextSelectedBg] = ImVec4{ teal.x, teal.y, teal.z, 0.24f };
		colors[ImGuiCol_DragDropTarget] = wheat;
		colors[ImGuiCol_NavHighlight] = tealHot;
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4{ 0.190f, 0.250f, 0.260f, 0.38f };
		colors[ImGuiCol_PlotLines] = teal;
		colors[ImGuiCol_PlotHistogram] = wheat;
		colors[ImGuiCol_TextSelectedBg] = ImVec4{ 0.225f, 0.670f, 0.680f, 0.30f };
		colors[ImGuiCol_DragDropTarget] = coral;
	}
}
