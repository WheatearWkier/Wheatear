#pragma once

#include "Build/AssetDependencyScanner.h"
#include "Build/ProjectSourceScanner.h"
#include "Editor/EditorToolRegistry.h"

#include <string>

namespace Wheatear {

    class ProjectHealthPanel
    {
    public:
        void Open(const EditorToolContext& context);
        void OnImGuiRender();

    private:
        void Refresh();
        void DrawSummary() const;
        void DrawAssetRegistry() const;
        void DrawMissingReferences() const;
        void DrawSceneTransitions() const;
        void DrawSourceSync() const;
        void DrawAssetList(const char* tableId,
            const std::vector<std::filesystem::path>& assets,
            size_t maxRows = 500) const;

    private:
        bool m_Open = false;
        bool m_EnableScripts = false;
        bool m_IncludeUnusedAssets = true;
        std::string m_StartupScene;
        AssetDependencyReport m_Report;
        ProjectSourceReport m_SourceReport;
        std::string m_Status;
    };

} // namespace Wheatear
