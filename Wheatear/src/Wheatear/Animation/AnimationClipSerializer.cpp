#include "wtpch.h"
#include "AnimationClipSerializer.h"

#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/SceneSerializerComponentSupport.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace Wheatear {

    bool AnimationClipSerializer::Save(const Ref<AnimationClip>& clip,
        const std::filesystem::path& filepath)
    {
        if (!clip)
            return false;

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "AnimationClip" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << clip->GetName();
        out << YAML::Key << "Looping" << YAML::Value << clip->IsLooping();

        out << YAML::Key << "Frames" << YAML::Value << YAML::BeginSeq;
        for (const auto& f : clip->GetFrames())
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Texture" << YAML::Value << (f.Texture ? f.Texture->GetPath() : "");
            out << YAML::Key << "UVMin" << YAML::Value << f.TexCoordMin;
            out << YAML::Key << "UVMax" << YAML::Value << f.TexCoordMax;
            out << YAML::Key << "Duration" << YAML::Value << f.Duration;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "Events" << YAML::Value << YAML::BeginSeq;
        for (const auto& event : clip->GetEvents())
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Time" << YAML::Value << event.Time;
            out << YAML::Key << "Name" << YAML::Value << event.Name;
            out << YAML::Key << "Command" << YAML::Value << YAML::DoubleQuoted << event.Command;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "PropertyTracks" << YAML::Value << YAML::BeginSeq;
        for (const auto& tb : clip->GetPropertyTracks())
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Property" << YAML::Value << (int)tb->Property;
            out << YAML::Key << "Keyframes" << YAML::Value << YAML::BeginSeq;
            if (tb->Property == AnimatedProperty::SpriteColor)
            {
                for (auto& kf : std::static_pointer_cast<PropertyTrack<glm::vec4>>(tb)->Keyframes)
                {
                    out << YAML::BeginMap << YAML::Key << "Time" << kf.Time
                        << YAML::Key << "Value" << kf.Value
                        << YAML::Key << "Mode" << (int)kf.Mode
                        << YAML::EndMap;
                }
            }
            else
            {
                for (auto& kf : std::static_pointer_cast<PropertyTrack<float>>(tb)->Keyframes)
                {
                    out << YAML::BeginMap << YAML::Key << "Time" << kf.Time
                        << YAML::Key << "Value" << kf.Value
                        << YAML::Key << "Mode" << (int)kf.Mode
                        << YAML::EndMap;
                }
            }
            out << YAML::EndSeq << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::EndMap; // AnimationClip
        out << YAML::EndMap; // root

        if (!out.good())
            return false;

        const std::filesystem::path resolvedPath = filepath;
        const std::filesystem::path parent = resolvedPath.parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);

        std::ofstream file(resolvedPath, std::ios::trunc);
        if (!file.is_open())
        {
            WT_CORE_ERROR("AnimationClipSerializer: cannot open '{}'", resolvedPath.string());
            return false;
        }
        file << out.c_str();
        return true;
    }

    Ref<AnimationClip> AnimationClipSerializer::Load(const std::filesystem::path& filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            WT_CORE_ERROR("AnimationClipSerializer: cannot open '{}'", filepath.string());
            return nullptr;
        }

        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return LoadFromString(text);
    }

    Ref<AnimationClip> AnimationClipSerializer::LoadFromString(const std::string& text)
    {
        YAML::Node root;
        try
        {
            root = YAML::Load(text);
        }
        catch (const YAML::Exception& e)
        {
            WT_CORE_ERROR("AnimationClipSerializer: YAML parse error: {}", e.what());
            return nullptr;
        }

        const YAML::Node node = root["AnimationClip"];
        if (!node)
        {
            WT_CORE_ERROR("AnimationClipSerializer: missing 'AnimationClip' root");
            return nullptr;
        }

        auto clip = AnimationClip::Create(
            node["Name"].as<std::string>(""),
            node["Looping"].as<bool>(true));

        if (auto fn = node["Frames"])
        {
            for (auto f : fn)
            {
                AnimationFrame frame;
                if (auto p = f["Texture"].as<std::string>(""); !p.empty())
                    frame.Texture = Texture2D::Create(p);
                frame.TexCoordMin = f["UVMin"].as<glm::vec2>(glm::vec2(0.0f));
                frame.TexCoordMax = f["UVMax"].as<glm::vec2>(glm::vec2(1.0f));
                frame.Duration = f["Duration"].as<float>(0.1f);
                clip->AddFrame(frame);
            }
        }

        if (auto tn = node["PropertyTracks"])
        {
            for (auto t : tn)
            {
                // Validate the property enum (hand-edited or older assets may
                // carry out-of-range values); a missing key defaults to -1.
                const int propValue = t["Property"].as<int>(-1);
                if (propValue < 0 || propValue > static_cast<int>(AnimatedProperty::ScaleUniform))
                {
                    WT_CORE_WARN("AnimationClipSerializer: skipping track with invalid Property={}", propValue);
                    continue;
                }
                const auto prop = static_cast<AnimatedProperty>(propValue);
                if (prop == AnimatedProperty::SpriteColor)
                {
                    auto track = clip->AddVec4Track(prop);
                    for (auto kf : t["Keyframes"])
                        track->AddKeyframe(kf["Time"].as<float>(),
                            kf["Value"].as<glm::vec4>(),
                            (InterpolationMode)kf["Mode"].as<int>(0));
                }
                else
                {
                    auto track = clip->AddFloatTrack(prop);
                    for (auto kf : t["Keyframes"])
                        track->AddKeyframe(kf["Time"].as<float>(),
                            kf["Value"].as<float>(),
                            (InterpolationMode)kf["Mode"].as<int>(0));
                }
            }
        }

        if (auto en = node["Events"])
        {
            for (auto e : en)
            {
                AnimationEvent event;
                event.Time = e["Time"].as<float>(0.0f);
                event.Name = e["Name"].as<std::string>("");
                event.Command = e["Command"].as<std::string>("");
                clip->AddEvent(event);
            }
        }

        return clip;
    }

} // namespace Wheatear
