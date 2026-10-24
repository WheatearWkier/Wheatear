#pragma once

// Sprite animation component.

#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Core/Core.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    // Plays baked animations from a skinned Model. The AnimationSystem fills
    // BonePalette each frame; RenderSystem uploads it to the skinning UBO.
    struct Animator3DComponent
    {
        std::string DefaultClip;
        bool PlayOnStart = true;
        bool Loop = true;
        float PlaybackSpeed = 1.0f;

        // Runtime playback state.
        int CurrentClipIndex = -1;
        int NextClipIndex = -1;
        float Time = 0.0f;
        float NextTime = 0.0f;
        float FadeDuration = 0.2f;
        float FadeElapsed = 0.0f;
        bool IsPlaying = false;

        // Per-entity bone palette (world bone * inverse bind).
        std::vector<glm::mat4> BonePalette;

        // Crossfade to a baked clip. Index 0 is also used as the bind-pose
        // fallback when a model has no animation clips.
        void PlayClip(int index, float fade = 0.15f)
        {
            if (index < 0)
                return;
            if (CurrentClipIndex < 0)
            {
                CurrentClipIndex = index;
                Time = 0.0f;
                NextClipIndex = -1;
                FadeElapsed = 0.0f;
                IsPlaying = true;
                return;
            }
            if (index == CurrentClipIndex || index == NextClipIndex)
                return;

            NextClipIndex = index;
            NextTime = 0.0f;
            FadeDuration = std::max(fade, 0.0f);
            FadeElapsed = 0.0f;
            IsPlaying = true;
        }

        void ResetPlayback()
        {
            CurrentClipIndex = -1;
            NextClipIndex = -1;
            Time = 0.0f;
            NextTime = 0.0f;
            FadeElapsed = 0.0f;
            IsPlaying = false;
        }

        Animator3DComponent() = default;
        Animator3DComponent(const Animator3DComponent&) = default;
    };

    struct SpriteAnimatorComponent
    {
        std::unordered_map<std::string, Ref<AnimationClip>> Clips;
        // Optional clip-name -> .wtanim asset path bindings. On runtime start the
        // AnimationSystem loads any referenced asset into Clips (overriding an
        // inline clip of the same name), so a clip can be authored once and
        // reused across entities without duplicating its data in every scene.
        std::unordered_map<std::string, std::string> ExternalClipAssets;
        std::string DefaultClipName;
        bool PlayOnStart = true;
        bool FireEvents = true;
        float PlaybackSpeed = 1.0f;

        std::string  CurrentClipName;
        int          CurrentFrameIndex = 0;
        float        ElapsedTime = 0.0f;
        bool         IsPlaying = false;
        bool         IsFinished = false;

        SpriteAnimatorComponent() = default;
        SpriteAnimatorComponent(const SpriteAnimatorComponent&) = default;

        void AddClip(const Ref<AnimationClip>& clip)
        {
            Clips[clip->GetName()] = clip;
        }

        void BindExternalClipAsset(const std::string& clipName, const std::string& assetPath)
        {
            ExternalClipAssets[clipName] = assetPath;
        }

        void Play(const std::string& clipName)
        {
            if (CurrentClipName == clipName) return;
            auto it = Clips.find(clipName);
            if (it == Clips.end()) return;

            CurrentClipName = clipName;
            CurrentFrameIndex = 0;
            ElapsedTime = 0.0f;
            IsPlaying = true;
            IsFinished = false;
        }

        Ref<AnimationClip> GetCurrentClip() const
        {
            auto it = Clips.find(CurrentClipName);
            return (it != Clips.end()) ? it->second : nullptr;
        }

        const AnimationFrame* GetCurrentFrame() const
        {
            auto clip = GetCurrentClip();
            const std::size_t frameCount = clip ? clip->GetFrameCount() : 0;
            if (frameCount == 0)
                return nullptr;
            // A clip can be swapped for a shorter one (hot-reloaded .wtanim,
            // runtime replacement) while CurrentFrameIndex still points past
            // the new clip's end; clamp instead of reading out of bounds.
            const int clampedIndex = std::clamp(CurrentFrameIndex, 0,
                static_cast<int>(frameCount) - 1);
            return &clip->GetFrames()[static_cast<std::size_t>(clampedIndex)];
        }
    };

} // namespace Wheatear
