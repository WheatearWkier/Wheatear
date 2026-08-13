#include "wtpch.h"
#include "TacticalCombatVisualService.h"

#include "TacticalCombatBoardService.h"
#include "TacticalCombatSkillService.h"
#include "Wheatear/Gameplay/Services/GameplayEntityService.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::TacticalCombatVisualService {

    namespace {

        static std::string SelectVisualClip(
            const TacticalUnitComponent& unit,
            const TacticalCombatLevelComponent& level,
            Entity entity)
        {
            if (!unit.RuntimeAlive)
                return "down";
            if (unit.RuntimeHitFlashTimer > 0.0f && (!unit.HitFramePattern.empty() || unit.HitFrameAtlas.IsValid()))
                return "hit";
            if (level.RuntimePhase == TacticalCombatPhase::Acting
                && entity
                && entity.GetUUID() == level.RuntimeActionActor
                && (!unit.AttackFramePattern.empty() || unit.AttackFrameAtlas.IsValid()))
            {
                return "attack";
            }
            return "idle";
        }

        static const std::string& PatternForClip(const TacticalUnitComponent& unit, const std::string& clip)
        {
            if (clip == "attack")
                return unit.AttackFramePattern;
            if (clip == "hit")
                return unit.HitFramePattern;
            if (clip == "down")
                return unit.DownFramePattern;
            return unit.IdleFramePattern;
        }

        static const GameplayVisualService::TextureAtlasFrameSpec& AtlasForClip(const TacticalUnitComponent& unit, const std::string& clip)
        {
            if (clip == "attack")
                return unit.AttackFrameAtlas;
            if (clip == "hit")
                return unit.HitFrameAtlas;
            if (clip == "down")
                return unit.DownFrameAtlas;
            return unit.IdleFrameAtlas;
        }

        static int FrameCountForClip(const TacticalUnitComponent& unit, const std::string& clip)
        {
            if (clip == "attack")
                return unit.AttackFrameCount;
            if (clip == "hit")
                return unit.HitFrameCount;
            if (clip == "down")
                return unit.DownFrameCount;
            return unit.IdleFrameCount;
        }

    } // namespace

    void UpdateUnitVisual(Scene* scene,
        const TacticalCombatLevelComponent& level,
        Entity entity,
        TacticalUnitComponent& unit,
        float dt)
    {
        if (!entity)
            return;

        const glm::vec2 topLeft = TacticalCombatBoardService::CellTopLeft(level, unit.GridX, unit.GridY)
            + glm::vec2(level.CellSize.x * 0.10f, level.CellSize.y * 0.02f);
        UIRuntimeTools::SetWidgetTopLeft(scene, entity.GetName(), topLeft, level.CellSize * glm::vec2(0.80f, 0.92f));

        const std::string clip = SelectVisualClip(unit, level, entity);
        if (unit.RuntimeVisualClip != clip)
        {
            unit.RuntimeVisualClip = clip;
            unit.RuntimeVisualTimer = 0.0f;
        }
        else
        {
            unit.RuntimeVisualTimer += dt;
        }

        const std::string& pattern = PatternForClip(unit, clip);
        const auto& atlas = AtlasForClip(unit, clip);
        const int frameCount = std::max(1, FrameCountForClip(unit, clip));
        if (!pattern.empty() || atlas.IsValid())
        {
            int frameIndex = 0;
            if (clip == "attack" && level.RuntimePhase == TacticalCombatPhase::Acting)
            {
                const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 0.999f);
                frameIndex = std::min(frameCount - 1, (int)(t * (float)frameCount));
            }
            else if (clip == "idle")
            {
                frameIndex = (int)(unit.RuntimeVisualTimer * std::max(1.0f, unit.AnimationFrameRate)) % frameCount;
            }
            else
            {
                frameIndex = std::min(frameCount - 1,
                    (int)(unit.RuntimeVisualTimer * std::max(1.0f, unit.AnimationFrameRate)));
            }
            if (!GameplayVisualService::ApplyUIImageAtlasFrame(scene, entity.GetName(), atlas, frameIndex + 1, false))
                GameplayVisualService::ApplyUIImageFrame(scene, entity.GetName(), pattern, frameIndex + 1, false);
        }

        glm::vec4 tint = { 1.0f, 1.0f, 1.0f, unit.RuntimeAlive ? 1.0f : 0.42f };
        if (unit.RuntimeHasActed && unit.RuntimeAlive)
            tint = { 0.56f, 0.62f, 0.68f, 0.86f };
        if (unit.RuntimeHitFlashTimer > 0.0f)
            tint = unit.Team == (int)TacticalCombatTeam::Player
                ? glm::vec4{ 0.72f, 1.0f, 0.82f, 1.0f }
                : glm::vec4{ 1.0f, 0.62f, 0.52f, 1.0f };
        UIRuntimeTools::SetImageColor(scene, entity.GetName(), tint);

        if (!unit.MarkerEntityName.empty())
        {
            const bool selected = entity.GetUUID() == level.RuntimeSelectedUnit;
            UIRuntimeTools::SetWidgetVisible(scene, unit.MarkerEntityName, selected);
            UIRuntimeTools::SetWidgetTopLeft(scene, unit.MarkerEntityName,
                TacticalCombatBoardService::CellTopLeft(level, unit.GridX, unit.GridY) + level.CellSize * 0.05f,
                level.CellSize * 0.90f);
        }
    }

    void UpdateActionEffect(Scene* scene, TacticalCombatLevelComponent& level)
    {
        const auto* skill = TacticalCombatSkillService::FindSkill(level.RuntimeActionSkillId);
        Entity target = GameplayEntityService::Resolve(scene, level.RuntimeActionTarget);
        if (!skill || !target || !target.HasComponent<TacticalUnitComponent>())
        {
            HideActionEffect(scene, level);
            return;
        }

        const auto& targetUnit = target.GetComponent<TacticalUnitComponent>();
        const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 1.0f);
        const float alpha = std::max(0.0f, std::sin(t * 3.14159265f));
        const int frameIndex = std::min(std::max(skill->EffectFrameCount, 1) - 1,
            (int)(t * (float)std::max(skill->EffectFrameCount, 1)));

        UIRuntimeTools::SetWidgetTopLeft(scene,
            level.ActionEffectEntityName,
            TacticalCombatBoardService::CellTopLeft(level, targetUnit.GridX, targetUnit.GridY) - level.CellSize * 0.22f,
            level.CellSize * 1.44f);
        if (!GameplayVisualService::ApplyUIImageAtlasFrame(scene, level.ActionEffectEntityName, skill->EffectAtlas, frameIndex + 1, true))
            GameplayVisualService::ApplyUIImageFrame(scene, level.ActionEffectEntityName, skill->EffectFramePattern, frameIndex + 1, true);
        UIRuntimeTools::SetImageColor(scene, level.ActionEffectEntityName, { 1.0f, 1.0f, 1.0f, alpha });
        UIRuntimeTools::SetWidgetVisible(scene, level.ActionEffectEntityName, alpha > 0.02f);
    }

    void HideActionEffect(Scene* scene, TacticalCombatLevelComponent& level)
    {
        UIRuntimeTools::SetImageAlpha(scene, level.ActionEffectEntityName, 0.0f);
    }

} // namespace Wheatear::TacticalCombatVisualService
