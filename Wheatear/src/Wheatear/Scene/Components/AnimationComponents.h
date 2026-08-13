#pragma once

// Sprite animation component.

#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Core/Core.h"

#include <string>
#include <unordered_map>

namespace Wheatear {

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
            if (!clip || clip->GetFrameCount() == 0) return nullptr;
            return &clip->GetFrames()[CurrentFrameIndex];
        }
    };

} // namespace Wheatear
