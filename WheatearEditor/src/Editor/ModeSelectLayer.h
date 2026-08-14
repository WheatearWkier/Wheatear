#pragma once

#include "Wheatear/Core/Layer.h"

#include <string>
#include <vector>

namespace Wheatear {

    // Launcher: pick an editor mode (2D/3D) and a project directory before
    // entering the editor. A project is a directory with an assets/ folder;
    // the engine resolves all asset paths against the active project root.
    class ModeSelectLayer : public Layer
    {
    public:
        ModeSelectLayer();
        ~ModeSelectLayer() override = default;

        void OnAttach()      override;
        void OnImGuiRender() override;

    private:
        void LaunchEditor2D();
        void LaunchEditor3D();

        // Project management: switches the engine's project root and refreshes
        // the asset registry before the editor panels are constructed.
        bool ApplyProject(const std::filesystem::path& projectRoot);
        void DrawProjectSection();

        std::vector<std::filesystem::path> m_ProjectList;
        char m_NewProjectName[64] = {};
        std::string m_ProjectMessage;
        bool m_ProjectListDirty = true;
        bool m_Decided = false;
    };

} // namespace Wheatear
