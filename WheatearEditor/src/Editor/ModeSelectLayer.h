#pragma once

#include "Editor/EditorTemplates.h"
#include "Wheatear/Core/Layer.h"
#include "Wheatear/Renderer/Texture.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    // Launcher: choose a 2D/3D workspace + template, or open an existing
    // project. A project is a directory with an assets/ folder; the engine
    // resolves all asset paths against the active project root.
    class ModeSelectLayer : public Layer
    {
    public:
        ModeSelectLayer();
        explicit ModeSelectLayer(EditorWorkspace startWorkspace);
        ~ModeSelectLayer() override = default;

        void OnAttach()      override;
        void OnImGuiRender() override;

    private:
        void LaunchEditor2D();
        void LaunchEditor3D();

        bool ApplyProject(const std::filesystem::path& projectRoot);
        bool CreateProjectFromTemplate(const std::string& name,
            const EditorTemplate& editorTemplate);
        void RefreshProjectList();
        void ReorderProject(const std::filesystem::path& moved,
            const std::filesystem::path& target);
        void SaveProjectOrder() const;
        bool ImportProjectFolder(const std::filesystem::path& folder);

        void DrawTitleBar();
        void DrawHeader();
        void DrawWorkspaceSelector(float width);
        void DrawTemplateGallery(float width);
        void DrawNewProjectPanel(float width);
        void DrawTemplateDetails();
        void DrawProjectList(float width);
        void DrawWorkspaceCard(const char* id, const char* title,
            const char* subtitle, const char* iconPath,
            EditorWorkspace workspace, float width);
        void DrawTemplateCard(const EditorTemplate& editorTemplate,
            bool selected, float width);

        Ref<Texture2D> GetIcon(const std::string& path);
        Ref<Texture2D> GetProjectThumbnail(const std::filesystem::path& projectRoot);

        std::vector<std::filesystem::path> m_ProjectList;
        std::vector<std::filesystem::path> m_ImportedProjects;
        std::unordered_map<std::string, Ref<Texture2D>> m_Icons;
        std::unordered_map<std::string, Ref<Texture2D>> m_ProjectThumbnails;
        char m_NewProjectName[64] = {};
        char m_ProjectSearch[128] = {};
        std::string m_ProjectMessage;
        std::string m_SelectedTemplateId = "2d-blank";
        std::filesystem::path m_SelectedProject;
        std::filesystem::path m_LastProject;
        std::vector<std::string> m_FavoriteProjects;
        std::vector<std::string> m_ProjectOrder;
        EditorWorkspace m_Workspace = EditorWorkspace::TwoD;
        bool m_ProjectListDirty = true;
        bool m_Decided = false;
        bool m_AutoLaunch = false;
    };

} // namespace Wheatear
