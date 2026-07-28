#include "ArcadeCombatDrawer.h"

#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

namespace Wheatear {

    namespace {

        static bool InputString(const char* label, std::string& value, size_t capacity = 128)
        {
            std::vector<char> buffer(capacity, 0);
            strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
            if (ImGui::InputText(label, buffer.data(), buffer.size()))
            {
                value = buffer.data();
                return true;
            }
            return false;
        }

        static void DrawTeamCombo(int& team)
        {
            static const char* labels[] = { "Neutral", "Player", "Enemy" };
            int index = std::clamp(team, 0, 2);
            if (ImGui::Combo("Team", &index, labels, 3))
                team = index;
        }

        static void DrawWeaponCombo(ArcadeWeaponType& weapon)
        {
            static const char* labels[] = { "Gun", "Cannon", "Katana" };
            int index = std::clamp((int)weapon, 0, 2);
            if (ImGui::Combo("Current Weapon", &index, labels, 3))
                weapon = (ArcadeWeaponType)index;
        }

        static void DrawTriggerCombo(ArcadeTriggerType& type)
        {
            static const char* labels[] = { "Boss Intro" };
            int index = std::clamp((int)type, 0, 0);
            if (ImGui::Combo("Type", &index, labels, 1))
                type = (ArcadeTriggerType)index;
        }

    } // namespace

    void DrawArcadeCombatLevelComponent(Entity entity)
    {
        DrawComponent<ArcadeCombatLevelComponent>("Arcade Combat Level", entity, [](auto& level)
            {
                ImGui::Checkbox("Play On Start", &level.PlayOnStart);
                ImGui::DragFloat2("Arena Min", glm::value_ptr(level.ArenaMin), 0.05f);
                ImGui::DragFloat2("Arena Max", glm::value_ptr(level.ArenaMax), 0.05f);
                ImGui::DragFloat("Start Fade Duration", &level.StartFadeDuration, 0.02f, 0.0f, 5.0f);
                ImGui::DragFloat("Victory Return Delay", &level.VictoryReturnDelay, 0.02f, 0.0f, 10.0f);
                ImGui::DragFloat("Defeat Return Delay", &level.DefeatReturnDelay, 0.02f, 0.0f, 10.0f);
                ImGui::DragFloat("Result Scene Fade", &level.ResultSceneFadeDuration, 0.02f, 0.0f, 5.0f);
                ImGui::DragFloat("Boss Defeat Fade", &level.BossDefeatFadeDuration, 0.02f, 0.0f, 10.0f);
                InputString("Victory Command", level.VictorySceneCommand, 256);
                InputString("Defeat Command", level.DefeatSceneCommand, 256);

                ImGui::Separator();
                ImGui::TextDisabled("Scene Bindings");
                InputString("Player", level.PlayerEntityName);
                InputString("Boss", level.BossEntityName);
                InputString("Fade", level.FadeEntityName);
                InputString("Pause Panel", level.PausePanelEntityName);
                InputString("Message Text", level.MessageTextEntityName);
                InputString("Weapon Text", level.WeaponTextEntityName);
                InputString("Player Health Bar", level.PlayerHealthBarEntityName);
                InputString("Player Health Text", level.PlayerHealthTextEntityName);
                InputString("Boss Health Bar", level.BossHealthBarEntityName);
                InputString("Boss Health Text", level.BossHealthTextEntityName);

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
        DrawComponent<ArcadeCombatantComponent>("Arcade Combatant", entity, [](auto& combatant)
            {
                DrawTeamCombo(combatant.Team);
                ImGui::DragFloat("Max Health", &combatant.MaxHealth, 1.0f, 1.0f, 9999.0f);
                ImGui::DragFloat("Health", &combatant.Health, 1.0f, 0.0f, combatant.MaxHealth);
                ImGui::DragFloat("Move Speed", &combatant.MoveSpeed, 0.05f, 0.0f, 50.0f);
                ImGui::DragFloat("Collision Radius", &combatant.CollisionRadius, 0.01f, 0.01f, 10.0f);
                ImGui::Checkbox("Invulnerable", &combatant.Invulnerable);

                ImGui::Separator();
                ImGui::TextDisabled("Runtime State");
                ImGui::Text("Alive: %s", combatant.Alive ? "true" : "false");
                ImGui::Text("Controls Locked: %s", combatant.ControlsLocked ? "true" : "false");
            });
    }

    void DrawArcadePlayerControllerComponent(Entity entity)
    {
        DrawComponent<ArcadePlayerControllerComponent>("Arcade Player Controller", entity, [](auto& controller)
            {
                DrawWeaponCombo(controller.CurrentWeapon);
                ImGui::Checkbox("Auto Aim", &controller.AutoAim);

                ImGui::Separator();
                ImGui::TextDisabled("Runtime State");
                ImGui::Text("Cooldown: %.2f", controller.WeaponCooldown);
            });
    }

    void DrawArcadeBossComponent(Entity entity)
    {
        DrawComponent<ArcadeBossComponent>("Arcade Boss", entity, [](auto& boss)
            {
                ImGui::Checkbox("Active On Start", &boss.Active);
                ImGui::DragFloat3("Intro Start", glm::value_ptr(boss.IntroStartPosition), 0.05f);
                ImGui::DragFloat3("Fight Position", glm::value_ptr(boss.FightPosition), 0.05f);
                ImGui::DragFloat("Intro Duration", &boss.IntroDuration, 0.02f, 0.05f, 10.0f);
                ImGui::DragFloat("Shoot Interval", &boss.ShootInterval, 0.02f, 0.05f, 20.0f);
                ImGui::DragFloat("Jump Interval", &boss.JumpInterval, 0.02f, 0.05f, 20.0f);
                ImGui::DragFloat("Jump Duration", &boss.JumpDuration, 0.02f, 0.05f, 10.0f);

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
                ImGui::DragFloat("Damage", &projectile.Damage, 0.5f, 0.0f, 9999.0f);
                ImGui::DragFloat("Lifetime", &projectile.Lifetime, 0.02f, 0.0f, 20.0f);
                ImGui::DragFloat("Radius", &projectile.Radius, 0.01f, 0.01f, 10.0f);
                DrawTeamCombo(projectile.Team);
                ImGui::Checkbox("Heavy", &projectile.Heavy);
                ImGui::Checkbox("Melee", &projectile.Melee);
            });
    }

    void DrawArcadeCoverComponent(Entity entity)
    {
        DrawComponent<ArcadeCoverComponent>("Arcade Cover", entity, [](auto& cover)
            {
                ImGui::DragFloat("Radius", &cover.Radius, 0.01f, 0.01f, 10.0f);
                ImGui::DragFloat("Max Health", &cover.MaxHealth, 1.0f, 1.0f, 9999.0f);
                ImGui::DragFloat("Health", &cover.Health, 1.0f, 0.0f, cover.MaxHealth);
                ImGui::Checkbox("Blocks Projectiles", &cover.BlocksProjectiles);
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
