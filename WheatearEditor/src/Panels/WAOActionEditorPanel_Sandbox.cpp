#include "wepch.h"
#include "WAOActionEditorPanel.h"
#include "WAOActionEditorPanelInternal.h"

#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Gameplay/Action/ActionRunner.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    using namespace WAOActionEditorInternal;

    void WAOActionEditorPanel::DrawValidationPanel()
    {
        const WAO::ActionRecipe* runtimeRecipe = FindSelectedRecipe(m_SelectedActionId);
        const WAO::ActionRecipe* recipe = (m_EditMode && m_EditingActionId == m_SelectedActionId) ? &m_EditRecipe : runtimeRecipe;
        if (!recipe)
            return;

        std::vector<std::string> errors;
        std::vector<std::string> warnings;

        if (recipe->Id.empty())
            errors.push_back("Action id is empty.");
        if (RecipeSourcePath(recipe->Id).empty())
            warnings.push_back("No YAML source mapping. Saving is disabled for this id prefix.");
        if (recipe->DisplayName.empty())
            warnings.push_back("Display name is empty.");
        if (recipe->Duration < 0.0f || recipe->Startup < 0.0f || recipe->Recovery < 0.0f || recipe->HitTime < 0.0f)
            errors.push_back("Timing values must not be negative.");
        if (recipe->Duration > 0.0f && recipe->HitTime > recipe->Duration)
            warnings.push_back("Hit time is later than duration.");
        if (recipe->CancelEnd > 0.0f && recipe->CancelEnd < recipe->CancelStart)
            errors.push_back("Cancel end is earlier than cancel start.");
        if (!recipe->IconPath.empty() && !EditorWidgets::ProjectAssetExists(recipe->IconPath))
            warnings.push_back("Icon path is missing: " + recipe->IconPath);
        if (!recipe->SoundPath.empty() && !EditorWidgets::ProjectAssetExists(recipe->SoundPath))
            warnings.push_back("SFX path is missing: " + recipe->SoundPath);
        if (!recipe->EffectPath.empty() && !EditorWidgets::ProjectAssetExists(recipe->EffectPath))
            warnings.push_back("VFX path is missing: " + recipe->EffectPath);

        for (const auto& [id, cost] : recipe->ResourceCost)
        {
            if (id.empty())
                errors.push_back("Resource cost has an empty id.");
            if (cost < 0.0f)
                errors.push_back("Resource cost is negative: " + id);
        }

        for (const auto& [id, value] : recipe->Params)
        {
            if (id.empty())
                errors.push_back("Recipe params contain an empty key.");
            if (value.empty())
                warnings.push_back("Recipe param has an empty value: " + id);

            const std::vector<std::string> choices = CommonParamValues(id);
            if (!choices.empty() && std::find(choices.begin(), choices.end(), value) == choices.end())
                warnings.push_back("Recipe param " + id + " uses a value outside the common schema: " + value);

            if (id == "defensePierce" || id == "range" || id == "radius" || id == "knockback" || id == "hitStop" || id == "cameraShake")
            {
                try
                {
                    (void)std::stof(value);
                }
                catch (...)
                {
                    errors.push_back("Recipe param " + id + " must be numeric.");
                }
            }
        }

        for (size_t i = 0; i < recipe->Effects.size(); ++i)
        {
            const WAO::EffectSpec& effect = recipe->Effects[i];
            const std::string prefix = "Effect " + std::to_string(i + 1) + ": ";
            if (effect.Type == WAO::EffectType::None)
                errors.push_back(prefix + "type is None.");
            if ((effect.Type == WAO::EffectType::ModifyAttribute || effect.Type == WAO::EffectType::Damage || effect.Type == WAO::EffectType::Heal)
                && effect.AttributeId.empty())
                warnings.push_back(prefix + "attribute id is empty.");
            else if ((effect.Type == WAO::EffectType::ModifyAttribute || effect.Type == WAO::EffectType::Damage || effect.Type == WAO::EffectType::Heal)
                && !IsKnownAttribute(effect.AttributeId))
                warnings.push_back(prefix + "attribute id '" + effect.AttributeId + "' is not a known attribute.");
            if ((effect.Type == WAO::EffectType::AddState || effect.Type == WAO::EffectType::RemoveState)
                && effect.StateId.empty())
                warnings.push_back(prefix + "state id is empty.");
            else if ((effect.Type == WAO::EffectType::AddState || effect.Type == WAO::EffectType::RemoveState)
                && WAO::FindStateDefinition(effect.StateId) == nullptr)
                warnings.push_back(prefix + "state id '" + effect.StateId + "' is not in StateRegistry.");
            if (effect.Type == WAO::EffectType::EmitSignal && effect.SignalId.empty())
                warnings.push_back(prefix + "signal id is empty.");
            if (effect.DurationPolicy == WAO::EffectDurationPolicy::Seconds && effect.Seconds <= 0.0f)
                warnings.push_back(prefix + "seconds policy needs Seconds > 0.");
            if (effect.DurationPolicy == WAO::EffectDurationPolicy::Turns && effect.Turns <= 0)
                warnings.push_back(prefix + "turns policy needs Turns > 0.");
        }

        if (errors.empty() && warnings.empty())
        {
            ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.45f, 1.0f), "Validation passed.");
            return;
        }

        if (!errors.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "Errors");
            for (const std::string& message : errors)
                ImGui::BulletText("%s", message.c_str());
        }

        if (!warnings.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.25f, 1.0f), "Warnings");
            for (const std::string& message : warnings)
                ImGui::BulletText("%s", message.c_str());
        }
    }
    void WAOActionEditorPanel::DrawPreviewPanel()
    {
        const WAO::ActionRecipe* runtimeRecipe = FindSelectedRecipe(m_SelectedActionId);
        const WAO::ActionRecipe* recipe = (m_EditMode && m_EditingActionId == m_SelectedActionId) ? &m_EditRecipe : runtimeRecipe;
        if (!recipe)
            return;

        LabelValue("Action", recipe->Id);
        LabelValue("Animation", recipe->AnimationId);
        EditorWidgets::DrawLabeledPathTools("Icon", recipe->IconPath);
        EditorWidgets::DrawLabeledPathTools("SFX", recipe->SoundPath);
        EditorWidgets::DrawLabeledPathTools("VFX", recipe->EffectPath);

        SectionHeader("Timing Preview");
        const float total = std::max({ 0.1f, recipe->Duration, recipe->Startup + recipe->Recovery, recipe->HitTime + recipe->Recovery, recipe->CancelEnd });
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float width = std::max(260.0f, ImGui::GetContentRegionAvail().x - 8.0f);
        const float height = 46.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 end = ImVec2(origin.x + width, origin.y + height);
        drawList->AddRectFilled(origin, end, IM_COL32(22, 28, 34, 255), 4.0f);

        auto drawSegment = [&](float start, float stop, ImU32 color)
        {
            start = std::clamp(start, 0.0f, total);
            stop = std::clamp(stop, 0.0f, total);
            if (stop <= start)
                return;
            const float x0 = origin.x + (start / total) * width;
            const float x1 = origin.x + (stop / total) * width;
            drawList->AddRectFilled(ImVec2(x0, origin.y + 8.0f), ImVec2(x1, origin.y + height - 8.0f), color, 3.0f);
        };

        drawSegment(0.0f, recipe->Startup, IM_COL32(70, 110, 180, 255));
        drawSegment(recipe->Startup, std::max(recipe->Startup, recipe->HitTime), IM_COL32(80, 160, 120, 255));
        drawSegment(std::max(recipe->HitTime, recipe->Startup), std::max(recipe->Duration, recipe->HitTime), IM_COL32(170, 125, 70, 255));
        drawSegment(recipe->CancelStart, recipe->CancelEnd, IM_COL32(190, 210, 80, 210));

        const float hitX = origin.x + (std::clamp(recipe->HitTime, 0.0f, total) / total) * width;
        drawList->AddLine(ImVec2(hitX, origin.y + 4.0f), ImVec2(hitX, origin.y + height - 4.0f), IM_COL32(255, 90, 90, 255), 2.0f);
        ImGui::Dummy(ImVec2(width, height + 4.0f));
        ImGui::TextDisabled("Blue startup, green active lead-in, orange recovery/body, yellow cancel window, red hit frame.");

        SectionHeader("Gameplay Output");
        ImGui::Text("Effects: %d", static_cast<int>(recipe->Effects.size()));
        ImGui::Text("Signals: %s", recipe->Signals.empty() ? "-" : EditorWidgets::JoinList(recipe->Signals).c_str());
        ImGui::Text("Cost: %s", recipe->ResourceCost.empty() ? "-" : JoinResourceCost(recipe->ResourceCost).c_str());
        ImGui::Text("Params: %s", recipe->Params.empty() ? "-" : JoinParams(recipe->Params).c_str());

        SectionHeader("Sandbox");
        ImGui::TextDisabled("Execute this recipe against a synthetic runtime. No Scene/Play mode required.");
        if (ImGui::Button(EditorLocale::Text("Run in Sandbox", "沙盒运行")))
            RunSandbox(*recipe);
        if (m_SandboxRan)
            DrawSandboxResult();
    }
    void WAOActionEditorPanel::RunSandbox(const WAO::ActionRecipe& recipe)
    {
        WAO::ActionRuntime runtime;
        // Seed a baseline so Damage/Heal/Modify land on visible numbers.
        runtime.Attributes.Set("Health", 100.0f);
        runtime.Attributes.Set("hp", 100.0f);
        runtime.Attributes.Set("atk", 10.0f);
        runtime.Attributes.Set("mana", 50.0f);
        for (const auto& [id, cost] : recipe.ResourceCost)
            runtime.Resources[id] = cost + 10.0f; // allow the cost check to pass
        runtime.Tags = recipe.RequiredTags;

        WAO::ActionIntent intent;
        intent.Actor = 1;
        intent.ActionId = recipe.Id;

        m_SandboxBefore.clear();
        for (const auto& [id, value] : runtime.Attributes.Values)
            m_SandboxBefore[id] = value;
        m_SandboxBefore["mana"] = 50.0f;

        m_SandboxResult = WAO::Execute(intent, recipe, runtime);
        m_SandboxRan = true;

        m_SandboxAfter.clear();
        for (const auto& [id, value] : runtime.Attributes.Values)
            m_SandboxAfter[id] = value;

        const bool affordable = WAO::CanAfford(recipe, runtime);
        if (!m_SandboxResult.Success)
            m_SandboxStatus = affordable ? "Execution failed (see ledger below)." : "Cannot afford resource cost.";
        else
            m_SandboxStatus = "Executed.";
    }
    void WAOActionEditorPanel::DrawSandboxResult()
    {
        ImGui::Separator();
        ImGui::TextColored(m_SandboxResult.Success ? ImVec4(0.35f, 0.90f, 0.45f, 1.0f) : ImVec4(1.0f, 0.35f, 0.30f, 1.0f),
            "%s", m_SandboxStatus.c_str());
        if (!m_SandboxResult.Success)
            ImGui::TextWrapped("Note: sandbox uses a synthetic runtime; resolver-side logic (module handlers) is bypassed.");

        if (!m_SandboxBefore.empty())
        {
            ImGui::TextDisabled(EditorLocale::Text("Attribute Deltas", "属性变化"));
            for (const auto& [id, before] : m_SandboxBefore)
            {
                const float after = m_SandboxAfter.count(id) > 0 ? m_SandboxAfter.at(id) : before;
                const float delta = after - before;
                if (std::abs(delta) > 0.0001f)
                {
                    const ImVec4 color = delta < 0.0f ? ImVec4(1.0f, 0.45f, 0.40f, 1.0f) : ImVec4(0.45f, 0.90f, 0.45f, 1.0f);
                    ImGui::TextColored(color, "  %s: %.2f -> %.2f (%+.2f)", id.c_str(), before, after, delta);
                }
            }
        }

        const auto& entries = m_SandboxResult.Ledger.Entries();
        if (!entries.empty())
        {
            ImGui::TextDisabled(EditorLocale::Text("Ledger", "运行记录"));
            for (const auto& entry : entries)
            {
                std::string label = EffectTypeName(entry.Type);
                if (!entry.Detail.empty())
                    label += " - " + entry.Detail;
                ImGui::BulletText("%s%s (%.2f)", label.c_str(), entry.Applied ? "" : " [blocked]", entry.Value);
            }
        }
    }
} // namespace Wheatear
