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
        UITemplate,
        Material,
        Data,
        Metadata,
        AnimationClip,
    };

    class ContentBrowserPanel
    {
    public:
        ContentBrowserPanel();
        void OnImGuiRender();

        // UE-style content drawer: the panel folds into a floating bar at the
        // bottom of the editor instead of a docked window.
        void SetDrawerMode(bool on) { m_DrawerMode = on; }
        bool IsDrawerMode() const { return m_DrawerMode; }

        // Double-click routing for scene / prefab / UI-template assets; the
        // editor layer registers these so the browser never touches the scene.
        void SetOnOpenSceneCallback(std::function<void(const std::filesystem::path&)> callback)
        {
            m_OnOpenScene = std::move(callback);
        }
        void SetOnInstantiatePrefabCallback(std::function<void(const std::filesystem::path&)> callback)
        {
            m_OnInstantiatePrefab = std::move(callback);
        }
        void SetOnInstantiateUITemplateCallback(std::function<void(const std::filesystem::path&)> callback)
        {
            m_OnInstantiateUITemplate = std::move(callback);
        }

    private:
        void DrawPanelContent();
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
        void OpenEntry(const std::filesystem::path& path);
        void CommitRename(const std::filesystem::path& oldPath, const char* newName);

    private:
        std::filesystem::path              m_CurrentDirectory;
        std::vector<std::filesystem::path> m_History;
        int                                m_HistoryIndex = -1;

        std::filesystem::path m_SelectedPath;
        std::filesystem::path m_RenameTarget;
        char m_RenameBuffer[256] = {};
        std::filesystem::path m_ConfirmDeletePath;
        bool m_DrawerMode = false;

        char m_SearchBuffer[256] = {};

        float m_ThumbnailSize = 72.0f;
        float m_Padding = 8.0f;
        bool  m_ShowSidebar = true;
        bool  m_ShowInspector = true;
        std::string m_RegistryStatus;

        std::unordered_map<AssetType, Ref<Texture2D>> m_Icons;
        // path -> real texture preview for image assets (nullptr = failed load).
        std::unordered_map<std::string, Ref<Texture2D>> m_ThumbnailCache;

        std::function<void(const std::filesystem::path&)> m_OnOpenScene;
        std::function<void(const std::filesystem::path&)> m_OnInstantiatePrefab;
        std::function<void(const std::filesystem::path&)> m_OnInstantiateUITemplate;
    };

} // namespace Wheatear
