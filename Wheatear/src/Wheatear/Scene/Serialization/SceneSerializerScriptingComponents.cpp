#include "wtpch.h"
#include "SceneSerializerComponentSupport.h"

#include <string>

namespace Wheatear {

    template<> struct ComponentSerializer<ScriptComponent> {
        static constexpr const char* Key = "ScriptComponent";
        static void Serialize(YAML::Emitter& o, const ScriptComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "ClassName" << YAML::Value << c.ClassName;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ScriptComponent& c) {
            c.ClassName = n["ClassName"].as<std::string>();
        }
    };

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

    static const char* ScriptFieldTypeToString(ScriptFieldType type)
    {
        switch (type)
        {
        case ScriptFieldType::Float:   return "Float";
        case ScriptFieldType::Double:  return "Double";
        case ScriptFieldType::Bool:    return "Bool";
        case ScriptFieldType::Byte:    return "Byte";
        case ScriptFieldType::Short:   return "Short";
        case ScriptFieldType::Int:     return "Int";
        case ScriptFieldType::Long:    return "Long";
        case ScriptFieldType::Vector2: return "Vector2";
        case ScriptFieldType::Vector3: return "Vector3";
        case ScriptFieldType::Vector4: return "Vector4";
        case ScriptFieldType::String:  return "String";
        default:                       return "None";
        }
    }

    static ScriptFieldType ScriptFieldTypeFromString(const std::string& type)
    {
        if (type == "Float")   return ScriptFieldType::Float;
        if (type == "Double")  return ScriptFieldType::Double;
        if (type == "Bool")    return ScriptFieldType::Bool;
        if (type == "Byte")    return ScriptFieldType::Byte;
        if (type == "Short")   return ScriptFieldType::Short;
        if (type == "Int")     return ScriptFieldType::Int;
        if (type == "Long")    return ScriptFieldType::Long;
        if (type == "Vector2") return ScriptFieldType::Vector2;
        if (type == "Vector3") return ScriptFieldType::Vector3;
        if (type == "Vector4") return ScriptFieldType::Vector4;
        if (type == "String")  return ScriptFieldType::String;
        return ScriptFieldType::None;
    }

    static void SerializeScriptFieldValue(YAML::Emitter& out, const ScriptFieldInstance& field)
    {
        switch (field.Field.Type)
        {
        case ScriptFieldType::Float:   out << field.GetValue<float>(); break;
        case ScriptFieldType::Double:  out << field.GetValue<double>(); break;
        case ScriptFieldType::Bool:    out << field.GetValue<bool>(); break;
        case ScriptFieldType::Byte:    out << static_cast<int>(field.GetValue<uint8_t>()); break;
        case ScriptFieldType::Short:   out << field.GetValue<int16_t>(); break;
        case ScriptFieldType::Int:     out << field.GetValue<int32_t>(); break;
        case ScriptFieldType::Long:    out << field.GetValue<int64_t>(); break;
        case ScriptFieldType::Vector2: out << field.GetValue<glm::vec2>(); break;
        case ScriptFieldType::Vector3: out << field.GetValue<glm::vec3>(); break;
        case ScriptFieldType::Vector4: out << field.GetValue<glm::vec4>(); break;
        case ScriptFieldType::String:  out << field.GetStringValue(); break;
        default:                       out << ""; break;
        }
    }

    static void DeserializeScriptFieldValue(const YAML::Node& valueNode, ScriptFieldInstance& field)
    {
        switch (field.Field.Type)
        {
        case ScriptFieldType::Float:   field.SetValue(valueNode.as<float>(0.0f)); break;
        case ScriptFieldType::Double:  field.SetValue(valueNode.as<double>(0.0)); break;
        case ScriptFieldType::Bool:    field.SetValue(valueNode.as<bool>(false)); break;
        case ScriptFieldType::Byte:    field.SetValue(static_cast<uint8_t>(valueNode.as<int>(0))); break;
        case ScriptFieldType::Short:   field.SetValue(static_cast<int16_t>(valueNode.as<int>(0))); break;
        case ScriptFieldType::Int:     field.SetValue(valueNode.as<int32_t>(0)); break;
        case ScriptFieldType::Long:    field.SetValue(valueNode.as<int64_t>(0)); break;
        case ScriptFieldType::Vector2: field.SetValue(valueNode.as<glm::vec2>(glm::vec2(0.0f))); break;
        case ScriptFieldType::Vector3: field.SetValue(valueNode.as<glm::vec3>(glm::vec3(0.0f))); break;
        case ScriptFieldType::Vector4: field.SetValue(valueNode.as<glm::vec4>(glm::vec4(0.0f))); break;
        case ScriptFieldType::String:  field.SetStringValue(valueNode.as<std::string>("")); break;
        default: break;
        }
    }

    static void SerializeScriptComponent(YAML::Emitter& out, Entity entity)
    {
        const auto& component = entity.GetComponent<ScriptComponent>();
        out << YAML::Key << "ScriptComponent" << YAML::BeginMap;
        out << YAML::Key << "ClassName" << YAML::Value << component.ClassName;

        if (ScriptEngine::IsInitialized())
            ScriptEngine::InitializeScriptFieldMap(entity);

        if (ScriptEngine::HasScriptFieldMap(entity.GetUUID()))
        {
            const auto& fields = ScriptEngine::GetScriptFieldMap(entity);
            if (!fields.empty())
            {
                std::vector<std::string> names;
                names.reserve(fields.size());
                for (const auto& [name, field] : fields)
                    names.push_back(name);
                std::sort(names.begin(), names.end());

                out << YAML::Key << "Fields" << YAML::Value << YAML::BeginSeq;
                for (const std::string& name : names)
                {
                    const ScriptFieldInstance& field = fields.at(name);
                    if (field.Field.Type == ScriptFieldType::None)
                        continue;

                    out << YAML::BeginMap;
                    out << YAML::Key << "Name" << YAML::Value << name;
                    out << YAML::Key << "Type" << YAML::Value << ScriptFieldTypeToString(field.Field.Type);
                    out << YAML::Key << "Value" << YAML::Value;
                    SerializeScriptFieldValue(out, field);
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
            }
        }

        out << YAML::EndMap;
    }

    static void DeserializeScriptComponent(const YAML::Node& node, Entity entity)
    {
        auto& component = entity.AddComponent<ScriptComponent>();
        component.ClassName = node["ClassName"].as<std::string>("");

        if (auto fieldsNode = node["Fields"])
        {
            auto& fields = ScriptEngine::GetScriptFieldMap(entity);
            fields.clear();
            for (const auto& fieldNode : fieldsNode)
            {
                std::string name = fieldNode["Name"].as<std::string>("");
                if (name.empty())
                    continue;

                ScriptFieldType type = ScriptFieldTypeFromString(fieldNode["Type"].as<std::string>("None"));
                if (type == ScriptFieldType::None)
                    continue;

                ScriptFieldInstance field;
                field.Field.Name = name;
                field.Field.Type = type;
                if (auto valueNode = fieldNode["Value"])
                    DeserializeScriptFieldValue(valueNode, field);
                fields[name] = field;
            }
        }
    }

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
        if (entity.HasComponent<ScriptComponent>())
            SerializeScriptComponent(out, entity);
        SerializeComponents(ScriptingSceneComponents{}, out, entity);
    }

    void DeserializeScriptingSceneComponents(const YAML::Node& node, Entity entity)
    {
        if (auto scriptNode = node["ScriptComponent"])
            DeserializeScriptComponent(scriptNode, entity);
        DeserializeComponents(ScriptingSceneComponents{}, node, entity);
    }

} // namespace Wheatear
