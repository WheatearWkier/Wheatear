#include "wtpch.h"
#include "EditorLayerBase.h"

#include "Wheatear/Scene/Components.h"
#include "Panels/AnimationEditorPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SpriteSheetPickerPanel.h"

namespace Wheatear {


    // =========================================================================

    EditorLayerBase::EditorLayerBase(const std::string& debugName)
        : Layer(debugName)
        , m_SceneHierarchyPanel(std::make_unique<SceneHierarchyPanel>())
        , m_ContentBrowserPanel(std::make_unique<ContentBrowserPanel>())
        , m_AnimationEditorPanel(std::make_unique<AnimationEditorPanel>())
        , m_SpriteSheetPickerPanel(std::make_unique<SpriteSheetPickerPanel>())
    {
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
