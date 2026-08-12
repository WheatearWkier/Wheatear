#include "TacticalCombatDrawer.h"

#include "Editor/CommandBuilder.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorWidgets.h"
#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatComponents.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Wheatear {

    namespace {

        using EditorCommandBuilder::DrawCommandBuilder;
        using EditorWidgets::DrawTeamCombo;
        using EditorWidgets::InputString;

        static const char* PhaseName(TacticalCombatPhase phase)
        {
            switch (phase)
            {
            case TacticalCombatPhase::Intro: return "Intro";
            case TacticalCombatPhase::PlayerTurn: return "Player Turn";
            case TacticalCombatPhase::AwaitCommand: return "Await Command";
            case TacticalCombatPhase::Targeting: return "Targeting";
            case TacticalCombatPhase::Acting: return "Acting";
            case TacticalCombatPhase::EnemyTurn: return "Enemy Turn";
            case TacticalCombatPhase::Victory: return "Victory";
            case TacticalCombatPhase::Defeat: return "Defeat";
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

            EditorContentPickers::DrawAssetField(sheetLabel.c_str(),
                atlas.SheetPath,
                EditorWidgets::AssetReferenceKind::Texture,
                pathWidth);
            ImGui::DragInt2(cellLabel.c_str(), &atlas.CellWidth, 1.0f, 0, 8192);
            ImGui::DragInt(columnsLabel.c_str(), &atlas.Columns, 1.0f, 0, 256);
            ImGui::DragInt(startLabel.c_str(), &atlas.StartFrame, 1.0f, 0, 4096);
        }

    } // namespace

    void DrawTacticalCombatLevelComponent(Entity entity)
    {
        DrawComponent<TacticalCombatLevelComponent>("Tactical Combat Level", entity, [entity](auto& level)
        {
            EditorWidgets::StatusBadge("Edits Scene", EditorWidgets::StatusKind::Success);
            ImGui::Checkbox("Play On Start", &level.PlayOnStart);
            InputString("Level Id", level.LevelId, 240);

            ImGui::Separator();
            ImGui::TextDisabled("Grid");
            ImGui::DragInt("Grid Width", &level.GridWidth, 1.0f, 1, 32);
            ImGui::DragInt("Grid Height", &level.GridHeight, 1.0f, 1, 32);
            ImGui::DragFloat2("Board Origin", &level.BoardOrigin.x, 0.002f, 0.0f, 1.0f);
            ImGui::DragFloat2("Cell Size", &level.CellSize.x, 0.002f, 0.005f, 0.3f);
            InputString("Cell Prefix", level.CellEntityPrefix);
            InputString("Unit Prefix", level.UnitEntityPrefix);

            ImGui::Separator();
            ImGui::TextDisabled("Timing");
            ImGui::DragFloat("Start Fade", &level.StartFadeDuration, 0.02f, 0.0f, 5.0f);
            ImGui::DragFloat("Intro Duration", &level.IntroDuration, 0.02f, 0.0f, 10.0f);
            ImGui::DragFloat("Action Duration", &level.ActionDuration, 0.02f, 0.1f, 5.0f);
            ImGui::DragFloat("Enemy Step", &level.EnemyStepDuration, 0.02f, 0.1f, 5.0f);
            ImGui::DragFloat("Victory Delay", &level.VictoryReturnDelay, 0.05f, 0.0f, 10.0f);
            ImGui::DragFloat("Defeat Delay", &level.DefeatReturnDelay, 0.05f, 0.0f, 10.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Scene Bindings");
            EditorContentPickers::DrawSceneEntityField("Fade", entity, level.FadeEntityName);
            EditorContentPickers::DrawSceneEntityField("Message Text", entity, level.MessageTextEntityName);
            EditorContentPickers::DrawSceneEntityField("Phase Text", entity, level.PhaseTextEntityName);
            EditorContentPickers::DrawSceneEntityField("Detail Text", entity, level.DetailTextEntityName);
            EditorContentPickers::DrawSceneEntityField("Command Panel", entity, level.CommandPanelEntityName);
            EditorContentPickers::DrawSceneEntityField("Action Effect", entity, level.ActionEffectEntityName);
            DrawCommandBuilder("Victory Command", level.VictorySceneCommand, 300);
            DrawCommandBuilder("Defeat Command", level.DefeatSceneCommand, 300);

            ImGui::Separator();
            ImGui::TextDisabled("Tile Highlight Colors");
            ImGui::ColorEdit4("Normal", &level.TileNormalColor.x);
            ImGui::ColorEdit4("Move", &level.TileMoveColor.x);
            ImGui::ColorEdit4("Attack", &level.TileAttackColor.x);
            ImGui::ColorEdit4("Selected", &level.TileSelectedColor.x);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Phase: %s", PhaseName(level.RuntimePhase));
            ImGui::Text("Round: %d", level.RuntimeRound);
            ImGui::Text("Selected: %llu", static_cast<unsigned long long>(level.RuntimeSelectedUnit));
            ImGui::Text("Skill: %s", level.RuntimeSelectedSkillId.c_str());
            ImGui::TextWrapped("%s", level.RuntimeMessage.c_str());
        });
    }

    void DrawTacticalUnitComponent(Entity entity)
    {
        DrawComponent<TacticalUnitComponent>("Tactical Unit", entity, [entity](auto& unit)
        {
            EditorWidgets::StatusBadge("Edits Scene", EditorWidgets::StatusKind::Success);
            DrawTeamCombo(unit.Team);
            ImGui::DragInt("Slot", &unit.Slot, 1.0f, 0, 64);
            ImGui::DragInt("Grid X", &unit.GridX, 1.0f, 0, 64);
            ImGui::DragInt("Grid Y", &unit.GridY, 1.0f, 0, 64);
            InputString("Display Name", unit.DisplayName);
            InputString("Class Name", unit.ClassName);
            ImGui::Checkbox("Controllable", &unit.Controllable);
            ImGui::Checkbox("Invulnerable", &unit.Invulnerable);

            ImGui::Separator();
            ImGui::TextDisabled("Stats");
            ImGui::DragFloat("Max HP", &unit.MaxHealth, 1.0f, 1.0f, 99999.0f);
            ImGui::DragFloat("HP", &unit.Health, 1.0f, 0.0f, unit.MaxHealth);
            ImGui::DragFloat("Attack", &unit.Attack, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Magic", &unit.Magic, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Defense", &unit.Defense, 0.5f, 0.0f, 9999.0f);
            ImGui::DragInt("Move Range", &unit.MoveRange, 1.0f, 0, 16);
            ImGui::DragInt("Attack Range", &unit.AttackRange, 1.0f, 0, 16);

            ImGui::Separator();
            ImGui::TextDisabled("Skill Slots");
            InputString("Basic", unit.BasicSkillId);
            InputString("Skill 1", unit.Skill1Id);
            InputString("Skill 2", unit.Skill2Id);

            ImGui::Separator();
            ImGui::TextDisabled("UI Bindings");
            EditorContentPickers::DrawSceneEntityField("HP Bar", entity, unit.HealthBarEntityName);
            EditorContentPickers::DrawSceneEntityField("Status Text", entity, unit.StatusTextEntityName);
            EditorContentPickers::DrawSceneEntityField("Marker", entity, unit.MarkerEntityName);

            ImGui::Separator();
            ImGui::TextDisabled("Animation Frames");
            InputString("Idle Pattern", unit.IdleFramePattern, 280);
            ImGui::DragInt("Idle Frames", &unit.IdleFrameCount, 1.0f, 1, 64);
            DrawAtlasFrameSpec("Idle Atlas", unit.IdleFrameAtlas, 280);
            InputString("Attack Pattern", unit.AttackFramePattern, 280);
            ImGui::DragInt("Attack Frames", &unit.AttackFrameCount, 1.0f, 1, 64);
            DrawAtlasFrameSpec("Attack Atlas", unit.AttackFrameAtlas, 280);
            InputString("Hit Pattern", unit.HitFramePattern, 280);
            ImGui::DragInt("Hit Frames", &unit.HitFrameCount, 1.0f, 1, 64);
            DrawAtlasFrameSpec("Hit Atlas", unit.HitFrameAtlas, 280);
            InputString("Down Pattern", unit.DownFramePattern, 280);
            ImGui::DragInt("Down Frames", &unit.DownFrameCount, 1.0f, 1, 64);
            DrawAtlasFrameSpec("Down Atlas", unit.DownFrameAtlas, 280);
            ImGui::DragFloat("Frame Rate", &unit.AnimationFrameRate, 0.25f, 1.0f, 60.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Alive: %s", unit.RuntimeAlive ? "true" : "false");
            ImGui::Text("Acted: %s", unit.RuntimeHasActed ? "true" : "false");
            ImGui::Text("Moved: %s", unit.RuntimeMoved ? "true" : "false");
            ImGui::Text("Guarding: %s", unit.RuntimeGuarding ? "true" : "false");
        });
    }

} // namespace Wheatear
