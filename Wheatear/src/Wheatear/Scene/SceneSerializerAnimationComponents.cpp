#include "wtpch.h"
#include "SceneSerializerComponentSupport.h"

namespace Wheatear {

    // SpriteAnimatorComponent 閫昏緫澶嶆潅锛屽崟鐙壒鍖栵紝浣嗘帴鍙ｅ畬鍏ㄤ竴鑷?
    template<> struct ComponentSerializer<SpriteAnimatorComponent> {
        static constexpr const char* Key = "SpriteAnimatorComponent";
        static void Serialize(YAML::Emitter& o, const SpriteAnimatorComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "DefaultClip" << YAML::Value << c.DefaultClipName;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
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

                o << YAML::Key << "PropertyTracks" << YAML::Value << YAML::BeginSeq;
                for (const auto& tb : clip->GetPropertyTracks()) {
                    o << YAML::BeginMap;
                    o << YAML::Key << "Property" << YAML::Value << (int)tb->Property;
                    o << YAML::Key << "Keyframes" << YAML::Value << YAML::BeginSeq;
                    if (tb->Property == AnimatedProperty::SpriteColor) {
                        for (auto& kf : std::static_pointer_cast<PropertyTrack<glm::vec4>>(tb)->Keyframes)
                        {
                            o << YAML::BeginMap << YAML::Key << "Time" << kf.Time
                                << YAML::Key << "Value" << kf.Value
                                << YAML::Key << "Mode" << (int)kf.Mode
                                << YAML::EndMap;
                        }
                    }
                    else {
                        for (auto& kf : std::static_pointer_cast<PropertyTrack<float>>(tb)->Keyframes)
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
                            auto prop = (AnimatedProperty)t["Property"].as<int>();
                            if (prop == AnimatedProperty::SpriteColor) {
                                auto track = clip->AddVec4Track(prop);
                                for (auto kf : t["Keyframes"])
                                    track->AddKeyframe(kf["Time"].as<float>(),
                                        kf["Value"].as<glm::vec4>(),
                                        (InterpolationMode)kf["Mode"].as<int>(0));
                            }
                            else {
                                auto track = clip->AddFloatTrack(prop);
                                for (auto kf : t["Keyframes"])
                                    track->AddKeyframe(kf["Time"].as<float>(),
                                        kf["Value"].as<float>(),
                                        (InterpolationMode)kf["Mode"].as<int>(0));
                            }
                        }
                    c.AddClip(clip);
                }
            }
            c.DefaultClipName = n["DefaultClip"].as<std::string>("");
            c.PlayOnStart = n["PlayOnStart"].as<bool>(true);
        }
    };

    // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
    //  娉ㄥ唽琛細鍞竴闇€瑕佺淮鎶ょ殑鍦版柟
    //  浠ュ悗鏂板缁勪欢锛?) 鍐欑壒鍖? 2) 鍦ㄨ繖閲屽姞涓€琛岀被鍨?
    // 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?

    // 娉ㄦ剰锛歍agComponent 鍜?TransformComponent 鏄瘡涓疄浣撳繀鏈夌殑锛?
    // 鍗曠嫭澶勭悊锛涘叾浣欑粍浠惰蛋涓嬮潰鐨勯€氱敤寰幆銆?

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
