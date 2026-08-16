#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    class Scene;
    enum class UITemplateKind;

    class SceneHierarchyPanel
    {
    public:
        SceneHierarchyPanel() = default;
        explicit SceneHierarchyPanel(const Ref<Scene>& context);

        void SetContext(const Ref<Scene>& context);
        void OnImGuiRender();

        // Toggles the "Play (runtime copy)" notice in the Properties panel;
        // set by the editor layer when entering/leaving play mode.
        void SetRuntimeMode(bool runtimeMode) { m_RuntimeMode = runtimeMode; }

        Entity GetSelectedEntity() const { return m_SelectionContext; }
        void SetSelectedEntity(Entity entity);
        void SetEntityActivatedCallback(std::function<void(Entity)> callback);

        // Create-entity menu items shared by the hierarchy's own right-click
        // context menu and the viewport's right-click menu.
        void DrawCreateEntityPopupItems();

    private:
        using UIChildMap = std::unordered_map<uint32_t, std::vector<Entity>>;
        using FolderChildMap = std::unordered_map<uint32_t, std::vector<Entity>>;

        bool EntityPassesFilter(Entity entity) const;
        bool EntityOrDescendantPassesFilter(Entity entity,
            const UIChildMap& childMap,
            std::unordered_set<uint32_t>& visiting) const;
        bool IsUIDescendantOf(Entity parent,
            Entity candidate,
            const UIChildMap& childMap,
            std::unordered_set<uint32_t>& visiting) const;
        void DrawEntityNode(Entity entity,
            const UIChildMap& childMap,
            const FolderChildMap& folderChildren,
            std::unordered_set<uint32_t>& drawn,
            bool& selectionVisible);
        void MarkUIDescendantsDrawn(Entity entity,
            const UIChildMap& childMap,
            std::unordered_set<uint32_t>& drawn,
            std::unordered_set<uint32_t>& visiting) const;
        std::vector<Entity> CollectUIChildrenRecursive(Entity entity,
            const UIChildMap& childMap) const;
        Entity FindSingleUICanvas() const;
        UUID ResolveUIParentID(Entity entity) const;
        bool CanReparentUI(Entity child,
            Entity parent,
            const UIChildMap& childMap) const;
        void ReparentUIWithUndo(Entity child,
            Entity parent,
            const UIChildMap& childMap);

        // Editor folder grouping (non-UI entities only).
        bool IsFolder(Entity entity) const;
        bool CanReparentToFolder(Entity child, Entity folder) const;
        void ReparentToFolderWithUndo(Entity child, Entity folder);
        void RemoveFromFolderWithUndo(Entity child);
        void CreateFolderWithUndo(Entity parentFolder = {});

        void DrawSceneSettings();
        Entity CreateEntityWithUndo(const std::string& name,
            const std::function<void(Entity)>& configure);
        Entity CreateUITemplateWithUndo(UITemplateKind kind, UUID parentID);
        void DrawCreateUIMenuItems(UUID parentID, bool includeCanvas);
        void DrawComponents(Entity entity);
        bool IsEntityHiddenInEditor(Entity entity) const;
        bool HasHiddenEditorEntities() const;
        void SetEntityHiddenInEditor(Entity entity, bool hidden);
        void ShowAllHiddenEditorEntities();

    private:
        Ref<Scene> m_Context;
        Entity m_SelectionContext;
        std::function<void(Entity)> m_EntityActivatedCallback;
        bool m_RuntimeMode = false;
        bool m_ScrollToSelection = false;
        bool m_RenameRequested = false;
        char m_RenameBuffer[256] = {};
        char m_SearchBuffer[128] = {};
        char m_AddComponentSearch[128] = {};
        bool m_ShowOnlyUI = false;
        bool m_SceneSettingsEditing = false;
        bool m_SceneSettingsEditStartCanSave = true;
        bool m_SceneSettingsEditStartCanLoad = true;
        std::string m_SceneSettingsEditStartSaveDirectory;
        int m_SceneSettingsEditStartAutoLoadSlot = 0;
    };

} // namespace Wheatear
