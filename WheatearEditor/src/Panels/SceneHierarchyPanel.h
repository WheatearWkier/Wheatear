#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

    class SceneHierarchyPanel
    {
    public:
        SceneHierarchyPanel() = default;
        explicit SceneHierarchyPanel(const Ref<Scene>& context);

        void SetContext(const Ref<Scene>& context);
        void OnImGuiRender();

        Entity GetSelectedEntity() const { return m_SelectionContext; }
        void SetSelectedEntity(Entity entity);

    private:
        void DrawEntityNode(Entity entity);
        void DrawComponents(Entity entity);

    private:
        Ref<Scene> m_Context;
        Entity m_SelectionContext;
        bool m_RenameRequested = false;
    };

} // namespace Wheatear