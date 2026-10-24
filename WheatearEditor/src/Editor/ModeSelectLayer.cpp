#include "wepch.h"
#include "ModeSelectLayer.h"

#include "Assets/AssetRegistry.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Config/PlayerConfig.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Utils/PlatformUtils.h"
#include "Wheatear/ImGui/ImGuiLayer.h"

#include "EditorLayer2D.h"
#include "EditorLayer3D.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace Wheatear {

    namespace {

        constexpr const char* kProjectsDirectoryName = "Projects";

        std::filesystem::path ProjectsRoot()
        {
            return AssetPath::GetEngineRoot().parent_path() / kProjectsDirectoryName;
        }

        std::string Trim(std::string value)
        {
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            return value;
        }

        std::filesystem::path EditorConfigDirectory()
        {
            std::error_code error;
            std::filesystem::path directory;
            if (const char* localAppData = std::getenv("LOCALAPPDATA"))
                directory = std::filesystem::path(localAppData) / "Wheatear";
            else
                directory = std::filesystem::temp_directory_path() / "Wheatear";
            std::filesystem::create_directories(directory, error);
            return directory;
        }

        void WriteLastProject(const std::filesystem::path& projectRoot)
        {
            std::ofstream output(EditorConfigDirectory() / "last_project.txt",
                std::ios::trunc);
            output << projectRoot.generic_string();
        }

        std::filesystem::path ReadLastProject()
        {
            std::ifstream input(EditorConfigDirectory() / "last_project.txt");
            if (!input.is_open())
                return {};
            std::string path;
            std::getline(input, path);
            return path.empty() ? std::filesystem::path{} : std::filesystem::path(path);
        }

        std::vector<std::string> LoadFavorites()
        {
            std::vector<std::string> favorites;
            std::ifstream input(EditorConfigDirectory() / "project_favorites.txt");
            if (!input.is_open())
                return favorites;
            std::string line;
            while (std::getline(input, line))
            {
                line = Trim(line);
                if (!line.empty())
                    favorites.push_back(line);
            }
            return favorites;
        }

        void SaveFavorites(const std::vector<std::string>& favorites)
        {
            std::ofstream output(EditorConfigDirectory() / "project_favorites.txt",
                std::ios::trunc);
            for (const std::string& favorite : favorites)
                output << favorite << "\n";
        }

        std::vector<std::filesystem::path> LoadImportedProjects()
        {
            std::vector<std::filesystem::path> projects;
            std::ifstream input(EditorConfigDirectory() / "imported_projects.txt");
            if (!input.is_open())
                return projects;
            std::string line;
            while (std::getline(input, line))
            {
                line = Trim(line);
                if (!line.empty())
                    projects.emplace_back(line);
            }
            return projects;
        }

        void SaveImportedProjects(
            const std::vector<std::filesystem::path>& projects)
        {
            std::ofstream output(EditorConfigDirectory() / "imported_projects.txt",
                std::ios::trunc);
            for (const auto& project : projects)
                output << project.generic_string() << "\n";
        }

        bool IsProjectDirectory(const std::filesystem::path& path)
        {
            std::error_code error;
            if (std::filesystem::is_regular_file(
                    path / "WheatearProject.wtproject", error))
                return true;

            // A bare assets/shaders folder is the portable editor itself, not
            // a game project. Require actual project content or scenes.
            return std::filesystem::is_directory(path / "assets" / "scenes", error)
                || std::filesystem::is_directory(path / "assets" / "vn", error)
                || std::filesystem::is_directory(path / "assets" / "gameplay", error);
        }

        std::vector<std::string> LoadProjectOrder()
        {
            std::vector<std::string> order;
            std::ifstream input(EditorConfigDirectory() / "project_order.txt");
            if (!input.is_open())
                return order;
            std::string line;
            while (std::getline(input, line))
            {
                line = Trim(line);
                if (!line.empty())
                    order.push_back(line);
            }
            return order;
        }

        void SaveProjectOrderToDisk(const std::vector<std::string>& order)
        {
            std::ofstream output(EditorConfigDirectory() / "project_order.txt",
                std::ios::trunc);
            for (const std::string& path : order)
                output << path << "\n";
        }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool ContainsNoCase(const std::string& haystack, const std::string& needle)
        {
            if (needle.empty())
                return true;
            return ToLowerAscii(haystack).find(ToLowerAscii(needle))
                != std::string::npos;
        }

        std::filesystem::path ProjectThumbnailPath(
            const std::filesystem::path& projectRoot)
        {
            return projectRoot / "Saved" / "Thumbnails" / "project_thumbnail.png";
        }

        // Every template ships with a small flow-controller script so scene
        // transitions/restarts are testable immediately, not just an empty
        // script slot. The entity is appended to the authored scene YAML.
        std::string WithTemplateFlowController(std::string scene)
        {
            if (scene.find("EventScriptComponent") != std::string::npos)
                return scene;
            scene += R"(
  - Entity: 1990000001
    TagComponent:
      Tag: Flow Controller
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    EventScriptComponent:
      ScriptPath: "assets/events/start_flow.wts"
      StartEvent: "on_start"
      RunOnStart: false
      RunOnce: false
      Enabled: true
)";
            return scene;
        }

        const char* const kTemplateFlowScript = R"(# Template flow controller.
# Attach UI/input events to these entries from the editor's Event Script tool.
event on_start:
end

event restart:
    scene:assets/scenes/Start.wt
end

event quit:
    quit
end
)";

        bool IsFavorite(const std::vector<std::string>& favorites,
            const std::filesystem::path& project)
        {
            const std::string key = project.generic_string();
            return std::find(favorites.begin(), favorites.end(), key)
                != favorites.end();
        }

        bool WriteFileText(const std::filesystem::path& path, const std::string& text)
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
                return false;
            output << text;
            return output.good();
        }

        void CopyContentTemplatesToProject(const std::filesystem::path& projectRoot)
        {
            const std::filesystem::path templateRoot =
                AssetPath::GetEngineRoot() / "ContentTemplates";
            if (!std::filesystem::is_directory(templateRoot))
                return;

            const char* const subdirectories[] = { "gameplay", "fonts", "input" };
            for (const char* sub : subdirectories)
            {
                std::error_code error;
                const std::filesystem::path source = templateRoot / sub;
                if (!std::filesystem::is_directory(source, error))
                    continue;

                const std::filesystem::path destination = projectRoot / "assets" / sub;
                std::filesystem::create_directories(destination.parent_path(), error);
                if (error)
                    continue;
                error.clear();
                std::filesystem::copy(source, destination,
                    std::filesystem::copy_options::recursive
                        | std::filesystem::copy_options::overwrite_existing,
                    error);
            }
        }

        // UE/Unity-style project skeleton. Game content stays under assets/
        // (Unity convention); generated caches, saves and binaries live in
        // Intermediate/, Saved/ and Binaries/ so asset folders stay clean.
        void EnsureProjectLayout(const std::filesystem::path& projectRoot)
        {
            std::error_code error;
            const char* const directories[] = {
                "assets",
                "Config",
                "Saved",
                "Saved/Logs",
                "Saved/Config",
                "Saved/Thumbnails",
                "Intermediate",
                "Intermediate/Cache",
                "Intermediate/Cooked",
                "Binaries",
                "Binaries/Win64"
            };
            for (const char* directory : directories)
                std::filesystem::create_directories(projectRoot / directory, error);
        }

        ImU32 ColorU32(float r, float g, float b, float a)
        {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
        }

        void DrawBadge(ImDrawList* drawList, ImVec2 position,
            const char* label, bool threeD)
        {
            const float s = ImGui::GetIO().FontGlobalScale;
            const ImVec2 textSize = ImGui::CalcTextSize(label);
            const ImVec2 size(textSize.x + 14.0f * s, textSize.y + 4.0f * s);
            const ImU32 accent = threeD
                ? ColorU32(0.400f, 0.760f, 0.560f, 1.0f)
                : ColorU32(0.420f, 0.650f, 0.900f, 1.0f);
            drawList->AddRectFilled(position,
                ImVec2(position.x + size.x, position.y + size.y),
                ColorU32(threeD ? 0.400f : 0.420f,
                    threeD ? 0.760f : 0.650f,
                    threeD ? 0.560f : 0.900f, 0.16f),
                size.y * 0.5f);
            drawList->AddText(
                ImVec2(position.x + 7.0f * s, position.y + 2.0f * s),
                accent, label);
        }

        bool IsProjectWorkspace3D(const std::filesystem::path& projectRoot)
        {
            const RuntimePlayerConfig config = LoadRuntimePlayerConfig(
                FindRuntimePlayerConfigPath(projectRoot));
            return config.Workspace == "3D";
        }

    } // namespace

    ModeSelectLayer::ModeSelectLayer()
        : Layer("ModeSelectLayer")
    {
    }

    ModeSelectLayer::ModeSelectLayer(EditorWorkspace startWorkspace)
        : Layer("ModeSelectLayer")
        , m_Workspace(startWorkspace)
        , m_AutoLaunch(true)
    {
        for (const EditorTemplate& editorTemplate : GetEditorTemplates())
        {
            if (editorTemplate.Workspace == startWorkspace)
            {
                m_SelectedTemplateId = editorTemplate.Id;
                break;
            }
        }
        m_SelectedProject = AssetPath::GetProjectRoot();
    }

    void ModeSelectLayer::OnAttach()
    {
        Application::Get().GetWindow().SetDecorated(false);
        Application::Get().GetWindow().MaximizeWindow();
        for (const EditorTemplate& editorTemplate : GetEditorTemplates())
            GetIcon(editorTemplate.IconPath);
        m_LastProject = ReadLastProject();
        m_FavoriteProjects = LoadFavorites();
        m_ProjectOrder = LoadProjectOrder();
        m_ImportedProjects = LoadImportedProjects();
    }

    Ref<Texture2D> ModeSelectLayer::GetProjectThumbnail(
        const std::filesystem::path& projectRoot)
    {
        std::filesystem::path path = ProjectThumbnailPath(projectRoot);
        std::error_code error;
        if (!std::filesystem::exists(path, error))
        {
            const std::filesystem::path legacy =
                projectRoot / "assets" / ".wheatear" / "project_thumbnail.png";
            if (std::filesystem::exists(legacy, error))
                path = legacy;
            else
                return {};
        }

        const std::string key = path.generic_string();
        auto it = m_ProjectThumbnails.find(key);
        if (it != m_ProjectThumbnails.end())
            return it->second;

        Ref<Texture2D> thumbnail = Texture2D::Create(key);
        if (thumbnail && thumbnail->IsLoaded())
            m_ProjectThumbnails[key] = thumbnail;
        return thumbnail;
    }

    Ref<Texture2D> ModeSelectLayer::GetIcon(const std::string& path)
    {
        auto it = m_Icons.find(path);
        if (it != m_Icons.end())
            return it->second;

        Ref<Texture2D> icon = Texture2D::Create(path);
        m_Icons[path] = icon;
        return icon;
    }

    void ModeSelectLayer::DrawTitleBar()
    {
        const float s = ImGui::GetIO().FontGlobalScale;
        const float titleHeight = 38.0f * s;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, titleHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.040f, 0.047f, 0.060f, 1.0f));
        ImGui::Begin("##LauncherTitleBar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + ImGui::GetWindowWidth(),
            min.y + ImGui::GetWindowHeight());
        const float centerY = min.y + titleHeight * 0.5f;
        Window& nativeWindow = Application::Get().GetWindow();

        ImGui::SetCursorScreenPos(min);
        ImGui::InvisibleButton("##LauncherTitleDrag",
            ImVec2(ImGui::GetWindowWidth() - 144.0f * s, titleHeight));
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            nativeWindow.ToggleMaximizeWindow();
        else if (ImGui::IsItemActivated())
            nativeWindow.BeginWindowDrag();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, IM_COL32(10, 12, 17, 255));
        drawList->AddLine(ImVec2(min.x, max.y - 1.0f),
            ImVec2(max.x, max.y - 1.0f), IM_COL32(43, 48, 60, 255), 1.0f);

        EditorWidgets::BrandMark(ImVec2(min.x + 22.0f * s, centerY),
            23.0f * s, false);
        drawList->AddText(ImGui::GetFont(), 17.0f * s,
            ImVec2(min.x + 40.0f * s, centerY - 10.0f * s),
            IM_COL32(205, 210, 220, 255), "Wheatear");
        const float brandWidth = ImGui::CalcTextSize("Wheatear").x
            * (17.0f / 16.0f);
        const float studioWidth = ImGui::CalcTextSize("STUDIO").x
            * (13.0f / 16.0f);
        drawList->AddText(ImGui::GetFont(), 13.0f * s,
            ImVec2(min.x + 40.0f * s + brandWidth + 7.0f * s,
                centerY - 8.0f * s),
            IM_COL32(217, 170, 95, 220), "STUDIO");
        drawList->AddText(ImGui::GetFont(), 13.0f * s,
            ImVec2(std::max(min.x + 178.0f * s,
                    min.x + 40.0f * s + brandWidth + studioWidth + 22.0f * s),
                centerY - 8.0f * s),
            IM_COL32(130, 136, 150, 255), "PROJECT LAUNCHER");

        auto windowButton = [&](const char* id, float x, bool closeButton,
            bool maximizeButton)
        {
            ImGui::SetCursorScreenPos(ImVec2(x, min.y));
            const bool clicked = ImGui::InvisibleButton(id,
                ImVec2(46.0f * s, titleHeight));
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 bmin(x, min.y);
            const ImVec2 bmax(x + 46.0f * s, max.y - 1.0f);
            if (hovered)
            {
                drawList->AddRectFilled(bmin, bmax,
                    closeButton ? IM_COL32(165, 45, 45, 255)
                                : IM_COL32(40, 44, 56, 255));
            }
            const ImVec2 c(x + 23.0f * s, centerY);
            const ImU32 ink = hovered ? IM_COL32(245, 245, 248, 255)
                                      : IM_COL32(175, 181, 194, 255);
            const float glyph = 5.0f * s;
            if (closeButton)
            {
                drawList->AddLine(ImVec2(c.x - glyph, c.y - glyph),
                    ImVec2(c.x + glyph, c.y + glyph), ink, 1.4f);
                drawList->AddLine(ImVec2(c.x + glyph, c.y - glyph),
                    ImVec2(c.x - glyph, c.y + glyph), ink, 1.4f);
            }
            else if (maximizeButton)
            {
                drawList->AddRect(ImVec2(c.x - glyph, c.y - glyph),
                    ImVec2(c.x + glyph, c.y + glyph), ink, 1.0f, 0, 1.2f);
            }
            else
            {
                drawList->AddLine(ImVec2(c.x - 6.0f * s, c.y),
                    ImVec2(c.x + 6.0f * s, c.y), ink, 1.4f);
            }

            if (clicked)
            {
                if (closeButton)
                    nativeWindow.CloseWindow();
                else if (maximizeButton)
                    nativeWindow.ToggleMaximizeWindow();
                else
                    nativeWindow.MinimizeWindow();
            }
        };
        windowButton("##LauncherMinimize", max.x - 138.0f * s, false, false);
        windowButton("##LauncherMaximize", max.x - 92.0f * s, false, true);
        windowButton("##LauncherClose", max.x - 46.0f * s, true, false);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void ModeSelectLayer::OnImGuiRender()
    {
        if (m_Decided)
            return;

        if (m_AutoLaunch)
        {
            if (m_Workspace == EditorWorkspace::ThreeD)
                LaunchEditor3D();
            else
                LaunchEditor2D();
            return;
        }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        // The launcher is laid out in a 1920x1080 design space and scales all
        // glyphs, paddings, cards and icons with the window. This keeps the
        // content filling the screen instead of leaving a small card on black.
        const float launcherScale = std::clamp(std::min(
            viewport->Size.x / 1920.0f,
            viewport->Size.y / 1080.0f), 0.80f, 1.50f);
        if (ImGuiLayer* gui = Application::Get().GetImGuiLayer())
        {
            // Counteract the editor's 1.3 text baseline: the launcher scales
            // its whole design space directly with the window.
            gui->SetUiScaleTransient(launcherScale / 1.43f);
        }
        const float uiScale = ImGui::GetIO().FontGlobalScale;
        const float margin = 32.0f * uiScale;

        DrawTitleBar();
        const float titleBarHeight = 38.0f * uiScale;
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x,
            viewport->Pos.y + titleBarHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x,
            viewport->Size.y - titleBarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.050f, 0.055f, 0.068f, 1.0f));
        ImGui::Begin("##ModeSelectBg", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 min = ImGui::GetWindowPos();
            const ImVec2 max(min.x + ImGui::GetWindowWidth(),
                min.y + ImGui::GetWindowHeight());
            drawList->AddRectFilledMultiColor(min, max,
                IM_COL32(17, 21, 30, 255), IM_COL32(14, 18, 27, 255),
                IM_COL32(4, 6, 10, 255), IM_COL32(5, 7, 12, 255));

            // Soft wheat/teal atmosphere behind the content card.
            const ImVec2 wheatCenter(min.x + ImGui::GetWindowWidth() * 0.30f,
                min.y + ImGui::GetWindowHeight() * 0.28f);
            const float wheatRadius = ImGui::GetWindowWidth() * 0.34f;
            for (int i = 5; i >= 1; --i)
            {
                drawList->AddCircleFilled(wheatCenter,
                    wheatRadius * i / 5.0f,
                    IM_COL32(217, 170, 95, 3 + (5 - i) * 2), 96);
            }
            drawList->AddCircleFilled(
                ImVec2(min.x + ImGui::GetWindowWidth() * 0.78f,
                    min.y + ImGui::GetWindowHeight() * 0.74f),
                ImGui::GetWindowWidth() * 0.30f,
                IM_COL32(59, 120, 150, 12), 96);

            // Faint engineering grid keeps the empty area intentional.
            constexpr float step = 72.0f;
            for (float x = min.x; x < max.x; x += step)
                drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y),
                    IM_COL32(120, 140, 160, 6));
            for (float y = min.y; y < max.y; y += step)
                drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y),
                    IM_COL32(120, 140, 160, 6));
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        const float width = viewport->Size.x - margin * 2.0f;
        const float height = viewport->Size.y - titleBarHeight - margin * 2.0f;
        ImGui::SetNextWindowPos(
            ImVec2(viewport->Pos.x + margin,
                viewport->Pos.y + titleBarHeight + margin),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f * uiScale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
            ImVec2(30.0f * uiScale, 24.0f * uiScale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
            ImVec2(12.0f * uiScale, 12.0f * uiScale));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.070f, 0.078f, 0.098f, 1.0f));
        ImGui::Begin("##ModeSelect", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::SetScrollY(0.0f);

        {
            ImDrawList* background = ImGui::GetWindowDrawList();
            const ImVec2 topLeft = ImGui::GetWindowPos();
            const ImVec2 bottomRight(
                topLeft.x + ImGui::GetWindowWidth(),
                topLeft.y + ImGui::GetWindowHeight());
            background->AddRectFilledMultiColor(topLeft, bottomRight,
                IM_COL32(21, 25, 35, 255), IM_COL32(21, 25, 35, 255),
                IM_COL32(8, 10, 15, 255), IM_COL32(8, 10, 15, 255));
            background->AddCircleFilled(
                ImVec2(topLeft.x + ImGui::GetWindowWidth() * 0.72f,
                    topLeft.y + 8.0f),
                ImGui::GetWindowWidth() * 0.34f,
                IM_COL32(217, 170, 95, 14), 64);
        }

        DrawHeader();
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f * uiScale));

        const float contentHeight = ImGui::GetContentRegionAvail().y - 8.0f * uiScale;
        if (ImGui::BeginTable("##LauncherColumns", 2,
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
        {
            ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch, 0.64f);
            ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch, 0.36f);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::BeginChild("##LauncherLeft", ImVec2(0.0f, contentHeight), false);
            const float leftWidth = ImGui::GetContentRegionAvail().x;
            DrawWorkspaceSelector(leftWidth);
            ImGui::Dummy(ImVec2(0.0f, 4.0f * uiScale));
            DrawTemplateGallery(leftWidth);
            ImGui::Dummy(ImVec2(0.0f, 4.0f * uiScale));
            DrawNewProjectPanel(leftWidth);
            DrawTemplateDetails();
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);
            ImGui::BeginChild("##LauncherRight", ImVec2(0.0f, contentHeight), false);
            DrawProjectList(ImGui::GetContentRegionAvail().x);
            ImGui::EndChild();
            ImGui::EndTable();
        }

        if (!m_ProjectMessage.empty())
        {
            ImGui::Dummy(ImVec2(0.0f, 2.0f * uiScale));
            ImGui::TextColored(ImVec4(0.50f, 0.86f, 0.70f, 1.0f),
                "%s", m_ProjectMessage.c_str());
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void ModeSelectLayer::DrawHeader()
    {
        const float s = ImGui::GetIO().FontGlobalScale;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float mark = 58.0f * s;
        EditorWidgets::BrandMark(
            ImVec2(cursor.x + mark * 0.5f, cursor.y + mark * 0.5f), mark, false);
        ImGui::Dummy(ImVec2(mark, mark));
        ImGui::SameLine();

        ImGuiLayer* layer = Application::Get().GetImGuiLayer();
        ImFont* heading = layer ? layer->GetHeadingFont() : nullptr;
        if (heading)
            ImGui::PushFont(heading);
        ImGui::TextUnformatted(EngineInfo::EditorName);
        if (heading)
            ImGui::PopFont();

        ImFont* small = layer ? layer->GetSmallFont() : nullptr;
        if (small)
            ImGui::PushFont(small);
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        ImGui::TextUnformatted(EditorLocale::Text(
            "2D / 3D game creation workspaces", "2D / 3D 游戏创作工作区"));
        ImGui::PopStyleColor();
        if (small)
            ImGui::PopFont();

        ImGui::SameLine(ImGui::GetWindowWidth() - 300.0f * s);
        ImGui::TextDisabled(EditorLocale::Text(
            "Pick a workspace and a template", "选择工作区与模板"));
    }

    void ModeSelectLayer::DrawWorkspaceCard(const char* id, const char* title,
        const char* subtitle, const char* iconPath, EditorWorkspace workspace,
        float width)
    {
        ImGui::PushID(id);
        const float s = ImGui::GetIO().FontGlobalScale;
        const ImVec2 size(width, 68.0f * s);
        const bool clicked = ImGui::InvisibleButton("##workspace", size);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const bool selected = m_Workspace == workspace;
        const bool hovered = ImGui::IsItemHovered();
        const bool threeD = workspace == EditorWorkspace::ThreeD;
        const ImU32 accent = threeD
            ? ColorU32(0.400f, 0.760f, 0.560f, 1.0f)
            : ColorU32(0.420f, 0.650f, 0.900f, 1.0f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max,
            selected ? ColorU32(threeD ? 0.16f : 0.14f,
                                 threeD ? 0.22f : 0.20f,
                                 threeD ? 0.20f : 0.28f, 1.0f)
                     : ColorU32(0.10f, 0.11f, 0.14f, 1.0f),
            10.0f * s);
        drawList->AddRect(min, max,
            selected ? accent
                     : (hovered ? ColorU32(0.35f, 0.38f, 0.46f, 1.0f)
                                : ColorU32(0.20f, 0.22f, 0.27f, 1.0f)),
            10.0f * s, 0, selected ? 2.0f * s : 1.0f * s);

        if (Ref<Texture2D> icon = GetIcon(iconPath))
        {
            drawList->AddImage(
                static_cast<ImTextureID>(
                    static_cast<uintptr_t>(icon->GetRendererID())),
                ImVec2(min.x + 16.0f * s, min.y + 20.0f * s),
                ImVec2(min.x + 44.0f * s, min.y + 48.0f * s),
                ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        }

        drawList->AddText(ImVec2(min.x + 56.0f * s, min.y + 12.0f * s),
            IM_COL32(235, 237, 242, 255), title);
        drawList->AddText(ImGui::GetFont(), 14.0f * s,
            ImVec2(min.x + 56.0f * s, min.y + 38.0f * s),
            ColorU32(0.55f, 0.58f, 0.65f, 1.0f), subtitle);

        if (selected)
        {
            drawList->AddCircleFilled(
                ImVec2(max.x - 18.0f * s, min.y + 18.0f * s),
                4.0f * s, accent, 16);
        }

        ImGui::PopID();
        if (clicked)
        {
            m_Workspace = workspace;
            for (const EditorTemplate& editorTemplate : GetEditorTemplates())
            {
                if (editorTemplate.Workspace == workspace)
                {
                    m_SelectedTemplateId = editorTemplate.Id;
                    break;
                }
            }
        }
    }

    void ModeSelectLayer::DrawWorkspaceSelector(float width)
    {
        ImGui::TextUnformatted(EditorLocale::Text("1  Workspace", "1  工作区"));
        ImGui::SameLine();
        ImGui::TextDisabled(EditorLocale::Text(
            "The editor shell and tools are shared; each workspace starts from its own templates.",
            "编辑器底层共享，各工作区从自己的模板开始。"));
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        const float gap = 10.0f * ImGui::GetIO().FontGlobalScale;
        const float cardWidth = (width - gap) * 0.5f;
        DrawWorkspaceCard("##Workspace2D", "2D Studio",
            "Sprites · UI · Physics · Animation",
            "Resources/Icons/Editor/ui_canvas.png", EditorWorkspace::TwoD,
            cardWidth);
        ImGui::SameLine(0.0f, gap);
        DrawWorkspaceCard("##Workspace3D", "3D Studio",
            "Mesh · PBR · Terrain · Animation",
            "Resources/Icons/Editor/box.png", EditorWorkspace::ThreeD,
            cardWidth);
    }

    void ModeSelectLayer::DrawTemplateCard(const EditorTemplate& editorTemplate,
        bool selected, float width)
    {
        ImGui::PushID(editorTemplate.Id.c_str());
        const float s = ImGui::GetIO().FontGlobalScale;
        const ImVec2 size(width, 136.0f * s);
        const bool clicked = ImGui::InvisibleButton("##template", size);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const bool hovered = ImGui::IsItemHovered();
        const bool threeD = editorTemplate.Workspace == EditorWorkspace::ThreeD;
        const ImU32 accent = threeD
            ? ColorU32(0.400f, 0.760f, 0.560f, 1.0f)
            : ColorU32(0.420f, 0.650f, 0.900f, 1.0f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max,
            selected ? ColorU32(0.13f, 0.15f, 0.19f, 1.0f)
                     : ColorU32(0.085f, 0.095f, 0.120f, 1.0f),
            9.0f * s);
        drawList->AddRect(min, max,
            selected ? accent
                     : (hovered ? ColorU32(0.34f, 0.37f, 0.45f, 1.0f)
                                : ColorU32(0.18f, 0.20f, 0.25f, 1.0f)),
            9.0f * s, 0, selected ? 2.0f * s : 1.0f * s);

        if (Ref<Texture2D> icon = GetIcon(editorTemplate.IconPath))
        {
            drawList->AddImage(
                static_cast<ImTextureID>(
                    static_cast<uintptr_t>(icon->GetRendererID())),
                ImVec2(min.x + 14.0f * s, min.y + 14.0f * s),
                ImVec2(min.x + 42.0f * s, min.y + 42.0f * s),
                ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        }

        drawList->AddText(ImVec2(min.x + 14.0f * s, min.y + 54.0f * s),
            IM_COL32(232, 234, 240, 255), editorTemplate.Name.c_str());
        drawList->AddText(ImGui::GetFont(), 14.0f * s,
            ImVec2(min.x + 14.0f * s, min.y + 78.0f * s),
            ColorU32(0.55f, 0.58f, 0.65f, 1.0f),
            editorTemplate.Subtitle.c_str());
        DrawBadge(drawList,
            ImVec2(min.x + 14.0f * s, max.y - 28.0f * s),
            WorkspaceLabel(editorTemplate.Workspace), threeD);

        ImGui::PopID();
        if (clicked)
        {
            m_SelectedTemplateId = editorTemplate.Id;
            m_Workspace = editorTemplate.Workspace;
        }
    }

    void ModeSelectLayer::DrawTemplateGallery(float width)
    {
        ImGui::TextUnformatted(EditorLocale::Text("2  Template", "2  模板"));
        ImGui::SameLine();
        ImGui::TextDisabled(EditorLocale::Text(
            "A template seeds the project scene; you can change everything afterwards.",
            "模板只决定项目初始场景，之后可以自由修改。"));
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        std::vector<const EditorTemplate*> visible;
        for (const EditorTemplate& editorTemplate : GetEditorTemplates())
        {
            if (editorTemplate.Workspace == m_Workspace)
                visible.push_back(&editorTemplate);
        }

        const float s = ImGui::GetIO().FontGlobalScale;
        const float gap = 8.0f * s;
        const float cardWidth = visible.empty() ? width
            : (width - gap * (visible.size() - 1)) / visible.size();
        for (size_t i = 0; i < visible.size(); ++i)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, gap);
            DrawTemplateCard(*visible[i],
                visible[i]->Id == m_SelectedTemplateId, cardWidth);
        }

        const EditorTemplate* selected = nullptr;
        for (const EditorTemplate& editorTemplate : GetEditorTemplates())
        {
            if (editorTemplate.Id == m_SelectedTemplateId)
            {
                selected = &editorTemplate;
                break;
            }
        }
        if (selected)
        {
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            ImGui::TextDisabled("%s", selected->Description.c_str());
        }
    }

    void ModeSelectLayer::DrawNewProjectPanel(float width)
    {
        const float s = ImGui::GetIO().FontGlobalScale;
        ImGui::TextUnformatted(EditorLocale::Text("3  New project", "3  新建项目"));
        ImGui::Dummy(ImVec2(0.0f, 2.0f * s));

        const float buttonWidth = 150.0f * s;
        ImGui::SetNextItemWidth(width - buttonWidth - 10.0f * s);
        ImGui::InputTextWithHint("##NewProjectName",
            EditorLocale::Text("Project name", "项目名称"),
            m_NewProjectName, sizeof(m_NewProjectName));
        ImGui::SameLine(0.0f, 10.0f * s);
        const char* createLabel = m_Workspace == EditorWorkspace::TwoD
            ? EditorLocale::Text("Create & Open 2D", "创建并打开 2D")
            : EditorLocale::Text("Create & Open 3D", "创建并打开 3D");
        if (ImGui::Button(createLabel, ImVec2(buttonWidth, 0.0f)))
        {
            const std::string name = Trim(m_NewProjectName);
            const EditorTemplate* selected = nullptr;
            for (const EditorTemplate& editorTemplate : GetEditorTemplates())
            {
                if (editorTemplate.Id == m_SelectedTemplateId)
                {
                    selected = &editorTemplate;
                    break;
                }
            }

            if (name.empty())
                m_ProjectMessage = EditorLocale::Text(
                    "Project name must not be empty.",
                    "项目名称不能为空。");
            else if (!selected)
                m_ProjectMessage = EditorLocale::Text(
                    "Select a template first.", "请先选择一个模板。");
            else if (CreateProjectFromTemplate(name, *selected))
            {
                if (selected->Workspace == EditorWorkspace::TwoD)
                    LaunchEditor2D();
                else
                    LaunchEditor3D();
            }
        }
        ImGui::TextDisabled("assets/scenes/Start.wt");
    }

    void ModeSelectLayer::DrawTemplateDetails()
    {
        const float s = ImGui::GetIO().FontGlobalScale;
        const float height = ImGui::GetContentRegionAvail().y - 4.0f * s;
        if (height < 120.0f * s)
            return;

        const EditorTemplate* selected = nullptr;
        for (const EditorTemplate& editorTemplate : GetEditorTemplates())
        {
            if (editorTemplate.Id == m_SelectedTemplateId)
            {
                selected = &editorTemplate;
                break;
            }
        }
        if (!selected)
            return;

        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * s);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
            ImVec2(16.0f * s, 14.0f * s));
        ImGui::BeginChild("##TemplateDetails", ImVec2(0.0f, height), true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + ImGui::GetWindowWidth(),
            min.y + ImGui::GetWindowHeight());

        if (Ref<Texture2D> icon = GetIcon(selected->IconPath))
        {
            const float iconSize = std::min(170.0f * s,
                std::max(90.0f * s, height * 0.48f));
            const ImVec2 iconMax(max.x - 20.0f * s, min.y + 18.0f * s + iconSize);
            drawList->AddImage(
                static_cast<ImTextureID>(
                    static_cast<uintptr_t>(icon->GetRendererID())),
                ImVec2(iconMax.x - iconSize, iconMax.y - iconSize),
                iconMax, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f),
                IM_COL32(255, 255, 255, 60));
        }

        ImGui::TextDisabled("%s", EditorLocale::Text("STARTER CONTENT", "模板内容"));
        ImGui::Text("%s", selected->Name.c_str());
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()
            + std::max(160.0f * s,
                ImGui::GetContentRegionAvail().x - 190.0f * s));
        ImGui::TextDisabled("%s", selected->Description.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::TextDisabled("%s", EditorLocale::Text("FILES", "文件"));
        ImGui::BulletText("assets/scenes/Start.wt");
        ImGui::BulletText("assets/events/start_flow.wts");
        ImGui::BulletText("TEMPLATE.md");

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void ModeSelectLayer::DrawProjectList(float width)
    {
        const float s = ImGui::GetIO().FontGlobalScale;
        ImGui::TextUnformatted(EditorLocale::Text("Open project", "打开项目"));
        ImGui::SameLine();
        ImGui::TextDisabled(EditorLocale::Text("recent & local", "本地项目"));
        ImGui::Dummy(ImVec2(0.0f, 5.0f * s));

        if (ImGui::Button(
                EditorLocale::Text("Open Project Folder...", "打开项目文件夹..."),
                ImVec2(width, 30.0f * s)))
        {
            const std::string folder = FileDialogs::OpenFolder(
                EditorLocale::Text("Select a Wheatear project folder",
                    "选择 Wheatear 项目文件夹"));
            if (!folder.empty())
                ImportProjectFolder(std::filesystem::path(folder));
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", EditorLocale::Text(
                "Open a project from anywhere; it is remembered as an imported project.",
                "从任意位置打开项目，并加入已导入项目列表。"));
        ImGui::Dummy(ImVec2(0.0f, 5.0f * s));

        ImGui::SetNextItemWidth(std::max(width - 36.0f * s, 80.0f * s));
        ImGui::InputTextWithHint("##ProjectSearch",
            EditorLocale::Text("Search projects...", "搜索项目..."),
            m_ProjectSearch, sizeof(m_ProjectSearch));
        ImGui::SameLine(0.0f, 4.0f * s);
        if (ImGui::Button("x", ImVec2(28.0f * s, 0.0f)))
            m_ProjectSearch[0] = '\0';
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", EditorLocale::Text(
                "Clear search", "清除搜索"));
        ImGui::Dummy(ImVec2(0.0f, 4.0f * s));

        RefreshProjectList();
        if (m_SelectedProject.empty() && !m_ProjectList.empty())
            m_SelectedProject = m_ProjectList.front();

        std::vector<std::filesystem::path> visibleProjects;
        visibleProjects.reserve(m_ProjectList.size());
        for (const std::filesystem::path& project : m_ProjectList)
        {
            if (ContainsNoCase(project.filename().string(), m_ProjectSearch)
                || ContainsNoCase(project.string(), m_ProjectSearch))
            {
                visibleProjects.push_back(project);
            }
        }

        const float buttonsHeight = 78.0f * s;
        ImGui::BeginChild("##ProjectList",
            ImVec2(width, ImGui::GetContentRegionAvail().y - buttonsHeight), true);
        if (m_ProjectList.empty())
        {
            EditorWidgets::EmptyState(
                EditorLocale::Text("No projects found", "未找到项目"),
                EditorLocale::Text(
                    "Create a project from a template on the left.",
                    "在左侧从模板创建一个项目。"));
        }
        else if (visibleProjects.empty())
        {
            EditorWidgets::EmptyState(
                EditorLocale::Text("No matching projects", "没有匹配的项目"),
                EditorLocale::Text(
                    "Clear the search field or try another name.",
                    "清除搜索条件或尝试其它名称。"));
        }
        for (const std::filesystem::path& project : visibleProjects)
        {
            const bool selected = m_SelectedProject == project;
            const bool threeD = IsProjectWorkspace3D(project);
            const bool favorite = IsFavorite(m_FavoriteProjects, project);
            ImGui::PushID(project.string().c_str());
            const ImVec2 size(ImGui::GetContentRegionAvail().x, 58.0f * s);
            const bool clicked = ImGui::InvisibleButton("##project", size);
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const bool hovered = ImGui::IsItemHovered();
            const bool active = ImGui::IsItemActive();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max,
                selected ? ColorU32(0.13f, 0.15f, 0.19f, 1.0f)
                         : (hovered ? ColorU32(0.10f, 0.11f, 0.14f, 1.0f)
                                    : ColorU32(0.075f, 0.085f, 0.105f, 1.0f)),
                8.0f * s);
            if (selected)
            {
                drawList->AddRect(min, max,
                    ColorU32(0.85f, 0.663f, 0.373f, 1.0f), 8.0f * s, 0,
                    1.5f * s);
            }
            if (active && hovered)
            {
                drawList->AddRect(min, max,
                    ColorU32(0.55f, 0.58f, 0.66f, 0.9f), 8.0f * s, 0,
                    1.0f * s);
            }

            // Real project capture when the editor has saved the project once;
            // otherwise fall back to the workspace glyph.
            const ImVec2 tileMin(min.x + 10.0f * s, min.y + 8.0f * s);
            const ImVec2 tileMax(tileMin.x + 42.0f * s,
                tileMin.y + 42.0f * s);
            drawList->AddRectFilled(tileMin, tileMax,
                threeD ? ColorU32(0.16f, 0.27f, 0.22f, 1.0f)
                       : ColorU32(0.14f, 0.20f, 0.30f, 1.0f), 7.0f * s);
            bool drewThumbnail = false;
            if (Ref<Texture2D> thumbnail = GetProjectThumbnail(project))
            {
                if (thumbnail->GetWidth() > 0 && thumbnail->GetHeight() > 0)
                {
                    const float imageAspect = static_cast<float>(thumbnail->GetWidth())
                        / static_cast<float>(thumbnail->GetHeight());
                    float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f;
                    if (imageAspect > 1.0f)
                    {
                        const float visibleWidth = 1.0f / imageAspect;
                        u0 = (1.0f - visibleWidth) * 0.5f;
                        u1 = u0 + visibleWidth;
                    }
                    else if (imageAspect < 1.0f)
                    {
                        const float visibleHeight = imageAspect;
                        v0 = (1.0f - visibleHeight) * 0.5f;
                        v1 = v0 + visibleHeight;
                    }
                    drawList->AddImageRounded(
                        static_cast<ImTextureID>(
                            static_cast<uintptr_t>(thumbnail->GetRendererID())),
                        tileMin, tileMax,
                        ImVec2(u0, v1), ImVec2(u1, v0),
                        IM_COL32_WHITE, 7.0f * s);
                    drewThumbnail = true;
                }
            }
            if (!drewThumbnail)
            {
                if (Ref<Texture2D> icon = GetIcon(threeD
                        ? "Resources/Icons/Editor/box.png"
                        : "Resources/Icons/Editor/ui_canvas.png"))
                {
                    drawList->AddImage(
                        static_cast<ImTextureID>(
                            static_cast<uintptr_t>(icon->GetRendererID())),
                        ImVec2(tileMin.x + 9.0f * s, tileMin.y + 9.0f * s),
                        ImVec2(tileMin.x + 33.0f * s, tileMin.y + 33.0f * s),
                        ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
                }
            }

            drawList->AddText(
                ImVec2(min.x + 60.0f * s, min.y + 9.0f * s),
                IM_COL32(232, 234, 240, 255),
                project.filename().string().c_str());
            drawList->PushClipRect(
                ImVec2(min.x + 60.0f * s, min.y + 30.0f * s),
                ImVec2(max.x - 96.0f * s, max.y), true);
            drawList->AddText(ImGui::GetFont(), 13.0f * s,
                ImVec2(min.x + 60.0f * s, min.y + 32.0f * s),
                ColorU32(0.48f, 0.51f, 0.58f, 1.0f),
                project.string().c_str());
            drawList->PopClipRect();

            const float starCenterX = max.x - 80.0f * s;
            const float starCenterY = min.y + 28.0f * s;
            ImVec2 starPoints[10];
            for (int i = 0; i < 10; ++i)
            {
                const float angle = -1.5708f + i * 0.6283185f;
                const float radius = (i % 2 == 0) ? 7.0f * s : 3.0f * s;
                starPoints[i] = ImVec2(starCenterX + std::cos(angle) * radius,
                    starCenterY + std::sin(angle) * radius);
            }
            drawList->AddConcavePolyFilled(starPoints, 10,
                favorite ? ColorU32(0.85f, 0.663f, 0.373f, 1.0f)
                         : ColorU32(0.34f, 0.37f, 0.43f, 1.0f));
            DrawBadge(drawList,
                ImVec2(max.x - 62.0f * s, min.y + 10.0f * s),
                WorkspaceLabel(threeD ? EditorWorkspace::ThreeD
                                      : EditorWorkspace::TwoD), threeD);

            // Drag to reorder; the order is stored beside favorites/recent.
            if (ImGui::BeginDragDropSource())
            {
                const std::string payload = project.generic_string();
                ImGui::SetDragDropPayload("PROJECT_ORDER", payload.c_str(),
                    payload.size() + 1);
                ImGui::TextUnformatted(project.filename().string().c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PROJECT_ORDER"))
                {
                    std::string moved(
                        static_cast<const char*>(payload->Data), payload->DataSize);
                    if (!moved.empty() && moved.back() == '\0')
                        moved.pop_back();
                    if (!moved.empty() && std::filesystem::path(moved) != project)
                        ReorderProject(std::filesystem::path(moved), project);
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SetCursorScreenPos(ImVec2(starCenterX - 10.0f * s,
                starCenterY - 10.0f * s));
            if (ImGui::InvisibleButton("##favorite",
                    ImVec2(20.0f * s, 20.0f * s)))
            {
                if (favorite)
                {
                    m_FavoriteProjects.erase(std::remove(
                        m_FavoriteProjects.begin(), m_FavoriteProjects.end(),
                        project.generic_string()), m_FavoriteProjects.end());
                }
                else
                {
                    m_FavoriteProjects.push_back(project.generic_string());
                }
                SaveFavorites(m_FavoriteProjects);
                m_ProjectListDirty = true;
            }
            ImGui::PopID();
            if (clicked)
            {
                m_SelectedProject = project;
                m_Workspace = threeD ? EditorWorkspace::ThreeD
                                     : EditorWorkspace::TwoD;
                for (const EditorTemplate& editorTemplate : GetEditorTemplates())
                {
                    if (editorTemplate.Workspace == m_Workspace)
                    {
                        m_SelectedTemplateId = editorTemplate.Id;
                        break;
                    }
                }
                ApplyProject(project);
            }
        }

        if (!visibleProjects.empty() && visibleProjects.size() <= 2)
        {
            // Fill the otherwise empty list body with a restrained empty-state
            // mark instead of leaving a large blank panel.
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const float remaining = ImGui::GetContentRegionAvail().y;
            if (remaining > 150.0f * s)
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const float centerX = ImGui::GetWindowPos().x
                    + ImGui::GetWindowWidth() * 0.5f;
                const float iconSize = 92.0f * s;
                const ImVec2 iconCenter(centerX,
                    cursor.y + remaining * 0.30f);
                if (Ref<Texture2D> icon = GetIcon(
                        "Resources/Icons/Editor/folder.png"))
                {
                    drawList->AddImage(
                        static_cast<ImTextureID>(
                            static_cast<uintptr_t>(icon->GetRendererID())),
                        ImVec2(iconCenter.x - iconSize * 0.5f,
                            iconCenter.y - iconSize * 0.5f),
                        ImVec2(iconCenter.x + iconSize * 0.5f,
                            iconCenter.y + iconSize * 0.5f),
                        ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f),
                        IM_COL32(255, 255, 255, 38));
                }

                const char* hint = EditorLocale::Text(
                    "More projects will appear here",
                    "更多项目会显示在这里");
                const char* hint2 = EditorLocale::Text(
                    "Create one from a template on the left",
                    "可在左侧从模板创建新项目");
                const ImVec2 hintSize = ImGui::CalcTextSize(hint);
                const ImVec2 hint2Size = ImGui::CalcTextSize(hint2);
                drawList->AddText(
                    ImVec2(centerX - hintSize.x * 0.5f,
                        iconCenter.y + iconSize * 0.6f),
                    ImGui::ColorConvertFloat4ToU32(
                        ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)),
                    hint);
                drawList->AddText(
                    ImVec2(centerX - hint2Size.x * 0.5f,
                        iconCenter.y + iconSize * 0.6f
                        + ImGui::GetTextLineHeight() + 4.0f * s),
                    ImGui::ColorConvertFloat4ToU32(
                        ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)),
                    hint2);
            }
        }
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0.0f, 2.0f * s));
        const float half = (width - 8.0f * s) * 0.5f;
        ImGui::BeginDisabled(m_SelectedProject.empty());
        if (ImGui::Button(EditorLocale::Text("Open 2D Studio", "打开 2D 工作区"),
                ImVec2(half, 34.0f * s)))
        {
            if (!m_SelectedProject.empty())
                ApplyProject(m_SelectedProject);
            LaunchEditor2D();
        }
        ImGui::SameLine(0.0f, 8.0f * s);
        if (ImGui::Button(EditorLocale::Text("Open 3D Studio", "打开 3D 工作区"),
                ImVec2(half, 34.0f * s)))
        {
            if (!m_SelectedProject.empty())
                ApplyProject(m_SelectedProject);
            LaunchEditor3D();
        }
        ImGui::EndDisabled();
        if (m_SelectedProject.empty())
            ImGui::TextDisabled("%s", EditorLocale::Text(
                "Open or create a project first", "请先打开或创建项目"));
    }

    void ModeSelectLayer::ReorderProject(const std::filesystem::path& moved,
        const std::filesystem::path& target)
    {
        std::vector<std::filesystem::path> current = m_ProjectList;
        current.erase(std::remove(current.begin(), current.end(), moved),
            current.end());
        auto targetIt = std::find(current.begin(), current.end(), target);
        if (targetIt == current.end())
            current.push_back(moved);
        else
            current.insert(targetIt, moved);

        m_ProjectOrder.clear();
        m_ProjectOrder.reserve(current.size());
        for (const std::filesystem::path& project : current)
            m_ProjectOrder.push_back(project.generic_string());
        SaveProjectOrder();
        m_ProjectListDirty = true;
        m_ProjectMessage = EditorLocale::Text(
            "Project order updated.", "项目顺序已更新。");
    }

    void ModeSelectLayer::SaveProjectOrder() const
    {
        SaveProjectOrderToDisk(m_ProjectOrder);
    }

    bool ModeSelectLayer::ImportProjectFolder(
        const std::filesystem::path& folder)
    {
        if (folder.empty())
            return false;

        std::filesystem::path projectRoot = folder;
        if (!IsProjectDirectory(projectRoot))
        {
            // Allow choosing a parent folder that contains exactly one project.
            std::filesystem::path candidate;
            int matches = 0;
            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(folder, error))
            {
                if (error || !entry.is_directory())
                    continue;
                if (IsProjectDirectory(entry.path()))
                {
                    candidate = entry.path();
                    ++matches;
                }
            }
            if (matches != 1)
            {
                m_ProjectMessage = EditorLocale::Text(
                    "Not a Wheatear project (missing assets/ or WheatearProject.wtproject).",
                    "不是 Wheatear 项目（缺少 assets/ 或 WheatearProject.wtproject）。");
                return false;
            }
            projectRoot = candidate;
        }

        projectRoot = std::filesystem::absolute(projectRoot).lexically_normal();
        if (!ApplyProject(projectRoot))
            return false;

        if (std::find(m_ImportedProjects.begin(), m_ImportedProjects.end(),
                projectRoot) == m_ImportedProjects.end())
        {
            m_ImportedProjects.push_back(projectRoot);
            SaveImportedProjects(m_ImportedProjects);
        }
        m_ProjectListDirty = true;
        return true;
    }

    void ModeSelectLayer::RefreshProjectList()
    {
        if (!m_ProjectListDirty)
            return;

        m_ProjectList.clear();
        const std::filesystem::path projectsRoot = ProjectsRoot();
        std::error_code error;
        if (std::filesystem::is_directory(projectsRoot, error))
        {
            for (const auto& entry : std::filesystem::directory_iterator(projectsRoot, error))
            {
                if (!error && entry.is_directory() &&
                    std::filesystem::is_directory(entry.path() / "assets"))
                {
                    m_ProjectList.push_back(entry.path());
                }
            }
        }

        // Projects opened from outside Projects/ (imported folders) remain in
        // the launcher without copying their content into the repository.
        for (const auto& imported : m_ImportedProjects)
        {
            if (!IsProjectDirectory(imported))
                continue;
            if (std::find(m_ProjectList.begin(), m_ProjectList.end(), imported)
                == m_ProjectList.end())
            {
                m_ProjectList.push_back(imported);
            }
        }

        // A portable editor may be launched away from the repository. Keep
        // the active/last project visible even if it is outside Projects/.
        for (const auto& candidate : { AssetPath::GetProjectRoot(), m_LastProject })
        {
            if (candidate.empty() || !IsProjectDirectory(candidate))
                continue;
            if (std::find(m_ProjectList.begin(), m_ProjectList.end(), candidate)
                == m_ProjectList.end())
            {
                m_ProjectList.push_back(candidate);
            }
        }
        auto orderIndex = [this](const std::filesystem::path& project)
        {
            const std::string key = project.generic_string();
            const auto it = std::find(m_ProjectOrder.begin(), m_ProjectOrder.end(), key);
            return it == m_ProjectOrder.end()
                ? std::numeric_limits<size_t>::max()
                : static_cast<size_t>(std::distance(m_ProjectOrder.begin(), it));
        };
        std::sort(m_ProjectList.begin(), m_ProjectList.end(),
            [this, &orderIndex](const auto& left, const auto& right)
            {
                const bool leftFavorite = IsFavorite(m_FavoriteProjects, left);
                const bool rightFavorite = IsFavorite(m_FavoriteProjects, right);
                if (leftFavorite != rightFavorite)
                    return leftFavorite;
                const size_t leftOrder = orderIndex(left);
                const size_t rightOrder = orderIndex(right);
                if (leftOrder != rightOrder)
                    return leftOrder < rightOrder;
                const bool leftRecent = left == m_LastProject;
                const bool rightRecent = right == m_LastProject;
                if (leftRecent != rightRecent)
                    return leftRecent;
                return left.filename().string() < right.filename().string();
            });

        const std::filesystem::path current = AssetPath::GetProjectRoot();
        if (std::find(m_ProjectList.begin(), m_ProjectList.end(), current)
            != m_ProjectList.end())
        {
            m_SelectedProject = current;
        }
        m_ProjectListDirty = false;
    }

    bool ModeSelectLayer::CreateProjectFromTemplate(const std::string& name,
        const EditorTemplate& editorTemplate)
    {
        const std::filesystem::path projectRoot = ProjectsRoot() / name;
        std::error_code error;
        if (std::filesystem::exists(projectRoot, error))
        {
            m_ProjectMessage = "Project already exists: " + projectRoot.string();
            return false;
        }

        const std::filesystem::path scenePath =
            projectRoot / "assets" / "scenes" / "Start.wt";
        const bool created =
            std::filesystem::create_directories(scenePath.parent_path(), error)
            && WriteFileText(scenePath,
                WithTemplateFlowController(editorTemplate.StartupScene));
        if (!created)
        {
            m_ProjectMessage = "Failed to create project: " + projectRoot.string();
            return false;
        }

        const std::filesystem::path eventsDirectory =
            projectRoot / "assets" / "events";
        std::filesystem::create_directories(eventsDirectory, error);
        WriteFileText(eventsDirectory / "start_flow.wts", kTemplateFlowScript);

        const std::filesystem::path readmePath = projectRoot / "TEMPLATE.md";
        std::string readme;
        readme += "# " + editorTemplate.Name + "\n\n";
        readme += editorTemplate.Description + "\n\n";
        readme += "- Scene: `assets/scenes/Start.wt`\n";
        readme += "- Flow script: `assets/events/start_flow.wts`\n";
        readme += "- Workspace: " + std::string(WorkspaceLabel(editorTemplate.Workspace)) + "\n\n";
        readme += "Open the project in Wheatear Studio, then use Content Browser "
            "and Scene Hierarchy to add assets, lights, UI or gameplay components.\n";
        WriteFileText(readmePath, readme);

        CopyContentTemplatesToProject(projectRoot);
        EnsureProjectLayout(projectRoot);
        RuntimePlayerConfig projectConfig;
        projectConfig.ProjectName = name;
        projectConfig.ProjectVersion = "0.1.0";
        projectConfig.Description = editorTemplate.Description;
        projectConfig.StartupScene = "assets/scenes/Start.wt";
        projectConfig.Workspace =
            editorTemplate.Workspace == EditorWorkspace::ThreeD ? "3D" : "2D";
        projectConfig.PackageName = name;
        SaveProjectConfig(projectRoot, projectConfig, EngineInfo::EditorName);

        if (!ApplyProject(projectRoot))
            return false;
        m_ProjectMessage = "Created " + name + " (" +
            WorkspaceLabel(editorTemplate.Workspace) + ")";
        return true;
    }

    void ModeSelectLayer::LaunchEditor2D()
    {
        if (m_Decided)
            return;
        m_Decided = true;
        Application::Get().PushLayer(std::make_unique<EditorLayer2D>());
        Application::Get().PopLayer(this);
    }

    void ModeSelectLayer::LaunchEditor3D()
    {
        if (m_Decided)
            return;
        m_Decided = true;
        Application::Get().PushLayer(std::make_unique<EditorLayer3D>());
        Application::Get().PopLayer(this);
    }

    bool ModeSelectLayer::ApplyProject(const std::filesystem::path& projectRoot)
    {
        std::error_code error;
        const std::filesystem::path assetsDirectory = projectRoot / "assets";
        if (!std::filesystem::is_directory(assetsDirectory, error))
        {
            m_ProjectMessage = "Not a project: '" + projectRoot.string() + "' (missing assets/).";
            return false;
        }

        AssetPath::SetProjectRoot(projectRoot);
        EnsureProjectLayout(projectRoot);
        AssetRegistry::Get().Scan(projectRoot);
        AssetRegistry::Get().WriteRegistry();
        WriteLastProject(projectRoot);
        m_LastProject = projectRoot;
        m_SelectedProject = projectRoot;
        m_ProjectMessage = "Active project: " + projectRoot.string();
        m_ProjectListDirty = true;
        return true;
    }

} // namespace Wheatear
