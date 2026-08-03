#include "wtpch.h"
#include "TacticalCombatAIService.h"

#include "TacticalCombatActionService.h"
#include "TacticalCombatBoardService.h"
#include "TacticalCombatFeedbackService.h"
#include "TacticalCombatSkillService.h"
#include "Wheatear/Core/AssetAliasRegistry.h"

#include <vector>

namespace Wheatear::TacticalCombatAIService {

    void ProcessEnemyStep(Scene* scene, TacticalCombatLevelComponent& level)
    {
        const std::vector<Entity> units = TacticalCombatBoardService::CollectUnits(scene);
        std::vector<Entity> enemies;
        for (Entity unit : units)
        {
            const auto& tactical = unit.GetComponent<TacticalUnitComponent>();
            if (tactical.Team == (int)TacticalCombatTeam::Enemy && tactical.RuntimeAlive)
                enemies.push_back(unit);
        }

        if (level.RuntimeEnemyCursor >= (int)enemies.size())
        {
            level.RuntimeRound += 1;
            level.RuntimePhase = TacticalCombatPhase::PlayerTurn;
            level.RuntimeSelectedUnit = 0;
            level.RuntimeCommandMenuPage = "root";
            level.RuntimeSelectedSkillId.clear();
            level.RuntimeMessage = "第" + std::to_string(level.RuntimeRound) + "回合，我方行动。";

            for (Entity unit : units)
            {
                auto& tactical = unit.GetComponent<TacticalUnitComponent>();
                TacticalCombatSkillService::TickStatusEffects(tactical);
                tactical.RuntimeHasActed = false;
                tactical.RuntimeMoved = false;
                tactical.RuntimeGuarding = false;
            }
            return;
        }

        Entity enemy = enemies[level.RuntimeEnemyCursor];
        auto& enemyUnit = enemy.GetComponent<TacticalUnitComponent>();
        Entity target = TacticalCombatBoardService::FindNearestAliveEnemy(scene, enemyUnit);
        if (!target || !target.HasComponent<TacticalUnitComponent>())
        {
            level.RuntimeEnemyCursor += 1;
            return;
        }

        const auto& targetUnit = target.GetComponent<TacticalUnitComponent>();
        const std::string skillId = enemyUnit.Skill1Id.empty()
            ? enemyUnit.BasicSkillId
            : enemyUnit.Skill1Id;
        const auto* skill = TacticalCombatSkillService::FindSkill(skillId);
        const int range = skill ? skill->Range : enemyUnit.AttackRange;

        if (TacticalCombatBoardService::Distance(
                enemyUnit.GridX, enemyUnit.GridY, targetUnit.GridX, targetUnit.GridY) > range)
        {
            if (TacticalCombatBoardService::StepUnitOneCellToward(
                    scene, level, enemyUnit, targetUnit))
            {
                TacticalCombatFeedbackService::PlaySound(
                    AssetAliasRegistry::Path("tactical.audio.move"), 0.38f);
            }
            level.RuntimeMessage = enemyUnit.DisplayName + " 正在接近。";
            level.RuntimeEnemyCursor += 1;
            level.RuntimeEnemyStepTimer = 0.0f;
            return;
        }

        TacticalCombatActionService::BeginAction(
            scene, level, enemy, skillId, target, TacticalCombatPhase::EnemyTurn);
    }

} // namespace Wheatear::TacticalCombatAIService
