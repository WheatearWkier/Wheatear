#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Texture.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    std::filesystem::path GetEditorAssetPath();

    // 文件类型枚举，方便以后按类型扩展 icon 或过滤
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
        // ── 渲染子区域 ────────────────────────────────────
        void DrawToolbar();
        void DrawSidebar();
        void DrawFileGrid();
        void DrawInspector();
        void DrawStatusBar();

        // ── 工具函数 ──────────────────────────────────────
        void NavigateTo(const std::filesystem::path& path);
        void NavigateBack();
        void NavigateForward();

        AssetType      GetAssetType(const std::filesystem::path& path) const;
        Ref<Texture2D> GetIconForType(AssetType type) const;

        // 过滤后的目录条目 搜索用
        std::vector<std::filesystem::directory_entry> GetFilteredEntries() const;

    private:
        // ── 导航历史 ──────────────────────────────────────
        std::filesystem::path              m_CurrentDirectory;
        std::vector<std::filesystem::path> m_History;
        int                                m_HistoryIndex = -1;

        // ── 选中状态 ──────────────────────────────────────
        std::filesystem::path m_SelectedPath;

        // ── 搜索 ──────────────────────────────────────────
        char m_SearchBuffer[256] = {};

        // ── 显示设置 ──────────────────────────────────────
        float m_ThumbnailSize = 72.0f;
        float m_Padding = 8.0f;
        bool  m_ShowSidebar = true;
        bool  m_ShowInspector = true;

        // ── 图标 按 AssetType 映射，方便扩展 ────────────
        std::unordered_map<AssetType, Ref<Texture2D>> m_Icons;
    };

} // namespace Wheatear