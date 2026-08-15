#include "wepch.h"
#include "SceneHierarchyPanel.h"

#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Utils/StringUtils.h"
#include "Editor/EditorComponentRegistry.h"
#include "Editor/EditorCommands.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"

#include <imgui/imgui.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

namespace Wheatear {
    namespace {

        static bool SavePolicyEquals(const SavePolicy& a, const SavePolicy& b)
        {
            return a.CanSave == b.CanSave
                && a.CanLoad == b.CanLoad
                && a.SaveDirectory == b.SaveDirectory
                && a.AutoLoadSlot == b.AutoLoadSlot;
        }

    } // namespace

    void SceneHierarchyPanel::DrawSceneSettings()
    {
        if (!m_Context)
            return;

        Scene* scene = m_Context.get();
        SavePolicy& policy = scene->GetSavePolicy();
        const SavePolicy beforeFrame = policy;

        EditorWidgets::SectionHeader(EditorLocale::Text("Save Policy", "存档策略"));
        ImGui::PushID("SceneSavePolicy");

        ImGui::Checkbox(EditorLocale::Text("Can Save", "允许保存"), &policy.CanSave);
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::Text("Can Load", "允许读取"), &policy.CanLoad);
        EditorWidgets::InputString(EditorLocale::Text("Save Directory", "存档目录"), policy.SaveDirectory);

        int autoLoadSlot = policy.AutoLoadSlot;
        if (ImGui::DragInt(EditorLocale::Text("Auto Load Slot", "自动读取槽位"),
            &autoLoadSlot,
            1.0f,
            0,
            GameProgress::GetMaxSaveSlots()))
        {
            policy.AutoLoadSlot = std::clamp(autoLoadSlot, 0, GameProgress::GetMaxSaveSlots());
        }

        const bool anyItemActive = ImGui::IsAnyItemActive();
        if (anyItemActive && !m_SceneSettingsEditing)
        {
            m_SceneSettingsEditStartCanSave = beforeFrame.CanSave;
            m_SceneSettingsEditStartCanLoad = beforeFrame.CanLoad;
            m_SceneSettingsEditStartSaveDirectory = beforeFrame.SaveDirectory;
            m_SceneSettingsEditStartAutoLoadSlot = beforeFrame.AutoLoadSlot;
            m_SceneSettingsEditing = true;
        }
        else if (!anyItemActive && m_SceneSettingsEditing)
        {
            SavePolicy before;
            before.CanSave = m_SceneSettingsEditStartCanSave;
            before.CanLoad = m_SceneSettingsEditStartCanLoad;
            before.SaveDirectory = m_SceneSettingsEditStartSaveDirectory;
            before.AutoLoadSlot = m_SceneSettingsEditStartAutoLoadSlot;

            const SavePolicy after = scene->GetSavePolicy();
            if (!SavePolicyEquals(before, after))
                CommandHistory::Get().Push(std::make_unique<SceneSavePolicyCommand>(scene, before, after));

            m_SceneSettingsEditing = false;
            m_SceneSettingsEditStartSaveDirectory.clear();
        }

        ImGui::PopID();
        ImGui::Spacing();
    }

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
            ImGui::SetNextItemWidth(240.0f);
            ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("##AddComponentSearch",
                EditorLocale::Text("Search components...", "搜索组件..."),
                m_AddComponentSearch, sizeof(m_AddComponentSearch));
            ImGui::Separator();

            const std::string query = StringUtils::ToLower(m_AddComponentSearch);

            std::string currentCategory;
            EditorComponentRegistry::ForEach([&](const EditorComponentDescriptor& descriptor)
            {
                if (!descriptor.CanAdd(m_SelectionContext))
                    return;

                const std::string& label = descriptor.Label;
                if (!query.empty()
                    && StringUtils::ToLower(label).find(query) == std::string::npos)
                    return;

                if (currentCategory != descriptor.Category)
                {
                    if (!currentCategory.empty())
                        ImGui::Separator();
                    currentCategory = descriptor.Category;
                    ImGui::TextDisabled("%s", currentCategory.c_str());
                }

                if (ImGui::MenuItem(label.c_str()))
                {
                    descriptor.Add(m_SelectionContext);
                    m_AddComponentSearch[0] = '\0';
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
