#include "wtpch.h"
#include "TacticalCombatBoardService.h"

#include "Wheatear/Gameplay/Services/GameplayTargetingService.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::TacticalCombatBoardService {

    std::string CellTag(const TacticalCombatLevelComponent& level, int x, int y)
    {
        return level.CellEntityPrefix + std::to_string(x) + "_" + std::to_string(y);
    }

    glm::vec2 CellTopLeft(const TacticalCombatLevelComponent& level, int x, int y)
    {
        return {
            level.BoardOrigin.x + (float)x * level.CellSize.x,
            level.BoardOrigin.y + (float)y * level.CellSize.y
        };
    }

    glm::vec2 CellCenter(const TacticalCombatLevelComponent& level, int x, int y)
    {
        return CellTopLeft(level, x, y) + level.CellSize * 0.5f;
    }

    int Distance(int ax, int ay, int bx, int by)
    {
        return std::abs(ax - bx) + std::abs(ay - by);
    }

    bool InBounds(const TacticalCombatLevelComponent& level, int x, int y)
    {
        return x >= 0 && y >= 0 && x < level.GridWidth && y < level.GridHeight;
    }

    std::vector<Entity> CollectUnits(Scene* scene)
    {
        std::vector<Entity> units = GameplayTargetingService::Collect<TacticalUnitComponent>(scene);
        std::sort(units.begin(), units.end(), [](Entity a, Entity b)
        {
            const auto& ac = a.GetComponent<TacticalUnitComponent>();
            const auto& bc = b.GetComponent<TacticalUnitComponent>();
            if (ac.Team != bc.Team)
                return ac.Team < bc.Team;
            return ac.Slot < bc.Slot;
        });
        return units;
    }

    Entity FindUnitAt(Scene* scene, int x, int y)
    {
        for (Entity unit : CollectUnits(scene))
        {
            const auto& tactical = unit.GetComponent<TacticalUnitComponent>();
            if (tactical.RuntimeAlive && tactical.GridX == x && tactical.GridY == y)
                return unit;
        }
        return {};
    }

    bool HasAliveTeam(Scene* scene, int team)
    {
        return GameplayTargetingService::HasAliveTeam<TacticalUnitComponent>(scene,
            team,
            [](const TacticalUnitComponent& unit) { return unit.RuntimeAlive; });
    }

    bool AllPlayerUnitsDone(Scene* scene)
    {
        bool hasAlivePlayer = false;
        for (Entity unit : CollectUnits(scene))
        {
            const auto& tactical = unit.GetComponent<TacticalUnitComponent>();
            if (tactical.Team != (int)TacticalCombatTeam::Player || !tactical.RuntimeAlive)
                continue;
            hasAlivePlayer = true;
            if (!tactical.RuntimeHasActed)
                return false;
        }
        return hasAlivePlayer;
    }

    bool IsValidTarget(
        const TacticalCombatSkillService::TacticalSkillDefinition& skill,
        const TacticalUnitComponent& actor,
        const TacticalUnitComponent& target)
    {
        if (!target.RuntimeAlive)
            return false;

        if (skill.TargetRule == TacticalCombatSkillService::TacticalTargetRule::Self)
            return actor.GridX == target.GridX && actor.GridY == target.GridY;

        if (skill.TargetRule == TacticalCombatSkillService::TacticalTargetRule::Enemy && actor.Team == target.Team)
            return false;
        if (skill.TargetRule == TacticalCombatSkillService::TacticalTargetRule::Ally && actor.Team != target.Team)
            return false;

        return Distance(actor.GridX, actor.GridY, target.GridX, target.GridY) <= skill.Range;
    }

    bool CanMoveTo(Scene* scene,
        const TacticalCombatLevelComponent& level,
        const TacticalUnitComponent& unit,
        int x,
        int y)
    {
        if (!InBounds(level, x, y))
            return false;
        if (unit.RuntimeMoved)
            return false;
        if (Distance(unit.GridX, unit.GridY, x, y) > unit.MoveRange)
            return false;

        Entity occupant = FindUnitAt(scene, x, y);
        return !occupant;
    }

    Entity FindNearestAliveEnemy(Scene* scene, const TacticalUnitComponent& actor)
    {
        return GameplayTargetingService::FindBest<TacticalUnitComponent>(scene,
            [&](Entity, const TacticalUnitComponent& unit)
            {
                return unit.RuntimeAlive && unit.Team != actor.Team;
            },
            [&](Entity, const TacticalUnitComponent& unit)
            {
                return static_cast<float>(Distance(actor.GridX, actor.GridY, unit.GridX, unit.GridY));
            });
    }

    bool StepUnitOneCellToward(Scene* scene,
        const TacticalCombatLevelComponent& level,
        TacticalUnitComponent& unit,
        const TacticalUnitComponent& target)
    {
        int bestX = unit.GridX;
        int bestY = unit.GridY;
        int bestDistance = Distance(unit.GridX, unit.GridY, target.GridX, target.GridY);

        const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        for (const auto& dir : dirs)
        {
            const int nx = unit.GridX + dir[0];
            const int ny = unit.GridY + dir[1];
            if (!InBounds(level, nx, ny) || FindUnitAt(scene, nx, ny))
                continue;

            const int candidate = Distance(nx, ny, target.GridX, target.GridY);
            if (candidate < bestDistance)
            {
                bestDistance = candidate;
                bestX = nx;
                bestY = ny;
            }
        }

        if (bestX == unit.GridX && bestY == unit.GridY)
            return false;

        unit.GridX = bestX;
        unit.GridY = bestY;
        return true;
    }

} // namespace Wheatear::TacticalCombatBoardService
