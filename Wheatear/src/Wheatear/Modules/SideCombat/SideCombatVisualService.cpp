#include "wtpch.h"
#include "SideCombatVisualService.h"

#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Gameplay/Services/GameplayTextService.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Wheatear::SideCombatVisualService {

    namespace {

        using SideAnimationSetTuning = SideCombatTuningService::SideAnimationSetTuning;
        using SideAnimationClipTuning = SideCombatTuningService::SideAnimationClipTuning;

        static std::string FormatFramePath(const std::string& pattern, int frame)
        {
            return GameplayTextService::FormatFramePath(pattern, frame);
        }

        static const SideAnimationSetTuning& SelectAnimationSet(
            entt::registry& registry,
            entt::entity entity,
            const SideCombatantComponent& combatant,
            const SideCombatTuningService::SideCombatTuning& tuning)
        {
            if (combatant.Team == (int)SideCombatTeam::Player)
                return tuning.PlayerAnimations;

            if (registry.all_of<SideEnemyAIComponent>(entity) &&
                registry.get<SideEnemyAIComponent>(entity).Kind == SideEnemyKind::BearBoss)
            {
                return tuning.BossAnimations;
            }

            return tuning.GruntAnimations;
        }

        static bool HasAnimationClip(const SideAnimationSetTuning& set, const std::string& key)
        {
            return set.Clips.find(key) != set.Clips.end();
        }

        static std::string SelectActionClipKey(
            const SideAnimationSetTuning& set,
            const std::string& attackId,
            float actionTimer,
            float hitboxTime)
        {
            if (attackId.empty())
                return {};

            const std::string windupKey = attackId + "_windup";
            if (actionTimer < hitboxTime && HasAnimationClip(set, windupKey))
                return windupKey;

            return HasAnimationClip(set, attackId) ? attackId : std::string{};
        }

        static bool IsActionClipKey(const std::string& attackId, const std::string& clipKey)
        {
            return attackId == clipKey || clipKey == attackId + "_windup";
        }

        static void EnsureAnimationClipLoaded(SpriteAnimatorComponent& animator,
            const std::string& key,
            const SideAnimationClipTuning& clipTuning)
        {
            if (key.empty() || animator.Clips.find(key) != animator.Clips.end())
                return;

            auto animationClip = AnimationClip::Create(key, clipTuning.Loop);
            const float duration = 1.0f / std::max(1.0f, clipTuning.FrameRate);
            const int frameCount = std::max(1, clipTuning.FrameCount);
            if (clipTuning.Atlas.IsValid())
            {
                for (int frame = 1; frame <= frameCount; ++frame)
                {
                    Ref<Texture2D> texture;
                    glm::vec2 uvMin{ 0.0f };
                    glm::vec2 uvMax{ 1.0f };
                    if (GameplayVisualService::ResolveAtlasFrame(clipTuning.Atlas, frame, &texture, &uvMin, &uvMax))
                        animationClip->AddFrame({ texture, uvMin, uvMax, duration });
                }
            }

            if (animationClip->GetFrameCount() == 0)
            {
                for (int frame = 1; frame <= frameCount; ++frame)
                {
                    if (Ref<Texture2D> texture = GameplayVisualService::LoadTextureCached(FormatFramePath(clipTuning.Pattern, frame)))
                        animationClip->AddFrame({ texture, duration });
                }
            }

            if (animationClip->GetFrameCount() > 0)
                animator.AddClip(animationClip);
        }

        static float GetRemoveAfterDeathAlpha(const SideCombatantComponent& combatant)
        {
            constexpr float FadeStart = 0.52f;
            constexpr float FadeDuration = 0.24f;
            return 1.0f - std::clamp((combatant.RuntimeDeathTimer - FadeStart) / FadeDuration, 0.0f, 1.0f);
        }

        static void ApplyAnimatorCurrentFrame(SpriteRendererComponent& sprite,
            const SpriteAnimatorComponent& animator)
        {
            const AnimationFrame* frame = animator.GetCurrentFrame();
            if (!frame || !frame->Texture)
                return;

            sprite.Texture = frame->Texture;
            sprite.UVMin = frame->TexCoordMin;
            sprite.UVMax = frame->TexCoordMax;
        }

        static std::string SelectVisualClipKey(
            entt::registry& registry,
            entt::entity entity,
            const SideCombatantComponent& combatant,
            const SideAnimationSetTuning& set)
        {
            if (!combatant.Alive || combatant.RuntimeState == SideCombatState::Dead)
                return HasAnimationClip(set, "dead") ? "dead" : "idle";

            if (combatant.Team == (int)SideCombatTeam::Player &&
                registry.all_of<SidePlayerControllerComponent>(entity))
            {
                const auto& controller = registry.get<SidePlayerControllerComponent>(entity);
                const bool actionActive = !controller.RuntimeActionAttackId.empty() &&
                    controller.RuntimeActionTimer < controller.RuntimeActionDuration;
                if (actionActive)
                {
                    const std::string actionClip = SelectActionClipKey(set,
                        controller.RuntimeActionAttackId,
                        controller.RuntimeActionTimer,
                        controller.RuntimeActionHitboxTime);
                    if (!actionClip.empty())
                        return actionClip;
                }
            }

            if (combatant.Team == (int)SideCombatTeam::Enemy &&
                registry.all_of<SideEnemyAIComponent>(entity))
            {
                const auto& ai = registry.get<SideEnemyAIComponent>(entity);
                const bool actionActive = !ai.RuntimeActionAttackId.empty() &&
                    ai.RuntimeActionTimer < ai.RuntimeActionDuration;
                if (actionActive)
                {
                    const std::string actionClip = SelectActionClipKey(set,
                        ai.RuntimeActionAttackId,
                        ai.RuntimeActionTimer,
                        ai.RuntimeActionHitboxTime);
                    if (!actionClip.empty())
                        return actionClip;
                }
            }

            if (combatant.RuntimeHitStun > 0.0f)
                return HasAnimationClip(set, "hit") ? "hit" : "idle";

            if (!combatant.RuntimeOnGround)
            {
                if (combatant.RuntimeAirVelocity > 0.2f && HasAnimationClip(set, "jump"))
                    return "jump";
                return HasAnimationClip(set, "fall") ? "fall" : "idle";
            }

            if (std::abs(combatant.RuntimeVelocity.x) > 0.10f ||
                std::abs(combatant.RuntimeVelocity.y) > 0.10f)
            {
                return HasAnimationClip(set, "run") ? "run" : "idle";
            }

            return "idle";
        }

        static uint32_t SelectVisualActionSequence(
            entt::registry& registry,
            entt::entity entity,
            const SideCombatantComponent& combatant,
            const std::string& clipKey)
        {
            if (combatant.Team == (int)SideCombatTeam::Player &&
                registry.all_of<SidePlayerControllerComponent>(entity))
            {
                const auto& controller = registry.get<SidePlayerControllerComponent>(entity);
                const bool actionActive = !controller.RuntimeActionAttackId.empty() &&
                    controller.RuntimeActionTimer < controller.RuntimeActionDuration;
                if (actionActive && IsActionClipKey(controller.RuntimeActionAttackId, clipKey))
                    return controller.RuntimeActionSequence;
            }

            if (combatant.Team == (int)SideCombatTeam::Enemy &&
                registry.all_of<SideEnemyAIComponent>(entity))
            {
                const auto& ai = registry.get<SideEnemyAIComponent>(entity);
                const bool actionActive = !ai.RuntimeActionAttackId.empty() &&
                    ai.RuntimeActionTimer < ai.RuntimeActionDuration;
                if (actionActive && IsActionClipKey(ai.RuntimeActionAttackId, clipKey))
                    return ai.RuntimeActionSequence;
            }

            return 0;
        }

        static void ApplyCombatantAnimation(
            entt::registry& registry,
            entt::entity entity,
            SideCombatantComponent& combatant,
            SpriteRendererComponent& sprite,
            const SideCombatTuningService::SideCombatTuning& tuning,
            float dt,
            float playbackSpeed)
        {
            const SideAnimationSetTuning& set = SelectAnimationSet(registry, entity, combatant, tuning);
            const std::string clipKey = SelectVisualClipKey(registry, entity, combatant, set);
            const uint32_t actionSequence = SelectVisualActionSequence(registry, entity, combatant, clipKey);
            auto clipIt = set.Clips.find(clipKey);
            if (clipIt == set.Clips.end())
                return;

            const auto& clip = clipIt->second;
            sprite.DrawOffset = clip.RenderOffset;
            sprite.DrawScale = clip.RenderScale;

            auto* animator = registry.try_get<SpriteAnimatorComponent>(entity);
            if (!animator)
            {
                animator = &registry.emplace<SpriteAnimatorComponent>(entity);
                animator->DefaultClipName = "idle";
                animator->PlayOnStart = false;
                animator->FireEvents = true;
            }
            animator->PlaybackSpeed = std::max(0.0f, playbackSpeed);

            if (const auto loadedClip = set.Clips.find(clipKey); loadedClip != set.Clips.end())
                EnsureAnimationClipLoaded(*animator, clipKey, loadedClip->second);

            if (!animator->DefaultClipName.empty() && animator->CurrentClipName.empty())
                animator->CurrentClipName = animator->DefaultClipName;

            if (combatant.RuntimeVisualClipKey != clipKey ||
                combatant.RuntimeVisualActionSequence != actionSequence)
            {
                combatant.RuntimeVisualClipKey = clipKey;
                combatant.RuntimeVisualActionSequence = actionSequence;
                combatant.RuntimeVisualTimer = 0.0f;

                if (animator->Clips.find(clipKey) != animator->Clips.end())
                {
                    animator->CurrentClipName.clear();
                    animator->Play(clipKey);
                    ApplyAnimatorCurrentFrame(sprite, *animator);
                }
            }
            else
            {
                combatant.RuntimeVisualTimer += dt;
                if (animator->CurrentClipName != clipKey && animator->Clips.find(clipKey) != animator->Clips.end())
                {
                    animator->CurrentClipName.clear();
                    animator->Play(clipKey);
                    ApplyAnimatorCurrentFrame(sprite, *animator);
                }
            }

            if (animator->Clips.find(clipKey) != animator->Clips.end())
            {
                if (!animator->IsPlaying && !clip.Loop)
                    animator->IsPlaying = true;
                return;
            }

            const int frameCount = std::max(1, clip.FrameCount);
            const float frameRate = std::max(1.0f, clip.FrameRate);
            int frame = 1 + (int)std::floor(combatant.RuntimeVisualTimer * frameRate);
            if (clip.Loop)
                frame = ((frame - 1) % frameCount) + 1;
            else
                frame = std::min(frame, frameCount);

            if (GameplayVisualService::ApplySpriteAtlasFrame(sprite, clip.Atlas, frame))
                return;

            GameplayVisualService::ApplySpriteFrame(sprite, clip.Pattern, frame);
        }

    } // namespace

    void UpdateCombatantVisual(Scene* scene,
        Entity entity,
        const SideCombatLevelComponent& level,
        const SideCombatTuningService::SideCombatTuning& tuning,
        float dt)
    {
        if (!scene || !entity || !entity.HasComponent<TransformComponent>() || !entity.HasComponent<SideCombatantComponent>())
            return;

        auto& transform = entity.GetComponent<TransformComponent>();
        auto& combatant = entity.GetComponent<SideCombatantComponent>();
        transform.Translation.x = combatant.RuntimeGroundPosition.x;
        transform.Translation.y = combatant.RuntimeGroundPosition.y + combatant.RuntimeAirHeight;
        transform.Translation.z = SideCombatTuningService::CalculateSortZ(combatant.RuntimeGroundPosition.y, tuning);

        if (entity.HasComponent<SpriteRendererComponent>())
        {
            auto& sprite = entity.GetComponent<SpriteRendererComponent>();
            if (entity.HasComponent<SideEnemyAIComponent>() &&
                !entity.GetComponent<SideEnemyAIComponent>().RuntimeAwake &&
                combatant.Alive)
            {
                sprite.Color.a = 0.0f;
            }
            else
            {
                ApplyCombatantAnimation(scene->GetRegistry(),
                    static_cast<entt::entity>(entity),
                    combatant,
                    sprite,
                    tuning,
                    dt,
                    level.RuntimeCinematicTimer > 0.0f
                        ? std::clamp(level.RuntimeCinematicTimeScale, 0.0f, 1.0f)
                        : 1.0f);
                sprite.FlipX = combatant.RuntimeFacing < 0.0f;
                if (!combatant.Alive)
                {
                    sprite.Color.r = 1.0f;
                    sprite.Color.g = 1.0f;
                    sprite.Color.b = 1.0f;
                    sprite.Color.a = combatant.RuntimeRemoveAfterDeath
                        ? GetRemoveAfterDeathAlpha(combatant)
                        : 0.18f;
                }
                else if (combatant.RuntimeState == SideCombatState::SuperArmor)
                {
                    sprite.Color.r = 1.0f;
                    sprite.Color.g = 0.88f;
                    sprite.Color.b = 0.34f;
                    sprite.Color.a = 1.0f;
                }
                else if (combatant.RuntimeState == SideCombatState::Broken)
                {
                    sprite.Color.r = 0.76f;
                    sprite.Color.g = 0.92f;
                    sprite.Color.b = 1.0f;
                    sprite.Color.a = 1.0f;
                }
                else if (combatant.RuntimeInvulnerableTimer > 0.0f ||
                    combatant.RuntimeHitStun > 0.0f ||
                    (combatant.Team == (int)SideCombatTeam::Enemy && !combatant.RuntimeOnGround))
                {
                    sprite.Color.r = 1.0f;
                    sprite.Color.g = 0.78f;
                    sprite.Color.b = 0.72f;
                    sprite.Color.a = 1.0f;
                }
                else
                {
                    sprite.Color = { 1.0f, 1.0f, 1.0f, sprite.Color.a };
                }
            }
        }

        if (!entity.HasComponent<TagComponent>())
            return;

        const std::string shadowName = entity.GetComponent<TagComponent>().Tag + "_Shadow";
        Entity shadow = SceneQueries::FindEntityByName(scene, shadowName);
        if (!shadow || !shadow.HasComponent<TransformComponent>() || !shadow.HasComponent<SpriteRendererComponent>())
            return;

        auto& shadowTransform = shadow.GetComponent<TransformComponent>();
        auto& shadowSprite = shadow.GetComponent<SpriteRendererComponent>();
        shadowTransform.Translation = {
            combatant.RuntimeGroundPosition.x + tuning.ShadowOffset.x,
            combatant.RuntimeGroundPosition.y + tuning.ShadowOffset.y,
            SideCombatTuningService::CalculateSortZ(combatant.RuntimeGroundPosition.y, tuning) - 0.02f
        };

        if (entity.HasComponent<SideEnemyAIComponent>() &&
            !entity.GetComponent<SideEnemyAIComponent>().RuntimeAwake &&
            combatant.Alive)
        {
            shadowSprite.Color = tuning.ShadowColor;
            shadowSprite.Color.a = 0.0f;
            return;
        }

        const float airFade = 1.0f - std::clamp(combatant.RuntimeAirHeight / std::max(0.01f, tuning.ShadowAirFadeHeight), 0.0f, 1.0f);
        const float alpha = tuning.ShadowMinAlpha + (tuning.ShadowMaxAlpha - tuning.ShadowMinAlpha) * airFade;
        shadowSprite.Color = tuning.ShadowColor;
        if (!combatant.Alive)
        {
            shadowSprite.Color.a *= combatant.RuntimeRemoveAfterDeath
                ? alpha * GetRemoveAfterDeathAlpha(combatant)
                : 0.0f;
            return;
        }

        shadowSprite.Color.a *= alpha;
    }

} // namespace Wheatear::SideCombatVisualService
