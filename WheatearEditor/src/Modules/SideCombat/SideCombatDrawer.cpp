#include "SideCombatDrawer.h"

#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

namespace Wheatear {

    namespace {

        static bool InputString(const char* label, std::string& value, size_t capacity = 160)
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

        static void DrawEnemyKindCombo(SideEnemyKind& kind)
        {
            static const char* labels[] = { "Grunt", "Thrower", "Pouncer", "Bear Boss" };
            int index = std::clamp((int)kind, 0, 3);
            if (ImGui::Combo("Kind", &index, labels, 4))
                kind = (SideEnemyKind)index;
        }

        static void DrawAttackKindCombo(SideAttackKind& kind)
        {
            static const char* labels[] = {
                "Basic",
                "Launcher",
                "Magic Bolt",
                "Ally Support",
                "Enemy Melee",
                "Enemy Projectile",
                "Enemy Shockwave",
                "Break Limit"
            };
            int index = std::clamp((int)kind, 0, 7);
            if (ImGui::Combo("Attack Kind", &index, labels, 8))
                kind = (SideAttackKind)index;
        }

        static const char* GetStateLabel(SideCombatState state)
        {
            static const char* labels[] = {
                "Normal",
                "Hit Stun",
                "Launched",
                "Knockdown",
                "Recovery",
                "Super Armor",
                "Broken",
                "Dead"
            };
            return labels[std::clamp((int)state, 0, 7)];
        }

    } // namespace

    void DrawSideCombatLevelComponent(Entity entity)
    {
        DrawComponent<SideCombatLevelComponent>("Side Combat Level", entity, [](auto& level)
        {
            ImGui::Checkbox("Play On Start", &level.PlayOnStart);
            InputString("Level Id / Unlock Profile", level.LevelId);
            ImGui::TextDisabled("Level Id selects progression.profiles in the tuning YAML.");
            InputString("Tuning Path", level.TuningPath, 260);
            ImGui::DragFloat2("Arena Min", glm::value_ptr(level.ArenaMin), 0.05f);
            ImGui::DragFloat2("Arena Max", glm::value_ptr(level.ArenaMax), 0.05f);
            ImGui::DragFloat("Ground Y", &level.GroundY, 0.02f, -20.0f, 20.0f);
            ImGui::DragFloat("Lane Min Y", &level.LaneMinY, 0.02f, -20.0f, 20.0f);
            ImGui::DragFloat("Lane Max Y", &level.LaneMaxY, 0.02f, -20.0f, 20.0f);
            ImGui::DragFloat("Start Fade", &level.StartFadeDuration, 0.02f, 0.0f, 5.0f);
            ImGui::DragFloat("Victory Return Delay", &level.VictoryReturnDelay, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Defeat Return Delay", &level.DefeatReturnDelay, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Result Fade", &level.ResultSceneFadeDuration, 0.02f, 0.0f, 5.0f);
            ImGui::DragFloat("Combo Drop Delay", &level.ComboDropDelay, 0.02f, 0.2f, 5.0f);
            InputString("Victory Command", level.VictorySceneCommand, 260);
            InputString("Defeat Command", level.DefeatSceneCommand, 260);
            InputString("First Clear Reward", level.FirstClearRewardText, 260);

            ImGui::Separator();
            ImGui::TextDisabled("Scene Bindings");
            InputString("Player", level.PlayerEntityName);
            InputString("Boss", level.BossEntityName);
            InputString("Fade", level.FadeEntityName);
            InputString("Message Text", level.MessageTextEntityName);
            InputString("Combo Text", level.ComboTextEntityName);
            InputString("Skill Text", level.SkillTextEntityName);
            InputString("Reward Text", level.RewardTextEntityName);
            InputString("Player Health Bar", level.PlayerHealthBarEntityName);
            InputString("Player Health Text", level.PlayerHealthTextEntityName);
            InputString("Boss Health Bar", level.BossHealthBarEntityName);
            InputString("Boss Health Text", level.BossHealthTextEntityName);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Result: %s", level.RuntimeVictory ? "Victory" : (level.RuntimeDefeat ? "Defeat" : "Playing"));
            ImGui::Text("Combo: %d / Best: %d", level.RuntimeComboCount, level.RuntimeBestCombo);
            ImGui::Text("Collected Pickups: %d", level.RuntimeCollectedPickups);
        });
    }

    void DrawSideCombatantComponent(Entity entity)
    {
        DrawComponent<SideCombatantComponent>("Side Combatant", entity, [](auto& combatant)
        {
            DrawTeamCombo(combatant.Team);
            ImGui::DragFloat("Max Health", &combatant.MaxHealth, 1.0f, 1.0f, 99999.0f);
            ImGui::DragFloat("Health", &combatant.Health, 1.0f, 0.0f, combatant.MaxHealth);
            ImGui::DragFloat("Attack", &combatant.Attack, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Defense", &combatant.Defense, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Move Speed", &combatant.MoveSpeed, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat2("Collision Size", glm::value_ptr(combatant.CollisionSize), 0.02f, 0.05f, 20.0f);
            ImGui::DragFloat("Collision Height", &combatant.CollisionHeight, 0.02f, 0.05f, 20.0f);
            ImGui::DragFloat("Gravity Scale", &combatant.GravityScale, 0.02f, 0.0f, 5.0f);
            ImGui::DragFloat("Knockback Resistance", &combatant.KnockbackResistance, 0.01f, 0.0f, 0.95f);
            ImGui::Checkbox("Invulnerable", &combatant.Invulnerable);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Alive: %s", combatant.Alive ? "true" : "false");
            ImGui::Text("State: %s / %.2f", GetStateLabel(combatant.RuntimeState), combatant.RuntimeStateTimer);
            ImGui::Text("On Ground: %s", combatant.RuntimeOnGround ? "true" : "false");
            ImGui::Text("Velocity: %.2f, %.2f", combatant.RuntimeVelocity.x, combatant.RuntimeVelocity.y);
            ImGui::Text("Ground: %.2f, %.2f / Air %.2f", combatant.RuntimeGroundPosition.x, combatant.RuntimeGroundPosition.y, combatant.RuntimeAirHeight);
            ImGui::Text("Hit Stun: %.2f", combatant.RuntimeHitStun);
            ImGui::Text("Protection: %.1f / %.1f", combatant.RuntimeProtection, combatant.RuntimeProtectionMax);
        });
    }

    void DrawSidePlayerControllerComponent(Entity entity)
    {
        DrawComponent<SidePlayerControllerComponent>("Side Player Controller", entity, [](auto& controller)
        {
            ImGui::DragInt("Max Jumps", &controller.MaxJumps, 1.0f, 1, 3);
            ImGui::DragFloat("Jump Impulse", &controller.JumpImpulse, 0.05f, 0.0f, 40.0f);
            ImGui::DragFloat("Gravity", &controller.Gravity, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat("Air Control", &controller.AirControl, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat("Lane Speed Scale", &controller.LaneSpeedScale, 0.01f, 0.0f, 3.0f);
            ImGui::DragFloat("Lane Acceleration", &controller.LaneAcceleration, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat("Ground Friction", &controller.GroundFriction, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat("Basic Cooldown", &controller.BasicCooldown, 0.01f, 0.01f, 5.0f);
            ImGui::DragFloat("Launcher Cooldown", &controller.LauncherCooldown, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Magic Bolt Cooldown", &controller.MagicBoltCooldown, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Ally Support Cooldown", &controller.AllySupportCooldown, 0.01f, 0.01f, 30.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime Cooldowns");
            ImGui::Text("Basic %.2f / Launcher %.2f / Magic %.2f / Support %.2f",
                controller.RuntimeBasicCooldown,
                controller.RuntimeLauncherCooldown,
                controller.RuntimeMagicBoltCooldown,
                controller.RuntimeAllySupportCooldown);
            ImGui::Text("Jumps: %d", controller.RuntimeJumpsRemaining);
            ImGui::Text("Air Actions: %d / Break Limit %.2f",
                controller.RuntimeAirActionsRemaining,
                controller.RuntimeBreakLimitCooldown);
            ImGui::Text("Magic Sword Gauge: %.2f / %.2f",
                controller.RuntimeMagicSwordGauge,
                controller.RuntimeMagicSwordGaugeMax);
        });
    }

    void DrawSideEnemyAIComponent(Entity entity)
    {
        DrawComponent<SideEnemyAIComponent>("Side Enemy AI", entity, [](auto& ai)
        {
            DrawEnemyKindCombo(ai.Kind);
            ImGui::DragFloat("Aggro Range", &ai.AggroRange, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat("Attack Range", &ai.AttackRange, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Preferred Range", &ai.PreferredRange, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Attack Interval", &ai.AttackInterval, 0.02f, 0.05f, 20.0f);
            ImGui::DragFloat("Patrol Min X", &ai.PatrolMinX, 0.05f);
            ImGui::DragFloat("Patrol Max X", &ai.PatrolMaxX, 0.05f);
            ImGui::DragFloat("Lane Tolerance", &ai.LaneTolerance, 0.02f, 0.0f, 5.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Attack Timer: %.2f", ai.RuntimeAttackTimer);
            ImGui::Text("Awake: %s", ai.RuntimeAwake ? "true" : "false");
        });
    }

    void DrawSideHitboxComponent(Entity entity)
    {
        DrawComponent<SideHitboxComponent>("Side Hitbox", entity, [](auto& hitbox)
        {
            DrawTeamCombo(hitbox.Team);
            DrawAttackKindCombo(hitbox.AttackKind);
            ImGui::DragFloat2("Size", glm::value_ptr(hitbox.Size), 0.02f, 0.01f, 20.0f);
            ImGui::DragFloat2("Velocity", glm::value_ptr(hitbox.Velocity), 0.05f);
            ImGui::DragFloat2("Launch Velocity", glm::value_ptr(hitbox.LaunchVelocity), 0.05f);
            ImGui::DragFloat("Air Height", &hitbox.AirHeight, 0.02f, 0.0f, 20.0f);
            ImGui::DragFloat("Air Range", &hitbox.AirRange, 0.02f, 0.01f, 20.0f);
            ImGui::DragFloat("Damage", &hitbox.Damage, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Lifetime", &hitbox.Lifetime, 0.01f, 0.0f, 20.0f);
            ImGui::DragFloat("Hit Stun", &hitbox.HitStun, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Attacker Air Impulse", &hitbox.AttackerAirImpulse, 0.02f, -20.0f, 20.0f);
            ImGui::DragFloat("Attacker Air Fall Step", &hitbox.AttackerAirFallStep, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Target Air Fall Step", &hitbox.TargetAirFallStep, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Protection Gain", &hitbox.ProtectionGain, 0.1f, 0.0f, 200.0f);
            ImGui::Checkbox("Destroy On Hit", &hitbox.DestroyOnHit);
            InputString("Frame Pattern", hitbox.TextureFramePattern, 260);
            ImGui::DragInt("Frame Count", &hitbox.TextureFrameCount, 1.0f, 1, 64);
            ImGui::DragFloat("Frame Rate", &hitbox.TextureFrameRate, 0.5f, 1.0f, 60.0f);
        });
    }

    void DrawSidePickupComponent(Entity entity)
    {
        DrawComponent<SidePickupComponent>("Side Pickup", entity, [](auto& pickup)
        {
            InputString("Item Id", pickup.ItemId);
            InputString("Display Name", pickup.DisplayName);
            ImGui::DragInt("Amount", &pickup.Amount, 1.0f, 1, 999);
            ImGui::DragFloat("Pickup Radius", &pickup.PickupRadius, 0.01f, 0.01f, 5.0f);
            ImGui::DragFloat("Attract Radius", &pickup.AttractRadius, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Attract Speed", &pickup.AttractSpeed, 0.05f, 0.0f, 40.0f);
        });
    }

} // namespace Wheatear
