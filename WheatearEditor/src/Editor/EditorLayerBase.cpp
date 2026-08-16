#include "wepch.h"
#include "EditorLayerBase.h"
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
            [this](Entity entity) { ActivateHierarchyEntity(entity); });

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
