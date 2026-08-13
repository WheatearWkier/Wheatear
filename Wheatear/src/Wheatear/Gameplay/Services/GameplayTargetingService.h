#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <vector>

namespace Wheatear::GameplayTargetingService {

    template<typename Component>
    std::vector<Entity> Collect(Scene* scene)
    {
        std::vector<Entity> entities;
        if (!scene)
            return entities;

        auto& registry = scene->GetRegistry();
        for (auto handle : registry.view<Component>())
            entities.emplace_back(handle, scene);
        return entities;
    }

    template<typename Component, typename AlivePredicate>
    std::vector<Entity> CollectAlive(Scene* scene, int team, AlivePredicate&& alivePredicate)
    {
        std::vector<Entity> entities;
        if (!scene)
            return entities;

        auto& registry = scene->GetRegistry();
        for (auto handle : registry.view<Component>())
        {
            auto& component = registry.get<Component>(handle);
            if (component.Team == team && alivePredicate(component))
                entities.emplace_back(handle, scene);
        }
        return entities;
    }

    template<typename Component, typename AlivePredicate>
    bool HasAliveTeam(Scene* scene, int team, AlivePredicate&& alivePredicate)
    {
        if (!scene)
            return false;

        auto& registry = scene->GetRegistry();
        for (auto handle : registry.view<Component>())
        {
            auto& component = registry.get<Component>(handle);
            if (component.Team == team && alivePredicate(component))
                return true;
        }
        return false;
    }

    template<typename Component, typename FilterPredicate, typename ScorePredicate>
    Entity FindBest(Scene* scene, FilterPredicate&& filterPredicate, ScorePredicate&& scorePredicate)
    {
        if (!scene)
            return {};

        Entity best;
        float bestScore = std::numeric_limits<float>::max();
        auto& registry = scene->GetRegistry();
        for (auto handle : registry.view<Component>())
        {
            Entity candidate(handle, scene);
            auto& component = registry.get<Component>(handle);
            if (!filterPredicate(candidate, component))
                continue;

            const float score = scorePredicate(candidate, component);
            if (!best || score < bestScore)
            {
                best = candidate;
                bestScore = score;
            }
        }
        return best;
    }

    template<typename Component, typename AlivePredicate>
    Entity FindLowestHealth(Scene* scene, int team, AlivePredicate&& alivePredicate)
    {
        return FindBest<Component>(scene,
            [team, &alivePredicate](Entity, const Component& component)
            {
                return component.Team == team && alivePredicate(component);
            },
            [](Entity, const Component& component)
            {
                return component.Health / std::max(component.MaxHealth, 1.0f);
            });
    }

} // namespace Wheatear::GameplayTargetingService
