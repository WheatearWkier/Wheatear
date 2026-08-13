#include "wtpch.h"
#include "SideCombatPickupService.h"

#include "SideCombatMath.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <vector>

namespace Wheatear::SideCombatPickupService {

    namespace {

        static bool MatchesDeathReward(
            const SideCombatLevelComponent::DeathReward& reward,
            const std::string& sourceEntityName,
            const SideEnemyAIComponent* ai)
        {
            if (!reward.Enabled)
                return false;
            if (reward.EnemyKind >= 0 && (!ai || reward.EnemyKind != static_cast<int>(ai->Kind)))
                return false;
            if (!reward.SourceEntityName.empty() && reward.SourceEntityName != sourceEntityName)
                return false;
            return true;
        }

    } // namespace

    Entity CreatePickup(Scene* scene,
        const std::string& name,
        const glm::vec3& position,
        const std::string& itemId,
        const std::string& displayName,
        int amount,
        const std::string& texturePath,
        const glm::vec3& scale,
        const SideCombatTuningService::SidePickupTuning& tuning)
    {
        if (!scene)
            return {};

        Entity pickup = scene->CreateEntity(name);
        auto& transform = pickup.GetComponent<TransformComponent>();
        transform.Translation = position;
        transform.Scale = scale;

        auto& sprite = pickup.AddComponent<SpriteRendererComponent>();
        sprite.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        if (Ref<Texture2D> texture = GameplayVisualService::LoadTextureCached(texturePath))
            sprite.Texture = texture;

        auto& component = pickup.AddComponent<SidePickupComponent>();
        component.ItemId = itemId;
        component.DisplayName = displayName;
        component.Amount = amount;
        component.PickupRadius = tuning.PickupRadius;
        component.AttractRadius = tuning.AttractRadius;
        component.AttractSpeed = tuning.AttractSpeed;
        return pickup;
    }

    void SpawnDeathRewards(Scene* scene,
        const SideCombatLevelComponent& level,
        const std::string& sourceEntityName,
        const TransformComponent& transform,
        const SideEnemyAIComponent* ai)
    {
        if (!scene)
            return;

        const auto& pickupTuning = SideCombatTuningService::GetTuning(level).Pickup;
        for (const auto& reward : level.DeathRewards)
        {
            if (!MatchesDeathReward(reward, sourceEntityName, ai))
                continue;

            const std::string spawnName = reward.SpawnEntityName.empty()
                ? reward.ItemId + "_Drop"
                : reward.SpawnEntityName;
            const std::string sourceSuffix = sourceEntityName.empty()
                ? std::string{}
                : "_" + sourceEntityName;

            CreatePickup(scene,
                spawnName + sourceSuffix,
                transform.Translation + reward.Offset,
                reward.ItemId,
                reward.DisplayName,
                std::max(1, reward.Amount),
                AssetAliasRegistry::Resolve(reward.TexturePath),
                reward.Scale,
                pickupTuning);
        }
    }

    void UpdatePickups(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        float dt)
    {
        if (!scene || !player || !player.HasComponent<TransformComponent>())
            return;

        const glm::vec3 playerPosition = player.GetComponent<TransformComponent>().Translation;
        auto& registry = scene->GetRegistry();
        std::vector<entt::entity> picked;

        for (auto e : registry.view<TransformComponent, SidePickupComponent>())
        {
            auto& transform = registry.get<TransformComponent>(e);
            auto& pickup = registry.get<SidePickupComponent>(e);
            const glm::vec2 toPlayer = SideCombatMath::ToVec2(playerPosition - transform.Translation);
            const float distance = glm::length(toPlayer);
            if (distance <= pickup.PickupRadius)
            {
                level.RuntimeCollectedPickups += pickup.Amount;
                GameProgress::AddMaterial(pickup.ItemId, pickup.DisplayName, pickup.Amount);
                picked.push_back(e);
                continue;
            }

            if (distance <= pickup.AttractRadius || level.RuntimeVictory)
            {
                const glm::vec2 direction = distance > 0.001f
                    ? toPlayer / distance
                    : glm::vec2{ 0.0f, 0.0f };
                transform.Translation += glm::vec3(direction * pickup.AttractSpeed * dt, 0.0f);
            }
        }

        for (auto e : picked)
        {
            if (registry.valid(e))
                scene->DestroyEntity({ e, scene });
        }
    }

} // namespace Wheatear::SideCombatPickupService
