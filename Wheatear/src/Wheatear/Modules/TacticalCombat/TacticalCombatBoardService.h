#pragma once

#include "TacticalCombatComponents.h"
#include "TacticalCombatSkillService.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TacticalCombatBoardService {

    WHEATEAR_API std::string CellTag(const TacticalCombatLevelComponent& level, int x, int y);
    WHEATEAR_API glm::vec2 CellTopLeft(const TacticalCombatLevelComponent& level, int x, int y);
    WHEATEAR_API glm::vec2 CellCenter(const TacticalCombatLevelComponent& level, int x, int y);
    WHEATEAR_API int Distance(int ax, int ay, int bx, int by);
    WHEATEAR_API bool InBounds(const TacticalCombatLevelComponent& level, int x, int y);
    WHEATEAR_API std::vector<Entity> CollectUnits(Scene* scene);
    WHEATEAR_API Entity FindUnitAt(Scene* scene, int x, int y);
    WHEATEAR_API bool HasAliveTeam(Scene* scene, int team);
    WHEATEAR_API bool AllPlayerUnitsDone(Scene* scene);
    WHEATEAR_API bool IsValidTarget(
        const TacticalCombatSkillService::TacticalSkillDefinition& skill,
        const TacticalUnitComponent& actor,
        const TacticalUnitComponent& target);
    WHEATEAR_API bool CanMoveTo(Scene* scene,
        const TacticalCombatLevelComponent& level,
        const TacticalUnitComponent& unit,
        int x,
        int y);
    WHEATEAR_API Entity FindNearestAliveEnemy(Scene* scene, const TacticalUnitComponent& actor);
    WHEATEAR_API bool StepUnitOneCellToward(Scene* scene,
        const TacticalCombatLevelComponent& level,
        TacticalUnitComponent& unit,
        const TacticalUnitComponent& target);

} // namespace Wheatear::TacticalCombatBoardService
