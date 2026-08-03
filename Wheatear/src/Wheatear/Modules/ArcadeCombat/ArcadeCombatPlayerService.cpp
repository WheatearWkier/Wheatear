#include "wtpch.h"
#include "ArcadeCombatPlayerService.h"

#include "ArcadeCombatMath.h"
#include "ArcadeCombatProjectileService.h"
#include "ArcadeCombatSignalHandlers.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/ActionResolver.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <glm/gtx/norm.hpp>

namespace Wheatear::ArcadeCombatPlayerService {

    namespace {

        static const char* WeaponActionId(ArcadeWeaponType weapon)
        {
            switch (weapon)
            {
            case ArcadeWeaponType::Gun: return "arcade.gun";
            case ArcadeWeaponType::Cannon: return "arcade.cannon";
            case ArcadeWeaponType::Katana: return "arcade.katana";
            default: return "arcade.gun";
            }
        }

        static const WAO::ActionRecipe* ResolveWeaponRecipe(ArcadeWeaponType weapon)
        {
            return WAO::FindRecipeOrWarn(WeaponActionId(weapon), "ArcadeCombat.Player");
        }

    } // namespace

    void UpdateWeaponSelection(Entity player,
        const PlayerInputState& input,
        const PlayerInputState& previousInput)
    {
        if (!player || !player.HasComponent<ArcadePlayerControllerComponent>())
            return;

        auto& controller = player.GetComponent<ArcadePlayerControllerComponent>();
        if (input.Weapon1Pressed && !previousInput.Weapon1Pressed)
            controller.CurrentWeapon = ArcadeWeaponType::Gun;
        if (input.Weapon2Pressed && !previousInput.Weapon2Pressed)
            controller.CurrentWeapon = ArcadeWeaponType::Cannon;
        if (input.Weapon3Pressed && !previousInput.Weapon3Pressed)
            controller.CurrentWeapon = ArcadeWeaponType::Katana;
    }

    void UpdatePlayer(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss,
        float dt,
        const PlayerInputState& input)
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
                ArcadeCombatProjectileService::DestroyProjectiles(scene);
            }
            return;
        }

        controller.WeaponCooldown = std::max(0.0f, controller.WeaponCooldown - dt);

        if (!combatant.ControlsLocked && !level.RuntimeVictory && !level.RuntimeDefeat)
        {
            glm::vec2 movement = input.Movement;
            if (glm::length2(movement) > 0.0f)
            {
                movement = glm::normalize(movement);
                transform.Translation += glm::vec3(movement * combatant.MoveSpeed * dt, 0.0f);
                transform.Translation.x = std::clamp(transform.Translation.x, level.ArenaMin.x, level.ArenaMax.x);
                transform.Translation.y = std::clamp(transform.Translation.y, level.ArenaMin.y, level.ArenaMax.y);
            }
        }

        if (!input.AttackHeld || controller.WeaponCooldown > 0.0f ||
            combatant.ControlsLocked || level.RuntimeVictory || level.RuntimeDefeat)
        {
            return;
        }

        glm::vec2 direction(1.0f, 0.0f);
        if (boss && boss.HasComponent<TransformComponent>())
            direction = ArcadeCombatMath::DirectionTo(transform.Translation, boss.GetComponent<TransformComponent>().Translation);

        const glm::vec3 muzzle = transform.Translation + glm::vec3(direction * 0.55f, 0.05f);
        const WAO::ActionRecipe* recipe = ResolveWeaponRecipe(controller.CurrentWeapon);
        if (!recipe)
            return;

        ArcadeCombatSignalHandlers::ProjectileSpawnPayload payload;
        payload.SceneContext = scene;
        payload.Team = (int)ArcadeTeam::Player;
        switch (controller.CurrentWeapon)
        {
        case ArcadeWeaponType::Gun:
            controller.WeaponCooldown = std::max(0.01f, recipe->Cooldown);
            payload.EntityName = "Arcade_PlayerBullet";
            payload.Position = muzzle;
            payload.Velocity = direction * 9.0f;
            payload.Damage = WAO::PrimaryEffectValue(*recipe, WAO::EffectType::Damage, 8.0f);
            payload.Lifetime = 1.4f;
            payload.Radius = 0.13f;
            payload.Color = { 1.0f, 0.90f, 0.35f, 1.0f };
            break;
        case ArcadeWeaponType::Cannon:
            controller.WeaponCooldown = std::max(0.01f, recipe->Cooldown);
            payload.EntityName = "Arcade_PlayerCannon";
            payload.Position = muzzle;
            payload.Velocity = direction * 5.3f;
            payload.Damage = WAO::PrimaryEffectValue(*recipe, WAO::EffectType::Damage, 24.0f);
            payload.Lifetime = 2.0f;
            payload.Radius = 0.28f;
            payload.Color = { 1.0f, 0.42f, 0.16f, 1.0f };
            payload.Heavy = true;
            break;
        case ArcadeWeaponType::Katana:
            controller.WeaponCooldown = std::max(0.01f, recipe->Cooldown);
            payload.EntityName = "Arcade_KatanaSlash";
            payload.Position = transform.Translation + glm::vec3(direction * 0.8f, 0.0f);
            payload.Velocity = direction;
            payload.Damage = WAO::PrimaryEffectValue(*recipe, WAO::EffectType::Damage, 18.0f);
            payload.Lifetime = 0.12f;
            payload.Radius = 0.75f;
            payload.Color = { 0.85f, 0.96f, 1.0f, 0.82f };
            payload.Melee = true;
            break;
        }

        const std::string detail = "fire " + recipe->DisplayName;
        WAO::ActionResolveContext actionContext;
        actionContext.SceneContext = scene;
        actionContext.Intent.Actor = player.GetUUID();
        actionContext.Intent.ExplicitTarget = boss ? boss.GetUUID() : UUID(0);
        actionContext.Intent.ActionId = recipe->Id;
        actionContext.Intent.Source = "ArcadeCombat.Player";
        actionContext.Detail = detail;
        actionContext.TransientPayload = &payload;
        WAO::ActionOrchestrator::ExecuteWithRecipe(actionContext, *recipe);
    }

} // namespace Wheatear::ArcadeCombatPlayerService
