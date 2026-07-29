#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
        using UIChildMap = std::unordered_map<uint32_t, std::vector<Entity>>;

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
            std::unordered_set<uint32_t>& drawn,
            bool& selectionVisible);
        void MarkUIDescendantsDrawn(Entity entity,
            const UIChildMap& childMap,
            std::unordered_set<uint32_t>& drawn,
            std::unordered_set<uint32_t>& visiting) const;
        std::vector<Entity> CollectUIChildrenRecursive(Entity entity,
            const UIChildMap& childMap) const;
        void DrawComponents(Entity entity);

    private:
        Ref<Scene> m_Context;
        Entity m_SelectionContext;
        bool m_ScrollToSelection = false;
        bool m_RenameRequested = false;
        char m_SearchBuffer[128] = {};
        bool m_ShowOnlyUI = false;
    };

} // namespace Wheatear
