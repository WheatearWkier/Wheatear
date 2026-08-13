#include "wepch.h"
#include "WAOActionEditorPanel.h"
#include "WAOActionEditorPanelInternal.h"

#include "Editor/EditorWidgets.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"

#include <imgui/imgui.h>

#include <string>

namespace Wheatear {

    using namespace WAOActionEditorInternal;

    void WAOActionEditorPanel::DrawEffectEditor()
    {
        SectionHeader("Effects");

        if (ImGui::Button("Add Damage"))
        {
            m_EditRecipe.Effects.push_back(MakeEffectTemplate(WAO::EffectType::Damage));
            m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
            m_EditDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Heal"))
        {
            m_EditRecipe.Effects.push_back(MakeEffectTemplate(WAO::EffectType::Heal));
            m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
            m_EditDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Add State"))
        {
            m_EditRecipe.Effects.push_back(MakeEffectTemplate(WAO::EffectType::AddState));
            m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
            m_EditDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Signal"))
        {
            m_EditRecipe.Effects.push_back(MakeEffectTemplate(WAO::EffectType::EmitSignal));
            m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
            m_EditDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("More"))
            ImGui::OpenPopup("##WAOEffectTemplatePopup");
        if (ImGui::BeginPopup("##WAOEffectTemplatePopup"))
        {
            for (WAO::EffectType type : EditableEffectTypes())
            {
                if (ImGui::Selectable(EffectTypeName(type)))
                {
                    m_EditRecipe.Effects.push_back(MakeEffectTemplate(type));
                    m_SelectedEffectIndex = static_cast<int>(m_EditRecipe.Effects.size()) - 1;
                    m_EditDirty = true;
                }
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        const bool canRemove = m_SelectedEffectIndex >= 0 && m_SelectedEffectIndex < static_cast<int>(m_EditRecipe.Effects.size());
        if (!canRemove)
            ImGui::BeginDisabled();
        if (ImGui::Button("Remove Selected") && canRemove)
        {
            m_EditRecipe.Effects.erase(m_EditRecipe.Effects.begin() + m_SelectedEffectIndex);
            m_SelectedEffectIndex = m_EditRecipe.Effects.empty()
                ? -1
                : std::min(m_SelectedEffectIndex, static_cast<int>(m_EditRecipe.Effects.size()) - 1);
            m_EditDirty = true;
        }
        if (!canRemove)
            ImGui::EndDisabled();

        if (m_EditRecipe.Effects.empty())
        {
            ImGui::TextDisabled("No effects. Add one to make this action drive gameplay.");
            return;
        }

        ImGui::BeginChild("##WAOEffectList", ImVec2(0.0f, 118.0f), true);
        for (int i = 0; i < static_cast<int>(m_EditRecipe.Effects.size()); ++i)
        {
            const WAO::EffectSpec& effect = m_EditRecipe.Effects[static_cast<size_t>(i)];
            std::string label = std::to_string(i + 1) + ". " + EffectTypeName(effect.Type) + " -> " + EffectTargetText(effect);
            label = EditorWidgets::LabelWithId(label, "wao_effect:" + std::to_string(i));
            if (ImGui::Selectable(label.c_str(), i == m_SelectedEffectIndex))
                m_SelectedEffectIndex = i;
        }
        ImGui::EndChild();

        if (m_SelectedEffectIndex < 0 || m_SelectedEffectIndex >= static_cast<int>(m_EditRecipe.Effects.size()))
            return;

        WAO::EffectSpec& effect = m_EditRecipe.Effects[static_cast<size_t>(m_SelectedEffectIndex)];
        bool changed = false;

        if (ImGui::BeginCombo("Effect Type", EffectTypeName(effect.Type)))
        {
            for (WAO::EffectType type : EditableEffectTypes())
            {
                const bool selected = effect.Type == type;
                if (ImGui::Selectable(EffectTypeName(type), selected))
                {
                    if (effect.Type != type)
                    {
                        effect = MakeEffectTemplate(type);
                        changed = true;
                    }
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        const bool usesAttribute =
            effect.Type == WAO::EffectType::Damage
            || effect.Type == WAO::EffectType::Heal
            || effect.Type == WAO::EffectType::ModifyAttribute
            || effect.Type == WAO::EffectType::ConsumeResource;
        const bool usesState =
            effect.Type == WAO::EffectType::AddState
            || effect.Type == WAO::EffectType::RemoveState;
        const bool usesSignal = effect.Type == WAO::EffectType::EmitSignal;
        const bool usesValue =
            usesAttribute
            || effect.Type == WAO::EffectType::Launch;

        if (usesAttribute)
            changed |= EditorWidgets::InputString("Attribute / Resource Id", effect.AttributeId, 128);
        if (usesState)
            changed |= EditorWidgets::InputString("State Id", effect.StateId, 128);
        if (usesSignal)
            changed |= EditorWidgets::InputString("Signal Id", effect.SignalId, 128);
        if (usesValue)
            changed |= ImGui::DragFloat("Value", &effect.Value, 0.05f, -100000.0f, 100000.0f, "%.3f");

        int turns = effect.Turns;
        if (effect.DurationPolicy == WAO::EffectDurationPolicy::Turns && ImGui::InputInt("Turns", &turns))
        {
            effect.Turns = std::max(0, turns);
            changed = true;
        }

        if (effect.DurationPolicy == WAO::EffectDurationPolicy::Seconds)
            changed |= ImGui::DragFloat("Seconds", &effect.Seconds, 0.05f, 0.0f, 3600.0f, "%.3f");
        if (ImGui::BeginCombo("Duration Policy", DurationPolicyName(effect.DurationPolicy)))
        {
            for (WAO::EffectDurationPolicy policy : EditableDurationPolicies())
            {
                const bool selected = effect.DurationPolicy == policy;
                if (ImGui::Selectable(DurationPolicyName(policy), selected))
                {
                    effect.DurationPolicy = policy;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (changed)
            m_EditDirty = true;
    }
    void WAOActionEditorPanel::DrawEffectsTable()
    {
        const WAO::ActionRecipe* recipe = FindSelectedRecipe(m_SelectedActionId);
        if (!recipe)
            return;

        if (recipe->Effects.empty())
        {
            ImGui::TextDisabled("No gameplay effects in this recipe.");
            return;
        }

        if (ImGui::BeginTable("##WAOEffects", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Target");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Duration");
            ImGui::TableSetupColumn("Signal");
            ImGui::TableHeadersRow();

            for (const WAO::EffectSpec& effect : recipe->Effects)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(EffectTypeName(effect.Type));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(EffectTargetText(effect).c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", effect.Value);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(FormatDuration(effect).c_str());
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(effect.SignalId.empty() ? "-" : effect.SignalId.c_str());
            }

            ImGui::EndTable();
        }
    }
} // namespace Wheatear
