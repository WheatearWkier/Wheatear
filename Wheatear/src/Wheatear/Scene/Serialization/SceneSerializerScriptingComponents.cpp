#include "wtpch.h"
#include "SceneSerializerComponentSupport.h"

#include <string>

namespace Wheatear {

    template<> struct ComponentSerializer<EventScriptComponent> {
        static constexpr const char* Key = "EventScriptComponent";
        static void Serialize(YAML::Emitter& o, const EventScriptComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "ScriptPath" << YAML::Value << YAML::DoubleQuoted << c.ScriptPath;
            o << YAML::Key << "StartEvent" << YAML::Value << YAML::DoubleQuoted << c.StartEvent;
            o << YAML::Key << "RunOnStart" << YAML::Value << c.RunOnStart;
            o << YAML::Key << "RunOnce" << YAML::Value << c.RunOnce;
            o << YAML::Key << "Enabled" << YAML::Value << c.Enabled;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, EventScriptComponent& c) {
            c.ScriptPath = n["ScriptPath"].as<std::string>("");
            c.StartEvent = n["StartEvent"].as<std::string>("on_start");
            c.RunOnStart = n["RunOnStart"].as<bool>(true);
            c.RunOnce = n["RunOnce"].as<bool>(true);
            c.Enabled = n["Enabled"].as<bool>(true);
            c.RuntimeActive = false;
            c.RuntimeCompleted = false;
            c.RuntimeStarted = false;
            c.RuntimeEventName.clear();
            c.RuntimeInstructionIndex = 0;
            c.RuntimeWaitRemaining = 0.0f;
        }
    };

    template<> struct ComponentSerializer<AudioSourceComponent> {
        static constexpr const char* Key = "AudioSourceComponent";
        static void Serialize(YAML::Emitter& o, const AudioSourceComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "AudioFilePath" << YAML::Value << c.AudioFilePath;
            o << YAML::Key << "Volume" << YAML::Value << c.Volume;
            o << YAML::Key << "Loop" << YAML::Value << c.Loop;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, AudioSourceComponent& c) {
            c.AudioFilePath = n["AudioFilePath"].as<std::string>();
            c.Volume = n["Volume"].as<float>();
            c.Loop = n["Loop"].as<bool>();
            c.PlayOnStart = n["PlayOnStart"].as<bool>();
        }
    };

    using ScriptingSceneComponents = ComponentGroup
    <
        EventScriptComponent,
        AudioSourceComponent
    >;

    void SerializeScriptingSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(ScriptingSceneComponents{}, out, entity);
    }

    void DeserializeScriptingSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(ScriptingSceneComponents{}, node, entity);
    }

} // namespace Wheatear
