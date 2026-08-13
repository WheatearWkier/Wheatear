#include "wtpch.h"
#include "SceneHierarchyPanel.h"

#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Editor/EditorComponentRegistry.h"
#include "Editor/EditorLocale.h"

#include <imgui/imgui.h>
#include <cstring>
#include <string>

namespace Wheatear {
    void SceneHierarchyPanel::DrawComponents(Entity entity)
    {
        if (entity.HasComponent<TagComponent>())
        {
            auto& tag = entity.GetComponent<TagComponent>().Tag;

            char buffer[256] = {};
            std::strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);

            if (m_RenameRequested)
            {
                ImGui::SetKeyboardFocusHere();
                m_RenameRequested = false;
            }

            if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
            {
                tag = buffer;
                if (Scene* scene = entity.GetScene())
                    scene->InvalidateEntityLookupCache();
            }
        }

        ImGui::SameLine();
        ImGui::PushItemWidth(-1);

        if (ImGui::Button(EditorLocale::Text("Add Component", "添加组件")))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent"))
        {
            std::string currentCategory;
            EditorComponentRegistry::ForEach([&](const EditorComponentDescriptor& descriptor)
            {
                if (!descriptor.CanAdd(m_SelectionContext))
                    return;

                if (currentCategory != descriptor.Category)
                {
                    if (!currentCategory.empty())
                        ImGui::Separator();
                    currentCategory = descriptor.Category;
                    ImGui::TextDisabled("%s", currentCategory.c_str());
                }

                if (ImGui::MenuItem(descriptor.Label.c_str()))
                {
                    descriptor.Add(m_SelectionContext);
                    ImGui::CloseCurrentPopup();
                }
            });
            ImGui::EndPopup();
        }

        ImGui::PopItemWidth();

        bool hiddenInEditor = IsEntityHiddenInEditor(entity);
        if (ImGui::Checkbox(EditorLocale::Text("Hidden in Editor", "编辑器中隐藏"), &hiddenInEditor))
            SetEntityHiddenInEditor(entity, hiddenInEditor);

        EditorComponentRegistry::ForEach([entity](const EditorComponentDescriptor& descriptor)
        {
            descriptor.Draw(entity);
        });
    }

} // namespace Wheatear
