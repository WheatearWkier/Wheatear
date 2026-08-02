#include "wtpch.h"
#include "TurnCombatTargetService.h"

#include "Wheatear/Modules/Common/GameplayEntityService.h"
#include "Wheatear/Modules/Common/GameplayTargetingService.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Wheatear::TurnCombatTargetService {

    std::vector<Entity> CollectCombatants(Scene* scene)
    {
        std::vector<Entity> entities = GameplayTargetingService::Collect<TurnCombatantComponent>(scene);
        std::sort(entities.begin(), entities.end(), [](Entity a, Entity b)
        {
            const auto& ac = a.GetComponent<TurnCombatantComponent>();
            const auto& bc = b.GetComponent<TurnCombatantComponent>();
            if (ac.Team != bc.Team)
                return ac.Team < bc.Team;
            return ac.Slot < bc.Slot;
        });
        return entities;
    }

    bool HasAliveTeam(Scene* scene, int team)
    {
        return GameplayTargetingService::HasAliveTeam<TurnCombatantComponent>(scene,
            team,
            [](const TurnCombatantComponent& combatant) { return combatant.RuntimeAlive; });
    }

    std::string JoinTurnOrder(Scene* scene, const TurnCombatLevelComponent& level)
    {
        std::ostringstream stream;
        int count = 0;
        for (int i = 0; i < (int)level.RuntimeTurnQueue.size() && count < 7; ++i)
        {
            const int index = (level.RuntimeTurnIndex + i) % (int)level.RuntimeTurnQueue.size();
            Entity entity = GameplayEntityService::Resolve(scene, level.RuntimeTurnQueue[index]);
            if (!entity || !entity.HasComponent<TurnCombatantComponent>())
                continue;

            const auto& combatant = entity.GetComponent<TurnCombatantComponent>();
            if (!combatant.RuntimeAlive)
                continue;

            if (count > 0)
                stream << "  >  ";
            stream << combatant.DisplayName;
            ++count;
        }
        return stream.str();
    }

    void BuildTurnQueue(Scene* scene, TurnCombatLevelComponent& level)
    {
        std::vector<Entity> entities = CollectCombatants(scene);
        std::sort(entities.begin(), entities.end(), [](Entity a, Entity b)
        {
            const auto& ac = a.GetComponent<TurnCombatantComponent>();
            const auto& bc = b.GetComponent<TurnCombatantComponent>();
            if (std::abs(ac.Speed - bc.Speed) > 0.001f)
                return ac.Speed > bc.Speed;
            return ac.Slot < bc.Slot;
        });

        level.RuntimeTurnQueue.clear();
        for (Entity entity : entities)
        {
            const auto& combatant = entity.GetComponent<TurnCombatantComponent>();
            if (combatant.RuntimeAlive)
                level.RuntimeTurnQueue.push_back(entity.GetUUID());
        }
        level.RuntimeTurnIndex = 0;
    }

    bool IsValidTarget(const TurnCombatSkillService::TurnSkillDefinition& skill,
        const TurnCombatantComponent& actor,
        const TurnCombatantComponent& target)
    {
        if (!target.RuntimeAlive)
            return false;

        switch (skill.TargetRule)
        {
        case TurnTargetRule::EnemySingle:
        case TurnTargetRule::EnemyAll:
            return target.Team != actor.Team && target.Team != (int)TurnCombatTeam::Neutral;
        case TurnTargetRule::AllySingle:
        case TurnTargetRule::AllyAll:
            return target.Team == actor.Team;
        case TurnTargetRule::Self:
            return true;
        }
        return false;
    }

    std::vector<Entity> ResolveTargets(Scene* scene,
        const TurnCombatSkillService::TurnSkillDefinition& skill,
        Entity actor,
        Entity explicitTarget)
    {
        std::vector<Entity> targets;
        if (!actor || !actor.HasComponent<TurnCombatantComponent>())
            return targets;

        const auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
        if (skill.TargetRule == TurnTargetRule::Self)
        {
            targets.push_back(actor);
            return targets;
        }

        if (skill.TargetRule == TurnTargetRule::EnemyAll || skill.TargetRule == TurnTargetRule::AllyAll)
        {
            for (Entity candidate : CollectCombatants(scene))
            {
                if (!candidate.HasComponent<TurnCombatantComponent>())
                    continue;
                const auto& targetCombatant = candidate.GetComponent<TurnCombatantComponent>();
                if (IsValidTarget(skill, actorCombatant, targetCombatant))
                    targets.push_back(candidate);
            }
            return targets;
        }

        if (explicitTarget && explicitTarget.HasComponent<TurnCombatantComponent>())
        {
            const auto& targetCombatant = explicitTarget.GetComponent<TurnCombatantComponent>();
            if (IsValidTarget(skill, actorCombatant, targetCombatant))
                targets.push_back(explicitTarget);
        }

        return targets;
    }

    Entity ChooseTargetForAI(Scene* scene,
        Entity actor,
        const TurnCombatSkillService::TurnSkillDefinition& skill)
    {
        if (!actor || !actor.HasComponent<TurnCombatantComponent>())
            return {};

        const auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
        return GameplayTargetingService::FindBest<TurnCombatantComponent>(scene,
            [&](Entity, const TurnCombatantComponent& targetCombatant)
            {
                return IsValidTarget(skill, actorCombatant, targetCombatant);
            },
            [](Entity, const TurnCombatantComponent& targetCombatant)
            {
                return targetCombatant.Health / std::max(targetCombatant.MaxHealth, 1.0f);
            });
    }

    Entity FindTurnTarget(Scene* scene, const std::string& token)
    {
        if (!scene || token.empty())
            return {};

        Entity target = SceneQueries::FindEntityByName(scene, token);
        if (target && target.HasComponent<TurnCombatantComponent>())
            return target;

        for (Entity candidate : CollectCombatants(scene))
        {
            const auto& combatant = candidate.GetComponent<TurnCombatantComponent>();
            if (combatant.TargetButtonEntityName == token
                || combatant.TargetButtonEntityName == token + "_Target"
                || combatant.TargetMarkerEntityName == token
                || combatant.StatusTextEntityName == token)
            {
                return candidate;
            }

            const std::string targetSuffix = "_Target";
            if (combatant.TargetButtonEntityName.size() > targetSuffix.size()
                && combatant.TargetButtonEntityName.rfind(targetSuffix)
                    == combatant.TargetButtonEntityName.size() - targetSuffix.size()
                && combatant.TargetButtonEntityName.substr(
                    0,
                    combatant.TargetButtonEntityName.size() - targetSuffix.size()) == token)
            {
                return candidate;
            }
        }

        return {};
    }

} // namespace Wheatear::TurnCombatTargetService
