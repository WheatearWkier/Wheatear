#include "wtpch.h"
#include "ArcadeCombatPlayerService.h"

#include "ArcadeCombatMath.h"
#include "ArcadeCombatProjectileService.h"
#include "ArcadeCombatSignalHandlers.h"
#include "ArcadeCombatTuningService.h"
#include "Wheatear/Input/InputBindingService.h"
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
        const PlayerInputState& input)
    {
        if (!player || !player.HasComponent<ArcadePlayerControllerComponent>())
            return;

        auto& controller = player.GetComponent<ArcadePlayerControllerComponent>();
        if (InputBindingService::IsActionPressed("arcade.weapon1"))
            controller.CurrentWeapon = ArcadeWeaponType::Gun;
        if (InputBindingService::IsActionPressed("arcade.weapon2"))
            controller.CurrentWeapon = ArcadeWeaponType::Cannon;
        if (InputBindingService::IsActionPressed("arcade.weapon3"))
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

        const WAO::ActionRecipe* recipe = ResolveWeaponRecipe(controller.CurrentWeapon);
        if (!recipe)
            return;

        const auto& tuning = ArcadeCombatTuningService::GetTuning(level);
        const auto& weapon = ArcadeCombatTuningService::GetWeaponTuning(
            tuning, controller.CurrentWeapon);

        controller.WeaponCooldown = std::max(0.01f, recipe->Cooldown);

        ArcadeCombatSignalHandlers::ProjectileSpawnPayload payload;
        payload.SceneContext = scene;
        payload.EntityName = weapon.EntityName;
        payload.Position = weapon.Melee
            ? transform.Translation + glm::vec3(direction * weapon.SlashOffset, 0.0f)
            : transform.Translation + glm::vec3(direction * weapon.MuzzleOffset.x, weapon.MuzzleOffset.y);
        payload.Velocity = direction * weapon.Speed;
        payload.Damage = WAO::PrimaryEffectValue(*recipe, WAO::EffectType::Damage,
            controller.CurrentWeapon == ArcadeWeaponType::Cannon ? 24.0f
                : controller.CurrentWeapon == ArcadeWeaponType::Katana ? 18.0f : 8.0f);
        payload.Lifetime = weapon.Lifetime;
        payload.Radius = weapon.Radius;
        payload.Team = (int)ArcadeTeam::Player;
        payload.Color = weapon.Color;
        payload.Heavy = weapon.Heavy;
        payload.Melee = weapon.Melee;

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
