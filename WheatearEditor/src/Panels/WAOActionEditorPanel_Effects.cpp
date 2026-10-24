#include "wepch.h"
#include "WAOActionEditorPanel.h"
#include "WAOActionEditorPanelInternal.h"

#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"
#include "Wheatear/Gameplay/Action/EffectRegistry.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Wheatear {

    using namespace WAOActionEditorInternal;

    void WAOActionEditorPanel::DrawEffectEditor()
    {
        SectionHeader("Effects");

        for (WAO::EffectSpec& effect : m_EditRecipe.Effects)
        {
            if (effect.EditorId == 0)
                effect.EditorId = m_NextEffectId++;
        }

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
            m_EffectsStructureChanged = true;
            m_SelectedEffectIndex = m_EditRecipe.Effects.empty()
                ? -1
                : std::min(m_SelectedEffectIndex, static_cast<int>(m_EditRecipe.Effects.size()) - 1);
            m_EditDirty = true;
        }
        if (!canRemove)
            ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!canRemove || m_SelectedEffectIndex <= 0);
        if (ImGui::Button(EditorLocale::Text("Move Up", "上移")))
        {
            std::swap(m_EditRecipe.Effects[m_SelectedEffectIndex],
                m_EditRecipe.Effects[m_SelectedEffectIndex - 1]);
            --m_SelectedEffectIndex;
            m_EffectsStructureChanged = true;
            m_EditDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!canRemove
            || m_SelectedEffectIndex + 1 >= static_cast<int>(m_EditRecipe.Effects.size()));
        if (ImGui::Button(EditorLocale::Text("Move Down", "下移")))
        {
            std::swap(m_EditRecipe.Effects[m_SelectedEffectIndex],
                m_EditRecipe.Effects[m_SelectedEffectIndex + 1]);
            ++m_SelectedEffectIndex;
            m_EffectsStructureChanged = true;
            m_EditDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!canRemove);
        if (ImGui::Button(EditorLocale::Text("Duplicate", "复制")))
        {
            const WAO::EffectSpec copy = m_EditRecipe.Effects[m_SelectedEffectIndex];
            const uint64_t sourceId = copy.EditorId;
            m_EditRecipe.Effects.insert(
                m_EditRecipe.Effects.begin() + m_SelectedEffectIndex + 1, copy);
            ++m_SelectedEffectIndex;
            WAO::EffectSpec& duplicated =
                m_EditRecipe.Effects[static_cast<size_t>(m_SelectedEffectIndex)];
            duplicated.EditorId = m_NextEffectId++;
            const auto origin = m_EffectOrigins.find(sourceId);
            if (origin != m_EffectOrigins.end())
                m_EffectOrigins[duplicated.EditorId] = origin->second;
            m_EffectsStructureChanged = true;
            m_EditDirty = true;
        }
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
            if (!effect.CustomType.empty())
                label += " [custom: " + effect.CustomType + "]";
            if (!effect.Formula.empty())
                label += " [formula]";
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

        if (ImGui::BeginCombo("Applies To", TargetSelectorName(effect.TargetSelector)))
        {
            for (WAO::EffectTargetSelector selector : {
                    WAO::EffectTargetSelector::Actor,
                    WAO::EffectTargetSelector::Target,
                    WAO::EffectTargetSelector::Source })
            {
                const bool selected = effect.TargetSelector == selector;
                if (ImGui::Selectable(TargetSelectorName(selector), selected))
                {
                    effect.TargetSelector = selector;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("Source", TargetSelectorName(effect.SourceSelector)))
        {
            for (WAO::EffectTargetSelector selector : {
                    WAO::EffectTargetSelector::Actor,
                    WAO::EffectTargetSelector::Target,
                    WAO::EffectTargetSelector::Source })
            {
                const bool selected = effect.SourceSelector == selector;
                if (ImGui::Selectable(TargetSelectorName(selector), selected))
                {
                    effect.SourceSelector = selector;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

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

        ImGui::Separator();
        ImGui::TextDisabled("Data-driven extensions (no C++ needed)");

        // Custom registered effect semantics: pick from the registry or type
        // an id; the runtime dispatches through WAO::EffectRegistry.
        {
            const std::vector<std::string> customIds = WAO::EffectRegistry::AllIds();
            std::string preview = effect.CustomType.empty()
                ? "(none)"
                : std::string(WAO::EffectRegistry::DisplayName(effect.CustomType))
                    + " [" + effect.CustomType + "]";
            if (ImGui::BeginCombo("Custom Effect", preview.c_str()))
            {
                if (ImGui::Selectable("(none)", effect.CustomType.empty()))
                {
                    effect.CustomType.clear();
                    changed = true;
                }
                for (const std::string& id : customIds)
                {
                    const bool selected = effect.CustomType == id;
                    const std::string label = std::string(WAO::EffectRegistry::DisplayName(id)) + " [" + id + "]";
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        effect.CustomType = id;
                        changed = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        // Numeric formula override: evaluated at apply time, result replaces
        // Value. Works with any effect type ("target.health + 50" on a Heal,
        // "min(target.max_health, target.health + 30)" on a Damage, ...).
        {
            std::string formula = effect.Formula;
            if (EditorWidgets::InputString("Formula (override value)", formula, 256))
            {
                effect.Formula = formula;
                changed = true;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip(
                    "Numeric expression evaluated at apply time; the result overrides Value.\n"
                    "Variables: source.health / source.attack / source.defense /\n"
                    "target.health / target.max_health / target.attack / target.defense /\n"
                    "controller.mana / controller.max_mana / controller.heal_amount / ...\n"
                    "Operators: + - * / %%  < <= > >= == !=  and or not\n"
                    "Functions: min(a,b) max(a,b) clamp(v,a,b) abs(x) round(x) floor(x) ceil(x) if(c,a,b)");
            }
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
