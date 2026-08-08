#include "wtpch.h"
#include "AnimationSystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Animation/AnimationClip.h"
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
            bool includeStart)
        {
            if (!scene || !animator.FireEvents)
                return;

            for (const auto& event : clip.GetEvents())
            {
                if (event.Command.empty())
                    continue;
                if (!IsAnimationEventInRange(event.Time, from, to, includeStart))
                    continue;

                CommandBus::Execute(scene, ExpandAnimationEventCommand(entity, clip, event));
            }
        }

        static void FireAnimationEvents(
            Scene* scene,
            Entity entity,
            const SpriteAnimatorComponent& animator,
            const AnimationClip& clip,
            float previousTime,
            float currentTime)
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
                    previousTime <= 0.0f);
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
                    previousTime <= 0.0f);
                return;
            }

            FireAnimationEventsInRange(scene,
                entity,
                animator,
                clip,
                previousLoopTime,
                totalDuration,
                false);
            FireAnimationEventsInRange(scene,
                entity,
                animator,
                clip,
                0.0f,
                currentLoopTime,
                true);
        }

    } // namespace

    void AnimationSystem::OnRuntimeStart(Scene* scene)
    {
        for (auto e : scene->GetRegistry().view<SpriteAnimatorComponent>())
        {
            auto& anim = scene->GetRegistry().get<SpriteAnimatorComponent>(e);
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

            sr.Texture = frames[0].Texture;
            sr.UVMin = frames[0].TexCoordMin;
            sr.UVMax = frames[0].TexCoordMax;
        }
    }

    void AnimationSystem::UpdateAnimations(Scene* scene, Timestep ts)
    {
        auto& registry = scene->GetRegistry();
        auto view = registry.view<SpriteAnimatorComponent, SpriteRendererComponent>();

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

            FireAnimationEvents(scene, Entity{ e, scene }, animator, *clip, previousTime, animator.ElapsedTime);

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
                sprite.Texture = frame.Texture;
                sprite.UVMin = frame.TexCoordMin;
                sprite.UVMax = frame.TexCoordMax;
            }

            TransformComponent* tc = registry.try_get<TransformComponent>(e);

            for (auto& trackBase : clip->GetPropertyTracks())
            {
                switch (trackBase->Property)
                {
                case AnimatedProperty::SpriteColorA:
                    std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [&sprite](const float& v) { sprite.Color.a = v; }; break;
                case AnimatedProperty::SpriteColorR:
                    std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [&sprite](const float& v) { sprite.Color.r = v; }; break;
                case AnimatedProperty::SpriteColorG:
                    std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [&sprite](const float& v) { sprite.Color.g = v; }; break;
                case AnimatedProperty::SpriteColorB:
                    std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [&sprite](const float& v) { sprite.Color.b = v; }; break;
                case AnimatedProperty::SpriteColor:
                    std::static_pointer_cast<PropertyTrack<glm::vec4>>(trackBase)->Writer =
                        [&sprite](const glm::vec4& v) { sprite.Color = v; }; break;
                case AnimatedProperty::PositionX:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Translation.x = v; }; break;
                case AnimatedProperty::PositionY:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Translation.y = v; }; break;
                case AnimatedProperty::PositionZ:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Translation.z = v; }; break;
                case AnimatedProperty::RotationZ:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Rotation.z = v; }; break;
                case AnimatedProperty::ScaleX:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Scale.x = v; }; break;
                case AnimatedProperty::ScaleY:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Scale.y = v; }; break;
                case AnimatedProperty::ScaleUniform:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Scale.x = tc->Scale.y = v; }; break;
                }
                trackBase->Sample(animator.ElapsedTime, clip->IsLooping(), totalDur);
            }
        }
    }

} // namespace Wheatear
