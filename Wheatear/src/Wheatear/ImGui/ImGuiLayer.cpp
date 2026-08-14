#include "wtpch.h"
#include "ImGuiLayer.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_glfw.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
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
		// Drag a docked window as a transparent ghost that follows the cursor
		// instead of an opaque window — docking feels much more responsive.
		io.ConfigDockingTransparentPayload = true;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge,
		
		float fontSize = 30.0f;
		std::filesystem::path fontPath = ResolveImGuiFontPath();
		if (!fontPath.empty())
		{
			io.FontDefault = io.Fonts->AddFontFromFileTTF(
				fontPath.string().c_str(),
				fontSize,
				nullptr,
				io.Fonts->GetGlyphRangesChineseFull());

			// Compact variant for status bars, badges and dense rows. Loading
			// the same TTF at a smaller size keeps the glyph set consistent.
			m_SmallFont = io.Fonts->AddFontFromFileTTF(
				fontPath.string().c_str(),
				20.0f,
				nullptr,
				io.Fonts->GetGlyphRangesChineseFull());
		}
		else
		{
			io.FontDefault = io.Fonts->AddFontDefault();
			m_SmallFont = io.FontDefault;
		}

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
		style.WindowPadding = ImVec2(12.0f, 10.0f);
		style.FramePadding = ImVec2(8.0f, 5.0f);
		style.CellPadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
		style.IndentSpacing = 15.0f;
		style.ScrollbarSize = 15.0f;
		style.GrabMinSize = 12.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.TabBorderSize = 0.0f;
		style.WindowRounding = 8.0f;
		style.ChildRounding = 8.0f;
		style.FrameRounding = 6.0f;
		style.PopupRounding = 8.0f;
		style.ScrollbarRounding = 7.0f;
		style.GrabRounding = 6.0f;
		style.TabRounding = 6.0f;
		style.DockingSeparatorSize = 4.0f;   // Thicker dock splitter for easier grabbing
		style.TabBarOverlineSize = 2.0f;     // Selected-tab top highlight bar (TabSelectedOverline)

		auto& colors = style.Colors;

		// Deep slate-blue surface scale with a teal accent (brand color).
		const ImVec4 bg0 = ImVec4{ 0.085f, 0.095f, 0.115f, 1.0f };    // window
		const ImVec4 bg1 = ImVec4{ 0.105f, 0.118f, 0.142f, 1.0f };    // child / menu bar / scrollbar bg
		const ImVec4 bg2 = ImVec4{ 0.155f, 0.172f, 0.200f, 1.0f };    // frame / button / header resting
		const ImVec4 bg3 = ImVec4{ 0.205f, 0.225f, 0.260f, 1.0f };    // hovered
		const ImVec4 bg4 = ImVec4{ 0.255f, 0.280f, 0.320f, 1.0f };    // active / selected
		const ImVec4 border = ImVec4{ 0.205f, 0.220f, 0.255f, 1.0f };
		const ImVec4 text = ImVec4{ 0.900f, 0.910f, 0.920f, 1.0f };
		const ImVec4 textDisabled = ImVec4{ 0.500f, 0.540f, 0.590f, 1.0f };
		const ImVec4 teal = ImVec4{ 0.230f, 0.720f, 0.800f, 1.0f };
		const ImVec4 tealHot = ImVec4{ 0.320f, 0.820f, 0.900f, 1.0f };
		const ImVec4 wheat = ImVec4{ 0.850f, 0.650f, 0.360f, 1.0f };
		const ImVec4 coral = ImVec4{ 0.880f, 0.420f, 0.360f, 1.0f };

		colors[ImGuiCol_Text] = text;
		colors[ImGuiCol_TextDisabled] = textDisabled;
		colors[ImGuiCol_WindowBg] = bg0;
		colors[ImGuiCol_ChildBg] = bg1;
		colors[ImGuiCol_PopupBg] = ImVec4{ 0.100f, 0.110f, 0.130f, 0.98f };
		colors[ImGuiCol_Border] = border;
		colors[ImGuiCol_BorderShadow] = ImVec4{ 0.000f, 0.000f, 0.000f, 0.0f };

		colors[ImGuiCol_FrameBg] = bg2;
		colors[ImGuiCol_FrameBgHovered] = bg3;
		colors[ImGuiCol_FrameBgActive] = bg4;
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.065f, 0.072f, 0.088f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.155f, 0.185f, 0.220f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = bg1;
		colors[ImGuiCol_MenuBarBg] = bg1;

		colors[ImGuiCol_Button] = bg2;
		colors[ImGuiCol_ButtonHovered] = bg3;
		colors[ImGuiCol_ButtonActive] = ImVec4{ teal.x, teal.y, teal.z, 0.35f };
		colors[ImGuiCol_Header] = bg2;
		colors[ImGuiCol_HeaderHovered] = bg3;
		colors[ImGuiCol_HeaderActive] = bg4;
		colors[ImGuiCol_CheckMark] = tealHot;
		colors[ImGuiCol_SliderGrab] = teal;
		colors[ImGuiCol_SliderGrabActive] = tealHot;

		colors[ImGuiCol_Tab] = bg1;
		colors[ImGuiCol_TabHovered] = bg3;
		colors[ImGuiCol_TabActive] = bg2;
		colors[ImGuiCol_TabSelectedOverline] = teal;
		colors[ImGuiCol_TabDimmed] = ImVec4{ 0.090f, 0.100f, 0.120f, 1.0f };
		colors[ImGuiCol_TabDimmedSelected] = bg2;
		colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4{ teal.x, teal.y, teal.z, 0.45f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.085f, 0.095f, 0.112f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = bg1;

		colors[ImGuiCol_Separator] = border;
		colors[ImGuiCol_SeparatorHovered] = teal;
		colors[ImGuiCol_SeparatorActive] = tealHot;
		colors[ImGuiCol_ResizeGrip] = ImVec4{ teal.x, teal.y, teal.z, 0.25f };
		colors[ImGuiCol_ResizeGripHovered] = ImVec4{ teal.x, teal.y, teal.z, 0.55f };
		colors[ImGuiCol_ResizeGripActive] = tealHot;
		colors[ImGuiCol_ScrollbarBg] = bg1;
		colors[ImGuiCol_ScrollbarGrab] = bg3;
		colors[ImGuiCol_ScrollbarGrabHovered] = bg4;
		colors[ImGuiCol_ScrollbarGrabActive] = teal;

		colors[ImGuiCol_DockingPreview] = ImVec4{ teal.x, teal.y, teal.z, 0.55f };
		colors[ImGuiCol_DockingEmptyBg] = bg1;
		colors[ImGuiCol_TableHeaderBg] = bg2;
		colors[ImGuiCol_TableBorderStrong] = border;
		colors[ImGuiCol_TableBorderLight] = ImVec4{ 0.220f, 0.240f, 0.280f, 1.0f };
		colors[ImGuiCol_TableRowBg] = ImVec4{ 1.000f, 1.000f, 1.000f, 0.0f };
		colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 1.000f, 1.000f, 1.000f, 0.030f };
		colors[ImGuiCol_TextSelectedBg] = ImVec4{ teal.x, teal.y, teal.z, 0.30f };
		colors[ImGuiCol_DragDropTarget] = teal;
		colors[ImGuiCol_NavHighlight] = tealHot;
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4{ 0.020f, 0.025f, 0.040f, 0.55f };
		colors[ImGuiCol_PlotLines] = teal;
		colors[ImGuiCol_PlotHistogram] = wheat;
	}
}
