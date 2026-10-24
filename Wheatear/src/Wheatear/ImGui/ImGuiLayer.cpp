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
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace Wheatear {

	namespace {

		std::filesystem::path ResolveCjkFontPath()
		{
			std::vector<std::filesystem::path> candidates;
			if (const char* windowsDir = std::getenv("WINDIR"))
			{
				const std::filesystem::path fonts = std::filesystem::path(windowsDir) / "Fonts";
				candidates.push_back(fonts / "msyh.ttc");
				candidates.push_back(fonts / "msyhbd.ttc");
				candidates.push_back(fonts / "msyhl.ttc");
			}

			candidates.push_back(AssetPath::Resolve(
				AssetAliasRegistry::Path("font.ui_default", "assets/fonts/wqy-microhei.ttc")));
			candidates.push_back(AssetPath::Resolve(
				AssetAliasRegistry::Path("font.ui_fallback_sc", "assets/fonts/NotoSansSC-VF.ttf")));

			for (const auto& candidate : candidates)
			{
				if (!candidate.empty() && std::filesystem::exists(candidate))
					return candidate;
			}
			return {};
		}

		std::filesystem::path ResolveInterFontPath()
		{
			const std::filesystem::path candidates[] = {
				AssetPath::ResolveResource("Fonts/InterVariable.ttf"),
				AssetPath::Resolve("Resources/Fonts/InterVariable.ttf"),
			};
			for (const auto& candidate : candidates)
			{
				if (!candidate.empty() && std::filesystem::exists(candidate))
					return candidate;
			}
			return {};
		}

		ImFont* AddUiFont(ImFontAtlas* atlas,
			const std::filesystem::path& latin,
			const std::filesystem::path& cjk,
			float size)
		{
			ImFontConfig config;
			config.OversampleH = 2;
			config.OversampleV = 2;
			config.PixelSnapH = true;

			ImFont* font = nullptr;
			if (!latin.empty())
			{
				font = atlas->AddFontFromFileTTF(latin.string().c_str(), size,
					&config, atlas->GetGlyphRangesDefault());
			}

			if (!cjk.empty())
			{
				if (font)
				{
					ImFontConfig merge;
					merge.MergeMode = true;
					merge.OversampleH = 2;
					merge.OversampleV = 2;
					merge.PixelSnapH = true;
					atlas->AddFontFromFileTTF(cjk.string().c_str(), size, &merge,
						atlas->GetGlyphRangesChineseFull());
				}
				else
				{
					// Packaged games may not ship the Latin UI font. Use the
					// CJK font as the primary face instead of merging into
					// ImGui's implicit default font (which asserts).
					font = atlas->AddFontFromFileTTF(cjk.string().c_str(), size,
						&config, atlas->GetGlyphRangesChineseFull());
				}
			}
			if (!font)
				font = atlas->AddFontDefault();
			return font;
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

		std::filesystem::path UiThemePath()
		{
			return std::filesystem::path(ResolveImGuiIniPath()).parent_path()
				/ "editor_theme.txt";
		}

		std::filesystem::path UiScalePath()
		{
			return std::filesystem::path(ResolveImGuiIniPath()).parent_path()
				/ "editor_ui_scale.txt";
		}

		float LoadUiScale()
		{
			float scale = 1.0f;
			if (std::ifstream input(UiScalePath()); input.is_open())
			{
				float stored = 1.0f;
				if (input >> stored)
					scale = stored;
			}
			return std::clamp(scale, 0.85f, 2.00f);
		}

		void PersistUiTheme(const char* theme)
		{
			std::ofstream output(UiThemePath(), std::ios::trunc);
			if (output.is_open())
				output << theme << "\n";
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
		m_IniPath = ResolveImGuiIniPath();
		io.IniFilename = m_IniPath.c_str();

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
		
		const std::filesystem::path latinFont = ResolveInterFontPath();
		const std::filesystem::path cjkFont = ResolveCjkFontPath();

		// Inter carries the Latin UI; the CJK font is merged into the same
		// atlas so Chinese labels keep consistent metrics and no glyph boxes.
		io.FontDefault = AddUiFont(io.Fonts, latinFont, cjkFont, 16.0f);
		m_SmallFont = AddUiFont(io.Fonts, latinFont, cjkFont, 13.0f);
		m_HeadingFont = AddUiFont(io.Fonts, latinFont, cjkFont, 22.0f);

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
		if (std::ifstream themeInput(UiThemePath()); themeInput.is_open())
		{
			std::string theme;
			std::getline(themeInput, theme);
			if (theme == "light")
				SetLightThemeColors();
		}
		m_UiScale = LoadUiScale();
		m_SavedUiScale = m_UiScale;
		if (m_HasBaseStyle)
			ApplyUiScale();

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


	void ApplyUiMetrics(ImGuiStyle& style)
	{
		style.WindowPadding = ImVec2(12.0f, 10.0f);
		style.FramePadding = ImVec2(8.0f, 5.0f);
		style.CellPadding = ImVec2(7.0f, 5.0f);
		style.ItemSpacing = ImVec2(8.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
		style.IndentSpacing = 16.0f;
		style.ScrollbarSize = 12.0f;
		style.GrabMinSize = 10.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.TabBorderSize = 1.0f;
		style.WindowRounding = 8.0f;
		style.ChildRounding = 8.0f;
		style.FrameRounding = 6.0f;
		style.PopupRounding = 8.0f;
		style.ScrollbarRounding = 8.0f;
		style.GrabRounding = 6.0f;
		style.TabRounding = 6.0f;
		style.DockingSeparatorSize = 2.0f;
		style.TabBarOverlineSize = 2.0f;
		style.SeparatorTextBorderSize = 1.0f;
		style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
		style.AntiAliasedLines = true;
		style.AntiAliasedFill = true;
		style.CurveTessellationTol = 1.25f;
	}

	void ImGuiLayer::SetUiScale(float scale)
	{
		scale = std::clamp(scale, 0.85f, 2.00f);
		if (std::abs(scale - m_UiScale) < 0.001f)
			return;
		m_UiScale = scale;
		m_SavedUiScale = scale;
		ApplyUiScale();
		PersistUiScale();
	}

	void ImGuiLayer::SetUiScaleTransient(float scale)
	{
		scale = std::clamp(scale, 0.50f, 2.00f);
		if (std::abs(scale - m_UiScale) < 0.001f)
			return;
		m_UiScale = scale;
		ApplyUiScale();
	}

	void ImGuiLayer::RestoreSavedUiScale()
	{
		SetUiScaleTransient(m_SavedUiScale);
	}

	void ImGuiLayer::AdjustUiScale(float delta)
	{
		const float snapped = std::round((m_UiScale + delta) * 20.0f) / 20.0f;
		SetUiScale(snapped);
	}

	void ImGuiLayer::ApplyUiScale()
	{
		if (!m_BaseStyle)
			return;
		// Text is the primary UI element: the previous 130% text size is the
		// new 100% baseline. Icons and thumbnails keep their own scale (the
		// Content Browser even has a dedicated Ctrl+wheel zoom).
		constexpr float kTextBaselineBoost = 1.43f;
		const float effectiveScale = m_UiScale * kTextBaselineBoost;
		ImGuiStyle& style = ImGui::GetStyle();
		style = *m_BaseStyle;
		style.ScaleAllSizes(effectiveScale);
		ImGui::GetIO().FontGlobalScale = effectiveScale;
	}

	void ImGuiLayer::PersistUiScale() const
	{
		std::ofstream output(UiScalePath(), std::ios::trunc);
		if (output.is_open())
			output << m_UiScale << "\n";
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ApplyUiMetrics(style);
		m_DarkTheme = true;

		auto& colors = style.Colors;

		// Midnight surfaces with a warm wheat accent. The accent is reserved
		// for focus/selection/brand; semantic colors stay blue/red/green.
		const ImVec4 bg0 = ImVec4{ 0.055f, 0.062f, 0.078f, 1.0f };
		const ImVec4 bg1 = ImVec4{ 0.078f, 0.087f, 0.108f, 1.0f };
		const ImVec4 bg2 = ImVec4{ 0.118f, 0.130f, 0.160f, 1.0f };
		const ImVec4 bg3 = ImVec4{ 0.165f, 0.180f, 0.215f, 1.0f };
		const ImVec4 bg4 = ImVec4{ 0.215f, 0.235f, 0.280f, 1.0f };
		const ImVec4 border = ImVec4{ 0.180f, 0.196f, 0.235f, 1.0f };
		const ImVec4 text = ImVec4{ 0.920f, 0.930f, 0.950f, 1.0f };
		const ImVec4 textMuted = ImVec4{ 0.540f, 0.570f, 0.630f, 1.0f };
		const ImVec4 wheat = ImVec4{ 0.850f, 0.663f, 0.373f, 1.0f };
		const ImVec4 wheatHot = ImVec4{ 0.980f, 0.790f, 0.500f, 1.0f };
		const ImVec4 blue = ImVec4{ 0.420f, 0.650f, 0.900f, 1.0f };
		const ImVec4 green = ImVec4{ 0.400f, 0.760f, 0.560f, 1.0f };

		colors[ImGuiCol_Text] = text;
		colors[ImGuiCol_TextDisabled] = textMuted;
		colors[ImGuiCol_WindowBg] = bg0;
		colors[ImGuiCol_ChildBg] = bg1;
		colors[ImGuiCol_PopupBg] = ImVec4{ 0.070f, 0.078f, 0.098f, 0.98f };
		colors[ImGuiCol_Border] = border;
		colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };

		colors[ImGuiCol_FrameBg] = bg2;
		colors[ImGuiCol_FrameBgHovered] = bg3;
		colors[ImGuiCol_FrameBgActive] = bg4;
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.045f, 0.050f, 0.064f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.090f, 0.102f, 0.128f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = bg1;
		colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.070f, 0.078f, 0.098f, 1.0f };

		colors[ImGuiCol_Button] = bg2;
		colors[ImGuiCol_ButtonHovered] = bg3;
		colors[ImGuiCol_ButtonActive] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.32f };
		colors[ImGuiCol_Header] = bg2;
		colors[ImGuiCol_HeaderHovered] = bg3;
		colors[ImGuiCol_HeaderActive] = bg4;
		colors[ImGuiCol_CheckMark] = wheatHot;
		colors[ImGuiCol_SliderGrab] = wheat;
		colors[ImGuiCol_SliderGrabActive] = wheatHot;
		colors[ImGuiCol_InputTextCursor] = wheatHot;
		colors[ImGuiCol_TextLink] = blue;

		colors[ImGuiCol_Tab] = bg1;
		colors[ImGuiCol_TabHovered] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.22f };
		colors[ImGuiCol_TabActive] = bg2;
		colors[ImGuiCol_TabSelectedOverline] = wheat;
		colors[ImGuiCol_TabDimmed] = ImVec4{ 0.065f, 0.073f, 0.090f, 1.0f };
		colors[ImGuiCol_TabDimmedSelected] = bg2;
		colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.40f };
		colors[ImGuiCol_TabUnfocused] = bg1;
		colors[ImGuiCol_TabUnfocusedActive] = bg2;

		colors[ImGuiCol_Separator] = border;
		colors[ImGuiCol_SeparatorHovered] = wheat;
		colors[ImGuiCol_SeparatorActive] = wheatHot;
		colors[ImGuiCol_ResizeGrip] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.22f };
		colors[ImGuiCol_ResizeGripHovered] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.55f };
		colors[ImGuiCol_ResizeGripActive] = wheatHot;
		colors[ImGuiCol_ScrollbarBg] = bg1;
		colors[ImGuiCol_ScrollbarGrab] = bg3;
		colors[ImGuiCol_ScrollbarGrabHovered] = bg4;
		colors[ImGuiCol_ScrollbarGrabActive] = wheat;

		colors[ImGuiCol_DockingPreview] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.50f };
		colors[ImGuiCol_DockingEmptyBg] = bg1;
		colors[ImGuiCol_TableHeaderBg] = bg2;
		colors[ImGuiCol_TableBorderStrong] = border;
		colors[ImGuiCol_TableBorderLight] = ImVec4{ 0.150f, 0.164f, 0.196f, 1.0f };
		colors[ImGuiCol_TableRowBg] = ImVec4{ 1.0f, 1.0f, 1.0f, 0.0f };
		colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 1.0f, 1.0f, 1.0f, 0.035f };
		colors[ImGuiCol_TextSelectedBg] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.26f };
		colors[ImGuiCol_DragDropTarget] = wheat;
		colors[ImGuiCol_NavCursor] = wheatHot;
		colors[ImGuiCol_NavHighlight] = wheatHot;
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4{ 0.015f, 0.018f, 0.028f, 0.60f };
		colors[ImGuiCol_PlotLines] = blue;
		colors[ImGuiCol_PlotHistogram] = green;
		colors[ImGuiCol_TreeLines] = ImVec4{ 0.230f, 0.250f, 0.300f, 1.0f };
		m_BaseStyle = std::make_unique<ImGuiStyle>(style);
		m_HasBaseStyle = true;
		ApplyUiScale();
		PersistUiTheme("dark");
	}

	void ImGuiLayer::SetLightThemeColors()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::StyleColorsLight();
		ApplyUiMetrics(style);
		m_DarkTheme = false;

		auto& colors = style.Colors;
		const ImVec4 bg0 = ImVec4{ 0.965f, 0.955f, 0.935f, 1.0f };
		const ImVec4 bg1 = ImVec4{ 0.985f, 0.978f, 0.962f, 1.0f };
		const ImVec4 bg2 = ImVec4{ 0.905f, 0.890f, 0.862f, 1.0f };
		const ImVec4 bg3 = ImVec4{ 0.855f, 0.835f, 0.798f, 1.0f };
		const ImVec4 bg4 = ImVec4{ 0.800f, 0.775f, 0.735f, 1.0f };
		const ImVec4 border = ImVec4{ 0.780f, 0.760f, 0.715f, 1.0f };
		const ImVec4 text = ImVec4{ 0.105f, 0.115f, 0.135f, 1.0f };
		const ImVec4 muted = ImVec4{ 0.410f, 0.425f, 0.465f, 1.0f };
		const ImVec4 wheat = ImVec4{ 0.630f, 0.420f, 0.150f, 1.0f };
		const ImVec4 wheatHot = ImVec4{ 0.780f, 0.550f, 0.240f, 1.0f };
		const ImVec4 blue = ImVec4{ 0.160f, 0.400f, 0.680f, 1.0f };
		const ImVec4 green = ImVec4{ 0.150f, 0.480f, 0.300f, 1.0f };

		colors[ImGuiCol_Text] = text;
		colors[ImGuiCol_TextDisabled] = muted;
		colors[ImGuiCol_WindowBg] = bg0;
		colors[ImGuiCol_ChildBg] = bg1;
		colors[ImGuiCol_PopupBg] = ImVec4{ 0.995f, 0.990f, 0.980f, 0.98f };
		colors[ImGuiCol_Border] = border;
		colors[ImGuiCol_FrameBg] = bg2;
		colors[ImGuiCol_FrameBgHovered] = bg3;
		colors[ImGuiCol_FrameBgActive] = bg4;
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.925f, 0.912f, 0.885f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.975f, 0.965f, 0.945f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = bg1;
		colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.945f, 0.935f, 0.915f, 1.0f };
		colors[ImGuiCol_Button] = bg2;
		colors[ImGuiCol_ButtonHovered] = bg3;
		colors[ImGuiCol_ButtonActive] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.24f };
		colors[ImGuiCol_Header] = bg2;
		colors[ImGuiCol_HeaderHovered] = bg3;
		colors[ImGuiCol_HeaderActive] = bg4;
		colors[ImGuiCol_CheckMark] = wheat;
		colors[ImGuiCol_SliderGrab] = wheat;
		colors[ImGuiCol_SliderGrabActive] = wheatHot;
		colors[ImGuiCol_InputTextCursor] = wheat;
		colors[ImGuiCol_TextLink] = blue;
		colors[ImGuiCol_Tab] = bg1;
		colors[ImGuiCol_TabHovered] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.18f };
		colors[ImGuiCol_TabActive] = bg2;
		colors[ImGuiCol_TabSelectedOverline] = wheat;
		colors[ImGuiCol_TabDimmed] = ImVec4{ 0.940f, 0.930f, 0.910f, 1.0f };
		colors[ImGuiCol_TabDimmedSelected] = bg2;
		colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.35f };
		colors[ImGuiCol_TabUnfocused] = bg1;
		colors[ImGuiCol_TabUnfocusedActive] = bg2;
		colors[ImGuiCol_Separator] = border;
		colors[ImGuiCol_SeparatorHovered] = wheat;
		colors[ImGuiCol_SeparatorActive] = wheatHot;
		colors[ImGuiCol_ResizeGrip] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.22f };
		colors[ImGuiCol_ResizeGripHovered] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.50f };
		colors[ImGuiCol_ResizeGripActive] = wheatHot;
		colors[ImGuiCol_ScrollbarBg] = bg1;
		colors[ImGuiCol_ScrollbarGrab] = bg3;
		colors[ImGuiCol_ScrollbarGrabHovered] = bg4;
		colors[ImGuiCol_ScrollbarGrabActive] = wheat;
		colors[ImGuiCol_DockingPreview] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.45f };
		colors[ImGuiCol_DockingEmptyBg] = bg1;
		colors[ImGuiCol_TableHeaderBg] = bg2;
		colors[ImGuiCol_TableBorderStrong] = border;
		colors[ImGuiCol_TableBorderLight] = ImVec4{ 0.860f, 0.845f, 0.815f, 1.0f };
		colors[ImGuiCol_TableRowBg] = ImVec4{ 1.0f, 1.0f, 1.0f, 0.0f };
		colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.025f };
		colors[ImGuiCol_TextSelectedBg] = ImVec4{ wheat.x, wheat.y, wheat.z, 0.22f };
		colors[ImGuiCol_DragDropTarget] = wheat;
		colors[ImGuiCol_NavCursor] = wheat;
		colors[ImGuiCol_NavHighlight] = wheat;
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4{ 0.10f, 0.10f, 0.12f, 0.35f };
		colors[ImGuiCol_PlotLines] = blue;
		colors[ImGuiCol_PlotHistogram] = green;
		colors[ImGuiCol_TreeLines] = ImVec4{ 0.800f, 0.780f, 0.740f, 1.0f };
		m_BaseStyle = std::make_unique<ImGuiStyle>(style);
		m_HasBaseStyle = true;
		ApplyUiScale();
		PersistUiTheme("light");
	}
}
