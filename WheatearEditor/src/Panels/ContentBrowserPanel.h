#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Texture.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    std::filesystem::path GetEditorAssetPath();

    enum class AssetType
    {
        Unknown = 0,
        Directory,
        Scene,
        Texture,
        Shader,
        Audio,
        Script,
        Prefab,
        Material,
    };

    class ContentBrowserPanel
    {
    public:
        ContentBrowserPanel();
        void OnImGuiRender();

    private:
        void DrawToolbar();
        void DrawSidebar();
        void DrawFileGrid();
        void DrawInspector();
        void DrawStatusBar();

        void NavigateTo(const std::filesystem::path& path);
        void NavigateBack();
        void NavigateForward();

        AssetType      GetAssetType(const std::filesystem::path& path) const;
        Ref<Texture2D> GetIconForType(AssetType type) const;

        std::vector<std::filesystem::directory_entry> GetFilteredEntries() const;

    private:
        std::filesystem::path              m_CurrentDirectory;
        std::vector<std::filesystem::path> m_History;
        int                                m_HistoryIndex = -1;

        std::filesystem::path m_SelectedPath;

        char m_SearchBuffer[256] = {};

        float m_ThumbnailSize = 72.0f;
        float m_Padding = 8.0f;
        bool  m_ShowSidebar = true;
        bool  m_ShowInspector = true;

        std::unordered_map<AssetType, Ref<Texture2D>> m_Icons;
    };

} // namespace Wheatear
