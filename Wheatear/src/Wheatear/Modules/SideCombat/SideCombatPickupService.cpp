#include "wtpch.h"
#include "SideCombatPickupService.h"

#include "SideCombatMath.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Modules/Common/GameplayVisualService.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <vector>

namespace Wheatear::SideCombatPickupService {

    Entity CreatePickup(Scene* scene,
        const std::string& name,
        const glm::vec3& position,
        const std::string& itemId,
        const std::string& displayName,
        int amount,
        const std::string& texturePath,
        const SideCombatTuningService::SidePickupTuning& tuning)
    {
        if (!scene)
            return {};

        Entity pickup = scene->CreateEntity(name);
        auto& transform = pickup.GetComponent<TransformComponent>();
        transform.Translation = position;
        transform.Scale = { 0.38f, 0.38f, 1.0f };

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
        const TransformComponent& transform,
        const SideEnemyAIComponent* ai)
    {
        const bool boss = ai && ai->Kind == SideEnemyKind::BearBoss;
        const auto& pickupTuning = SideCombatTuningService::GetTuning(level).Pickup;
        if (boss)
        {
            CreatePickup(scene, "Drop_MagicCore",
                transform.Translation + glm::vec3(-0.42f, 0.55f, 0.03f),
                "MAT-MAGIC-CORE-T0", "魔核碎片", 1,
                AssetAliasRegistry::Path("side.drop.magic_core"),
                pickupTuning);
            CreatePickup(scene, "Drop_BeastSinew",
                transform.Translation + glm::vec3(0.0f, 0.72f, 0.03f),
                "MAT-BEAST-SINEW", "兽筋", 2,
                AssetAliasRegistry::Path("side.drop.beast_sinew"),
                pickupTuning);
            CreatePickup(scene, "Drop_BeastClaw",
                transform.Translation + glm::vec3(0.42f, 0.55f, 0.03f),
                "MAT-BEAST-CLAW", "熊爪", 1,
                AssetAliasRegistry::Path("side.drop.beast_claw"),
                pickupTuning);
            return;
        }

        CreatePickup(scene, "Drop_BeastSinew",
            transform.Translation + glm::vec3(0.0f, 0.45f, 0.03f),
            "MAT-BEAST-SINEW", "兽筋", 1,
            AssetAliasRegistry::Path("side.drop.beast_sinew"),
            pickupTuning);
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
