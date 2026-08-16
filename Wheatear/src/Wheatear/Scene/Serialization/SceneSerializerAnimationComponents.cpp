#include "wtpch.h"
#include "SceneSerializerComponentSupport.h"

namespace Wheatear {

    template<> struct ComponentSerializer<SpriteAnimatorComponent> {
        static constexpr const char* Key = "SpriteAnimatorComponent";
        static void Serialize(YAML::Emitter& o, const SpriteAnimatorComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "DefaultClip" << YAML::Value << c.DefaultClipName;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "FireEvents" << YAML::Value << c.FireEvents;
            o << YAML::Key << "PlaybackSpeed" << YAML::Value << c.PlaybackSpeed;
            o << YAML::Key << "ExternalClipAssets" << YAML::Value << YAML::BeginSeq;
            for (const auto& [name, path] : c.ExternalClipAssets)
            {
                o << YAML::BeginMap;
                o << YAML::Key << "Name" << YAML::Value << name;
                o << YAML::Key << "Asset" << YAML::Value << path;
                o << YAML::EndMap;
            }
            o << YAML::EndSeq;
            o << YAML::Key << "Clips" << YAML::Value << YAML::BeginSeq;
            for (const auto& [name, clip] : c.Clips)
            {
                o << YAML::BeginMap;
                o << YAML::Key << "Name" << YAML::Value << clip->GetName();
                o << YAML::Key << "Looping" << YAML::Value << clip->IsLooping();

                o << YAML::Key << "Frames" << YAML::Value << YAML::BeginSeq;
                for (const auto& f : clip->GetFrames()) {
                    o << YAML::BeginMap;
                    o << YAML::Key << "Texture" << YAML::Value << (f.Texture ? f.Texture->GetPath() : "");
                    o << YAML::Key << "UVMin" << YAML::Value << f.TexCoordMin;
                    o << YAML::Key << "UVMax" << YAML::Value << f.TexCoordMax;
                    o << YAML::Key << "Duration" << YAML::Value << f.Duration;
                    o << YAML::EndMap;
                }
                o << YAML::EndSeq;

                o << YAML::Key << "Events" << YAML::Value << YAML::BeginSeq;
                for (const auto& event : clip->GetEvents()) {
                    o << YAML::BeginMap;
                    o << YAML::Key << "Time" << YAML::Value << event.Time;
                    o << YAML::Key << "Name" << YAML::Value << event.Name;
                    o << YAML::Key << "Command" << YAML::Value << YAML::DoubleQuoted << event.Command;
                    o << YAML::EndMap;
                }
                o << YAML::EndSeq;

                o << YAML::Key << "PropertyTracks" << YAML::Value << YAML::BeginSeq;
                for (const auto& tb : clip->GetPropertyTracks()) {
                    // Match the cast to the track's actual data type instead of
                    // trusting the Property enum, so a float track tagged
                    // SpriteColor (or any other mismatch) cannot be
                    // reinterpreted as a different object (UB).
                    const auto vec4Track = std::dynamic_pointer_cast<PropertyTrack<glm::vec4>>(tb);
                    const auto floatTrack = std::dynamic_pointer_cast<PropertyTrack<float>>(tb);
                    if (!vec4Track && !floatTrack)
                    {
                        WT_CORE_WARN("SpriteAnimator serializer: skipping property track with unsupported data type (property {})", (int)tb->Property);
                        continue;
                    }
                    o << YAML::BeginMap;
                    o << YAML::Key << "Property" << YAML::Value << (int)tb->Property;
                    o << YAML::Key << "Keyframes" << YAML::Value << YAML::BeginSeq;
                    if (vec4Track) {
                        for (auto& kf : vec4Track->Keyframes)
                        {
                            o << YAML::BeginMap << YAML::Key << "Time" << kf.Time
                                << YAML::Key << "Value" << kf.Value
                                << YAML::Key << "Mode" << (int)kf.Mode
                                << YAML::EndMap;
                        }
                    }
                    else {
                        for (auto& kf : floatTrack->Keyframes)
                        {
                            o << YAML::BeginMap << YAML::Key << "Time" << kf.Time
                                << YAML::Key << "Value" << kf.Value
                                << YAML::Key << "Mode" << (int)kf.Mode
                                << YAML::EndMap;
                        }
                    }
                    o << YAML::EndSeq << YAML::EndMap;
                }
                o << YAML::EndSeq << YAML::EndMap;
            }
            o << YAML::EndSeq << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SpriteAnimatorComponent& c) {
            if (auto clipsNode = n["Clips"]) {
                for (auto cn : clipsNode) {
                    auto clip = AnimationClip::Create(
                        cn["Name"].as<std::string>(), cn["Looping"].as<bool>(true));
                    if (auto fn = cn["Frames"])
                        for (auto f : fn) {
                            AnimationFrame frame;
                            if (auto p = f["Texture"].as<std::string>(""); !p.empty())
                                frame.Texture = Texture2D::Create(p);
                            frame.TexCoordMin = f["UVMin"].as<glm::vec2>(glm::vec2(0.0f));
                            frame.TexCoordMax = f["UVMax"].as<glm::vec2>(glm::vec2(1.0f));
                            frame.Duration = f["Duration"].as<float>(0.1f);
                            clip->AddFrame(frame);
                        }
                    if (auto tn = cn["PropertyTracks"])
                        for (auto t : tn) {
                            const int propValue = t["Property"].as<int>(-1);
                            if (propValue < 0 || propValue > static_cast<int>(AnimatedProperty::ScaleUniform))
                            {
                                WT_CORE_WARN("SpriteAnimator deserializer: skipping track with invalid Property={}", propValue);
                                continue;
                            }
                            const auto prop = static_cast<AnimatedProperty>(propValue);
                            if (prop == AnimatedProperty::SpriteColor) {
                                auto track = clip->AddVec4Track(prop);
                                for (auto kf : t["Keyframes"])
                                    track->AddKeyframe(kf["Time"].as<float>(0.0f),
                                        kf["Value"].as<glm::vec4>(glm::vec4(1.0f)),
                                        (InterpolationMode)kf["Mode"].as<int>(0));
                            }
                            else {
                                auto track = clip->AddFloatTrack(prop);
                                for (auto kf : t["Keyframes"])
                                    track->AddKeyframe(kf["Time"].as<float>(0.0f),
                                        kf["Value"].as<float>(0.0f),
                                        (InterpolationMode)kf["Mode"].as<int>(0));
                            }
                        }
                    if (auto en = cn["Events"])
                        for (auto e : en) {
                            AnimationEvent event;
                            event.Time = e["Time"].as<float>(0.0f);
                            event.Name = e["Name"].as<std::string>("");
                            event.Command = e["Command"].as<std::string>("");
                            clip->AddEvent(event);
                        }
                    c.AddClip(clip);
                }
            }
            if (auto ex = n["ExternalClipAssets"])
            {
                for (auto e : ex)
                {
                    const std::string name = e["Name"].as<std::string>("");
                    const std::string asset = e["Asset"].as<std::string>("");
                    if (!name.empty() && !asset.empty())
                        c.ExternalClipAssets[name] = asset;
                }
            }
            c.DefaultClipName = n["DefaultClip"].as<std::string>("");
            c.PlayOnStart = n["PlayOnStart"].as<bool>(true);
            c.FireEvents = n["FireEvents"].as<bool>(true);
            c.PlaybackSpeed = n["PlaybackSpeed"].as<float>(1.0f);
        }
    };



    using AnimationSceneComponents = ComponentGroup
    <
        SpriteAnimatorComponent
    >;

    void SerializeAnimationSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(AnimationSceneComponents{}, out, entity);
    }

    void DeserializeAnimationSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(AnimationSceneComponents{}, node, entity);
    }

} // namespace Wheatear
