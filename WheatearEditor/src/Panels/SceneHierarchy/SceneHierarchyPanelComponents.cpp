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
        ImGui::SetNextItemWidth(std::max(90.0f,
            ImGui::GetContentRegionAvail().x
            - ImGui::CalcTextSize(EditorLocale::Text("Auto Load Slot", "自动读取槽位")).x
            - ImGui::GetStyle().ItemInnerSpacing.x));
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
        const float textScale = ImGui::GetFontSize() / 16.0f;
        const float addButtonWidth = 132.0f * textScale;
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

            ImGui::SetNextItemWidth(std::max(80.0f,
                ImGui::GetContentRegionAvail().x - addButtonWidth
                - ImGui::GetStyle().ItemSpacing.x));
            if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
            {
                tag = buffer;
                if (Scene* scene = entity.GetScene())
                    scene->InvalidateEntityLookupCache();
            }
        }

        ImGui::SameLine();

        // Structural edits (add/remove/reset components) are disabled while
        // playing: the runtime copy is ephemeral and such edits would be
        // silently discarded on stop.
        ImGui::BeginDisabled(m_RuntimeMode);
        if (ImGui::Button(EditorLocale::Text("+ Add Component", "+ 添加组件"),
                ImVec2(addButtonWidth, 0.0f)))
        {
            m_AddComponentPopupRequested = true;
            m_AddComponentPopupJustOpened = true;
        }
        ImGui::EndDisabled();

        if (m_AddComponentPopupRequested
            && !ImGui::IsPopupOpen("##PropertiesContext"))
        {
            ImGui::OpenPopup("AddComponent");
            m_AddComponentPopupRequested = false;
        }

        if (ImGui::BeginPopup("AddComponent"))
        {
            if (m_AddComponentPopupJustOpened)
            {
                m_AddComponentSearch[0] = '\0';
                ImGui::SetKeyboardFocusHere();
                m_AddComponentPopupJustOpened = false;
            }
            ImGui::SetNextItemWidth(280.0f * textScale);
            ImGui::InputTextWithHint("##AddComponentSearch",
                EditorLocale::Text("Search components...", "搜索组件..."),
                m_AddComponentSearch, sizeof(m_AddComponentSearch));
            ImGui::Separator();

            const std::string query = StringUtils::ToLower(m_AddComponentSearch);

            ImGui::BeginChild("##AddComponentList",
                ImVec2(320.0f * textScale, 360.0f * textScale),
                false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
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

                if (ImGui::Selectable(label.c_str()))
                {
                    descriptor.Add(m_SelectionContext);
                    m_AddComponentSearch[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            });
            ImGui::EndChild();

            ImGui::Separator();
            if (ImGui::Button(EditorLocale::Text("Cancel", "取消"),
                    ImVec2(110.0f * textScale, 0.0f)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", EditorLocale::Text(
                "Type to filter; Esc closes", "输入以筛选；Esc 关闭"));
            ImGui::EndPopup();
        }

        EditorComponentRegistry::ForEach([entity](const EditorComponentDescriptor& descriptor)
        {
            descriptor.Draw(entity);
        });
    }

} // namespace Wheatear
