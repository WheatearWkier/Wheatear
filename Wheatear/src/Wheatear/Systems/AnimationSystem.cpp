#include "wtpch.h"
#include "AnimationSystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Renderer/Model.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Animation/AnimationClipSerializer.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Assets/SpriteSheetAsset.h"
#include "Wheatear/Systems/SpriteSheetSystem.h"
#include "Wheatear/Runtime/CommandBus.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

namespace Wheatear {

    namespace {

        static void ReplaceAll(std::string& value, const std::string& from, const std::string& to)
        {
            if (from.empty())
                return;

            size_t position = 0;
            while ((position = value.find(from, position)) != std::string::npos)
            {
                value.replace(position, from.size(), to);
                position += to.size();
            }
        }

        static std::string ExpandAnimationEventCommand(
            Entity entity,
            const AnimationClip& clip,
            const AnimationEvent& event)
        {
            std::string command = event.Command;
            ReplaceAll(command, "{entity}", entity ? entity.GetName() : "");
            ReplaceAll(command, "{clip}", clip.GetName());
            ReplaceAll(command, "{event}", event.Name);
            return command;
        }

        // Applies a frame to a sprite component. Frames linked to a .wtsheet
        // cell resolve through the shared sheet cache (hot-reloading with the
        // sheet file) and drive a FollowAnimation collider; embedded
        // texture/UV are the fallback for older assets.
        static void ApplyFrameToSprite(Scene* scene, Entity entity, const AnimationFrame& frame,
            Ref<Texture2D>& texture, glm::vec2& uvMin, glm::vec2& uvMax)
        {
            if (!frame.SpriteSheet.empty() && frame.CellIndex >= 0)
            {
                SpriteSheetAsset::ResolvedCell resolved;
                if (SpriteSheetAsset::ResolveCell(frame.SpriteSheet, frame.CellIndex, resolved))
                {
                    texture = resolved.Texture;
                    uvMin = resolved.UVMin;
                    uvMax = resolved.UVMax;
                    SpriteSheetSystem::ApplyColliderToEntity(entity, resolved);
                    return;
                }
            }

            texture = frame.Texture;
            uvMin = frame.TexCoordMin;
            uvMax = frame.TexCoordMax;
        }

        static bool IsAnimationEventInRange(float eventTime, float from, float to, bool includeStart)
        {
            if (includeStart && eventTime >= from && eventTime <= to)
                return true;
            return eventTime > from && eventTime <= to;
        }

        // ---------------------------------------------------------------------
        //  3D skeletal animation
        // ---------------------------------------------------------------------

        struct BonePose
        {
            std::vector<glm::vec3> Translations;
            std::vector<glm::quat> Rotations;
            std::vector<glm::vec3> Scales;
        };

        static glm::mat4 ComposeBone(const glm::vec3& translation,
            const glm::quat& rotation,
            const glm::vec3& scale)
        {
            return glm::translate(glm::mat4(1.0f), translation)
                * glm::mat4_cast(rotation)
                * glm::scale(glm::mat4(1.0f), scale);
        }

        static void SampleBonePose(const Model& model,
            const ModelAnimation& animation,
            float time,
            BonePose& pose)
        {
            const auto& bones = model.GetBones();
            pose.Translations.resize(bones.size());
            pose.Rotations.resize(bones.size());
            pose.Scales.resize(bones.size());

            const float duration = std::max(animation.Duration, 0.0f);
            const float localTime = duration > 0.0f
                ? std::clamp(time, 0.0f, duration)
                : 0.0f;

            const float framePosition = localTime * animation.FramesPerSecond;
            const size_t frame0 = std::min<size_t>(
                static_cast<size_t>(std::max(0.0f, std::floor(framePosition))),
                animation.FrameCount);
            const size_t frame1 = std::min(frame0 + 1,
                static_cast<size_t>(animation.FrameCount));
            const float alpha = std::clamp(
                framePosition - static_cast<float>(frame0), 0.0f, 1.0f);

            for (size_t bone = 0; bone < bones.size(); ++bone)
            {
                const bool hasSamples = bone < animation.Translations.size()
                    && frame1 < animation.Translations[bone].size();
                if (!hasSamples)
                {
                    pose.Translations[bone] = bones[bone].BindTranslation;
                    pose.Rotations[bone] = bones[bone].BindRotation;
                    pose.Scales[bone] = bones[bone].BindScale;
                    continue;
                }

                pose.Translations[bone] = glm::mix(
                    animation.Translations[bone][frame0],
                    animation.Translations[bone][frame1], alpha);
                pose.Rotations[bone] = glm::normalize(glm::slerp(
                    animation.Rotations[bone][frame0],
                    animation.Rotations[bone][frame1], alpha));
                pose.Scales[bone] = glm::mix(
                    animation.Scales[bone][frame0],
                    animation.Scales[bone][frame1], alpha);
            }
        }

        static void BlendBonePose(BonePose& target,
            const BonePose& other,
            float alpha)
        {
            const size_t count = std::min(target.Translations.size(),
                other.Translations.size());
            for (size_t bone = 0; bone < count; ++bone)
            {
                target.Translations[bone] = glm::mix(
                    target.Translations[bone], other.Translations[bone], alpha);
                target.Rotations[bone] = glm::normalize(glm::slerp(
                    target.Rotations[bone], other.Rotations[bone], alpha));
                target.Scales[bone] = glm::mix(
                    target.Scales[bone], other.Scales[bone], alpha);
            }
        }

        static void BuildBonePalette(const Model& model,
            const BonePose& pose,
            std::vector<glm::mat4>& palette)
        {
            const auto& bones = model.GetBones();
            const size_t count = bones.size();
            std::vector<glm::mat4> local(count, glm::mat4(1.0f));
            std::vector<glm::mat4> world(count, glm::mat4(1.0f));
            std::vector<uint8_t> state(count, 0);

            for (size_t bone = 0; bone < count; ++bone)
            {
                local[bone] = ComposeBone(
                    pose.Translations[bone], pose.Rotations[bone], pose.Scales[bone]);
            }

            std::function<void(size_t)> evaluate = [&](size_t bone)
            {
                if (bone >= count || state[bone] == 2)
                    return;
                if (state[bone] == 1)
                {
                    // Malformed cycle: fall back to the bind local transform.
                    world[bone] = local[bone];
                    state[bone] = 2;
                    return;
                }

                state[bone] = 1;
                const int parent = bones[bone].Parent;
                if (parent >= 0 && static_cast<size_t>(parent) < count)
                {
                    evaluate(static_cast<size_t>(parent));
                    world[bone] = world[parent] * local[bone];
                }
                else
                {
                    world[bone] = local[bone];
                }
                state[bone] = 2;
            };

            palette.resize(count);
            for (size_t bone = 0; bone < count; ++bone)
            {
                evaluate(bone);
                palette[bone] = world[bone] * bones[bone].InverseBind;
            }
        }

        static void FireAnimationEventsInRange(
            Scene* scene,
            Entity entity,
            const SpriteAnimatorComponent& animator,
            const AnimationClip& clip,
            float from,
            float to,
            bool includeStart,
            std::vector<std::string>& outCommands)
        {
            if (!scene || !animator.FireEvents)
                return;

            for (const auto& event : clip.GetEvents())
            {
                if (event.Command.empty())
                    continue;
                if (!IsAnimationEventInRange(event.Time, from, to, includeStart))
                    continue;

                outCommands.push_back(ExpandAnimationEventCommand(entity, clip, event));
            }
        }

        static void FireAnimationEvents(
            Scene* scene,
            Entity entity,
            const SpriteAnimatorComponent& animator,
            const AnimationClip& clip,
            float previousTime,
            float currentTime,
            std::vector<std::string>& outCommands)
        {
            const float totalDuration = clip.GetTotalDuration();
            if (totalDuration <= 0.0f || clip.GetEvents().empty())
                return;

            if (!clip.IsLooping())
            {
                FireAnimationEventsInRange(scene,
                    entity,
                    animator,
                    clip,
                    previousTime,
                    std::min(currentTime, totalDuration),
                    previousTime <= 0.0f,
                    outCommands);
                return;
            }

            const float previousLoopTime = std::fmod(previousTime, totalDuration);
            const float currentLoopTime = std::fmod(currentTime, totalDuration);
            const int previousLoop = (int)std::floor(previousTime / totalDuration);
            const int currentLoop = (int)std::floor(currentTime / totalDuration);

            if (currentLoop == previousLoop && currentLoopTime >= previousLoopTime)
            {
                FireAnimationEventsInRange(scene,
                    entity,
                    animator,
                    clip,
                    previousLoopTime,
                    currentLoopTime,
                    previousTime <= 0.0f,
                    outCommands);
                return;
            }

            FireAnimationEventsInRange(scene,
                entity,
                animator,
                clip,
                previousLoopTime,
                totalDuration,
                false,
                outCommands);
            FireAnimationEventsInRange(scene,
                entity,
                animator,
                clip,
                0.0f,
                currentLoopTime,
                true,
                outCommands);
        }

        static void ApplyFloatProperty(AnimatedProperty property,
            float value,
            SpriteRendererComponent& sprite,
            TransformComponent* transform)
        {
            switch (property)
            {
            case AnimatedProperty::SpriteColorA: sprite.Color.a = value; break;
            case AnimatedProperty::SpriteColorR: sprite.Color.r = value; break;
            case AnimatedProperty::SpriteColorG: sprite.Color.g = value; break;
            case AnimatedProperty::SpriteColorB: sprite.Color.b = value; break;
            case AnimatedProperty::PositionX: if (transform) transform->Translation.x = value; break;
            case AnimatedProperty::PositionY: if (transform) transform->Translation.y = value; break;
            case AnimatedProperty::PositionZ: if (transform) transform->Translation.z = value; break;
            case AnimatedProperty::RotationZ: if (transform) transform->Rotation.z = value; break;
            case AnimatedProperty::ScaleX: if (transform) transform->Scale.x = value; break;
            case AnimatedProperty::ScaleY: if (transform) transform->Scale.y = value; break;
            case AnimatedProperty::ScaleUniform:
                if (transform)
                    transform->Scale.x = transform->Scale.y = value;
                break;
            case AnimatedProperty::SpriteColor:
                break;
            }
        }

        static void ApplyVec4Property(AnimatedProperty property,
            const glm::vec4& value,
            SpriteRendererComponent& sprite)
        {
            if (property == AnimatedProperty::SpriteColor)
                sprite.Color = value;
        }

        static void ApplyPropertyTrackSample(const PropertyTrackBase& track,
            const TrackSampleValue& value,
            SpriteRendererComponent& sprite,
            TransformComponent* transform)
        {
            if (const float* floatValue = std::get_if<float>(&value))
            {
                ApplyFloatProperty(track.Property, *floatValue, sprite, transform);
                return;
            }

            if (const glm::vec4* vec4Value = std::get_if<glm::vec4>(&value))
                ApplyVec4Property(track.Property, *vec4Value, sprite);
        }

    } // namespace

    void AnimationSystem::OnRuntimeStart(Scene* scene)
    {
        for (auto e : scene->GetRegistry().view<SpriteAnimatorComponent>())
        {
            auto& anim = scene->GetRegistry().get<SpriteAnimatorComponent>(e);

            // Load external .wtanim bindings so a clip authored once as an asset
            // can drive any entity without duplicating its data in the scene.
            for (const auto& [clipName, assetPath] : anim.ExternalClipAssets)
            {
                if (assetPath.empty())
                    continue;
                const std::filesystem::path resolved = AssetPath::Resolve(assetPath);
                Ref<AnimationClip> clip = AnimationClipSerializer::Load(resolved);
                if (!clip)
                {
                    WT_CORE_WARN("AnimationSystem: failed to load external clip '{}' for '{}'",
                        assetPath, clipName);
                    continue;
                }
                if (clip->GetName().empty())
                    clip->SetName(clipName);
                anim.AddClip(clip);
            }

            anim.CurrentClipName = anim.DefaultClipName;
            anim.ElapsedTime = 0.0f;
            anim.IsPlaying = anim.PlayOnStart;
        }
    }

    void AnimationSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        UpdateAnimations(scene, ts);
    }

    void AnimationSystem::OnUpdateEditor(Scene* scene, Timestep ts)
    {
        if (!m_EditorPreviewActive)
            SyncEditorPreviewFrame(scene);
    }


    void AnimationSystem::SyncEditorPreviewFrame(Scene* scene)
    {
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<SpriteRendererComponent, SpriteAnimatorComponent>())
        {
            auto& sr = registry.get<SpriteRendererComponent>(e);
            auto& anim = registry.get<SpriteAnimatorComponent>(e);

            if (anim.CurrentClipName.empty()) continue;
            auto it = anim.Clips.find(anim.CurrentClipName);
            if (it == anim.Clips.end()) continue;
            const auto& frames = it->second->GetFrames();
            if (frames.empty()) continue;

            ApplyFrameToSprite(scene, Entity{ e, scene }, frames[0], sr.Texture, sr.UVMin, sr.UVMax);
        }
    }

    void AnimationSystem::UpdateAnimations(Scene* scene, Timestep ts)
    {
        auto& registry = scene->GetRegistry();
        auto view = registry.view<SpriteAnimatorComponent, SpriteRendererComponent>();

        // Animation-event commands are collected and executed only after the
        // view loop: CommandBus::Execute runs synchronously and may destroy or
        // modify entities, which would invalidate the view iteration and the
        // animator/sprite references below.
        std::vector<std::string> pendingCommands;

        for (auto e : view)
        {
            auto& animator = view.get<SpriteAnimatorComponent>(e);
            auto& sprite = view.get<SpriteRendererComponent>(e);

            if (!animator.IsPlaying) continue;
            auto clip = animator.GetCurrentClip();
            if (!clip) continue;

            const float previousTime = animator.ElapsedTime;
            animator.ElapsedTime += ts * std::max(0.0f, animator.PlaybackSpeed);
            float totalDur = clip->GetTotalDuration();

            FireAnimationEvents(scene, Entity{ e, scene }, animator, *clip,
                previousTime, animator.ElapsedTime, pendingCommands);

            clip = animator.GetCurrentClip();
            if (!clip) continue;
            totalDur = clip->GetTotalDuration();

            if (totalDur > 0.0f && !clip->IsLooping() && animator.ElapsedTime >= totalDur)
            {
                animator.ElapsedTime = totalDur;
                animator.IsPlaying = false;
                animator.IsFinished = true;
            }

            if (clip->GetFrameCount() > 0)
            {
                const auto& frames = clip->GetFrames();
                float t = animator.ElapsedTime;
                if (clip->IsLooping() && totalDur > 0.0f)
                    t = std::fmod(t, totalDur);

                int   frameIdx = static_cast<int>(frames.size()) - 1;
                float acc = 0.0f;
                for (int i = 0; i < static_cast<int>(frames.size()); ++i)
                {
                    acc += frames[i].Duration;
                    if (t < acc) { frameIdx = i; break; }
                }
                animator.CurrentFrameIndex = frameIdx;

                const auto& frame = frames[frameIdx];
                ApplyFrameToSprite(scene, Entity{ e, scene }, frame, sprite.Texture, sprite.UVMin, sprite.UVMax);
            }

            TransformComponent* tc = registry.try_get<TransformComponent>(e);

            for (auto& trackBase : clip->GetPropertyTracks())
            {
                TrackSampleValue value;
                if (trackBase->SampleValue(animator.ElapsedTime, clip->IsLooping(), totalDur, value))
                    ApplyPropertyTrackSample(*trackBase, value, sprite, tc);
            }
        }

        // Execute collected animation-event commands now that no component
        // references are live inside the iteration.
        for (const std::string& command : pendingCommands)
            CommandBus::Execute(scene, command);

        // 3D skeletal animation. Palettes are computed once per frame and
        // reused by the color and shadow passes.
        for (auto e : registry.view<Animator3DComponent, MeshRendererComponent>())
        {
            auto& animator = registry.get<Animator3DComponent>(e);
            auto& renderer = registry.get<MeshRendererComponent>(e);
            if (!renderer.Model || !renderer.Model->HasSkin())
                continue;

            const Model& model = *renderer.Model;
            const auto& animations = model.GetAnimations();
            const auto& bones = model.GetBones();
            animator.BonePalette.resize(bones.size(), glm::mat4(1.0f));

            if (animations.empty())
            {
                // No clips: leave the palette empty so RenderSystem draws the
                // mesh in its imported bind pose (u_Skinned = 0).
                animator.BonePalette.clear();
                continue;
            }

            if (animator.CurrentClipIndex < 0
                || static_cast<size_t>(animator.CurrentClipIndex) >= animations.size())
            {
                int clipIndex = 0;
                if (!animator.DefaultClip.empty())
                {
                    for (size_t i = 0; i < animations.size(); ++i)
                    {
                        if (animations[i].Name == animator.DefaultClip)
                        {
                            clipIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }
                animator.CurrentClipIndex = clipIndex;
                animator.Time = 0.0f;
                animator.IsPlaying = animator.PlayOnStart;
                WT_CORE_INFO("Animator3D '{}': {} clip(s), playing '{}' ({})",
                    Entity(e, scene).GetName(),
                    animations.size(),
                    animations[clipIndex].Name,
                    animator.IsPlaying ? "yes" : "no");
            }

            if (animator.IsPlaying)
            {
                const float speed = std::max(0.0f, animator.PlaybackSpeed);
                animator.Time += ts * speed;

                const float duration = animations[animator.CurrentClipIndex].Duration;
                if (duration > 0.0f)
                {
                    if (animator.Loop)
                        animator.Time = std::fmod(animator.Time, duration);
                    else if (animator.Time >= duration)
                    {
                        animator.Time = duration;
                        animator.IsPlaying = false;
                    }
                }
            }

            const ModelAnimation& currentAnimation =
                animations[animator.CurrentClipIndex];
            if (currentAnimation.Precomputed)
            {
                const float duration = std::max(currentAnimation.Duration, 0.0f);
                const float clampedTime = duration > 0.0f
                    ? std::clamp(animator.Time, 0.0f, duration)
                    : 0.0f;
                size_t frame = static_cast<size_t>(
                    std::llround(clampedTime * currentAnimation.FramesPerSecond));
                frame = std::min(frame,
                    static_cast<size_t>(currentAnimation.FrameCount));

                for (size_t bone = 0; bone < currentAnimation.BoneMatrices.size(); ++bone)
                {
                    if (frame < currentAnimation.BoneMatrices[bone].size())
                        animator.BonePalette[bone] =
                            currentAnimation.BoneMatrices[bone][frame];
                }

                continue;
            }

            BonePose pose;
            SampleBonePose(model, currentAnimation,
                animator.Time, pose);

            if (animator.NextClipIndex >= 0
                && static_cast<size_t>(animator.NextClipIndex) < animations.size())
            {
                const float speed = std::max(0.0f, animator.PlaybackSpeed);
                animator.NextTime += ts * speed;
                animator.FadeElapsed += ts;

                BonePose nextPose;
                SampleBonePose(model, animations[animator.NextClipIndex],
                    animator.NextTime, nextPose);

                const float alpha = animator.FadeDuration > 0.0f
                    ? std::clamp(animator.FadeElapsed / animator.FadeDuration, 0.0f, 1.0f)
                    : 1.0f;
                BlendBonePose(pose, nextPose, alpha);

                if (alpha >= 1.0f)
                {
                    animator.CurrentClipIndex = animator.NextClipIndex;
                    animator.Time = animator.NextTime;
                    animator.NextClipIndex = -1;
                    animator.FadeElapsed = 0.0f;
                }
            }

            BuildBonePalette(model, pose, animator.BonePalette);

            if (animator.Time < 0.05f)
            {
                float maxTranslation = 0.0f;
                for (const glm::mat4& matrix : animator.BonePalette)
                    maxTranslation = std::max(maxTranslation,
                        glm::length(glm::vec3(matrix[3])));
                WT_CORE_INFO("Animator3D '{}' palette {} bones, max bone translation {:.3f}",
                    Entity(e, scene).GetName(), animator.BonePalette.size(),
                    maxTranslation);
            }
        }
    }

} // namespace Wheatear
