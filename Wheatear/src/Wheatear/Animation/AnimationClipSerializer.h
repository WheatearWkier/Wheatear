#pragma once
#include "Wheatear/Core/Core.h"
#include "AnimationClip.h"
#include <filesystem>
#include <string>

namespace Wheatear {

    // Serializes a single AnimationClip to / from a standalone .wtanim YAML file,
    // mirroring the per-clip body that SceneSerializerAnimationComponents writes
    // for inline scene clips. A clip authored once in the Animation Editor can be
    // saved as an asset and reused on any entity (SpriteAnimatorComponent
    // ExternalClipAssets, or manual AddClip in the editor).
    //
    // Format (top-level map):
    //   AnimationClip:
    //     Name, Looping,
    //     Frames: [{ Texture, UVMin, UVMax, Duration }]
    //     Events: [{ Time, Name, Command }]
    //     PropertyTracks: [{ Property, Keyframes: [{ Time, Value, Mode }] }]
    class WHEATEAR_API AnimationClipSerializer
    {
    public:
        static bool Save(const Ref<AnimationClip>& clip,
            const std::filesystem::path& filepath);
        static Ref<AnimationClip> Load(const std::filesystem::path& filepath);
        static Ref<AnimationClip> LoadFromString(const std::string& text);
    };

} // namespace Wheatear
