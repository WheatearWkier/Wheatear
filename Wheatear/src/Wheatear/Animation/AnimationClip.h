#pragma once
#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Texture.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <string>
#include <vector>
#include <functional>

namespace Wheatear {

    // =========================================================
    // =========================================================
    struct AnimationFrame
    {
        Ref<Texture2D> Texture;
        glm::vec2      TexCoordMin = { 0.0f, 0.0f };
        glm::vec2      TexCoordMax = { 1.0f, 1.0f };
        float          Duration = 0.1f;

        AnimationFrame() = default;

        AnimationFrame(const Ref<Texture2D>& texture, float duration = 0.1f)
            : Texture(texture)
            , TexCoordMin(0.0f, 0.0f)
            , TexCoordMax(1.0f, 1.0f)
            , Duration(duration)
        {
        }

        AnimationFrame(const Ref<Texture2D>& texture,
            const glm::vec2& uvMin,
            const glm::vec2& uvMax,
            float duration = 0.1f)
            : Texture(texture)
            , TexCoordMin(uvMin)
            , TexCoordMax(uvMax)
            , Duration(duration)
        {
        }
    };

    struct AnimationEvent
    {
        float Time = 0.0f;
        std::string Name;
        std::string Command;
    };

    // =========================================================
    // =========================================================
    enum class AnimatedProperty
    {
        // SpriteRendererComponent
        SpriteColor,    // glm::vec4
        SpriteColorR,   // float
        SpriteColorG,   // float
        SpriteColorB,   // float
        SpriteColorA,

        // TransformComponent
        PositionX,      // float
        PositionY,      // float
        PositionZ,      // float
        RotationZ,
        ScaleX,         // float
        ScaleY,         // float
        ScaleUniform,
    };

    inline const char* AnimatedPropertyName(AnimatedProperty p)
    {
        switch (p)
        {
        case AnimatedProperty::SpriteColor:  return "Sprite Color (RGBA)";
        case AnimatedProperty::SpriteColorR: return "Sprite Color R";
        case AnimatedProperty::SpriteColorG: return "Sprite Color G";
        case AnimatedProperty::SpriteColorB: return "Sprite Color B";
        case AnimatedProperty::SpriteColorA: return "Sprite Color A";
        case AnimatedProperty::PositionX:    return "Position X";
        case AnimatedProperty::PositionY:    return "Position Y";
        case AnimatedProperty::PositionZ:    return "Position Z";
        case AnimatedProperty::RotationZ:    return "Rotation Z";
        case AnimatedProperty::ScaleX:       return "Scale X";
        case AnimatedProperty::ScaleY:       return "Scale Y";
        case AnimatedProperty::ScaleUniform: return "Scale (Uniform)";
        }
        return "Unknown";
    }

    // =========================================================
    // =========================================================
    enum class InterpolationMode
    {
        Linear,
        Step,
        EaseIn,
        EaseOut,
        EaseInOut,
    };

    inline const char* InterpolationModeName(InterpolationMode m)
    {
        switch (m)
        {
        case InterpolationMode::Linear:    return "Linear";
        case InterpolationMode::Step:      return "Step";
        case InterpolationMode::EaseIn:    return "EaseIn";
        case InterpolationMode::EaseOut:   return "EaseOut";
        case InterpolationMode::EaseInOut: return "EaseInOut";
        }
        return "Linear";
    }

    // =========================================================
    // =========================================================
    template<typename T>
    struct Keyframe
    {
        float             Time = 0.0f;
        T                 Value = {};
        InterpolationMode Mode = InterpolationMode::Linear;
    };

    // =========================================================
    enum class TrackDataType
    {
        Float,
        Vec2,
        Vec3,
        Vec4
    };

    // =========================================================
    // =========================================================
    struct PropertyTrackBase
    {
        AnimatedProperty Property = AnimatedProperty::SpriteColorA;

        virtual ~PropertyTrackBase() = default;
        virtual void  Sample(float time, bool looping, float totalDuration) = 0;
        virtual float GetDuration()      const = 0;
        virtual int   GetKeyframeCount() const = 0;

        virtual TrackDataType GetDataType() const = 0;

        // Deep copy for undo snapshots; Writer is a runtime-only delegate and is
        // intentionally not copied.
        virtual Ref<PropertyTrackBase> Clone() const = 0;
    };

    // =========================================================
    // =========================================================
    template<typename T>
    struct PropertyTrack : public PropertyTrackBase
    {
        std::vector<Keyframe<T>>  Keyframes;
        std::function<void(const T&)> Writer;

        TrackDataType GetDataType() const override;

        std::vector<Keyframe<T>>& GetKeyframesForEditor() { return Keyframes; }

        void AddKeyframe(float time, const T& value,
            InterpolationMode mode = InterpolationMode::Linear)
        {
            Keyframe<T> kf{ time, value, mode };
            auto it = std::lower_bound(Keyframes.begin(), Keyframes.end(), kf,
                [](const Keyframe<T>& a, const Keyframe<T>& b) { return a.Time < b.Time; });
            Keyframes.insert(it, kf);
        }

        void RemoveKeyframe(int index)
        {
            if (index >= 0 && index < (int)Keyframes.size())
                Keyframes.erase(Keyframes.begin() + index);
        }

        float GetDuration() const override
        {
            return Keyframes.empty() ? 0.0f : Keyframes.back().Time;
        }

        int GetKeyframeCount() const override
        {
            return (int)Keyframes.size();
        }

        Ref<PropertyTrackBase> Clone() const override
        {
            auto copy = CreateRef<PropertyTrack<T>>();
            copy->Property = Property;
            copy->Keyframes = Keyframes;
            return copy;
        }

        void Sample(float time, bool looping, float totalDuration) override
        {
            if (Keyframes.empty() || !Writer) return;

            if (looping && totalDuration > 0.0f)
                time = std::fmod(time, totalDuration);

            if (Keyframes.size() == 1) { Writer(Keyframes[0].Value); return; }
            if (time <= Keyframes.front().Time) { Writer(Keyframes.front().Value); return; }
            if (time >= Keyframes.back().Time) { Writer(Keyframes.back().Value); return; }

            int nextIdx = 1;
            while (nextIdx < (int)Keyframes.size() && Keyframes[nextIdx].Time < time)
                nextIdx++;

            const auto& prev = Keyframes[nextIdx - 1];
            const auto& next = Keyframes[nextIdx];
            float span = next.Time - prev.Time;
            float t = (span > 0.0f) ? (time - prev.Time) / span : 0.0f;

            t = ApplyCurve(t, prev.Mode);
            Writer(Lerp(prev.Value, next.Value, t));
        }

    private:
        static float ApplyCurve(float t, InterpolationMode mode)
        {
            switch (mode)
            {
            case InterpolationMode::Step:      return 0.0f;
            case InterpolationMode::Linear:    return t;
            case InterpolationMode::EaseIn:    return t * t;
            case InterpolationMode::EaseOut:   return t * (2.0f - t);
            case InterpolationMode::EaseInOut: return t < 0.5f
                ? 2.0f * t * t
                : -1.0f + (4.0f - 2.0f * t) * t;
            }
            return t;
        }

        static T Lerp(const T& a, const T& b, float t) { return a + (b - a) * t; }
    };

    template<> inline TrackDataType PropertyTrack<float>::GetDataType() const { return TrackDataType::Float; }
    template<> inline TrackDataType PropertyTrack<glm::vec2>::GetDataType() const { return TrackDataType::Vec2; }
    template<> inline TrackDataType PropertyTrack<glm::vec3>::GetDataType() const { return TrackDataType::Vec3; }
    template<> inline TrackDataType PropertyTrack<glm::vec4>::GetDataType() const { return TrackDataType::Vec4; }

    // =========================================================
    // =========================================================
    class AnimationClip
    {
    public:
        AnimationClip() = default;
        AnimationClip(const std::string& name, bool looping = true)
            : m_Name(name), m_Looping(looping) {
        }

        void AddFrame(const AnimationFrame& frame) { m_Frames.push_back(frame); }
        void ClearFrames() { m_Frames.clear(); }

        void AddFramesFromAtlas(const Ref<Texture2D>& texture,
            int       frameCount,
            glm::vec2 cellSize,
            glm::vec2 startCoord = { 0.0f, 0.0f },
            float     duration = 0.1f)
        {
            float uStep = cellSize.x / (float)texture->GetWidth();
            float vStep = cellSize.y / (float)texture->GetHeight();
            for (int i = 0; i < frameCount; i++)
            {
                float col = startCoord.x + (float)i;
                float row = startCoord.y;
                m_Frames.push_back({ texture,
                    { col * uStep,          row * vStep          },
                    { (col + 1.0f) * uStep, (row + 1.0f) * vStep },
                    duration });
            }
        }

        Ref<PropertyTrack<float>> AddFloatTrack(AnimatedProperty property)
        {
            auto track = CreateRef<PropertyTrack<float>>();
            track->Property = property;
            m_PropertyTracks.push_back(track);
            return track;
        }

        Ref<PropertyTrack<glm::vec4>> AddVec4Track(AnimatedProperty property)
        {
            auto track = CreateRef<PropertyTrack<glm::vec4>>();
            track->Property = property;
            m_PropertyTracks.push_back(track);
            return track;
        }

        void RemovePropertyTrack(int index)
        {
            if (index >= 0 && index < (int)m_PropertyTracks.size())
                m_PropertyTracks.erase(m_PropertyTracks.begin() + index);
        }

        void ClearPropertyTracks() { m_PropertyTracks.clear(); }

        void AddEvent(const AnimationEvent& event)
        {
            auto it = std::lower_bound(m_Events.begin(), m_Events.end(), event.Time,
                [](const AnimationEvent& current, float time) { return current.Time < time; });
            m_Events.insert(it, event);
        }

        void RemoveEvent(int index)
        {
            if (index >= 0 && index < (int)m_Events.size())
                m_Events.erase(m_Events.begin() + index);
        }

        void SortEvents()
        {
            std::sort(m_Events.begin(), m_Events.end(),
                [](const AnimationEvent& a, const AnimationEvent& b)
                {
                    return a.Time < b.Time;
                });
        }

        float GetTotalDuration() const
        {
            float frameDur = 0.0f;
            for (const auto& f : m_Frames) frameDur += f.Duration;
            float trackDur = 0.0f;
            for (const auto& t : m_PropertyTracks)
                trackDur = std::max(trackDur, t->GetDuration());
            float eventDur = 0.0f;
            for (const auto& event : m_Events)
                eventDur = std::max(eventDur, event.Time);
            return std::max(std::max(frameDur, trackDur), eventDur);
        }

        const std::string& GetName()           const { return m_Name; }
        bool                                       IsLooping()         const { return m_Looping; }
        const std::vector<AnimationFrame>& GetFrames()         const { return m_Frames; }
        std::vector<AnimationFrame>& GetFrames() { return m_Frames; }
        int                                        GetFrameCount()     const { return (int)m_Frames.size(); }
        const std::vector<Ref<PropertyTrackBase>>& GetPropertyTracks() const { return m_PropertyTracks; }
        std::vector<Ref<PropertyTrackBase>>& GetPropertyTracks() { return m_PropertyTracks; }
        const std::vector<AnimationEvent>& GetEvents() const { return m_Events; }
        std::vector<AnimationEvent>& GetEvents() { return m_Events; }

        void SetName(const std::string& name) { m_Name = name; }
        void SetLooping(bool looping) { m_Looping = looping; }

        static Ref<AnimationClip> Create(const std::string& name, bool looping = true)
        {
            return CreateRef<AnimationClip>(name, looping);
        }

        // Deep copy used by undo snapshots: frames/events are value-copied,
        // property tracks are cloned per-type, and textures (assets) are shared.
        Ref<AnimationClip> Clone() const
        {
            auto copy = CreateRef<AnimationClip>(m_Name, m_Looping);
            copy->m_Frames = m_Frames;
            copy->m_Events = m_Events;
            copy->m_PropertyTracks.reserve(m_PropertyTracks.size());
            for (const auto& track : m_PropertyTracks)
                copy->m_PropertyTracks.push_back(track->Clone());
            return copy;
        }

    private:
        std::string                         m_Name;
        bool                                m_Looping = true;
        std::vector<AnimationFrame>         m_Frames;
        std::vector<Ref<PropertyTrackBase>> m_PropertyTracks;
        std::vector<AnimationEvent>         m_Events;
    };

} // namespace Wheatear
