#include "wepch.h"
#include "EditorLayerBase.h"
#include "EditorRequests.h"
#include "Wheatear/Assets/AssetPath.h"

#include "Wheatear/Scene/Components.h"
#include "Panels/AnimationEditorPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/DataFileEditorPanel.h"
#include "Panels/SceneHierarchy/SceneHierarchyPanel.h"
#include "Panels/EditorHelpPanel.h"
#include "Panels/InputBindingsPanel.h"
#include "Panels/SpriteSheetPickerPanel.h"

#include <ImGuizmo.h>

namespace Wheatear {

    // =========================================================================
    // EditorRequests implementation (consumed in SyncPanels).

    namespace EditorRequests {

        static bool s_OpenInputBindingsRequested = false;
        static bool s_SelectSideCombatLevelRequested = false;
        static bool s_SelectEntityRequested = false;
        static UUID s_SelectEntityUUID = 0;

        void RequestOpenInputBindings()
        {
            s_OpenInputBindingsRequested = true;
        }

        bool ConsumeOpenInputBindingsRequest()
        {
            const bool value = s_OpenInputBindingsRequested;
            s_OpenInputBindingsRequested = false;
            return value;
        }

        void RequestSelectSideCombatLevelEntity()
        {
            s_SelectSideCombatLevelRequested = true;
        }

        bool ConsumeSelectSideCombatLevelEntityRequest()
        {
            const bool value = s_SelectSideCombatLevelRequested;
            s_SelectSideCombatLevelRequested = false;
            return value;
        }

        void RequestSelectEntity(UUID uuid)
        {
            s_SelectEntityUUID = uuid;
            s_SelectEntityRequested = true;
        }

        bool ConsumeSelectEntityRequest(UUID& uuid)
        {
            if (!s_SelectEntityRequested)
                return false;
            uuid = s_SelectEntityUUID;
            s_SelectEntityRequested = false;
            return true;
        }

    } // namespace EditorRequests

    // =========================================================================

    EditorLayerBase::EditorLayerBase(const std::string& debugName)
        : Layer(debugName)
        , m_SceneHierarchyPanel(std::make_unique<SceneHierarchyPanel>())
        , m_ContentBrowserPanel(std::make_unique<ContentBrowserPanel>())
        , m_AnimationEditorPanel(std::make_unique<AnimationEditorPanel>())
        , m_SpriteSheetPickerPanel(std::make_unique<SpriteSheetPickerPanel>())
        , m_DataFileEditorPanel(std::make_unique<DataFileEditorPanel>())
        , m_HelpPanel(std::make_unique<EditorHelpPanel>())
        , m_InputBindingsPanel(std::make_unique<InputBindingsPanel>())
        , m_ConsolePanel(std::make_unique<ConsolePanel>())
        , m_GizmoType(ImGuizmo::OPERATION::TRANSLATE)
    {
        m_SceneHierarchyPanel->SetEntityActivatedCallback(
            [this](Entity entity, bool additive) { ActivateHierarchyEntity(entity, additive); });

        m_ContentBrowserPanel->SetOnOpenSceneCallback(
            [this](const std::filesystem::path& path) { OpenScene(path); });
        m_ContentBrowserPanel->SetOnInstantiatePrefabCallback(
            [this](const std::filesystem::path& path) { InstantiatePrefab(path); });
        m_ContentBrowserPanel->SetOnInstantiateUITemplateCallback(
            [this](const std::filesystem::path& path) { InstantiateUITemplate(path); });
        m_ContentBrowserPanel->SetOnOpenSpriteSheetCallback(
            [this](const std::filesystem::path& path)
            {
                m_SpriteSheetPickerPanel->OpenSheet(
                    AssetPath::ToProjectRelative(path).generic_string());
            });
    }

    EditorLayerBase::~EditorLayerBase() = default;

    SceneHierarchyPanel& EditorLayerBase::GetHierarchyPanel()
    {
        return *m_SceneHierarchyPanel;
    }

    ContentBrowserPanel& EditorLayerBase::GetContentBrowserPanel()
    {
        return *m_ContentBrowserPanel;
    }

    // =========================================================================
    // =========================================================================

} // namespace Wheatear
