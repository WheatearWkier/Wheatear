#include "wepch.h"
#include "ArcadeCombatDrawer.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatComponents.h"

#include "Editor/CommandBuilder.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Modules/ArcadeCombat/ArcadeCombatTuningEditorPanel.h"
#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatTuningService.h"
#include "Wheatear/Gameplay/SystemBindingRegistry.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <filesystem>

namespace Wheatear {

    namespace {

        using EditorCommandBuilder::DrawCommandBuilder;
        using EditorWidgets::DrawTeamCombo;
        using EditorWidgets::InputString;

        static void DrawWeaponCombo(ArcadeWeaponType& weapon)
        {
            static const char* labels[] = { "Gun", "Cannon", "Katana" };
            int index = std::clamp((int)weapon, 0, 2);
            if (ImGui::Combo(EditorLocale::Text("Current Weapon", "当前武器"), &index, labels, 3))
                weapon = (ArcadeWeaponType)index;
        }

        static void DrawTriggerCombo(ArcadeTriggerType& type)
        {
            static const char* labels[] = { "Boss Intro" };
            int index = std::clamp((int)type, 0, 0);
            if (ImGui::Combo(EditorLocale::Text("Type", "类型"), &index, labels, 1))
                type = (ArcadeTriggerType)index;
        }

        static const ArcadeCombatLevelComponent* FindArcadeLevel(Scene* scene)
        {
            if (!scene)
                return nullptr;

            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<ArcadeCombatLevelComponent>())
                return &registry.get<ArcadeCombatLevelComponent>(entity);
            return nullptr;
        }

        static bool IsArcadeTuningLoaded(Scene* scene, std::filesystem::path* tuningPath = nullptr)
        {
            const ArcadeCombatLevelComponent* level = FindArcadeLevel(scene);
            if (!level)
                return false;

            const std::filesystem::path path = ArcadeCombatTuningService::TuningSourcePath(*level);
            if (tuningPath)
                *tuningPath = path;
            return !path.empty() && std::filesystem::exists(path);
        }

        static bool IsArcadeEntityName(const Entity& entity, const std::string& targetName)
        {
            return entity && entity.GetName() == targetName;
        }

        static void DrawArcadeTuningAuthorityTools(Scene* scene, const char* message)
        {
            const ArcadeCombatLevelComponent* level = FindArcadeLevel(scene);
            if (!level)
                return;

            EditorWidgets::InlineStatus(message, EditorWidgets::StatusKind::Warning);
            ImGui::SameLine();
            ImGui::PushID(message);
            if (ImGui::Button(EditorLocale::Text("Open Arcade Combat Tuning Editor", "打开街机战斗调参编辑器")))
                ArcadeCombatEditorRequests::RequestOpenTuning(level->TuningPath);
            ImGui::PopID();
        }

    } // namespace

    void DrawArcadeCombatLevelComponent(Entity entity)
    {
        DrawComponent<ArcadeCombatLevelComponent>("Arcade Combat Level", entity, [entity](auto& level)
            {
                EditorWidgets::StatusBadge("Edits Scene", EditorWidgets::StatusKind::Success);
                ImGui::Checkbox(EditorLocale::Text("Play On Start", "开始时播放"), &level.PlayOnStart);
                ImGui::DragFloat2("Arena Min", glm::value_ptr(level.ArenaMin), 0.05f);
                ImGui::DragFloat2("Arena Max", glm::value_ptr(level.ArenaMax), 0.05f);
                const std::filesystem::path tuningPath = ArcadeCombatTuningService::TuningSourcePath(level);
                const bool tuningLoaded = !tuningPath.empty() && std::filesystem::exists(tuningPath);
                if (tuningLoaded)
                {
                    EditorWidgets::InlineStatus(
                        "Arcade tuning YAML is authoritative for the flow timings below.",
                        EditorWidgets::StatusKind::Warning);
                    ImGui::SameLine();
                    if (ImGui::Button(EditorLocale::Text("Open Arcade Combat Tuning Editor", "打开街机战斗调参编辑器")))
                        ArcadeCombatEditorRequests::RequestOpenTuning(level.TuningPath);
                }
                else if (!level.TuningPath.empty())
                {
                    EditorWidgets::InlineStatus(
                        "Arcade tuning YAML was not found; scene flow timing fields remain editable fallbacks.",
                        EditorWidgets::StatusKind::Warning);
                }
                ImGui::BeginDisabled(tuningLoaded);
                ImGui::DragFloat("Start Fade Duration", &level.StartFadeDuration, 0.02f, 0.0f, 5.0f);
                ImGui::DragFloat("Victory Return Delay", &level.VictoryReturnDelay, 0.02f, 0.0f, 10.0f);
                ImGui::DragFloat(EditorLocale::Text("Defeat Return Delay", "战败返回延迟"), &level.DefeatReturnDelay, 0.02f, 0.0f, 10.0f);
                ImGui::DragFloat("Result Scene Fade", &level.ResultSceneFadeDuration, 0.02f, 0.0f, 5.0f);
                ImGui::DragFloat(EditorLocale::Text("Boss Defeat Fade", "Boss 击败淡出"), &level.BossDefeatFadeDuration, 0.02f, 0.0f, 10.0f);
                ImGui::EndDisabled();
                DrawCommandBuilder("Victory Command", level.VictorySceneCommand, 256);
                DrawCommandBuilder("Defeat Command", level.DefeatSceneCommand, 256);

                ImGui::Separator();
                ImGui::TextDisabled("Scene Bindings");
                EditorContentPickers::DrawSceneEntityField("Player", entity, level.PlayerEntityName);
                EditorContentPickers::DrawSceneEntityField("Boss", entity, level.BossEntityName);
                EditorContentPickers::DrawSceneEntityField("Fade", entity, level.FadeEntityName);
                EditorContentPickers::DrawSceneEntityField("Pause Panel", entity, level.PausePanelEntityName);
                EditorContentPickers::DrawSceneEntityField("Message Text", entity, level.MessageTextEntityName);
                EditorContentPickers::DrawSceneEntityField("Weapon Text", entity, level.WeaponTextEntityName);
                EditorContentPickers::DrawSceneEntityField("Player Health Bar", entity, level.PlayerHealthBarEntityName);
                EditorContentPickers::DrawSceneEntityField("Player Health Text", entity, level.PlayerHealthTextEntityName);
                EditorContentPickers::DrawSceneEntityField("Boss Health Bar", entity, level.BossHealthBarEntityName);
                EditorContentPickers::DrawSceneEntityField("Boss Health Text", entity, level.BossHealthTextEntityName);

                ImGui::Separator();
                ImGui::TextDisabled("Runtime State");
                ImGui::Text("Paused: %s", level.RuntimePaused ? "true" : "false");
                ImGui::Text("Boss Intro: %s / %s",
                    level.RuntimeBossIntroStarted ? "started" : "waiting",
                    level.RuntimeBossIntroFinished ? "finished" : "running");
                ImGui::Text("Result: %s",
                    level.RuntimeVictory ? "victory" : (level.RuntimeDefeat ? "defeat" : "playing"));
            });
    }

    void DrawArcadeCombatantComponent(Entity entity)
    {
        DrawComponent<ArcadeCombatantComponent>("Arcade Combatant", entity, [entity](auto& combatant)
            {
                Scene* scene = entity.GetScene();
                const ArcadeCombatLevelComponent* level = FindArcadeLevel(scene);
                const bool tuningLoaded = IsArcadeTuningLoaded(scene);
                DrawTeamCombo(combatant.Team);
                ImGui::DragFloat("Max Health", &combatant.MaxHealth, 1.0f, 1.0f, 9999.0f);
                ImGui::DragFloat("Health", &combatant.Health, 1.0f, 0.0f, combatant.MaxHealth);
                const bool lockMoveSpeed = tuningLoaded && level &&
                    (IsArcadeEntityName(entity, level->PlayerEntityName) || IsArcadeEntityName(entity, level->BossEntityName));
                if (lockMoveSpeed)
                    DrawArcadeTuningAuthorityTools(scene, "Arcade tuning YAML is authoritative for Move Speed.");
                ImGui::BeginDisabled(lockMoveSpeed);
                ImGui::DragFloat("Move Speed", &combatant.MoveSpeed, 0.05f, 0.0f, 50.0f);
                ImGui::EndDisabled();
                ImGui::DragFloat(EditorLocale::Text("Collision Radius", "碰撞半径"), &combatant.CollisionRadius, 0.01f, 0.01f, 10.0f);
                ImGui::Checkbox(EditorLocale::Text("Invulnerable", "无敌"), &combatant.Invulnerable);

                ImGui::Separator();
                ImGui::TextDisabled("Runtime State");
                ImGui::Text("Alive: %s", combatant.Alive ? "true" : "false");
                ImGui::Text("Controls Locked: %s", combatant.ControlsLocked ? "true" : "false");
            });
    }

    void DrawArcadePlayerControllerComponent(Entity entity)
    {
        DrawComponent<ArcadePlayerControllerComponent>("Arcade Player Controller", entity, [entity](auto& controller)
            {
                Scene* scene = entity.GetScene();
                const ArcadeCombatLevelComponent* level = FindArcadeLevel(scene);
                const bool tuningLoaded = IsArcadeTuningLoaded(scene);
                const bool lockAutoAim = tuningLoaded && level && IsArcadeEntityName(entity, level->PlayerEntityName);
                if (lockAutoAim)
                    DrawArcadeTuningAuthorityTools(scene, "Arcade tuning YAML is authoritative for Auto Aim.");
                DrawWeaponCombo(controller.CurrentWeapon);
                ImGui::BeginDisabled(lockAutoAim);
                ImGui::Checkbox(EditorLocale::Text("Auto Aim", "自动瞄准"), &controller.AutoAim);
                ImGui::EndDisabled();

                ImGui::Separator();
                ImGui::TextDisabled("Runtime State");
                ImGui::Text("Cooldown: %.2f", controller.WeaponCooldown);
            });
    }

    void DrawArcadeBossComponent(Entity entity)
    {
        DrawComponent<ArcadeBossComponent>("Arcade Boss", entity, [entity](auto& boss)
            {
                Scene* scene = entity.GetScene();
                const ArcadeCombatLevelComponent* level = FindArcadeLevel(scene);
                const bool tuningLoaded = IsArcadeTuningLoaded(scene);
                const bool lockBossTuning = tuningLoaded && level && IsArcadeEntityName(entity, level->BossEntityName);
                if (lockBossTuning)
                    DrawArcadeTuningAuthorityTools(scene, "Arcade tuning YAML is authoritative for boss timing fields.");
                ImGui::Checkbox(EditorLocale::Text("Active On Start", "开始时激活"), &boss.Active);
                ImGui::DragFloat3("Intro Start", glm::value_ptr(boss.IntroStartPosition), 0.05f);
                ImGui::DragFloat3("Fight Position", glm::value_ptr(boss.FightPosition), 0.05f);
                ImGui::BeginDisabled(lockBossTuning);
                ImGui::DragFloat("Intro Duration", &boss.IntroDuration, 0.02f, 0.05f, 10.0f);
                ImGui::DragFloat("Shoot Interval", &boss.ShootInterval, 0.02f, 0.05f, 20.0f);
                ImGui::DragFloat("Jump Interval", &boss.JumpInterval, 0.02f, 0.05f, 20.0f);
                ImGui::DragFloat("Jump Duration", &boss.JumpDuration, 0.02f, 0.05f, 10.0f);
                ImGui::EndDisabled();

                ImGui::Separator();
                ImGui::TextDisabled("Runtime State");
                ImGui::Text("Intro Timer: %.2f", boss.RuntimeIntroTimer);
                ImGui::Text("Jumping: %s", boss.RuntimeJumping ? "true" : "false");
            });
    }

    void DrawArcadeProjectileComponent(Entity entity)
    {
        DrawComponent<ArcadeProjectileComponent>("Arcade Projectile", entity, [](auto& projectile)
            {
                ImGui::DragFloat2("Velocity", glm::value_ptr(projectile.Velocity), 0.05f);
                ImGui::DragFloat(EditorLocale::Text("Damage", "伤害"), &projectile.Damage, 0.5f, 0.0f, 9999.0f);
                ImGui::DragFloat("Lifetime", &projectile.Lifetime, 0.02f, 0.0f, 20.0f);
                ImGui::DragFloat("Radius", &projectile.Radius, 0.01f, 0.01f, 10.0f);
                DrawTeamCombo(projectile.Team);
                ImGui::Checkbox(EditorLocale::Text("Heavy", "重型"), &projectile.Heavy);
                ImGui::Checkbox(EditorLocale::Text("Melee", "近战"), &projectile.Melee);
            });
    }

    void DrawArcadeCoverComponent(Entity entity)
    {
        DrawComponent<ArcadeCoverComponent>("Arcade Cover", entity, [](auto& cover)
            {
                ImGui::DragFloat("Radius", &cover.Radius, 0.01f, 0.01f, 10.0f);
                ImGui::DragFloat("Max Health", &cover.MaxHealth, 1.0f, 1.0f, 9999.0f);
                ImGui::DragFloat("Health", &cover.Health, 1.0f, 0.0f, cover.MaxHealth);
                ImGui::Checkbox(EditorLocale::Text("Blocks Projectiles", "阻挡子弹"), &cover.BlocksProjectiles);
            });
    }

    void DrawArcadeTriggerComponent(Entity entity)
    {
        DrawComponent<ArcadeTriggerComponent>("Arcade Trigger", entity, [](auto& trigger)
            {
                DrawTriggerCombo(trigger.Type);
                InputString("Name", trigger.TriggerName);
                ImGui::DragFloat("Radius", &trigger.Radius, 0.01f, 0.01f, 10.0f);

                ImGui::Separator();
                ImGui::TextDisabled("Runtime State");
                ImGui::Text("Triggered: %s", trigger.Triggered ? "true" : "false");
            });
    }

} // namespace Wheatear
