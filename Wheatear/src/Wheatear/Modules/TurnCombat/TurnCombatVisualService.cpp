#include "wtpch.h"
#include "TurnCombatVisualService.h"

#include "TurnCombatSkillService.h"
#include "TurnCombatTargetService.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Gameplay/Services/GameplayEntityService.h"
#include "Wheatear/Gameplay/Services/GameplayTextService.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::TurnCombatVisualService {

    namespace {

        static std::string FormatAnimationFramePath(const std::string& pattern, int frameIndex)
        {
            return GameplayTextService::FormatFramePath(pattern, frameIndex + 1);
        }

        static const std::string& AnimationPatternForClip(
            const TurnCombatantComponent& combatant,
            const std::string& clip)
        {
            if (clip == "attack")
                return combatant.AttackFramePattern;
            if (clip == "hit")
                return combatant.HitFramePattern;
            if (clip == "down")
                return combatant.DownFramePattern;
            return combatant.IdleFramePattern;
        }

        static int AnimationFrameCountForClip(
            const TurnCombatantComponent& combatant,
            const std::string& clip)
        {
            if (clip == "attack")
                return combatant.AttackFrameCount;
            if (clip == "hit")
                return combatant.HitFrameCount;
            if (clip == "down")
                return combatant.DownFrameCount;
            return combatant.IdleFrameCount;
        }

        static const GameplayVisualService::TextureAtlasFrameSpec& AnimationAtlasForClip(
            const TurnCombatantComponent& combatant,
            const std::string& clip)
        {
            if (clip == "attack")
                return combatant.AttackFrameAtlas;
            if (clip == "hit")
                return combatant.HitFrameAtlas;
            if (clip == "down")
                return combatant.DownFrameAtlas;
            return combatant.IdleFrameAtlas;
        }

        static bool HasAnimationForClip(
            const TurnCombatantComponent& combatant,
            const std::string& clip)
        {
            return !AnimationPatternForClip(combatant, clip).empty() ||
                AnimationAtlasForClip(combatant, clip).IsValid();
        }

        static bool AnimationClipLoops(const std::string& clip)
        {
            return clip == "idle";
        }

        static std::string SelectCombatantAnimationClip(
            Entity entity,
            const TurnCombatantComponent& combatant,
            const TurnCombatLevelComponent& level)
        {
            if (!combatant.RuntimeAlive)
                return "down";
            if (combatant.RuntimeHitFlashTimer > 0.0f && HasAnimationForClip(combatant, "hit"))
                return "hit";
            if (level.RuntimePhase == TurnCombatPhase::Acting
                && entity
                && entity.GetUUID() == level.RuntimeActionActor
                && HasAnimationForClip(combatant, "attack"))
            {
                return "attack";
            }
            return "idle";
        }

        static void ApplyCombatantAnimationFrame(
            Entity entity,
            TurnCombatantComponent& combatant,
            const TurnCombatLevelComponent& level,
            float dt)
        {
            if (!entity || !entity.HasComponent<SpriteRendererComponent>())
                return;

            const std::string clip = SelectCombatantAnimationClip(entity, combatant, level);
            const std::string& pattern = AnimationPatternForClip(combatant, clip);
            const int frameCount = std::max(AnimationFrameCountForClip(combatant, clip), 1);
            const auto& atlas = AnimationAtlasForClip(combatant, clip);
            if (pattern.empty() && !atlas.IsValid())
                return;

            if (!entity.HasComponent<SpriteAnimatorComponent>())
                return;

            auto& animator = entity.GetComponent<SpriteAnimatorComponent>();
            animator.DefaultClipName = animator.DefaultClipName.empty() ? "idle" : animator.DefaultClipName;
            animator.PlayOnStart = false;
            animator.FireEvents = true;

            const auto ensureClip = [&](const std::string& clipName,
                const std::string& clipPattern,
                const GameplayVisualService::TextureAtlasFrameSpec& clipAtlas,
                int clipFrameCount)
            {
                if ((clipPattern.empty() && !clipAtlas.IsValid()) ||
                    animator.Clips.find(clipName) != animator.Clips.end())
                {
                    return;
                }

                const bool looping = AnimationClipLoops(clipName);
                auto animationClip = AnimationClip::Create(clipName, looping);
                const int safeFrameCount = std::max(clipFrameCount, 1);
                const float duration = clipName == "attack"
                    ? std::max(level.ActionDuration, 0.01f) / (float)safeFrameCount
                    : 1.0f / std::max(combatant.AnimationFrameRate, 1.0f);

                if (clipAtlas.IsValid())
                {
                    for (int frameIndex = 0; frameIndex < safeFrameCount; ++frameIndex)
                    {
                        Ref<Texture2D> texture;
                        glm::vec2 uvMin{ 0.0f };
                        glm::vec2 uvMax{ 1.0f };
                        if (GameplayVisualService::ResolveAtlasFrame(clipAtlas, frameIndex + 1, &texture, &uvMin, &uvMax))
                            animationClip->AddFrame({ texture, uvMin, uvMax, duration });
                    }
                }

                if (animationClip->GetFrameCount() == 0)
                {
                    for (int frameIndex = 0; frameIndex < safeFrameCount; ++frameIndex)
                    {
                        if (Ref<Texture2D> texture = GameplayVisualService::LoadTextureCached(FormatAnimationFramePath(clipPattern, frameIndex)))
                            animationClip->AddFrame({ texture, duration });
                    }
                }

                if (animationClip->GetFrameCount() > 0)
                    animator.AddClip(animationClip);
            };

            ensureClip("idle", combatant.IdleFramePattern, combatant.IdleFrameAtlas, combatant.IdleFrameCount);
            ensureClip("attack", combatant.AttackFramePattern, combatant.AttackFrameAtlas, combatant.AttackFrameCount);
            ensureClip("hit", combatant.HitFramePattern, combatant.HitFrameAtlas, combatant.HitFrameCount);
            ensureClip("down", combatant.DownFramePattern, combatant.DownFrameAtlas, combatant.DownFrameCount);

            if (combatant.RuntimeAnimationClip != clip)
            {
                combatant.RuntimeAnimationClip = clip;
                combatant.RuntimeAnimationTimer = 0.0f;
                if (animator.Clips.find(clip) != animator.Clips.end())
                {
                    animator.CurrentClipName.clear();
                    animator.Play(clip);
                }
            }
            else
            {
                combatant.RuntimeAnimationTimer += dt;
                if (animator.CurrentClipName != clip && animator.Clips.find(clip) != animator.Clips.end())
                {
                    animator.CurrentClipName.clear();
                    animator.Play(clip);
                }
            }

            if (animator.Clips.find(clip) != animator.Clips.end())
                return;

            int frameIndex = 0;
            if (clip == "attack" && level.RuntimePhase == TurnCombatPhase::Acting)
            {
                const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 0.999f);
                frameIndex = std::min(frameCount - 1, (int)(t * (float)frameCount));
            }
            else if (AnimationClipLoops(clip))
            {
                frameIndex = (int)(combatant.RuntimeAnimationTimer * std::max(combatant.AnimationFrameRate, 1.0f)) % frameCount;
            }
            else
            {
                frameIndex = std::min(frameCount - 1,
                    (int)(combatant.RuntimeAnimationTimer * std::max(combatant.AnimationFrameRate, 1.0f)));
            }

            auto& sprite = entity.GetComponent<SpriteRendererComponent>();
            if (GameplayVisualService::ApplySpriteAtlasFrame(sprite, atlas, frameIndex + 1))
                return;

            GameplayVisualService::ApplySpriteFrame(sprite, pattern, frameIndex + 1);
        }

    } // namespace

    void CacheVisuals(Entity entity)
    {
        if (!entity || !entity.HasComponent<TurnCombatantComponent>())
            return;

        auto& combatant = entity.GetComponent<TurnCombatantComponent>();
        if (combatant.RuntimeVisualCached || !entity.HasComponent<TransformComponent>())
            return;

        const auto& transform = entity.GetComponent<TransformComponent>();
        combatant.RuntimeBaseTranslation = transform.Translation;
        combatant.RuntimeBaseScale = transform.Scale;
        combatant.RuntimeVisualCached = true;
    }

    void RestoreVisual(Entity entity)
    {
        if (!entity || !entity.HasComponent<TurnCombatantComponent>())
            return;

        auto& combatant = entity.GetComponent<TurnCombatantComponent>();
        if (entity.HasComponent<TransformComponent>() && combatant.RuntimeVisualCached)
        {
            auto& transform = entity.GetComponent<TransformComponent>();
            transform.Translation = combatant.RuntimeBaseTranslation;
            transform.Scale = combatant.RuntimeBaseScale;
        }

        if (entity.HasComponent<SpriteRendererComponent>())
            entity.GetComponent<SpriteRendererComponent>().Color = { 1.0f, 1.0f, 1.0f, combatant.RuntimeAlive ? 1.0f : 0.34f };
    }

    void MarkHit(Entity entity)
    {
        if (!entity || !entity.HasComponent<TurnCombatantComponent>())
            return;
        entity.GetComponent<TurnCombatantComponent>().RuntimeHitFlashTimer = 0.28f;
    }

    void UpdateVisuals(Scene* scene, TurnCombatLevelComponent& level, float dt)
    {
        for (Entity entity : TurnCombatTargetService::CollectCombatants(scene))
        {
            auto& combatant = entity.GetComponent<TurnCombatantComponent>();
            CacheVisuals(entity);

            if (combatant.RuntimeHitFlashTimer > 0.0f)
                combatant.RuntimeHitFlashTimer = std::max(0.0f, combatant.RuntimeHitFlashTimer - dt);

            if (entity.HasComponent<SpriteRendererComponent>())
            {
                ApplyCombatantAnimationFrame(entity, combatant, level, dt);
                auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                if (!combatant.RuntimeAlive)
                {
                    sprite.Color = { 0.45f, 0.48f, 0.52f, 0.35f };
                }
                else if (combatant.RuntimeHitFlashTimer > 0.0f)
                {
                    sprite.Color = combatant.Team == (int)TurnCombatTeam::Player
                        ? glm::vec4{ 0.75f, 1.0f, 0.82f, 1.0f }
                        : glm::vec4{ 1.0f, 0.58f, 0.48f, 1.0f };
                }
                else
                {
                    sprite.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
                }
            }

            if (entity.HasComponent<TransformComponent>() && combatant.RuntimeVisualCached)
            {
                auto& transform = entity.GetComponent<TransformComponent>();
                transform.Translation = combatant.RuntimeBaseTranslation;
                transform.Scale = combatant.RuntimeBaseScale;

                if (entity.GetUUID() == level.RuntimeActionActor && level.RuntimePhase == TurnCombatPhase::Acting)
                {
                    const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 1.0f);
                    const float pulse = std::sin(t * 3.14159265f);
                    const float direction = combatant.Team == (int)TurnCombatTeam::Player ? 1.0f : -1.0f;
                    transform.Translation.x += direction * pulse * 0.42f;
                    transform.Translation.y += pulse * 0.10f;
                    transform.Scale = combatant.RuntimeBaseScale * (1.0f + pulse * 0.055f);
                }
            }
        }

        if (level.RuntimePhase == TurnCombatPhase::Acting)
        {
            const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 1.0f);
            const float alpha = std::max(0.0f, std::sin(t * 3.14159265f) * 0.20f);
            UIRuntimeTools::SetImageAlpha(scene, level.ActionFlashEntityName, alpha);

            Entity effect = SceneQueries::FindEntityByName(scene, level.ActionEffectEntityName);
            const auto* skill = TurnCombatSkillService::FindSkill(level.RuntimeActionSkillId);
            if (effect && skill && effect.HasComponent<SpriteRendererComponent>() && effect.HasComponent<TransformComponent>())
            {
                auto& sprite = effect.GetComponent<SpriteRendererComponent>();
                const int effectFrameCount = std::max(1, skill->EffectFrameCount);
                const int effectFrame = std::min(
                    effectFrameCount,
                    std::max(1, (int)(t * (float)effectFrameCount) + 1));
                if (!GameplayVisualService::ApplySpriteAtlasFrame(sprite, skill->EffectAtlas, effectFrame))
                {
                    if (Ref<Texture2D> texture = GameplayVisualService::LoadTextureCached(skill->EffectPath))
                    {
                        sprite.Texture = texture;
                        sprite.UVMin = { 0.0f, 0.0f };
                        sprite.UVMax = { 1.0f, 1.0f };
                    }
                }

                const float effectAlpha = std::max(0.0f, std::sin(t * 3.14159265f));
                sprite.Color = { 1.0f, 1.0f, 1.0f, effectAlpha };

                Entity target = GameplayEntityService::Resolve(scene, level.RuntimeActionTarget);
                if (!target)
                    target = GameplayEntityService::Resolve(scene, level.RuntimeActionActor);

                if (target && target.HasComponent<TransformComponent>())
                {
                    auto& transform = effect.GetComponent<TransformComponent>();
                    transform.Translation = target.GetComponent<TransformComponent>().Translation;
                    transform.Translation.z = -0.035f;
                    const float scale = 1.0f + effectAlpha * 0.22f;
                    transform.Scale = { scale * 1.55f, scale * 1.15f, 1.0f };
                    transform.Rotation.z = (skill->HealPower > 0.0f || skill->Guard) ? t * 4.8f : -0.35f + t * 0.45f;
                }
            }
        }
        else
        {
            UIRuntimeTools::SetImageAlpha(scene, level.ActionFlashEntityName, 0.0f);
            Entity effect = SceneQueries::FindEntityByName(scene, level.ActionEffectEntityName);
            if (effect && effect.HasComponent<SpriteRendererComponent>())
                effect.GetComponent<SpriteRendererComponent>().Color.a = 0.0f;
        }
    }

} // namespace Wheatear::TurnCombatVisualService
