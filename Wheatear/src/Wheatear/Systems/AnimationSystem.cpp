#include "wtpch.h"
#include "AnimationSystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Animation/AnimationClipSerializer.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Assets/SpriteSheetAsset.h"
#include "Wheatear/Systems/SpriteSheetSystem.h"
#include "Wheatear/Runtime/CommandBus.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
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
    }

} // namespace Wheatear
