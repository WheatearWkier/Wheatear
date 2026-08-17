#include "wepch.h"
#include "ModeSelectLayer.h"

#include "Assets/AssetRegistry.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Config/PlayerConfig.h"

#include "EditorLayer2D.h"
#include "EditorLayer3D.h"

#include <imgui/imgui.h>

#include <filesystem>
#include <cstdlib>
#include <fstream>

namespace Wheatear {

    namespace {

        constexpr const char* kProjectsDirectoryName = "Projects";

        // Projects live next to the engine repository (engine root's parent),
        // not inside the active project.
        std::filesystem::path ProjectsRoot()
        {
            return AssetPath::GetEngineRoot().parent_path() / kProjectsDirectoryName;
        }

        void WriteLastProject(const std::filesystem::path& projectRoot)
        {
            std::error_code error;
            std::filesystem::path configDirectory;
            if (const char* localAppData = std::getenv("LOCALAPPDATA"))
                configDirectory = std::filesystem::path(localAppData) / "Wheatear";
            else
                configDirectory = std::filesystem::temp_directory_path() / "Wheatear";
            std::filesystem::create_directories(configDirectory, error);
            if (error)
                return;
            std::ofstream output(configDirectory / "last_project.txt", std::ios::trunc);
            output << projectRoot.generic_string();
        }

        // Minimal scene template for newly created projects: an orthogonal
        // camera + a UI canvas, ready for 2D content, with a permissive save
        // policy so new games can save/load immediately.
        const char* kNewProjectSceneTemplate =
            "Scene: Start\n"
            "SavePolicy:\n"
            "  CanSave: true\n"
            "  CanLoad: true\n"
            "  SaveDirectory: assets/saves\n"
            "  AutoLoadSlot: 0\n"
            "Entities:\n"
            "  - Entity: 1000000001\n"
            "    TagComponent:\n"
            "      Tag: Main Camera\n"
            "    TransformComponent:\n"
            "      Translation: [0, 0, 10]\n"
            "      Rotation: [0, 0, 0]\n"
            "      Scale: [1, 1, 1]\n"
            "    CameraComponent:\n"
            "      Camera:\n"
            "        ProjectionType: 1\n"
            "        OrthographicSize: 5\n"
            "        OrthographicNear: -1\n"
            "        OrthographicFar: 100\n"
            "      Primary: true\n"
            "      FixedAspectRatio: false\n"
            "  - Entity: 1000000002\n"
            "    TagComponent:\n"
            "      Tag: WT_UI_Canvas\n"
            "    TransformComponent:\n"
            "      Translation: [0, 0, 0]\n"
            "      Rotation: [0, 0, 0]\n"
            "      Scale: [1, 1, 1]\n"
            "    UICanvasComponent:\n"
            "      Visible: true\n"
            "      ReferenceWidth: 1920\n"
            "      ReferenceHeight: 1080\n"
            "    UIWidgetComponent:\n"
            "      Visible: true\n"
            "      Position: [0, 0]\n"
            "      Size: [1, 1]\n"
            "      Rotation: 0\n"
            "      Anchor: 0\n"
            "      SortOrder: 0\n"
            "      ParentEntity: 0\n";

        bool WriteFileText(const std::filesystem::path& path, const std::string& text)
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
                return false;
            output << text;
            return output.good();
        }

        // Seed a freshly created project with the shared content templates
        // (gameplay data, fonts, input bindings) so the project is
        // self-contained from day one. The engine root keeps no live copy of
        // this content; ContentTemplates is a pure creation-time seed.
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

                std::filesystem::copy(
                    source,
                    destination,
                    std::filesystem::copy_options::recursive
                        | std::filesystem::copy_options::overwrite_existing,
                    error);
                if (error)
                    continue;
            }
        }

    } // namespace

    ModeSelectLayer::ModeSelectLayer()
        : Layer("ModeSelectLayer")
    {
    }

    void ModeSelectLayer::OnAttach()
    {
    }

    void ModeSelectLayer::OnImGuiRender()
    {
        if (m_Decided)
            return;

        // =====================================================================
        // =====================================================================

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            ImVec4(0.050f, 0.055f, 0.068f, 1.0f));

        ImGui::Begin(
            "##ModeSelectBg",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // =====================================================================
        // =====================================================================

        ImVec2 center = ImVec2(
            viewport->Pos.x + viewport->Size.x * 0.5f,
            viewport->Pos.y + viewport->Size.y * 0.5f
        );

        ImGui::SetNextWindowPos(
            center,
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f));

        ImGui::SetNextWindowSize(
            ImVec2(960, 600),
            ImGuiCond_Always);

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowRounding,
            12.0f);

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(32, 32));

        ImGui::PushStyleVar(
            ImGuiStyleVar_ItemSpacing,
            ImVec2(16, 20));

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            ImVec4(0.125f, 0.140f, 0.168f, 1.0f));

        ImGui::Begin(
            "##ModeSelect",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings);

        // =====================================================================
        // =====================================================================

        // Engine monogram: rounded teal tile with the initial, drawn in code so
        // the launcher needs no external asset and matches the editor theme.
        const float logoSize = 76.0f;
        const float availWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 logoOrigin = ImGui::GetCursorScreenPos();
        const ImVec2 logoMin(logoOrigin.x + (availWidth - logoSize) * 0.5f, logoOrigin.y);
        const ImVec2 logoMax(logoMin.x + logoSize, logoMin.y + logoSize);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            logoMin, logoMax,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.230f, 0.720f, 0.800f, 1.0f)),
            16.0f);
        ImGui::SetWindowFontScale(2.8f);
        const ImVec2 markSize = ImGui::CalcTextSize("W");
        drawList->AddText(
            ImVec2(logoMin.x + (logoSize - markSize.x) * 0.5f,
                logoMin.y + (logoSize - markSize.y) * 0.5f),
            IM_COL32(9, 11, 15, 255),
            "W");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, logoSize + 10.0f));

        ImGui::SetWindowFontScale(1.8f);

        const char* title = EngineInfo::EditorName;

        float titleWidth = ImGui::CalcTextSize(title).x;

        ImGui::SetCursorPosX(
            (ImGui::GetContentRegionAvail().x - titleWidth) * 0.5f);

        ImGui::PushStyleColor(
            ImGuiCol_Text,
            ImVec4(0.850f, 0.650f, 0.360f, 1.0f));

        ImGui::Text("%s", title);

        ImGui::PopStyleColor();

        ImGui::SetWindowFontScale(1.0f);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::Separator();

        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        ImGui::SetWindowFontScale(1.3f);

        ImGui::TextWrapped(
            "选择编辑模式启动：");

        ImGui::SetWindowFontScale(1.0f);

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        // =====================================================================
        // =====================================================================

        const ImVec2 buttonSize =
        {
            ImGui::GetContentRegionAvail().x,
            84.0f
        };

        // =====================================================================
        // =====================================================================

        ImGui::PushStyleVar(
            ImGuiStyleVar_FramePadding,
            ImVec2(12, 12));

        // 2D is the primary path: teal accent button.
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(0.150f, 0.420f, 0.480f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4(0.190f, 0.520f, 0.600f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonActive,
            ImVec4(0.120f, 0.340f, 0.400f, 1.0f));

        if (ImGui::Button(
            "2D 模式\n精灵 | 物理 | 动画",
            buttonSize))
        {
            LaunchEditor2D();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::Dummy(ImVec2(0.0f, 18.0f));

        // =====================================================================
        // =====================================================================

        ImGui::PushStyleVar(
            ImGuiStyleVar_FramePadding,
            ImVec2(12, 12));

        // 3D is the secondary path: neutral surface that brightens on hover.
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(0.170f, 0.190f, 0.230f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4(0.220f, 0.250f, 0.310f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonActive,
            ImVec4(0.130f, 0.150f, 0.190f, 1.0f));

        if (ImGui::Button(
            "3D 模式\n网格 | PBR / IBL | 光照",
            buttonSize))
        {
            LaunchEditor3D();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        // =====================================================================

        DrawProjectSection();

        // =====================================================================

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    // =========================================================================
    // =========================================================================

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

        // Switch the engine's project root (all asset path resolution follows)
        // and refresh the editor asset registry for the new project.
        AssetPath::SetProjectRoot(projectRoot);
        AssetRegistry::Get().Scan(projectRoot);
        AssetRegistry::Get().WriteRegistry();
        WriteLastProject(projectRoot);

        m_ProjectMessage = "Active project: " + AssetPath::GetProjectRoot().string();
        m_ProjectListDirty = true;
        return true;
    }

    void ModeSelectLayer::DrawProjectSection()
    {
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        ImGui::Text("项目");
        ImGui::TextDisabled("当前: %s", AssetPath::GetProjectRoot().string().c_str());
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // Projects discovered under <engine>/Projects (next to the engine
        // repository, independent of the active project).
        if (m_ProjectListDirty)
        {
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
            m_ProjectListDirty = false;
        }

        if (!m_ProjectList.empty())
        {
            for (const auto& project : m_ProjectList)
            {
                ImGui::PushID(project.string().c_str());
                if (ImGui::Button(project.filename().string().c_str(), ImVec2(240.0f, 0.0f)))
                    ApplyProject(project);
                ImGui::PopID();
            }
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
        }

        ImGui::Text("新建项目");
        ImGui::SetNextItemWidth(240.0f);
        ImGui::InputText("##NewProjectName", m_NewProjectName, sizeof(m_NewProjectName));
        ImGui::SameLine();
        if (ImGui::Button("创建", ImVec2(80.0f, 0.0f)))
        {
            std::string name = m_NewProjectName;
            name.erase(0, name.find_first_not_of(" \t\r\n"));
            name.erase(name.find_last_not_of(" \t\r\n") + 1);
            if (name.empty())
            {
                m_ProjectMessage = "Project name must not be empty.";
            }
            else
            {
                const std::filesystem::path projectRoot =
                    ProjectsRoot() / name;
                std::error_code error;
                if (std::filesystem::exists(projectRoot, error))
                {
                    m_ProjectMessage = "Project already exists: " + projectRoot.string();
                }
                else
                {
                    const std::filesystem::path scenePath =
                        projectRoot / "assets" / "scenes" / "Start.wt";
                    const bool created =
                        std::filesystem::create_directories(scenePath.parent_path(), error)
                        && WriteFileText(scenePath, kNewProjectSceneTemplate);
                    if (created)
                    {
                        CopyContentTemplatesToProject(projectRoot);
                        // New projects boot into their template scene until
                        // the designer changes it in Project Health.
                        RuntimePlayerConfig projectConfig;
                        projectConfig.StartupScene = "assets/scenes/Start.wt";
                        SaveRuntimePlayerConfig(
                            projectRoot / "assets" / "game" / "player.config",
                            projectConfig,
                            EngineInfo::EditorName);
                        if (ApplyProject(projectRoot))
                            m_ProjectMessage += " (created)";
                    }
                    else
                    {
                        m_ProjectMessage = "Failed to create project: " + projectRoot.string();
                    }
                }
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("assets/scenes/Start.wt");

        if (!m_ProjectMessage.empty())
        {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::TextColored(ImVec4(0.42f, 0.88f, 0.72f, 1.0f), "%s", m_ProjectMessage.c_str());
        }
    }

} // namespace Wheatear
