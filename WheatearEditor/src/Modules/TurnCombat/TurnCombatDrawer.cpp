#include "TurnCombatDrawer.h"

#include "Editor/EditorWidgets.h"
#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Modules/TurnCombat/TurnCombatComponents.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Wheatear {

    namespace {

        using EditorWidgets::DrawTeamCombo;
        using EditorWidgets::InputString;

        static const char* PhaseName(TurnCombatPhase phase)
        {
            switch (phase)
            {
            case TurnCombatPhase::Intro: return "Intro";
            case TurnCombatPhase::AwaitCommand: return "Await Command";
            case TurnCombatPhase::AwaitTarget: return "Await Target";
            case TurnCombatPhase::Acting: return "Acting";
            case TurnCombatPhase::Victory: return "Victory";
            case TurnCombatPhase::Defeat: return "Defeat";
            }
            return "Unknown";
        }

        static void DrawAtlasFrameSpec(
            const char* label,
            GameplayVisualService::TextureAtlasFrameSpec& atlas,
            int pathWidth)
        {
            const std::string sheetLabel = std::string(label) + " Sheet";
            const std::string cellLabel = std::string(label) + " Cell";
            const std::string columnsLabel = std::string(label) + " Columns";
            const std::string startLabel = std::string(label) + " Start";

            InputString(sheetLabel.c_str(), atlas.SheetPath, pathWidth);
            ImGui::DragInt2(cellLabel.c_str(), &atlas.CellWidth, 1.0f, 0, 8192);
            ImGui::DragInt(columnsLabel.c_str(), &atlas.Columns, 1.0f, 0, 256);
            ImGui::DragInt(startLabel.c_str(), &atlas.StartFrame, 1.0f, 0, 4096);
        }

    } // namespace

    void DrawTurnCombatLevelComponent(Entity entity)
    {
        DrawComponent<TurnCombatLevelComponent>("Turn Combat Level", entity, [](auto& level)
        {
            ImGui::Checkbox("Play On Start", &level.PlayOnStart);
            InputString("Level Id", level.LevelId, 220);
            ImGui::DragFloat("Start Fade", &level.StartFadeDuration, 0.02f, 0.0f, 5.0f);
            ImGui::DragFloat("Intro Duration", &level.IntroDuration, 0.02f, 0.0f, 10.0f);
            ImGui::DragFloat("Action Duration", &level.ActionDuration, 0.02f, 0.1f, 5.0f);
            ImGui::DragFloat("Victory Return Delay", &level.VictoryReturnDelay, 0.05f, 0.0f, 10.0f);
            ImGui::DragFloat("Defeat Return Delay", &level.DefeatReturnDelay, 0.05f, 0.0f, 10.0f);
            InputString("Victory Command", level.VictorySceneCommand, 260);
            InputString("Defeat Command", level.DefeatSceneCommand, 260);

            ImGui::Separator();
            ImGui::TextDisabled("Scene Bindings");
            InputString("Fade", level.FadeEntityName);
            InputString("Message Text", level.MessageTextEntityName);
            InputString("Active Actor Text", level.ActiveActorTextEntityName);
            InputString("Turn Order Text", level.TurnOrderTextEntityName);
            InputString("Skill Detail Text", level.SkillDetailTextEntityName);
            InputString("Command Panel", level.CommandPanelEntityName);
            InputString("Target Hint Text", level.TargetHintTextEntityName);
            InputString("Action Flash", level.ActionFlashEntityName);
            InputString("Action Effect", level.ActionEffectEntityName);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Phase: %s", PhaseName(level.RuntimePhase));
            ImGui::Text("Round: %d", level.RuntimeRound);
            ImGui::Text("Active: %llu", static_cast<unsigned long long>(level.RuntimeActiveActor));
            ImGui::Text("Selected Skill: %s", level.RuntimeSelectedSkillId.c_str());
            ImGui::TextWrapped("%s", level.RuntimeMessage.c_str());
        });
    }

    void DrawTurnCombatantComponent(Entity entity)
    {
        DrawComponent<TurnCombatantComponent>("Turn Combatant", entity, [](auto& combatant)
        {
            DrawTeamCombo(combatant.Team);
            ImGui::DragInt("Slot", &combatant.Slot, 1.0f, 0, 16);
            InputString("Display Name", combatant.DisplayName);
            InputString("Role Name", combatant.RoleName);
            ImGui::Checkbox("Controllable", &combatant.Controllable);
            ImGui::Checkbox("Invulnerable", &combatant.Invulnerable);

            ImGui::Separator();
            ImGui::DragFloat("Max HP", &combatant.MaxHealth, 1.0f, 1.0f, 99999.0f);
            ImGui::DragFloat("HP", &combatant.Health, 1.0f, 0.0f, combatant.MaxHealth);
            ImGui::DragFloat("Max MP", &combatant.MaxMana, 1.0f, 0.0f, 99999.0f);
            ImGui::DragFloat("MP", &combatant.Mana, 1.0f, 0.0f, combatant.MaxMana);
            ImGui::DragFloat("Attack", &combatant.Attack, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Magic", &combatant.Magic, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Defense", &combatant.Defense, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Speed", &combatant.Speed, 0.5f, 0.0f, 9999.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Skill Slots");
            InputString("Basic", combatant.BasicSkillId);
            InputString("Skill 1", combatant.Skill1Id);
            InputString("Skill 2", combatant.Skill2Id);
            InputString("Skill 3", combatant.Skill3Id);

            ImGui::Separator();
            ImGui::TextDisabled("UI Bindings");
            InputString("Health Bar", combatant.HealthBarEntityName);
            InputString("Mana Bar", combatant.ManaBarEntityName);
            InputString("Status Text", combatant.StatusTextEntityName);
            InputString("Target Button", combatant.TargetButtonEntityName);
            InputString("Target Marker", combatant.TargetMarkerEntityName);

            ImGui::Separator();
            ImGui::TextDisabled("Animation Frames");
            InputString("Idle Pattern", combatant.IdleFramePattern, 260);
            ImGui::DragInt("Idle Frames", &combatant.IdleFrameCount, 1.0f, 1, 64);
            DrawAtlasFrameSpec("Idle Atlas", combatant.IdleFrameAtlas, 260);
            InputString("Attack Pattern", combatant.AttackFramePattern, 260);
            ImGui::DragInt("Attack Frames", &combatant.AttackFrameCount, 1.0f, 1, 64);
            DrawAtlasFrameSpec("Attack Atlas", combatant.AttackFrameAtlas, 260);
            InputString("Hit Pattern", combatant.HitFramePattern, 260);
            ImGui::DragInt("Hit Frames", &combatant.HitFrameCount, 1.0f, 1, 64);
            DrawAtlasFrameSpec("Hit Atlas", combatant.HitFrameAtlas, 260);
            InputString("Down Pattern", combatant.DownFramePattern, 260);
            ImGui::DragInt("Down Frames", &combatant.DownFrameCount, 1.0f, 1, 64);
            DrawAtlasFrameSpec("Down Atlas", combatant.DownFrameAtlas, 260);
            ImGui::DragFloat("Frame Rate", &combatant.AnimationFrameRate, 0.25f, 1.0f, 60.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Alive: %s", combatant.RuntimeAlive ? "true" : "false");
            ImGui::Text("Guarding: %s", combatant.RuntimeGuarding ? "true" : "false");
            ImGui::Text("Hit Flash: %.2f", combatant.RuntimeHitFlashTimer);
        });
    }

} // namespace Wheatear
