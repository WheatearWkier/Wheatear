#include "wtpch.h"
#include "ArcadeCombatSystem.h"

#include "Wheatear/Core/Input.h"
#include "Wheatear/Core/KeyCodes.h"
#include "Wheatear/Core/MouseButtonCodes.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

namespace Wheatear {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetImageAlpha;
        using UIRuntimeTools::SetProgress;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetVisible;

        constexpr float Pi = 3.1415926535f;

        static glm::vec2 ToVec2(const glm::vec3& value)
        {
            return { value.x, value.y };
        }

        static float Distance2D(const glm::vec3& a, const glm::vec3& b)
        {
            return glm::length(ToVec2(a) - ToVec2(b));
        }

        static glm::vec2 DirectionTo(const glm::vec3& from, const glm::vec3& to)
        {
            glm::vec2 direction = ToVec2(to) - ToVec2(from);
            if (glm::length2(direction) <= 0.0001f)
                return { 1.0f, 0.0f };
            return glm::normalize(direction);
        }

        static const char* WeaponName(ArcadeWeaponType weapon)
        {
            switch (weapon)
            {
            case ArcadeWeaponType::Gun:    return "手枪";
            case ArcadeWeaponType::Cannon: return "重炮";
            case ArcadeWeaponType::Katana: return "太刀";
            }
            return "手枪";
        }

        static void ApplyDamage(ArcadeCombatantComponent& target, float damage)
        {
            if (!target.Alive || target.Invulnerable)
                return;

            target.Health = std::max(0.0f, target.Health - std::max(0.0f, damage));
            target.Alive = target.Health > 0.0f;
        }

        static Ref<Texture2D> LoadBattleTexture(const std::string& texturePath)
        {
            if (texturePath.empty())
                return nullptr;

            static std::unordered_map<std::string, Ref<Texture2D>> textureCache;
            if (auto it = textureCache.find(texturePath); it != textureCache.end())
                return it->second;

            Ref<Texture2D> texture = Texture2D::Create(texturePath);
            if (!texture || !texture->IsLoaded())
                return nullptr;

            textureCache[texturePath] = texture;
            return texture;
        }

        static const char* ResolveProjectileTexturePath(int team, bool heavy, bool melee)
        {
            if (melee)
                return "assets/vertical_slice/arcade_combat/effects/vfx_arcade_katana_slash.png";
            if (team == (int)ArcadeTeam::Enemy)
                return "assets/vertical_slice/arcade_combat/projectiles/proj_arcade_boss_orb.png";
            return heavy
                ? "assets/vertical_slice/arcade_combat/effects/vfx_arcade_cannon_blast.png"
                : "assets/vertical_slice/arcade_combat/projectiles/proj_arcade_player_bolt.png";
        }

        static void CreateProjectile(Scene* scene,
            const std::string& name,
            const glm::vec3& position,
            const glm::vec2& velocity,
            float damage,
            float lifetime,
            float radius,
            int team,
            const glm::vec4& color,
            bool heavy = false,
            bool melee = false)
        {
            Entity projectile = scene->CreateEntity(name);
            auto& transform = projectile.GetComponent<TransformComponent>();
            transform.Translation = position;
            transform.Scale = melee
                ? glm::vec3(radius * 1.65f, radius * 0.36f, 1.0f)
                : glm::vec3(radius * 2.0f, radius * 2.0f, 1.0f);

            if (melee)
                transform.Rotation.z = std::atan2(velocity.y, velocity.x);

            auto& sprite = projectile.AddComponent<SpriteRendererComponent>();
            sprite.Color = color;
            if (Ref<Texture2D> texture = LoadBattleTexture(ResolveProjectileTexturePath(team, heavy, melee)))
            {
                sprite.Texture = texture;
                sprite.Color = { 1.0f, 1.0f, 1.0f, color.a };
            }

            auto& projectileComponent = projectile.AddComponent<ArcadeProjectileComponent>();
            projectileComponent.Velocity = velocity;
            projectileComponent.Damage = damage;
            projectileComponent.Lifetime = lifetime;
            projectileComponent.Radius = radius;
            projectileComponent.Team = team;
            projectileComponent.Heavy = heavy;
            projectileComponent.Melee = melee;
        }

        static void DestroyProjectiles(Scene* scene)
        {
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            std::vector<entt::entity> projectiles;
            for (auto e : registry.view<ArcadeProjectileComponent>())
                projectiles.push_back(e);

            for (auto e : projectiles)
            {
                if (registry.valid(e))
                    scene->DestroyEntity({ e, scene });
            }
        }

        static void ResetLevelRuntime(Scene* scene, ArcadeCombatLevelComponent& level)
        {
            level.RuntimeElapsed = 0.0f;
            level.RuntimeFadeAlpha = 1.0f;
            level.RuntimePaused = false;
            level.RuntimeBossIntroStarted = false;
            level.RuntimeBossIntroFinished = false;
            level.RuntimeVictory = false;
            level.RuntimeDefeat = false;
            level.RuntimeResultTimer = 0.0f;
            level.RuntimeResultCommandIssued = false;
            level.RuntimeRequestedCommand.clear();

            SetWidgetVisible(scene, level.PausePanelEntityName, false);
            SetImageAlpha(scene, level.FadeEntityName, 1.0f);
        }

        static void ResetCombatants(Scene* scene)
        {
            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<ArcadeCombatantComponent>())
            {
                auto& combatant = registry.get<ArcadeCombatantComponent>(e);
                combatant.Health = std::max(0.0f, combatant.MaxHealth);
                combatant.Alive = combatant.Health > 0.0f;
                combatant.ControlsLocked = false;
            }

            for (auto e : registry.view<ArcadeCoverComponent>())
            {
                auto& cover = registry.get<ArcadeCoverComponent>(e);
                cover.Health = cover.MaxHealth;
            }

            for (auto e : registry.view<ArcadeTriggerComponent>())
                registry.get<ArcadeTriggerComponent>(e).Triggered = false;
        }

        static void ResetBossPresentation(Entity boss)
        {
            if (!boss || !boss.HasComponent<ArcadeBossComponent>())
                return;

            auto& bossComponent = boss.GetComponent<ArcadeBossComponent>();
            bossComponent.RuntimeIntroTimer = 0.0f;
            bossComponent.RuntimeShootTimer = 0.0f;
            bossComponent.RuntimeJumpTimer = 0.0f;
            bossComponent.RuntimeJumpProgress = 0.0f;
            bossComponent.RuntimeJumping = false;

            if (boss.HasComponent<TransformComponent>())
            {
                boss.GetComponent<TransformComponent>().Translation =
                    bossComponent.Active ? bossComponent.FightPosition : bossComponent.IntroStartPosition;
            }
            if (boss.HasComponent<SpriteRendererComponent>())
                boss.GetComponent<SpriteRendererComponent>().Color.a = bossComponent.Active ? 1.0f : 0.0f;
        }

        static void UpdateStartFade(Scene* scene, ArcadeCombatLevelComponent& level, float dt)
        {
            if (level.StartFadeDuration <= 0.0f)
            {
                level.RuntimeFadeAlpha = 0.0f;
                SetImageAlpha(scene, level.FadeEntityName, 0.0f);
                return;
            }

            level.RuntimeFadeAlpha = std::max(0.0f,
                level.RuntimeFadeAlpha - dt / level.StartFadeDuration);
            SetImageAlpha(scene, level.FadeEntityName, level.RuntimeFadeAlpha);
        }

        static void UpdateTriggerGlow(Scene* scene, ArcadeCombatLevelComponent& level)
        {
            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<ArcadeTriggerComponent, CircleRendererComponent>())
            {
                auto& trigger = registry.get<ArcadeTriggerComponent>(e);
                auto& circle = registry.get<CircleRendererComponent>(e);
                const float pulse = 0.55f + 0.25f * std::sin(level.RuntimeElapsed * 5.0f);
                circle.Color.a = trigger.Triggered ? 0.15f : pulse;
            }
        }

        static void UpdateBossIntro(Scene* scene,
            ArcadeCombatLevelComponent& level,
            Entity player,
            Entity boss,
            float dt)
        {
            if (!boss || !boss.HasComponent<ArcadeBossComponent>())
            {
                level.RuntimeBossIntroFinished = true;
                return;
            }

            auto& bossComponent = boss.GetComponent<ArcadeBossComponent>();
            bossComponent.RuntimeIntroTimer += dt;

            const float duration = std::max(0.01f, bossComponent.IntroDuration);
            const float t = std::clamp(bossComponent.RuntimeIntroTimer / duration, 0.0f, 1.0f);
            const float ease = 1.0f - (1.0f - t) * (1.0f - t);

            if (boss.HasComponent<TransformComponent>())
            {
                auto& transform = boss.GetComponent<TransformComponent>();
                transform.Translation = glm::mix(
                    bossComponent.IntroStartPosition,
                    bossComponent.FightPosition,
                    ease);
                transform.Translation.y += std::sin(t * Pi) * 0.7f;
            }

            if (boss.HasComponent<SpriteRendererComponent>())
                boss.GetComponent<SpriteRendererComponent>().Color.a = t;

            if (player && player.HasComponent<ArcadeCombatantComponent>())
                player.GetComponent<ArcadeCombatantComponent>().ControlsLocked = true;

            if (t >= 1.0f)
            {
                bossComponent.Active = true;
                level.RuntimeBossIntroFinished = true;
                if (player && player.HasComponent<ArcadeCombatantComponent>())
                    player.GetComponent<ArcadeCombatantComponent>().ControlsLocked = false;
            }
        }

        static void UpdateIntroTrigger(Scene* scene,
            ArcadeCombatLevelComponent& level,
            Entity player,
            Entity boss)
        {
            if (!player || !player.HasComponent<TransformComponent>())
                return;

            auto& registry = scene->GetRegistry();
            auto& playerTransform = player.GetComponent<TransformComponent>();

            for (auto e : registry.view<TransformComponent, ArcadeTriggerComponent>())
            {
                auto& triggerTransform = registry.get<TransformComponent>(e);
                auto& trigger = registry.get<ArcadeTriggerComponent>(e);
                if (trigger.Triggered || trigger.Type != ArcadeTriggerType::BossIntro)
                    continue;

                if (Distance2D(playerTransform.Translation, triggerTransform.Translation) <= trigger.Radius)
                {
                    trigger.Triggered = true;
                    level.RuntimeBossIntroStarted = true;
                    level.RuntimeBossIntroFinished = false;
                    if (boss && boss.HasComponent<ArcadeBossComponent>())
                        boss.GetComponent<ArcadeBossComponent>().RuntimeIntroTimer = 0.0f;
                    break;
                }
            }
        }

        static void UpdatePlayer(Scene* scene,
            ArcadeCombatLevelComponent& level,
            Entity player,
            Entity boss,
            float dt)
        {
            if (!player || !player.HasComponent<TransformComponent>() ||
                !player.HasComponent<ArcadeCombatantComponent>() ||
                !player.HasComponent<ArcadePlayerControllerComponent>())
                return;

            auto& transform = player.GetComponent<TransformComponent>();
            auto& combatant = player.GetComponent<ArcadeCombatantComponent>();
            auto& controller = player.GetComponent<ArcadePlayerControllerComponent>();

            if (!combatant.Alive)
            {
                if (!level.RuntimeDefeat)
                {
                    level.RuntimeDefeat = true;
                    level.RuntimeResultTimer = 0.0f;
                    level.RuntimeResultCommandIssued = false;
                    level.RuntimePaused = false;
                    combatant.ControlsLocked = true;
                    DestroyProjectiles(scene);
                }
                return;
            }

            controller.WeaponCooldown = std::max(0.0f, controller.WeaponCooldown - dt);

            if (!combatant.ControlsLocked && !level.RuntimeVictory && !level.RuntimeDefeat)
            {
                glm::vec2 movement(0.0f);
                if (Input::IsKeyPressed(WT_KEY_A) || Input::IsKeyPressed(WT_KEY_LEFT))  movement.x -= 1.0f;
                if (Input::IsKeyPressed(WT_KEY_D) || Input::IsKeyPressed(WT_KEY_RIGHT)) movement.x += 1.0f;
                if (Input::IsKeyPressed(WT_KEY_W) || Input::IsKeyPressed(WT_KEY_UP))    movement.y += 1.0f;
                if (Input::IsKeyPressed(WT_KEY_S) || Input::IsKeyPressed(WT_KEY_DOWN))  movement.y -= 1.0f;

                if (glm::length2(movement) > 0.0f)
                {
                    movement = glm::normalize(movement);
                    transform.Translation += glm::vec3(movement * combatant.MoveSpeed * dt, 0.0f);
                    transform.Translation.x = std::clamp(transform.Translation.x, level.ArenaMin.x, level.ArenaMax.x);
                    transform.Translation.y = std::clamp(transform.Translation.y, level.ArenaMin.y, level.ArenaMax.y);
                }
            }

            const bool attackHeld =
                Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT) ||
                Input::IsKeyPressed(WT_KEY_J) ||
                Input::IsKeyPressed(WT_KEY_SPACE);

            if (!attackHeld || controller.WeaponCooldown > 0.0f ||
                combatant.ControlsLocked || level.RuntimeVictory || level.RuntimeDefeat)
                return;

            glm::vec2 direction(1.0f, 0.0f);
            if (boss && boss.HasComponent<TransformComponent>())
                direction = DirectionTo(transform.Translation, boss.GetComponent<TransformComponent>().Translation);

            const glm::vec3 muzzle = transform.Translation + glm::vec3(direction * 0.55f, 0.05f);
            switch (controller.CurrentWeapon)
            {
            case ArcadeWeaponType::Gun:
                controller.WeaponCooldown = 0.16f;
                CreateProjectile(scene, "Arcade_PlayerBullet", muzzle, direction * 9.0f,
                    8.0f, 1.4f, 0.13f, (int)ArcadeTeam::Player,
                    { 1.0f, 0.90f, 0.35f, 1.0f });
                break;
            case ArcadeWeaponType::Cannon:
                controller.WeaponCooldown = 0.75f;
                CreateProjectile(scene, "Arcade_PlayerCannon", muzzle, direction * 5.3f,
                    24.0f, 2.0f, 0.28f, (int)ArcadeTeam::Player,
                    { 1.0f, 0.42f, 0.16f, 1.0f }, true);
                break;
            case ArcadeWeaponType::Katana:
                controller.WeaponCooldown = 0.38f;
                CreateProjectile(scene, "Arcade_KatanaSlash",
                    transform.Translation + glm::vec3(direction * 0.8f, 0.0f),
                    direction, 18.0f, 0.12f, 0.75f, (int)ArcadeTeam::Player,
                    { 0.85f, 0.96f, 1.0f, 0.82f }, false, true);
                break;
            }
        }

        static void StartBossJump(ArcadeCombatLevelComponent& level,
            ArcadeBossComponent& boss,
            TransformComponent& transform)
        {
            boss.RuntimeJumping = true;
            boss.RuntimeJumpProgress = 0.0f;
            boss.RuntimeJumpTimer = 0.0f;
            boss.RuntimeJumpStart = transform.Translation;

            const float x = std::sin(level.RuntimeElapsed * 1.73f) * 5.4f;
            const float y = 2.0f + std::cos(level.RuntimeElapsed * 1.11f) * 0.65f;
            boss.RuntimeJumpTarget = {
                std::clamp(x, level.ArenaMin.x + 1.2f, level.ArenaMax.x - 1.2f),
                std::clamp(y, level.ArenaMin.y + 1.2f, level.ArenaMax.y - 0.9f),
                transform.Translation.z
            };
        }

        static void UpdateBoss(Scene* scene,
            ArcadeCombatLevelComponent& level,
            Entity boss,
            Entity player,
            float dt)
        {
            if (!boss || !boss.HasComponent<ArcadeBossComponent>() ||
                !boss.HasComponent<ArcadeCombatantComponent>() ||
                !boss.HasComponent<TransformComponent>())
                return;

            auto& bossComponent = boss.GetComponent<ArcadeBossComponent>();
            auto& combatant = boss.GetComponent<ArcadeCombatantComponent>();
            auto& transform = boss.GetComponent<TransformComponent>();

            if (!combatant.Alive)
            {
                if (!level.RuntimeVictory)
                {
                    bossComponent.Active = false;
                    bossComponent.RuntimeJumping = false;
                    level.RuntimeVictory = true;
                    level.RuntimeResultTimer = 0.0f;
                    level.RuntimeResultCommandIssued = false;
                    level.RuntimePaused = false;

                    if (player && player.HasComponent<ArcadeCombatantComponent>())
                        player.GetComponent<ArcadeCombatantComponent>().ControlsLocked = true;

                    DestroyProjectiles(scene);
                }
                return;
            }

            if (!bossComponent.Active || !player || !player.HasComponent<TransformComponent>())
                return;

            bossComponent.RuntimeJumpTimer += dt;
            if (!bossComponent.RuntimeJumping && bossComponent.RuntimeJumpTimer >= bossComponent.JumpInterval)
                StartBossJump(level, bossComponent, transform);

            if (bossComponent.RuntimeJumping)
            {
                bossComponent.RuntimeJumpProgress += dt / std::max(0.01f, bossComponent.JumpDuration);
                const float t = std::clamp(bossComponent.RuntimeJumpProgress, 0.0f, 1.0f);
                transform.Translation = glm::mix(bossComponent.RuntimeJumpStart, bossComponent.RuntimeJumpTarget, t);
                transform.Translation.y += std::sin(t * Pi) * 0.85f;
                if (t >= 1.0f)
                    bossComponent.RuntimeJumping = false;
            }

            bossComponent.RuntimeShootTimer += dt;
            if (bossComponent.RuntimeShootTimer >= bossComponent.ShootInterval)
            {
                bossComponent.RuntimeShootTimer = 0.0f;
                const glm::vec2 direction = DirectionTo(
                    transform.Translation,
                    player.GetComponent<TransformComponent>().Translation);

                CreateProjectile(scene, "Arcade_BossBullet",
                    transform.Translation + glm::vec3(direction * 0.65f, -0.1f),
                    direction * 4.2f, 12.0f, 2.4f, 0.19f, (int)ArcadeTeam::Enemy,
                    { 0.95f, 0.22f, 0.34f, 1.0f });
            }
        }

        static void UpdateResultTransition(Scene* scene,
            ArcadeCombatLevelComponent& level,
            Entity player,
            Entity boss,
            float dt)
        {
            if (!level.RuntimeVictory && !level.RuntimeDefeat)
                return;

            level.RuntimeResultTimer += dt;

            if (player && player.HasComponent<ArcadeCombatantComponent>())
                player.GetComponent<ArcadeCombatantComponent>().ControlsLocked = true;

            if (level.RuntimeVictory && boss && boss.HasComponent<SpriteRendererComponent>())
            {
                const float bossFadeDuration = std::max(0.01f, level.BossDefeatFadeDuration);
                const float bossFade = std::clamp(level.RuntimeResultTimer / bossFadeDuration, 0.0f, 1.0f);
                boss.GetComponent<SpriteRendererComponent>().Color.a = 1.0f - bossFade;
            }

            const bool victory = level.RuntimeVictory;
            const std::string& command = victory ? level.VictorySceneCommand : level.DefeatSceneCommand;
            if (command.empty())
                return;

            const float delay = std::max(0.0f, victory ? level.VictoryReturnDelay : level.DefeatReturnDelay);
            const float sceneFadeDuration = std::max(0.01f, level.ResultSceneFadeDuration);
            const float sceneFadeStart = std::max(0.0f, delay - sceneFadeDuration);
            const float sceneFade = std::clamp(
                (level.RuntimeResultTimer - sceneFadeStart) / sceneFadeDuration,
                0.0f,
                1.0f);
            SetImageAlpha(scene, level.FadeEntityName, sceneFade);

            if (!level.RuntimeResultCommandIssued && level.RuntimeResultTimer >= delay)
            {
                level.RuntimeResultCommandIssued = true;
                if (level.RuntimeRequestedCommand.empty())
                    level.RuntimeRequestedCommand = command;
            }
        }

        static bool ProjectileBlockedByCover(Scene* scene,
            Entity projectileEntity,
            ArcadeProjectileComponent& projectile,
            const TransformComponent& projectileTransform)
        {
            if (projectile.Melee)
                return false;

            auto& registry = scene->GetRegistry();
            for (auto coverEntity : registry.view<TransformComponent, ArcadeCoverComponent>())
            {
                auto& coverTransform = registry.get<TransformComponent>(coverEntity);
                auto& cover = registry.get<ArcadeCoverComponent>(coverEntity);
                if (!cover.BlocksProjectiles || cover.Health <= 0.0f)
                    continue;

                if (Distance2D(projectileTransform.Translation, coverTransform.Translation) <= projectile.Radius + cover.Radius)
                {
                    cover.Health = std::max(0.0f, cover.Health - projectile.Damage * (projectile.Heavy ? 0.75f : 0.35f));
                    if (auto* sprite = registry.try_get<SpriteRendererComponent>(coverEntity))
                    {
                        const float normalized = cover.MaxHealth <= 0.0f ? 0.0f : cover.Health / cover.MaxHealth;
                        sprite->Color.a = 0.22f + normalized * 0.78f;
                    }
                    scene->DestroyEntity(projectileEntity);
                    return true;
                }
            }

            return false;
        }

        static bool ProjectileHitCombatant(Scene* scene,
            Entity projectileEntity,
            ArcadeProjectileComponent& projectile,
            const TransformComponent& projectileTransform)
        {
            auto& registry = scene->GetRegistry();
            for (auto targetEntity : registry.view<TransformComponent, ArcadeCombatantComponent>())
            {
                auto& targetTransform = registry.get<TransformComponent>(targetEntity);
                auto& target = registry.get<ArcadeCombatantComponent>(targetEntity);
                if (!target.Alive || target.Team == projectile.Team || target.Team == (int)ArcadeTeam::Neutral)
                    continue;

                if (Distance2D(projectileTransform.Translation, targetTransform.Translation) <= projectile.Radius + target.CollisionRadius)
                {
                    ApplyDamage(target, projectile.Damage);
                    scene->DestroyEntity(projectileEntity);
                    return true;
                }
            }
            return false;
        }

        static void UpdateProjectiles(Scene* scene, float dt)
        {
            auto& registry = scene->GetRegistry();
            std::vector<entt::entity> projectiles;
            for (auto e : registry.view<TransformComponent, ArcadeProjectileComponent>())
                projectiles.push_back(e);

            for (auto e : projectiles)
            {
                if (!registry.valid(e))
                    continue;

                Entity projectileEntity{ e, scene };
                auto& transform = registry.get<TransformComponent>(e);
                auto& projectile = registry.get<ArcadeProjectileComponent>(e);

                projectile.Lifetime -= dt;
                transform.Translation += glm::vec3(projectile.Velocity * dt, 0.0f);

                if (projectile.Lifetime <= 0.0f)
                {
                    scene->DestroyEntity(projectileEntity);
                    continue;
                }

                if (ProjectileBlockedByCover(scene, projectileEntity, projectile, transform))
                    continue;

                ProjectileHitCombatant(scene, projectileEntity, projectile, transform);
            }
        }

        static std::string BuildMessage(const ArcadeCombatLevelComponent& level,
            const ArcadeCombatantComponent* player,
            const ArcadeBossComponent* boss)
        {
            if (level.RuntimePaused)
                return "已暂停  |  按 P 或 Esc 继续";
            if (level.RuntimeDefeat)
                return "你倒下了。掩体很重要，请从主菜单重新挑战。";
            if (level.RuntimeVictory)
                return "Boss 已击败，正在返回剧情...";
            if (level.RuntimeBossIntroStarted && !level.RuntimeBossIntroFinished)
                return "Boss 出现中，演出期间角色无法移动...";
            if (!level.RuntimeBossIntroStarted)
                return "移动到发光点。WASD 移动，鼠标/J 攻击，1/2/3 切换武器，P 暂停。";
            if (player && player->ControlsLocked)
                return "战斗演出中，角色控制已锁定。";
            if (boss && boss->Active)
                return "战斗！利用掩体，切换武器，保持移动。";
            return "准备就绪。";
        }

        static void UpdateHUD(Scene* scene,
            ArcadeCombatLevelComponent& level,
            Entity player,
            Entity boss)
        {
            const ArcadeCombatantComponent* playerCombatant =
                player && player.HasComponent<ArcadeCombatantComponent>()
                ? &player.GetComponent<ArcadeCombatantComponent>()
                : nullptr;
            const ArcadeCombatantComponent* bossCombatant =
                boss && boss.HasComponent<ArcadeCombatantComponent>()
                ? &boss.GetComponent<ArcadeCombatantComponent>()
                : nullptr;
            const ArcadeBossComponent* bossComponent =
                boss && boss.HasComponent<ArcadeBossComponent>()
                ? &boss.GetComponent<ArcadeBossComponent>()
                : nullptr;

            if (playerCombatant)
            {
                SetProgress(scene, level.PlayerHealthBarEntityName,
                    playerCombatant->Health, playerCombatant->MaxHealth);
                SetText(scene, level.PlayerHealthTextEntityName,
                    "生命 " + std::to_string((int)playerCombatant->Health) + "/" +
                    std::to_string((int)playerCombatant->MaxHealth));
            }

            if (bossCombatant)
            {
                SetProgress(scene, level.BossHealthBarEntityName,
                    bossCombatant->Health, bossCombatant->MaxHealth);
                SetText(scene, level.BossHealthTextEntityName,
                    bossComponent && bossComponent->Active ? "首领" : "首领 ???");
            }

            if (player && player.HasComponent<ArcadePlayerControllerComponent>())
            {
                const auto& controller = player.GetComponent<ArcadePlayerControllerComponent>();
                SetText(scene, level.WeaponTextEntityName,
                    std::string("武器: ") + WeaponName(controller.CurrentWeapon) +
                    "  [1 手枪] [2 重炮] [3 太刀]");
            }

            SetWidgetVisible(scene, level.PausePanelEntityName, level.RuntimePaused);
            SetText(scene, level.MessageTextEntityName, BuildMessage(level, playerCombatant, bossComponent));
        }

    } // namespace

    void ArcadeCombatSystem::ResetInputState()
    {
        m_PreviousPausePressed = false;
        m_PreviousWeapon1Pressed = false;
        m_PreviousWeapon2Pressed = false;
        m_PreviousWeapon3Pressed = false;
        m_PreviousAttackPressed = false;
    }

    void ArcadeCombatSystem::OnRuntimeStart(Scene* scene)
    {
        ResetInputState();
        if (!scene)
            return;

        ResetCombatants(scene);

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<ArcadeCombatLevelComponent>())
        {
            auto& level = registry.get<ArcadeCombatLevelComponent>(e);
            ResetLevelRuntime(scene, level);
            Entity boss = FindEntityByName(scene, level.BossEntityName);
            ResetBossPresentation(boss);
            if (boss && boss.HasComponent<ArcadeBossComponent>() &&
                boss.GetComponent<ArcadeBossComponent>().Active)
            {
                level.RuntimeBossIntroStarted = true;
                level.RuntimeBossIntroFinished = true;
            }
        }
    }

    void ArcadeCombatSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        const float dt = ts.GetSeconds();
        auto& registry = scene->GetRegistry();

        for (auto levelEntity : registry.view<ArcadeCombatLevelComponent>())
        {
            auto& level = registry.get<ArcadeCombatLevelComponent>(levelEntity);
            if (!level.PlayOnStart)
                continue;

            level.RuntimeElapsed += dt;
            UpdateStartFade(scene, level, dt);
            UpdateTriggerGlow(scene, level);

            Entity player = FindEntityByName(scene, level.PlayerEntityName);
            Entity boss = FindEntityByName(scene, level.BossEntityName);

            const bool pausePressed = Input::IsKeyPressed(WT_KEY_P) || Input::IsKeyPressed(WT_KEY_ESCAPE);
            if (pausePressed && !m_PreviousPausePressed && !level.RuntimeVictory && !level.RuntimeDefeat)
                level.RuntimePaused = !level.RuntimePaused;
            m_PreviousPausePressed = pausePressed;

            if (player && player.HasComponent<ArcadePlayerControllerComponent>())
            {
                auto& controller = player.GetComponent<ArcadePlayerControllerComponent>();
                const bool weapon1Pressed = Input::IsKeyPressed(WT_KEY_1);
                const bool weapon2Pressed = Input::IsKeyPressed(WT_KEY_2);
                const bool weapon3Pressed = Input::IsKeyPressed(WT_KEY_3);

                if (weapon1Pressed && !m_PreviousWeapon1Pressed) controller.CurrentWeapon = ArcadeWeaponType::Gun;
                if (weapon2Pressed && !m_PreviousWeapon2Pressed) controller.CurrentWeapon = ArcadeWeaponType::Cannon;
                if (weapon3Pressed && !m_PreviousWeapon3Pressed) controller.CurrentWeapon = ArcadeWeaponType::Katana;

                m_PreviousWeapon1Pressed = weapon1Pressed;
                m_PreviousWeapon2Pressed = weapon2Pressed;
                m_PreviousWeapon3Pressed = weapon3Pressed;
            }

            if (!level.RuntimePaused)
            {
                if (!level.RuntimeBossIntroStarted)
                    UpdateIntroTrigger(scene, level, player, boss);

                if (level.RuntimeBossIntroStarted && !level.RuntimeBossIntroFinished)
                    UpdateBossIntro(scene, level, player, boss, dt);

                if (level.RuntimeBossIntroFinished)
                    UpdateBoss(scene, level, boss, player, dt);

                if (!level.RuntimeVictory && !level.RuntimeDefeat)
                {
                    UpdatePlayer(scene, level, player, boss, dt);
                    UpdateProjectiles(scene, dt);
                }

                UpdateResultTransition(scene, level, player, boss, dt);
            }

            m_PreviousAttackPressed =
                Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT) ||
                Input::IsKeyPressed(WT_KEY_J) ||
                Input::IsKeyPressed(WT_KEY_SPACE);

            UpdateHUD(scene, level, player, boss);
        }
    }

} // namespace Wheatear
