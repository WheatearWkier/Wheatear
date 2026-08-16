#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Assets/SpriteSheetAsset.h"
#include "Wheatear/Renderer/Texture.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    class Framebuffer;

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
        Mesh,
        SpriteSheet,
    };

    class ContentBrowserPanel
    {
    public:
        ContentBrowserPanel();
        void OnImGuiRender();

        // Frame-by-frame thumbnail rendering for non-image assets (scenes,
        // prefabs, UI templates, meshes). Called from the editor layer update.
        void OnUpdate();

        // UE-style content drawer: the panel folds into a floating bar at the
        // bottom of the editor instead of a docked window.
        void SetDrawerMode(bool on) { m_DrawerMode = on; }

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
        void SetOnOpenSpriteSheetCallback(std::function<void(const std::filesystem::path&)> callback)
        {
            m_OnOpenSpriteSheet = std::move(callback);
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

        // OS file import: multi-select dialog -> copy into the current
        // directory (unique names) -> registry rescan.
        void ImportAssetsIntoCurrentDirectory();
        void CreateNewSceneFile();
        void CreateNewDataFile();
        void CreateNewSpriteSheetFile();

        AssetType      GetAssetType(const std::filesystem::path& path) const;
        Ref<Texture2D> GetIconForType(AssetType type) const;

        std::vector<std::filesystem::directory_entry> GetFilteredEntries() const;
        void OpenEntry(const std::filesystem::path& path);
        void CommitRename(const std::filesystem::path& oldPath, const char* newName);
        bool RenderThumbnail(const std::string& key, AssetType type);

        // Unity-style inline cell strip shown below an expanded .wtsheet item.
        int  ComputeSheetStripHeight(const std::filesystem::path& path);
        void DrawSheetCellStrip(const std::filesystem::path& path);

    private:
        std::filesystem::path              m_CurrentDirectory;
        std::vector<std::filesystem::path> m_History;
        int                                m_HistoryIndex = -1;

        std::filesystem::path m_SelectedPath;
        std::filesystem::path m_RenameTarget;
        char m_RenameBuffer[256] = {};
        std::filesystem::path m_ConfirmDeletePath;
        bool m_DrawerMode = false;

        // Reveal highlight: asset requested via ContentBrowserRequests gets
        // selected and flashed in the grid for a couple of seconds.
        std::filesystem::path m_RevealHighlightPath;
        float m_RevealHighlightTimer = 0.0f;

        uint32_t m_AudioPreviewHandle = 0;
        std::string m_AudioPreviewPath;

        char m_SearchBuffer[256] = {};
        // UE-style type filter: bitmask of enabled AssetType bits; 0 = all.
        uint32_t m_TypeFilterMask = 0;

        float m_ThumbnailSize = 72.0f;
        float m_Padding = 14.0f;
        float m_SidebarWidth = 150.0f;
        float m_InspectorWidth = 190.0f;
        bool  m_ShowSidebar = true;
        bool  m_ShowInspector = true;
        std::string m_RegistryStatus;

        std::unordered_map<AssetType, Ref<Texture2D>> m_Icons;
        // path -> real texture preview for image assets (nullptr = failed load).
        std::unordered_map<std::string, Ref<Texture2D>> m_ThumbnailCache;
        // Pending render-thumbnail queue (scenes/prefabs/UI templates/meshes).
        std::vector<std::string> m_ThumbnailRenderQueue;
        std::unordered_set<std::string> m_ThumbnailQueued;
        std::unordered_map<std::string, AssetType> m_ThumbnailQueueTypes;
        Ref<Framebuffer> m_ThumbnailFramebuffer;

        // Currently expanded .wtsheet (full path); cells render inline below it.
        std::string m_ExpandedSheetPath;
        struct SheetCellCacheEntry
        {
            SpriteSheetData Data;
            Ref<Texture2D> Texture;
        };
        std::unordered_map<std::string, SheetCellCacheEntry> m_SheetCellCache;

        // Scanned-entry cache (dir + search filter keyed) so the grid does not
        // hit the file system every frame on large folders.
        mutable std::vector<std::filesystem::directory_entry> m_EntryCache;
        mutable std::filesystem::path m_EntryCacheDir;
        mutable std::string m_EntryCacheFilter;
        mutable std::chrono::steady_clock::time_point m_LastEntryScan;

        std::function<void(const std::filesystem::path&)> m_OnOpenScene;
        std::function<void(const std::filesystem::path&)> m_OnInstantiatePrefab;
        std::function<void(const std::filesystem::path&)> m_OnInstantiateUITemplate;
        std::function<void(const std::filesystem::path&)> m_OnOpenSpriteSheet;
    };

} // namespace Wheatear
