#include "wepch.h"
#include "WAOActionEditorPanel.h"
#include "WAOActionEditorPanelInternal.h"

#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Gameplay/Action/ActionRunner.h"
#include "Wheatear/Gameplay/Action/EffectFormula.h"
#include "Wheatear/Gameplay/Action/EffectRegistry.h"
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
        CollectValidationMessages(*recipe, errors, warnings);

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

    void WAOActionEditorPanel::CollectValidationMessages(
        const WAO::ActionRecipe& recipe,
        std::vector<std::string>& errors,
        std::vector<std::string>& warnings) const
    {
        if (recipe.Id.empty())
            errors.push_back("Action id is empty.");
        if (RecipeSourcePath(recipe.Id).empty())
            warnings.push_back("No YAML source mapping. Saving is disabled for this id prefix.");
        if (recipe.DisplayName.empty())
            warnings.push_back("Display name is empty.");
        if (recipe.Duration < 0.0f || recipe.Startup < 0.0f || recipe.Recovery < 0.0f || recipe.HitTime < 0.0f)
            errors.push_back("Timing values must not be negative.");
        if (recipe.Duration > 0.0f && recipe.HitTime > recipe.Duration)
            warnings.push_back("Hit time is later than duration.");
        if (recipe.CancelEnd > 0.0f && recipe.CancelEnd < recipe.CancelStart)
            errors.push_back("Cancel end is earlier than cancel start.");
        if (!recipe.IconPath.empty() && !EditorWidgets::ProjectAssetExists(recipe.IconPath))
            warnings.push_back("Icon path is missing: " + recipe.IconPath);
        if (!recipe.SoundPath.empty() && !EditorWidgets::ProjectAssetExists(recipe.SoundPath))
            warnings.push_back("SFX path is missing: " + recipe.SoundPath);
        if (!recipe.EffectPath.empty() && !EditorWidgets::ProjectAssetExists(recipe.EffectPath))
            warnings.push_back("VFX path is missing: " + recipe.EffectPath);
        if (!recipe.BehaviorId.empty() || !recipe.BehaviorEntry.empty() || !recipe.BehaviorFile.empty())
        {
            if (recipe.BehaviorId.empty() && recipe.BehaviorEntry.empty())
                errors.push_back("Behavior wiring needs Behavior Id or Behavior Entry.");
            if (!recipe.BehaviorFile.empty() && !EditorWidgets::ProjectAssetExists(recipe.BehaviorFile))
                warnings.push_back("Behavior file is missing: " + recipe.BehaviorFile);
            if (recipe.BehaviorVersion == 0)
                warnings.push_back("Behavior version is not pinned; hot reload will use the latest entry.");
        }

        for (const auto& [id, cost] : recipe.ResourceCost)
        {
            if (id.empty())
                errors.push_back("Resource cost has an empty id.");
            if (cost < 0.0f)
                errors.push_back("Resource cost is negative: " + id);
        }

        for (const auto& [id, value] : recipe.Params)
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

        for (size_t i = 0; i < recipe.Effects.size(); ++i)
        {
            const WAO::EffectSpec& effect = recipe.Effects[i];
            const std::string prefix = "Effect " + std::to_string(i + 1) + ": ";
            if (effect.Type == WAO::EffectType::None && effect.CustomType.empty())
                errors.push_back(prefix + "type is None.");

            if (!effect.CustomType.empty() && !WAO::EffectRegistry::Has(effect.CustomType))
                errors.push_back(prefix + "unknown custom effect: " + effect.CustomType);

            if (!effect.Formula.empty())
            {
                std::string formulaError;
                if (!WAO::ValidateEffectFormula(effect.Formula, &formulaError))
                    errors.push_back(prefix + formulaError);
            }

            const bool usesAttribute =
                effect.Type == WAO::EffectType::Damage
                || effect.Type == WAO::EffectType::Heal
                || effect.Type == WAO::EffectType::ModifyAttribute;
            if (usesAttribute && effect.AttributeId.empty())
            {
                if (effect.Type == WAO::EffectType::ModifyAttribute)
                    errors.push_back(prefix + "ModifyAttribute needs an attribute id.");
                else
                    warnings.push_back(prefix + "attribute id is empty; Health is used.");
            }
            else if (usesAttribute && !IsKnownAttribute(effect.AttributeId))
            {
                warnings.push_back(prefix + "attribute id '" + effect.AttributeId + "' is not a known attribute.");
            }

            if (effect.Type == WAO::EffectType::ConsumeResource && effect.AttributeId.empty())
                errors.push_back(prefix + "ConsumeResource needs a resource id.");

            if ((effect.Type == WAO::EffectType::AddState || effect.Type == WAO::EffectType::RemoveState)
                && effect.StateId.empty())
            {
                errors.push_back(prefix + "state id is empty.");
            }
            else if ((effect.Type == WAO::EffectType::AddState || effect.Type == WAO::EffectType::RemoveState)
                && WAO::FindStateDefinition(effect.StateId) == nullptr)
            {
                warnings.push_back(prefix + "state id '" + effect.StateId + "' is not in StateRegistry.");
            }

            if (effect.Type == WAO::EffectType::EmitSignal && effect.SignalId.empty())
                warnings.push_back(prefix + "signal id is empty.");

            if (effect.Type == WAO::EffectType::StartCooldown
                && effect.Seconds <= 0.0f && effect.Value <= 0.0f)
            {
                errors.push_back(prefix + "StartCooldown needs Seconds > 0.");
            }

            if (effect.DurationPolicy == WAO::EffectDurationPolicy::Seconds
                && effect.Seconds <= 0.0f)
            {
                warnings.push_back(prefix + "seconds policy needs Seconds > 0.");
            }
            if (effect.DurationPolicy == WAO::EffectDurationPolicy::Turns
                && effect.Turns <= 0)
            {
                warnings.push_back(prefix + "turns policy needs Turns > 0.");
            }
            if (effect.Type == WAO::EffectType::AddState
                && effect.DurationPolicy != WAO::EffectDurationPolicy::Infinite
                && effect.Turns <= 0 && effect.Seconds <= 0.0f)
            {
                errors.push_back(prefix + "AddState needs Turns, Seconds, or Infinite duration.");
            }

            if ((effect.Type == WAO::EffectType::Launch
                    || effect.Type == WAO::EffectType::HitStun)
                && effect.Seconds <= 0.0f)
            {
                warnings.push_back(prefix + "default duration will be used (0.2s HitStun / 0.3s Launch).");
            }
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
        const bool darkTheme =
            ImGui::GetStyleColorVec4(ImGuiCol_WindowBg).x < 0.5f;
        drawList->AddRectFilled(origin, end,
            darkTheme ? IM_COL32(22, 28, 34, 255)
                      : IM_COL32(226, 220, 208, 255), 4.0f);

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
        WAO::ActionRuntime actorRuntime;
        WAO::ActionRuntime targetRuntime;
        // Seed both sides so actor- and target-applied effects are visible.
        for (WAO::ActionRuntime* runtime : { &actorRuntime, &targetRuntime })
        {
            runtime->Attributes.Set("Health", 100.0f);
            runtime->Attributes.Set("MaxHealth", 100.0f);
            runtime->Attributes.Set("hp", 100.0f);
            runtime->Attributes.Set("max_hp", 100.0f);
            runtime->Attributes.Set("atk", 10.0f);
            runtime->Attributes.Set("attack", 10.0f);
            runtime->Attributes.Set("mana", 50.0f);
            runtime->Resources["mana"] = 50.0f;
        }

        for (const auto& [id, cost] : recipe.ResourceCost)
        {
            actorRuntime.Resources[id] = cost + 10.0f;
            targetRuntime.Resources[id] = cost + 10.0f;
        }
        actorRuntime.Tags = recipe.RequiredTags;
        targetRuntime.Tags = recipe.RequiredTags;

        WAO::ActionIntent intent;
        intent.Actor = 1;
        intent.ExplicitTarget = 2;
        intent.ActionId = recipe.Id;

        m_SandboxBefore.clear();
        auto capture = [this](const char* prefix, const WAO::ActionRuntime& runtime,
            std::unordered_map<std::string, float>& out)
        {
            for (const auto& [id, value] : runtime.Attributes.Values)
                out[std::string(prefix) + "." + id] = value;
            for (const auto& [id, value] : runtime.Resources)
                out[std::string(prefix) + "." + id] = value;
        };
        capture("actor", actorRuntime, m_SandboxBefore);
        capture("target", targetRuntime, m_SandboxBefore);

        m_SandboxResult = WAO::Execute(
            intent, recipe, actorRuntime, &targetRuntime, &actorRuntime);
        m_SandboxRan = true;

        m_SandboxAfter.clear();
        capture("actor", actorRuntime, m_SandboxAfter);
        capture("target", targetRuntime, m_SandboxAfter);

        const bool affordable = WAO::CanAfford(recipe, actorRuntime);
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
