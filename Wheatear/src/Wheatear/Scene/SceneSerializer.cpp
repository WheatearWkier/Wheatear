#include "wtpch.h"
#include "SceneSerializer.h"
#include "Entity.h"
#include "Components.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Scripting/ScriptEngine.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <vector>
#include <fstream>
#include <filesystem>

// ═══════════════════════════════════════════════════════════════════
//  YAML 转换器（glm 类型，不变）
// ═══════════════════════════════════════════════════════════════════

namespace YAML {

    template<> struct convert<glm::vec2> {
        static Node encode(const glm::vec2& v) {
            Node n; n.push_back(v.x); n.push_back(v.y); return n;
        }
        static bool decode(const Node& n, glm::vec2& v) {
            if (!n.IsSequence() || n.size() != 2) return false;
            v = { n[0].as<float>(), n[1].as<float>() }; return true;
        }
    };
    template<> struct convert<glm::vec3> {
        static Node encode(const glm::vec3& v) {
            Node n; n.push_back(v.x); n.push_back(v.y); n.push_back(v.z); return n;
        }
        static bool decode(const Node& n, glm::vec3& v) {
            if (!n.IsSequence() || n.size() != 3) return false;
            v = { n[0].as<float>(), n[1].as<float>(), n[2].as<float>() }; return true;
        }
    };
    template<> struct convert<glm::vec4> {
        static Node encode(const glm::vec4& v) {
            Node n; n.push_back(v.x); n.push_back(v.y);
            n.push_back(v.z); n.push_back(v.w); return n;
        }
        static bool decode(const Node& n, glm::vec4& v) {
            if (!n.IsSequence() || n.size() != 4) return false;
            v = { n[0].as<float>(), n[1].as<float>(),
                  n[2].as<float>(), n[3].as<float>() }; return true;
        }
    };

} // namespace YAML

namespace Wheatear {

    // ── Emitter 辅助（不变）──────────────────────────────────────────
    static YAML::Emitter& operator<<(YAML::Emitter& o, const glm::vec2& v)
    {
        return o << YAML::Flow << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
    }
    static YAML::Emitter& operator<<(YAML::Emitter& o, const glm::vec3& v)
    {
        return o << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
    }
    static YAML::Emitter& operator<<(YAML::Emitter& o, const glm::vec4& v)
    {
        return o << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
    }

    // ── BodyType 转换（不变）────────────────────────────────────────
    static const char* BodyTypeToString(Rigidbody2DComponent::BodyType t) {
        switch (t) {
        case Rigidbody2DComponent::BodyType::Static:    return "Static";
        case Rigidbody2DComponent::BodyType::Dynamic:   return "Dynamic";
        case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
        }
        return "Static";
    }
    static Rigidbody2DComponent::BodyType BodyTypeFromString(const std::string& s) {
        if (s == "Dynamic")   return Rigidbody2DComponent::BodyType::Dynamic;
        if (s == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;
        return Rigidbody2DComponent::BodyType::Static;
    }

    // ═══════════════════════════════════════════════════════════════════
    //  ComponentSerializer 特化
    //  每个特化提供两个静态方法：
    //    Serialize(YAML::Emitter&, const T&)
    //    Deserialize(const YAML::Node&, T&)
    //  以及一个静态字符串 Key（YAML 中的键名）
    // ═══════════════════════════════════════════════════════════════════

    template<> struct ComponentSerializer<TagComponent> {
        static constexpr const char* Key = "TagComponent";
        static void Serialize(YAML::Emitter& o, const TagComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Tag" << YAML::Value << c.Tag;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, TagComponent& c) {
            c.Tag = n["Tag"].as<std::string>();
        }
    };

    template<> struct ComponentSerializer<TransformComponent> {
        static constexpr const char* Key = "TransformComponent";
        static void Serialize(YAML::Emitter& o, const TransformComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Translation" << YAML::Value << c.Translation;
            o << YAML::Key << "Rotation" << YAML::Value << c.Rotation;
            o << YAML::Key << "Scale" << YAML::Value << c.Scale;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, TransformComponent& c) {
            c.Translation = n["Translation"].as<glm::vec3>();
            c.Rotation = n["Rotation"].as<glm::vec3>();
            c.Scale = n["Scale"].as<glm::vec3>();
        }
    };

    template<> struct ComponentSerializer<CameraComponent> {
        static constexpr const char* Key = "CameraComponent";
        static void Serialize(YAML::Emitter& o, const CameraComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Camera" << YAML::BeginMap;
            o << YAML::Key << "ProjectionType" << YAML::Value << (int)c.Camera.GetProjectionType();
            o << YAML::Key << "PerspectiveFOV" << YAML::Value << c.Camera.GetPerspectiveVerticalFOV();
            o << YAML::Key << "PerspectiveNear" << YAML::Value << c.Camera.GetPerspectiveNearClip();
            o << YAML::Key << "PerspectiveFar" << YAML::Value << c.Camera.GetPerspectiveFarClip();
            o << YAML::Key << "OrthographicSize" << YAML::Value << c.Camera.GetOrthographicSize();
            o << YAML::Key << "OrthographicNear" << YAML::Value << c.Camera.GetOrthographicNearClip();
            o << YAML::Key << "OrthographicFar" << YAML::Value << c.Camera.GetOrthographicFarClip();
            o << YAML::EndMap;
            o << YAML::Key << "Primary" << YAML::Value << c.Primary;
            o << YAML::Key << "FixedAspectRatio" << YAML::Value << c.FixedAspectRatio;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, CameraComponent& c) {
            auto cam = n["Camera"];
            c.Camera.SetProjectionType(
                (SceneCamera::ProjectionType)cam["ProjectionType"].as<int>());
            c.Camera.SetPerspectiveVerticalFOV(cam["PerspectiveFOV"].as<float>());
            c.Camera.SetPerspectiveNearClip(cam["PerspectiveNear"].as<float>());
            c.Camera.SetPerspectiveFarClip(cam["PerspectiveFar"].as<float>());
            c.Camera.SetOrthographicSize(cam["OrthographicSize"].as<float>());
            c.Camera.SetOrthographicNearClip(cam["OrthographicNear"].as<float>());
            c.Camera.SetOrthographicFarClip(cam["OrthographicFar"].as<float>());
            c.Primary = n["Primary"].as<bool>();
            c.FixedAspectRatio = n["FixedAspectRatio"].as<bool>();
        }
    };

    template<> struct ComponentSerializer<SpriteRendererComponent> {
        static constexpr const char* Key = "SpriteRendererComponent";
        static void Serialize(YAML::Emitter& o, const SpriteRendererComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Texture" << YAML::Value << (c.Texture ? c.Texture->GetPath() : "");
            o << YAML::Key << "TilingFactor" << YAML::Value << c.TilingFactor;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SpriteRendererComponent& c) {
            c.Color = n["Color"].as<glm::vec4>();
            c.TilingFactor = n["TilingFactor"].as<float>(1.0f);
            if (auto p = n["Texture"].as<std::string>(""); !p.empty())
                c.Texture = Texture2D::Create(p);
        }
    };

    template<> struct ComponentSerializer<CircleRendererComponent> {
        static constexpr const char* Key = "CircleRendererComponent";
        static void Serialize(YAML::Emitter& o, const CircleRendererComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Thickness" << YAML::Value << c.Thickness;
            o << YAML::Key << "Fade" << YAML::Value << c.Fade;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, CircleRendererComponent& c) {
            c.Color = n["Color"].as<glm::vec4>();
            c.Thickness = n["Thickness"].as<float>();
            c.Fade = n["Fade"].as<float>();
        }
    };

    template<> struct ComponentSerializer<Rigidbody2DComponent> {
        static constexpr const char* Key = "Rigidbody2DComponent";
        static void Serialize(YAML::Emitter& o, const Rigidbody2DComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "BodyType" << YAML::Value << BodyTypeToString(c.Type);
            o << YAML::Key << "FixedRotation" << YAML::Value << c.FixedRotation;
            o << YAML::Key << "GravityScale" << YAML::Value << c.GravityScale;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, Rigidbody2DComponent& c) {
            c.Type = BodyTypeFromString(n["BodyType"].as<std::string>());
            c.FixedRotation = n["FixedRotation"].as<bool>();
            c.GravityScale = n["GravityScale"].as<float>(1.0f);
        }
    };

    template<> struct ComponentSerializer<BoxCollider2DComponent> {
        static constexpr const char* Key = "BoxCollider2DComponent";
        static void Serialize(YAML::Emitter& o, const BoxCollider2DComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Offset" << YAML::Value << c.Offset;
            o << YAML::Key << "Size" << YAML::Value << c.Size;
            o << YAML::Key << "Density" << YAML::Value << c.Density;
            o << YAML::Key << "Friction" << YAML::Value << c.Friction;
            o << YAML::Key << "Restitution" << YAML::Value << c.Restitution;
            o << YAML::Key << "RestitutionThreshold" << YAML::Value << c.RestitutionThreshold;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, BoxCollider2DComponent& c) {
            c.Offset = n["Offset"].as<glm::vec2>();
            c.Size = n["Size"].as<glm::vec2>();
            c.Density = n["Density"].as<float>();
            c.Friction = n["Friction"].as<float>();
            c.Restitution = n["Restitution"].as<float>();
            c.RestitutionThreshold = n["RestitutionThreshold"].as<float>();
        }
    };

    template<> struct ComponentSerializer<CircleCollider2DComponent> {
        static constexpr const char* Key = "CircleCollider2DComponent";
        static void Serialize(YAML::Emitter& o, const CircleCollider2DComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Offset" << YAML::Value << c.Offset;
            o << YAML::Key << "Radius" << YAML::Value << c.Radius;
            o << YAML::Key << "Density" << YAML::Value << c.Density;
            o << YAML::Key << "Friction" << YAML::Value << c.Friction;
            o << YAML::Key << "Restitution" << YAML::Value << c.Restitution;
            o << YAML::Key << "RestitutionThreshold" << YAML::Value << c.RestitutionThreshold;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, CircleCollider2DComponent& c) {
            c.Offset = n["Offset"].as<glm::vec2>();
            c.Radius = n["Radius"].as<float>();
            c.Density = n["Density"].as<float>();
            c.Friction = n["Friction"].as<float>();
            c.Restitution = n["Restitution"].as<float>();
            c.RestitutionThreshold = n["RestitutionThreshold"].as<float>();
        }
    };

    //template<> struct ComponentSerializer<MeshRendererComponent>
    //{
    //    static constexpr const char* Key = "MeshRendererComponent";
    //    static void Serialize(YAML::Emitter& o, const MeshRendererComponent& c)
    //    {
    //        o << YAML::Key << Key << YAML::BeginMap;
    //        o << YAML::Key << "MeshPath" << YAML::Value << (c.Mesh ? c.Mesh->GetFilepath() : "");
    //        o << YAML::Key << "Albedo" << YAML::Value << c.Material.Albedo;
    //        o << YAML::Key << "Metallic" << YAML::Value << c.Material.Metallic;
    //        o << YAML::Key << "Roughness" << YAML::Value << c.Material.Roughness;
    //        o << YAML::EndMap;
    //    }
    //    static void Deserialize(const YAML::Node& n, MeshRendererComponent& c)
    //    {
    //        c.Material.Albedo = n["Albedo"].as<glm::vec4>(glm::vec4(1.0f));
    //        c.Material.Metallic = n["Metallic"].as<float>(0.0f);
    //        c.Material.Roughness = n["Roughness"].as<float>(0.5f);
    //        auto path = n["MeshPath"].as<std::string>("");
    //        if (!path.empty())
    //            c.Mesh = Mesh::Create(path);
    //        else
    //            c.Mesh = Mesh::CreateCube(); // 路径为空则用默认立方体
    //    }
    //};

    template<> struct ComponentSerializer<MeshRendererComponent>
    {
        static constexpr const char* Key = "MeshRendererComponent";
        static void Serialize(YAML::Emitter& o, const MeshRendererComponent& c)
        {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "MeshPath" << YAML::Value
                << (c.Mesh ? c.Mesh->GetFilepath() : "");
            o << YAML::Key << "MaterialPath" << YAML::Value
                << (c.Material ? c.Material->GetPath() : "");
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, MeshRendererComponent& c)
        {
            auto meshPath = n["MeshPath"].as<std::string>("");
            if (!meshPath.empty()) c.Mesh = Mesh::Create(meshPath);
            else                   c.Mesh = Mesh::CreateCube();

            auto matPath = n["MaterialPath"].as<std::string>("");
            if (!matPath.empty()) c.Material = Material::Load(matPath);
            else                  c.Material = Material::Create();
        }
    };

    template<> struct ComponentSerializer<DirectionalLightComponent>
    {
        static constexpr const char* Key = "DirectionalLightComponent";
        static void Serialize(YAML::Emitter& o, const DirectionalLightComponent& c)
        {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Intensity" << YAML::Value << c.Intensity;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, DirectionalLightComponent& c)
        {
            c.Color = n["Color"].as<glm::vec3>();
            c.Intensity = n["Intensity"].as<float>(1.0f);
        }
    };

    template<> struct ComponentSerializer<PointLightComponent>
    {
        static constexpr const char* Key = "PointLightComponent";
        static void Serialize(YAML::Emitter& o, const PointLightComponent& c)
        {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Intensity" << YAML::Value << c.Intensity;
            o << YAML::Key << "Constant" << YAML::Value << c.Constant;
            o << YAML::Key << "Linear" << YAML::Value << c.Linear;
            o << YAML::Key << "Quadratic" << YAML::Value << c.Quadratic;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, PointLightComponent& c)
        {
            c.Color = n["Color"].as<glm::vec3>();
            c.Intensity = n["Intensity"].as<float>(1.0f);
            c.Constant = n["Constant"].as<float>(1.0f);
            c.Linear = n["Linear"].as<float>(0.09f);
            c.Quadratic = n["Quadratic"].as<float>(0.032f);
        }
    };

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

    template<> struct ComponentSerializer<VisualNovelComponent> {
        static constexpr const char* Key = "VisualNovelComponent";
        static void Serialize(YAML::Emitter& o, const VisualNovelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "ScriptPath" << YAML::Value << c.ScriptPath;
            o << YAML::Key << "CharactersPerSecond" << YAML::Value << c.CharactersPerSecond;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "RestartOnFinish" << YAML::Value << c.RestartOnFinish;
            o << YAML::Key << "SpeakerTextEntityName" << YAML::Value << c.SpeakerTextEntityName;
            o << YAML::Key << "BodyTextEntityName" << YAML::Value << c.BodyTextEntityName;
            o << YAML::Key << "AdvanceHintEntityName" << YAML::Value << c.AdvanceHintEntityName;
            o << YAML::Key << "BackgroundEntityName" << YAML::Value << c.BackgroundEntityName;
            o << YAML::Key << "FloorEntityName" << YAML::Value << c.FloorEntityName;
            o << YAML::Key << "CharacterEntityPrefix" << YAML::Value << c.CharacterEntityPrefix;
            o << YAML::Key << "ChoiceEntityPrefix" << YAML::Value << c.ChoiceEntityPrefix;
            o << YAML::Key << "MaxVisibleChoices" << YAML::Value << c.MaxVisibleChoices;
            o << YAML::Key << "AutoPlayOnStart" << YAML::Value << c.AutoPlayOnStart;
            o << YAML::Key << "AutoPlayDelay" << YAML::Value << c.AutoPlayDelay;
            o << YAML::Key << "HistoryTextEntityName" << YAML::Value << c.HistoryTextEntityName;
            o << YAML::Key << "AutoPlayIndicatorEntityName" << YAML::Value << c.AutoPlayIndicatorEntityName;
            o << YAML::Key << "CommandBarEntityName" << YAML::Value << c.CommandBarEntityName;
            o << YAML::Key << "HistoryPanelEntityName" << YAML::Value << c.HistoryPanelEntityName;
            o << YAML::Key << "SettingsPanelEntityName" << YAML::Value << c.SettingsPanelEntityName;
            o << YAML::Key << "SettingsTextEntityName" << YAML::Value << c.SettingsTextEntityName;
            o << YAML::Key << "SystemMessageEntityName" << YAML::Value << c.SystemMessageEntityName;
            o << YAML::Key << "SaveDirectory" << YAML::Value << c.SaveDirectory;
            o << YAML::Key << "AutoLoadSlot" << YAML::Value << c.AutoLoadSlot;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, VisualNovelComponent& c) {
            c.ScriptPath = n["ScriptPath"].as<std::string>(c.ScriptPath);
            c.CharactersPerSecond = n["CharactersPerSecond"].as<float>(c.CharactersPerSecond);
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.RestartOnFinish = n["RestartOnFinish"].as<bool>(c.RestartOnFinish);
            c.SpeakerTextEntityName = n["SpeakerTextEntityName"].as<std::string>(c.SpeakerTextEntityName);
            c.BodyTextEntityName = n["BodyTextEntityName"].as<std::string>(c.BodyTextEntityName);
            c.AdvanceHintEntityName = n["AdvanceHintEntityName"].as<std::string>(c.AdvanceHintEntityName);
            c.BackgroundEntityName = n["BackgroundEntityName"].as<std::string>(c.BackgroundEntityName);
            c.FloorEntityName = n["FloorEntityName"].as<std::string>(c.FloorEntityName);
            c.CharacterEntityPrefix = n["CharacterEntityPrefix"].as<std::string>(c.CharacterEntityPrefix);
            c.ChoiceEntityPrefix = n["ChoiceEntityPrefix"].as<std::string>(c.ChoiceEntityPrefix);
            c.MaxVisibleChoices = n["MaxVisibleChoices"].as<uint32_t>(c.MaxVisibleChoices);
            c.AutoPlayOnStart = n["AutoPlayOnStart"].as<bool>(c.AutoPlayOnStart);
            c.AutoPlayDelay = n["AutoPlayDelay"].as<float>(c.AutoPlayDelay);
            c.HistoryTextEntityName = n["HistoryTextEntityName"].as<std::string>(c.HistoryTextEntityName);
            c.AutoPlayIndicatorEntityName = n["AutoPlayIndicatorEntityName"].as<std::string>(c.AutoPlayIndicatorEntityName);
            c.CommandBarEntityName = n["CommandBarEntityName"].as<std::string>(c.CommandBarEntityName);
            c.HistoryPanelEntityName = n["HistoryPanelEntityName"].as<std::string>(c.HistoryPanelEntityName);
            c.SettingsPanelEntityName = n["SettingsPanelEntityName"].as<std::string>(c.SettingsPanelEntityName);
            c.SettingsTextEntityName = n["SettingsTextEntityName"].as<std::string>(c.SettingsTextEntityName);
            c.SystemMessageEntityName = n["SystemMessageEntityName"].as<std::string>(c.SystemMessageEntityName);
            c.SaveDirectory = n["SaveDirectory"].as<std::string>(c.SaveDirectory);
            c.AutoLoadSlot = n["AutoLoadSlot"].as<int>(c.AutoLoadSlot);
        }
    };
    template<> struct ComponentSerializer<ArcadeCombatLevelComponent> {
        static constexpr const char* Key = "ArcadeCombatLevelComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeCombatLevelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "ArenaMin" << YAML::Value << c.ArenaMin;
            o << YAML::Key << "ArenaMax" << YAML::Value << c.ArenaMax;
            o << YAML::Key << "PlayerEntityName" << YAML::Value << c.PlayerEntityName;
            o << YAML::Key << "BossEntityName" << YAML::Value << c.BossEntityName;
            o << YAML::Key << "FadeEntityName" << YAML::Value << c.FadeEntityName;
            o << YAML::Key << "PausePanelEntityName" << YAML::Value << c.PausePanelEntityName;
            o << YAML::Key << "MessageTextEntityName" << YAML::Value << c.MessageTextEntityName;
            o << YAML::Key << "WeaponTextEntityName" << YAML::Value << c.WeaponTextEntityName;
            o << YAML::Key << "PlayerHealthBarEntityName" << YAML::Value << c.PlayerHealthBarEntityName;
            o << YAML::Key << "PlayerHealthTextEntityName" << YAML::Value << c.PlayerHealthTextEntityName;
            o << YAML::Key << "BossHealthBarEntityName" << YAML::Value << c.BossHealthBarEntityName;
            o << YAML::Key << "BossHealthTextEntityName" << YAML::Value << c.BossHealthTextEntityName;
            o << YAML::Key << "StartFadeDuration" << YAML::Value << c.StartFadeDuration;
            o << YAML::Key << "VictoryReturnDelay" << YAML::Value << c.VictoryReturnDelay;
            o << YAML::Key << "DefeatReturnDelay" << YAML::Value << c.DefeatReturnDelay;
            o << YAML::Key << "ResultSceneFadeDuration" << YAML::Value << c.ResultSceneFadeDuration;
            o << YAML::Key << "BossDefeatFadeDuration" << YAML::Value << c.BossDefeatFadeDuration;
            o << YAML::Key << "VictorySceneCommand" << YAML::Value << YAML::DoubleQuoted << c.VictorySceneCommand;
            o << YAML::Key << "DefeatSceneCommand" << YAML::Value << YAML::DoubleQuoted << c.DefeatSceneCommand;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeCombatLevelComponent& c) {
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.ArenaMin = n["ArenaMin"].as<glm::vec2>(c.ArenaMin);
            c.ArenaMax = n["ArenaMax"].as<glm::vec2>(c.ArenaMax);
            c.PlayerEntityName = n["PlayerEntityName"].as<std::string>(c.PlayerEntityName);
            c.BossEntityName = n["BossEntityName"].as<std::string>(c.BossEntityName);
            c.FadeEntityName = n["FadeEntityName"].as<std::string>(c.FadeEntityName);
            c.PausePanelEntityName = n["PausePanelEntityName"].as<std::string>(c.PausePanelEntityName);
            c.MessageTextEntityName = n["MessageTextEntityName"].as<std::string>(c.MessageTextEntityName);
            c.WeaponTextEntityName = n["WeaponTextEntityName"].as<std::string>(c.WeaponTextEntityName);
            c.PlayerHealthBarEntityName = n["PlayerHealthBarEntityName"].as<std::string>(c.PlayerHealthBarEntityName);
            c.PlayerHealthTextEntityName = n["PlayerHealthTextEntityName"].as<std::string>(c.PlayerHealthTextEntityName);
            c.BossHealthBarEntityName = n["BossHealthBarEntityName"].as<std::string>(c.BossHealthBarEntityName);
            c.BossHealthTextEntityName = n["BossHealthTextEntityName"].as<std::string>(c.BossHealthTextEntityName);
            c.StartFadeDuration = n["StartFadeDuration"].as<float>(c.StartFadeDuration);
            c.VictoryReturnDelay = n["VictoryReturnDelay"].as<float>(c.VictoryReturnDelay);
            c.DefeatReturnDelay = n["DefeatReturnDelay"].as<float>(c.DefeatReturnDelay);
            c.ResultSceneFadeDuration = n["ResultSceneFadeDuration"].as<float>(c.ResultSceneFadeDuration);
            c.BossDefeatFadeDuration = n["BossDefeatFadeDuration"].as<float>(c.BossDefeatFadeDuration);
            c.VictorySceneCommand = n["VictorySceneCommand"].as<std::string>(c.VictorySceneCommand);
            c.DefeatSceneCommand = n["DefeatSceneCommand"].as<std::string>(c.DefeatSceneCommand);
        }
    };

    template<> struct ComponentSerializer<ArcadeCombatantComponent> {
        static constexpr const char* Key = "ArcadeCombatantComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeCombatantComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "MaxHealth" << YAML::Value << c.MaxHealth;
            o << YAML::Key << "Health" << YAML::Value << c.Health;
            o << YAML::Key << "MoveSpeed" << YAML::Value << c.MoveSpeed;
            o << YAML::Key << "CollisionRadius" << YAML::Value << c.CollisionRadius;
            o << YAML::Key << "Invulnerable" << YAML::Value << c.Invulnerable;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeCombatantComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.MaxHealth = n["MaxHealth"].as<float>(c.MaxHealth);
            c.Health = n["Health"].as<float>(c.Health);
            c.MoveSpeed = n["MoveSpeed"].as<float>(c.MoveSpeed);
            c.CollisionRadius = n["CollisionRadius"].as<float>(c.CollisionRadius);
            c.Invulnerable = n["Invulnerable"].as<bool>(c.Invulnerable);
        }
    };

    template<> struct ComponentSerializer<ArcadePlayerControllerComponent> {
        static constexpr const char* Key = "ArcadePlayerControllerComponent";
        static void Serialize(YAML::Emitter& o, const ArcadePlayerControllerComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "CurrentWeapon" << YAML::Value << (int)c.CurrentWeapon;
            o << YAML::Key << "AutoAim" << YAML::Value << c.AutoAim;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadePlayerControllerComponent& c) {
            c.CurrentWeapon = (ArcadeWeaponType)n["CurrentWeapon"].as<int>((int)c.CurrentWeapon);
            c.AutoAim = n["AutoAim"].as<bool>(c.AutoAim);
        }
    };

    template<> struct ComponentSerializer<ArcadeBossComponent> {
        static constexpr const char* Key = "ArcadeBossComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeBossComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Active" << YAML::Value << c.Active;
            o << YAML::Key << "IntroStartPosition" << YAML::Value << c.IntroStartPosition;
            o << YAML::Key << "FightPosition" << YAML::Value << c.FightPosition;
            o << YAML::Key << "IntroDuration" << YAML::Value << c.IntroDuration;
            o << YAML::Key << "ShootInterval" << YAML::Value << c.ShootInterval;
            o << YAML::Key << "JumpInterval" << YAML::Value << c.JumpInterval;
            o << YAML::Key << "JumpDuration" << YAML::Value << c.JumpDuration;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeBossComponent& c) {
            c.Active = n["Active"].as<bool>(c.Active);
            c.IntroStartPosition = n["IntroStartPosition"].as<glm::vec3>(c.IntroStartPosition);
            c.FightPosition = n["FightPosition"].as<glm::vec3>(c.FightPosition);
            c.IntroDuration = n["IntroDuration"].as<float>(c.IntroDuration);
            c.ShootInterval = n["ShootInterval"].as<float>(c.ShootInterval);
            c.JumpInterval = n["JumpInterval"].as<float>(c.JumpInterval);
            c.JumpDuration = n["JumpDuration"].as<float>(c.JumpDuration);
        }
    };

    template<> struct ComponentSerializer<ArcadeProjectileComponent> {
        static constexpr const char* Key = "ArcadeProjectileComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeProjectileComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Velocity" << YAML::Value << c.Velocity;
            o << YAML::Key << "Damage" << YAML::Value << c.Damage;
            o << YAML::Key << "Lifetime" << YAML::Value << c.Lifetime;
            o << YAML::Key << "Radius" << YAML::Value << c.Radius;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "Heavy" << YAML::Value << c.Heavy;
            o << YAML::Key << "Melee" << YAML::Value << c.Melee;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeProjectileComponent& c) {
            c.Velocity = n["Velocity"].as<glm::vec2>(c.Velocity);
            c.Damage = n["Damage"].as<float>(c.Damage);
            c.Lifetime = n["Lifetime"].as<float>(c.Lifetime);
            c.Radius = n["Radius"].as<float>(c.Radius);
            c.Team = n["Team"].as<int>(c.Team);
            c.Heavy = n["Heavy"].as<bool>(c.Heavy);
            c.Melee = n["Melee"].as<bool>(c.Melee);
        }
    };

    template<> struct ComponentSerializer<ArcadeCoverComponent> {
        static constexpr const char* Key = "ArcadeCoverComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeCoverComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Radius" << YAML::Value << c.Radius;
            o << YAML::Key << "MaxHealth" << YAML::Value << c.MaxHealth;
            o << YAML::Key << "Health" << YAML::Value << c.Health;
            o << YAML::Key << "BlocksProjectiles" << YAML::Value << c.BlocksProjectiles;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeCoverComponent& c) {
            c.Radius = n["Radius"].as<float>(c.Radius);
            c.MaxHealth = n["MaxHealth"].as<float>(c.MaxHealth);
            c.Health = n["Health"].as<float>(c.Health);
            c.BlocksProjectiles = n["BlocksProjectiles"].as<bool>(c.BlocksProjectiles);
        }
    };

    template<> struct ComponentSerializer<ArcadeTriggerComponent> {
        static constexpr const char* Key = "ArcadeTriggerComponent";
        static void Serialize(YAML::Emitter& o, const ArcadeTriggerComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Type" << YAML::Value << (int)c.Type;
            o << YAML::Key << "TriggerName" << YAML::Value << c.TriggerName;
            o << YAML::Key << "Radius" << YAML::Value << c.Radius;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, ArcadeTriggerComponent& c) {
            c.Type = (ArcadeTriggerType)n["Type"].as<int>((int)c.Type);
            c.TriggerName = n["TriggerName"].as<std::string>(c.TriggerName);
            c.Radius = n["Radius"].as<float>(c.Radius);
        }
    };

    template<> struct ComponentSerializer<SideCombatLevelComponent> {
        static constexpr const char* Key = "SideCombatLevelComponent";
        static void Serialize(YAML::Emitter& o, const SideCombatLevelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "LevelId" << YAML::Value << c.LevelId;
            o << YAML::Key << "TuningPath" << YAML::Value << c.TuningPath;
            o << YAML::Key << "ArenaMin" << YAML::Value << c.ArenaMin;
            o << YAML::Key << "ArenaMax" << YAML::Value << c.ArenaMax;
            o << YAML::Key << "GroundY" << YAML::Value << c.GroundY;
            o << YAML::Key << "LaneMinY" << YAML::Value << c.LaneMinY;
            o << YAML::Key << "LaneMaxY" << YAML::Value << c.LaneMaxY;
            o << YAML::Key << "PlayerEntityName" << YAML::Value << c.PlayerEntityName;
            o << YAML::Key << "BossEntityName" << YAML::Value << c.BossEntityName;
            o << YAML::Key << "FadeEntityName" << YAML::Value << c.FadeEntityName;
            o << YAML::Key << "MessageTextEntityName" << YAML::Value << c.MessageTextEntityName;
            o << YAML::Key << "ComboTextEntityName" << YAML::Value << c.ComboTextEntityName;
            o << YAML::Key << "SkillTextEntityName" << YAML::Value << c.SkillTextEntityName;
            o << YAML::Key << "RewardTextEntityName" << YAML::Value << c.RewardTextEntityName;
            o << YAML::Key << "PlayerHealthBarEntityName" << YAML::Value << c.PlayerHealthBarEntityName;
            o << YAML::Key << "PlayerHealthTextEntityName" << YAML::Value << c.PlayerHealthTextEntityName;
            o << YAML::Key << "BossHealthBarEntityName" << YAML::Value << c.BossHealthBarEntityName;
            o << YAML::Key << "BossHealthTextEntityName" << YAML::Value << c.BossHealthTextEntityName;
            o << YAML::Key << "StartFadeDuration" << YAML::Value << c.StartFadeDuration;
            o << YAML::Key << "VictoryReturnDelay" << YAML::Value << c.VictoryReturnDelay;
            o << YAML::Key << "DefeatReturnDelay" << YAML::Value << c.DefeatReturnDelay;
            o << YAML::Key << "ResultSceneFadeDuration" << YAML::Value << c.ResultSceneFadeDuration;
            o << YAML::Key << "VictorySceneCommand" << YAML::Value << YAML::DoubleQuoted << c.VictorySceneCommand;
            o << YAML::Key << "DefeatSceneCommand" << YAML::Value << YAML::DoubleQuoted << c.DefeatSceneCommand;
            o << YAML::Key << "ComboDropDelay" << YAML::Value << c.ComboDropDelay;
            o << YAML::Key << "FirstClearRewardText" << YAML::Value << YAML::DoubleQuoted << c.FirstClearRewardText;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideCombatLevelComponent& c) {
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.LevelId = n["LevelId"].as<std::string>(c.LevelId);
            c.TuningPath = n["TuningPath"].as<std::string>(c.TuningPath);
            c.ArenaMin = n["ArenaMin"].as<glm::vec2>(c.ArenaMin);
            c.ArenaMax = n["ArenaMax"].as<glm::vec2>(c.ArenaMax);
            c.GroundY = n["GroundY"].as<float>(c.GroundY);
            c.LaneMinY = n["LaneMinY"].as<float>(c.LaneMinY);
            c.LaneMaxY = n["LaneMaxY"].as<float>(c.LaneMaxY);
            c.PlayerEntityName = n["PlayerEntityName"].as<std::string>(c.PlayerEntityName);
            c.BossEntityName = n["BossEntityName"].as<std::string>(c.BossEntityName);
            c.FadeEntityName = n["FadeEntityName"].as<std::string>(c.FadeEntityName);
            c.MessageTextEntityName = n["MessageTextEntityName"].as<std::string>(c.MessageTextEntityName);
            c.ComboTextEntityName = n["ComboTextEntityName"].as<std::string>(c.ComboTextEntityName);
            c.SkillTextEntityName = n["SkillTextEntityName"].as<std::string>(c.SkillTextEntityName);
            c.RewardTextEntityName = n["RewardTextEntityName"].as<std::string>(c.RewardTextEntityName);
            c.PlayerHealthBarEntityName = n["PlayerHealthBarEntityName"].as<std::string>(c.PlayerHealthBarEntityName);
            c.PlayerHealthTextEntityName = n["PlayerHealthTextEntityName"].as<std::string>(c.PlayerHealthTextEntityName);
            c.BossHealthBarEntityName = n["BossHealthBarEntityName"].as<std::string>(c.BossHealthBarEntityName);
            c.BossHealthTextEntityName = n["BossHealthTextEntityName"].as<std::string>(c.BossHealthTextEntityName);
            c.StartFadeDuration = n["StartFadeDuration"].as<float>(c.StartFadeDuration);
            c.VictoryReturnDelay = n["VictoryReturnDelay"].as<float>(c.VictoryReturnDelay);
            c.DefeatReturnDelay = n["DefeatReturnDelay"].as<float>(c.DefeatReturnDelay);
            c.ResultSceneFadeDuration = n["ResultSceneFadeDuration"].as<float>(c.ResultSceneFadeDuration);
            c.VictorySceneCommand = n["VictorySceneCommand"].as<std::string>(c.VictorySceneCommand);
            c.DefeatSceneCommand = n["DefeatSceneCommand"].as<std::string>(c.DefeatSceneCommand);
            c.ComboDropDelay = n["ComboDropDelay"].as<float>(c.ComboDropDelay);
            c.FirstClearRewardText = n["FirstClearRewardText"].as<std::string>(c.FirstClearRewardText);
        }
    };

    template<> struct ComponentSerializer<SideCombatantComponent> {
        static constexpr const char* Key = "SideCombatantComponent";
        static void Serialize(YAML::Emitter& o, const SideCombatantComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "MaxHealth" << YAML::Value << c.MaxHealth;
            o << YAML::Key << "Health" << YAML::Value << c.Health;
            o << YAML::Key << "Attack" << YAML::Value << c.Attack;
            o << YAML::Key << "Defense" << YAML::Value << c.Defense;
            o << YAML::Key << "MoveSpeed" << YAML::Value << c.MoveSpeed;
            o << YAML::Key << "CollisionSize" << YAML::Value << c.CollisionSize;
            o << YAML::Key << "CollisionHeight" << YAML::Value << c.CollisionHeight;
            o << YAML::Key << "GravityScale" << YAML::Value << c.GravityScale;
            o << YAML::Key << "KnockbackResistance" << YAML::Value << c.KnockbackResistance;
            o << YAML::Key << "Invulnerable" << YAML::Value << c.Invulnerable;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideCombatantComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.MaxHealth = n["MaxHealth"].as<float>(c.MaxHealth);
            c.Health = n["Health"].as<float>(c.Health);
            c.Attack = n["Attack"].as<float>(c.Attack);
            c.Defense = n["Defense"].as<float>(c.Defense);
            c.MoveSpeed = n["MoveSpeed"].as<float>(c.MoveSpeed);
            c.CollisionSize = n["CollisionSize"].as<glm::vec2>(c.CollisionSize);
            c.CollisionHeight = n["CollisionHeight"].as<float>(c.CollisionHeight);
            c.GravityScale = n["GravityScale"].as<float>(c.GravityScale);
            c.KnockbackResistance = n["KnockbackResistance"].as<float>(c.KnockbackResistance);
            c.Invulnerable = n["Invulnerable"].as<bool>(c.Invulnerable);
        }
    };

    template<> struct ComponentSerializer<SidePlayerControllerComponent> {
        static constexpr const char* Key = "SidePlayerControllerComponent";
        static void Serialize(YAML::Emitter& o, const SidePlayerControllerComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "MaxJumps" << YAML::Value << c.MaxJumps;
            o << YAML::Key << "JumpImpulse" << YAML::Value << c.JumpImpulse;
            o << YAML::Key << "Gravity" << YAML::Value << c.Gravity;
            o << YAML::Key << "AirControl" << YAML::Value << c.AirControl;
            o << YAML::Key << "LaneSpeedScale" << YAML::Value << c.LaneSpeedScale;
            o << YAML::Key << "LaneAcceleration" << YAML::Value << c.LaneAcceleration;
            o << YAML::Key << "GroundFriction" << YAML::Value << c.GroundFriction;
            o << YAML::Key << "BasicCooldown" << YAML::Value << c.BasicCooldown;
            o << YAML::Key << "LauncherCooldown" << YAML::Value << c.LauncherCooldown;
            o << YAML::Key << "MagicBoltCooldown" << YAML::Value << c.MagicBoltCooldown;
            o << YAML::Key << "AllySupportCooldown" << YAML::Value << c.AllySupportCooldown;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SidePlayerControllerComponent& c) {
            c.MaxJumps = n["MaxJumps"].as<int>(c.MaxJumps);
            c.JumpImpulse = n["JumpImpulse"].as<float>(c.JumpImpulse);
            c.Gravity = n["Gravity"].as<float>(c.Gravity);
            c.AirControl = n["AirControl"].as<float>(c.AirControl);
            c.LaneSpeedScale = n["LaneSpeedScale"].as<float>(c.LaneSpeedScale);
            c.LaneAcceleration = n["LaneAcceleration"].as<float>(c.LaneAcceleration);
            c.GroundFriction = n["GroundFriction"].as<float>(c.GroundFriction);
            c.BasicCooldown = n["BasicCooldown"].as<float>(c.BasicCooldown);
            c.LauncherCooldown = n["LauncherCooldown"].as<float>(c.LauncherCooldown);
            c.MagicBoltCooldown = n["MagicBoltCooldown"].as<float>(c.MagicBoltCooldown);
            c.AllySupportCooldown = n["AllySupportCooldown"].as<float>(c.AllySupportCooldown);
        }
    };

    template<> struct ComponentSerializer<SideEnemyAIComponent> {
        static constexpr const char* Key = "SideEnemyAIComponent";
        static void Serialize(YAML::Emitter& o, const SideEnemyAIComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Kind" << YAML::Value << (int)c.Kind;
            o << YAML::Key << "AggroRange" << YAML::Value << c.AggroRange;
            o << YAML::Key << "AttackRange" << YAML::Value << c.AttackRange;
            o << YAML::Key << "PreferredRange" << YAML::Value << c.PreferredRange;
            o << YAML::Key << "AttackInterval" << YAML::Value << c.AttackInterval;
            o << YAML::Key << "PatrolMinX" << YAML::Value << c.PatrolMinX;
            o << YAML::Key << "PatrolMaxX" << YAML::Value << c.PatrolMaxX;
            o << YAML::Key << "LaneTolerance" << YAML::Value << c.LaneTolerance;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideEnemyAIComponent& c) {
            c.Kind = (SideEnemyKind)n["Kind"].as<int>((int)c.Kind);
            c.AggroRange = n["AggroRange"].as<float>(c.AggroRange);
            c.AttackRange = n["AttackRange"].as<float>(c.AttackRange);
            c.PreferredRange = n["PreferredRange"].as<float>(c.PreferredRange);
            c.AttackInterval = n["AttackInterval"].as<float>(c.AttackInterval);
            c.PatrolMinX = n["PatrolMinX"].as<float>(c.PatrolMinX);
            c.PatrolMaxX = n["PatrolMaxX"].as<float>(c.PatrolMaxX);
            c.LaneTolerance = n["LaneTolerance"].as<float>(c.LaneTolerance);
        }
    };

    template<> struct ComponentSerializer<SideHitboxComponent> {
        static constexpr const char* Key = "SideHitboxComponent";
        static void Serialize(YAML::Emitter& o, const SideHitboxComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "AttackKind" << YAML::Value << (int)c.AttackKind;
            o << YAML::Key << "Size" << YAML::Value << c.Size;
            o << YAML::Key << "Velocity" << YAML::Value << c.Velocity;
            o << YAML::Key << "LaunchVelocity" << YAML::Value << c.LaunchVelocity;
            o << YAML::Key << "AirHeight" << YAML::Value << c.AirHeight;
            o << YAML::Key << "AirRange" << YAML::Value << c.AirRange;
            o << YAML::Key << "Damage" << YAML::Value << c.Damage;
            o << YAML::Key << "Lifetime" << YAML::Value << c.Lifetime;
            o << YAML::Key << "HitStun" << YAML::Value << c.HitStun;
            o << YAML::Key << "AttackerAirImpulse" << YAML::Value << c.AttackerAirImpulse;
            o << YAML::Key << "AttackerAirFallStep" << YAML::Value << c.AttackerAirFallStep;
            o << YAML::Key << "TargetAirFallStep" << YAML::Value << c.TargetAirFallStep;
            o << YAML::Key << "ProtectionGain" << YAML::Value << c.ProtectionGain;
            o << YAML::Key << "DestroyOnHit" << YAML::Value << c.DestroyOnHit;
            o << YAML::Key << "TextureFramePattern" << YAML::Value << c.TextureFramePattern;
            o << YAML::Key << "TextureFrameCount" << YAML::Value << c.TextureFrameCount;
            o << YAML::Key << "TextureFrameRate" << YAML::Value << c.TextureFrameRate;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideHitboxComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.AttackKind = (SideAttackKind)n["AttackKind"].as<int>((int)c.AttackKind);
            c.Size = n["Size"].as<glm::vec2>(c.Size);
            c.Velocity = n["Velocity"].as<glm::vec2>(c.Velocity);
            c.LaunchVelocity = n["LaunchVelocity"].as<glm::vec2>(c.LaunchVelocity);
            c.AirHeight = n["AirHeight"].as<float>(c.AirHeight);
            c.AirRange = n["AirRange"].as<float>(c.AirRange);
            c.Damage = n["Damage"].as<float>(c.Damage);
            c.Lifetime = n["Lifetime"].as<float>(c.Lifetime);
            c.HitStun = n["HitStun"].as<float>(c.HitStun);
            c.AttackerAirImpulse = n["AttackerAirImpulse"].as<float>(c.AttackerAirImpulse);
            c.AttackerAirFallStep = n["AttackerAirFallStep"].as<float>(c.AttackerAirFallStep);
            c.TargetAirFallStep = n["TargetAirFallStep"].as<float>(c.TargetAirFallStep);
            c.ProtectionGain = n["ProtectionGain"].as<float>(c.ProtectionGain);
            c.DestroyOnHit = n["DestroyOnHit"].as<bool>(c.DestroyOnHit);
            c.TextureFramePattern = n["TextureFramePattern"].as<std::string>(c.TextureFramePattern);
            c.TextureFrameCount = n["TextureFrameCount"].as<int>(c.TextureFrameCount);
            c.TextureFrameRate = n["TextureFrameRate"].as<float>(c.TextureFrameRate);
        }
    };

    template<> struct ComponentSerializer<SidePickupComponent> {
        static constexpr const char* Key = "SidePickupComponent";
        static void Serialize(YAML::Emitter& o, const SidePickupComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "ItemId" << YAML::Value << c.ItemId;
            o << YAML::Key << "DisplayName" << YAML::Value << c.DisplayName;
            o << YAML::Key << "Amount" << YAML::Value << c.Amount;
            o << YAML::Key << "PickupRadius" << YAML::Value << c.PickupRadius;
            o << YAML::Key << "AttractRadius" << YAML::Value << c.AttractRadius;
            o << YAML::Key << "AttractSpeed" << YAML::Value << c.AttractSpeed;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SidePickupComponent& c) {
            c.ItemId = n["ItemId"].as<std::string>(c.ItemId);
            c.DisplayName = n["DisplayName"].as<std::string>(c.DisplayName);
            c.Amount = n["Amount"].as<int>(c.Amount);
            c.PickupRadius = n["PickupRadius"].as<float>(c.PickupRadius);
            c.AttractRadius = n["AttractRadius"].as<float>(c.AttractRadius);
            c.AttractSpeed = n["AttractSpeed"].as<float>(c.AttractSpeed);
        }
    };
    template<> struct ComponentSerializer<UICanvasComponent> {
        static constexpr const char* Key = "UICanvasComponent";
        static void Serialize(YAML::Emitter& o, const UICanvasComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Visible" << YAML::Value << c.Visible;
            o << YAML::Key << "ReferenceWidth" << YAML::Value << c.ReferenceWidth;
            o << YAML::Key << "ReferenceHeight" << YAML::Value << c.ReferenceHeight;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UICanvasComponent& c) {
            c.Visible = n["Visible"].as<bool>();
            c.ReferenceWidth = n["ReferenceWidth"].as<float>();
            c.ReferenceHeight = n["ReferenceHeight"].as<float>();
        }
    };

    template<> struct ComponentSerializer<UIWidgetComponent> {
        static constexpr const char* Key = "UIWidgetComponent";
        static void Serialize(YAML::Emitter& o, const UIWidgetComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Visible" << YAML::Value << c.Visible;
            o << YAML::Key << "Position" << YAML::Value << c.Position;
            o << YAML::Key << "Size" << YAML::Value << c.Size;
            o << YAML::Key << "Rotation" << YAML::Value << c.Rotation;
            o << YAML::Key << "Anchor" << YAML::Value << (int)c.Anchor;
            o << YAML::Key << "SortOrder" << YAML::Value << c.SortOrder;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIWidgetComponent& c) {
            c.Visible = n["Visible"].as<bool>();
            c.Position = n["Position"].as<glm::vec2>();
            c.Size = n["Size"].as<glm::vec2>();
            c.Rotation = n["Rotation"].as<float>();
            c.Anchor = (UIAnchor)n["Anchor"].as<int>();
            c.SortOrder = n["SortOrder"].as<int>();
        }
    };

    template<> struct ComponentSerializer<UIImageComponent> {
        static constexpr const char* Key = "UIImageComponent";
        static void Serialize(YAML::Emitter& o, const UIImageComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "TexturePath" << YAML::Value << (c.Texture ? c.Texture->GetPath() : "");
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIImageComponent& c) {
            c.Color = n["Color"].as<glm::vec4>();
            if (auto p = n["TexturePath"].as<std::string>(""); !p.empty())
                c.Texture = Texture2D::Create(p);
        }
    };
    template<> struct ComponentSerializer<UIPanelComponent> {
        static constexpr const char* Key = "UIPanelComponent";
        static void Serialize(YAML::Emitter& o, const UIPanelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "BackgroundColor" << YAML::Value << c.BackgroundColor;
            o << YAML::Key << "BorderColor" << YAML::Value << c.BorderColor;
            o << YAML::Key << "BorderThickness" << YAML::Value << c.BorderThickness;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIPanelComponent& c) {
            c.BackgroundColor = n["BackgroundColor"].as<glm::vec4>(c.BackgroundColor);
            c.BorderColor = n["BorderColor"].as<glm::vec4>(c.BorderColor);
            c.BorderThickness = n["BorderThickness"].as<float>(c.BorderThickness);
        }
    };

    template<> struct ComponentSerializer<UITextComponent> {
        static constexpr const char* Key = "UITextComponent";
        static void Serialize(YAML::Emitter& o, const UITextComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Text" << YAML::Value << YAML::DoubleQuoted << c.Text;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "FontSize" << YAML::Value << c.FontSize;
            o << YAML::Key << "FontPath" << YAML::Value << c.FontPath;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UITextComponent& c) {
            c.Text = n["Text"].as<std::string>();
            c.Color = n["Color"].as<glm::vec4>();
            c.FontSize = n["FontSize"].as<float>();
            c.FontPath = n["FontPath"].as<std::string>(c.FontPath);
        }
    };

    template<> struct ComponentSerializer<UIButtonComponent> {
        static constexpr const char* Key = "UIButtonComponent";
        static void Serialize(YAML::Emitter& o, const UIButtonComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "NormalColor" << YAML::Value << c.NormalColor;
            o << YAML::Key << "HoverColor" << YAML::Value << c.HoverColor;
            o << YAML::Key << "PressedColor" << YAML::Value << c.PressedColor;
            o << YAML::Key << "OnClickFunction" << YAML::Value << YAML::DoubleQuoted << c.OnClickFunction;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIButtonComponent& c) {
            c.NormalColor = n["NormalColor"].as<glm::vec4>();
            c.HoverColor = n["HoverColor"].as<glm::vec4>();
            c.PressedColor = n["PressedColor"].as<glm::vec4>();
            c.OnClickFunction = n["OnClickFunction"].as<std::string>();
        }
    };

    template<> struct ComponentSerializer<UIProgressBarComponent> {
        static constexpr const char* Key = "UIProgressBarComponent";
        static void Serialize(YAML::Emitter& o, const UIProgressBarComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Value" << YAML::Value << c.Value;
            o << YAML::Key << "MaxValue" << YAML::Value << c.MaxValue;
            o << YAML::Key << "ForegroundColor" << YAML::Value << c.ForegroundColor;
            o << YAML::Key << "BackgroundColor" << YAML::Value << c.BackgroundColor;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIProgressBarComponent& c) {
            c.Value = n["Value"].as<float>();
            c.MaxValue = n["MaxValue"].as<float>();
            c.ForegroundColor = n["ForegroundColor"].as<glm::vec4>();
            c.BackgroundColor = n["BackgroundColor"].as<glm::vec4>();
        }
    };
    template<> struct ComponentSerializer<UISliderComponent> {
        static constexpr const char* Key = "UISliderComponent";
        static void Serialize(YAML::Emitter& o, const UISliderComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Value" << YAML::Value << c.Value;
            o << YAML::Key << "MinValue" << YAML::Value << c.MinValue;
            o << YAML::Key << "MaxValue" << YAML::Value << c.MaxValue;
            o << YAML::Key << "TrackColor" << YAML::Value << c.TrackColor;
            o << YAML::Key << "FillColor" << YAML::Value << c.FillColor;
            o << YAML::Key << "HandleColor" << YAML::Value << c.HandleColor;
            o << YAML::Key << "HoverColor" << YAML::Value << c.HoverColor;
            o << YAML::Key << "OnValueChangedFunction" << YAML::Value << YAML::DoubleQuoted << c.OnValueChangedFunction;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UISliderComponent& c) {
            c.Value = n["Value"].as<float>(c.Value);
            c.MinValue = n["MinValue"].as<float>(c.MinValue);
            c.MaxValue = n["MaxValue"].as<float>(c.MaxValue);
            c.TrackColor = n["TrackColor"].as<glm::vec4>(c.TrackColor);
            c.FillColor = n["FillColor"].as<glm::vec4>(c.FillColor);
            c.HandleColor = n["HandleColor"].as<glm::vec4>(c.HandleColor);
            c.HoverColor = n["HoverColor"].as<glm::vec4>(c.HoverColor);
            c.OnValueChangedFunction = n["OnValueChangedFunction"].as<std::string>(c.OnValueChangedFunction);
        }
    };

    template<> struct ComponentSerializer<UICheckboxComponent> {
        static constexpr const char* Key = "UICheckboxComponent";
        static void Serialize(YAML::Emitter& o, const UICheckboxComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Checked" << YAML::Value << c.Checked;
            o << YAML::Key << "BoxColor" << YAML::Value << c.BoxColor;
            o << YAML::Key << "CheckColor" << YAML::Value << c.CheckColor;
            o << YAML::Key << "HoverColor" << YAML::Value << c.HoverColor;
            o << YAML::Key << "PressedColor" << YAML::Value << c.PressedColor;
            o << YAML::Key << "OnValueChangedFunction" << YAML::Value << YAML::DoubleQuoted << c.OnValueChangedFunction;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UICheckboxComponent& c) {
            c.Checked = n["Checked"].as<bool>(c.Checked);
            c.BoxColor = n["BoxColor"].as<glm::vec4>(c.BoxColor);
            c.CheckColor = n["CheckColor"].as<glm::vec4>(c.CheckColor);
            c.HoverColor = n["HoverColor"].as<glm::vec4>(c.HoverColor);
            c.PressedColor = n["PressedColor"].as<glm::vec4>(c.PressedColor);
            c.OnValueChangedFunction = n["OnValueChangedFunction"].as<std::string>(c.OnValueChangedFunction);
        }
    };

    // SpriteAnimatorComponent 逻辑复杂，单独特化，但接口完全一致
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

    // ═══════════════════════════════════════════════════════════════════
    //  注册表：唯一需要维护的地方
    //  以后新增组件：1) 写特化  2) 在这里加一行类型
    // ═══════════════════════════════════════════════════════════════════

    // 注意：TagComponent 和 TransformComponent 是每个实体必有的，
    // 单独处理；其余组件走下面的通用循环。
    using SerializableComponents = ComponentGroup
    <
        CameraComponent,
        SpriteRendererComponent,
        SpriteAnimatorComponent,
        CircleRendererComponent,
        Rigidbody2DComponent,
        BoxCollider2DComponent,
        CircleCollider2DComponent,
        MeshRendererComponent,
        DirectionalLightComponent,
        PointLightComponent,
        AudioSourceComponent,
        VisualNovelComponent,
        ArcadeCombatLevelComponent,
        ArcadeCombatantComponent,
        ArcadePlayerControllerComponent,
        ArcadeBossComponent,
        ArcadeProjectileComponent,
        ArcadeCoverComponent,
        ArcadeTriggerComponent,
        SideCombatLevelComponent,
        SideCombatantComponent,
        SidePlayerControllerComponent,
        SideEnemyAIComponent,
        SideHitboxComponent,
        SidePickupComponent,
        UICanvasComponent,
        UIWidgetComponent,
        UIImageComponent,
        UIPanelComponent,
        UITextComponent,
        UIButtonComponent,
        UIProgressBarComponent,
        UISliderComponent,
        UICheckboxComponent
    > ;

    // TransformComponent 在 ComponentGroup 里走 GetComponent 分支（已存在）

// 其余走 AddComponent 分支

    using AllDeserializable = ComponentGroup
    <
        TransformComponent,
        CameraComponent,
        SpriteRendererComponent,
        SpriteAnimatorComponent,
        CircleRendererComponent,
        Rigidbody2DComponent,
        BoxCollider2DComponent,
        CircleCollider2DComponent,
        MeshRendererComponent,
        DirectionalLightComponent,
        PointLightComponent,
        AudioSourceComponent,
        VisualNovelComponent,
        ArcadeCombatLevelComponent,
        ArcadeCombatantComponent,
        ArcadePlayerControllerComponent,
        ArcadeBossComponent,
        ArcadeProjectileComponent,
        ArcadeCoverComponent,
        ArcadeTriggerComponent,
        SideCombatLevelComponent,
        SideCombatantComponent,
        SidePlayerControllerComponent,
        SideEnemyAIComponent,
        SideHitboxComponent,
        SidePickupComponent,
        UICanvasComponent,
        UIWidgetComponent,
        UIImageComponent,
        UIPanelComponent,
        UITextComponent,
        UIButtonComponent,
        UIProgressBarComponent,
        UISliderComponent,
        UICheckboxComponent
    > ;

    // ─── 通用序列化循环（fold expression 展开）──────────────────────────
    template<typename... Ts>
    static void SerializeComponents(ComponentGroup<Ts...>,
        YAML::Emitter& out, Entity entity)
    {
        ([&] {
            if (entity.HasComponent<Ts>()) {
                ComponentSerializer<Ts>::Serialize(out, entity.GetComponent<Ts>());
            }
            }(), ...);
    }

    // ─── 通用反序列化循环──────────────────────────────────────────────
    template<typename... Ts>
    static void DeserializeComponents(ComponentGroup<Ts...>,
        const YAML::Node& node, Entity entity)
    {
        ([&] {
            constexpr const char* key = ComponentSerializer<Ts>::Key;
            if (auto n = node[key]) {
                // TransformComponent 已存在（CreateEntity 时自动添加），特殊处理
                if constexpr (std::is_same_v<Ts, TransformComponent>) {
                    ComponentSerializer<Ts>::Deserialize(n, entity.GetComponent<Ts>());
                }
                else {
                    ComponentSerializer<Ts>::Deserialize(n, entity.AddComponent<Ts>());
                }
            }
            }(), ...);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  实体序列化 / 反序列化（主逻辑，极度精简）
    // ═══════════════════════════════════════════════════════════════════

    static void SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        WT_CORE_ASSERT(entity.HasComponent<IDComponent>(), "Entity missing IDComponent");

        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

        // Tag 和 Transform 是必有组件，直接序列化
        ComponentSerializer<TagComponent>::Serialize(out, entity.GetComponent<TagComponent>());
        ComponentSerializer<TransformComponent>::Serialize(out, entity.GetComponent<TransformComponent>());

        if (entity.HasComponent<ScriptComponent>())
            SerializeScriptComponent(out, entity);

        // 其余组件：有就序列化，没有跳过
        SerializeComponents(SerializableComponents{}, out, entity);

        out << YAML::EndMap;
    }

    static Entity DeserializeEntityFromNode(const YAML::Node& node,
        Scene* scene, bool newUUID)
    {
        uint64_t uuid = newUUID ? (uint64_t)UUID() : node["Entity"].as<uint64_t>();
        std::string name;
        if (auto n = node["TagComponent"]) name = n["Tag"].as<std::string>();

        Entity entity = scene->CreateEntityWithUUID(uuid, name);

        if (auto scriptNode = node["ScriptComponent"])
            DeserializeScriptComponent(scriptNode, entity);

        DeserializeComponents(AllDeserializable{}, node, entity);

        return entity;
    }

    // ═══════════════════════════════════════════════════════════════════
    //  SceneSerializer 公共接口（基本不用动）
    // ═══════════════════════════════════════════════════════════════════

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
        : m_Scene(scene) {
    }

    void SceneSerializer::SerializeYaml(const std::filesystem::path& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << "Untitled";
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
        m_Scene->m_Registry.each([&](auto id) {
            SerializeEntity(out, { id, m_Scene.get() });
            });
        out << YAML::EndSeq << YAML::EndMap;

        const std::filesystem::path resolvedPath = AssetPath::Resolve(filepath);
        const std::filesystem::path parentPath = resolvedPath.parent_path();
        if (!parentPath.empty())
            std::filesystem::create_directories(parentPath);
        std::ofstream f(resolvedPath);
        WT_CORE_ASSERT(f.is_open(), "Failed to open file for serialization");
        f << out.c_str();
    }

    bool SceneSerializer::DeserializeYaml(const std::filesystem::path& filepath)
    {
        const std::filesystem::path resolvedPath = AssetPath::Resolve(filepath);
        YAML::Node data;
        try { data = YAML::LoadFile(resolvedPath.string()); }
        catch (const YAML::Exception& e) {
            WT_CORE_ERROR("Failed to load '{}': {}", resolvedPath.string(), e.what());
            return false;
        }
        if (!data["Scene"]) return false;
        if (auto entities = data["Entities"])
            for (auto n : entities)
                DeserializeEntityFromNode(n, m_Scene.get(), false);
        return true;
    }

    bool SceneSerializer::SerializePrefab(Entity entity,
        const std::filesystem::path& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Prefab" << YAML::Value << entity.GetName();
        out << YAML::Key << "Entity";
        SerializeEntity(out, entity);
        out << YAML::EndMap;

        const std::filesystem::path resolvedPath = AssetPath::Resolve(filepath);
        const std::filesystem::path parentPath = resolvedPath.parent_path();
        if (!parentPath.empty())
            std::filesystem::create_directories(parentPath);
        std::ofstream f(resolvedPath);
        if (!f.is_open()) {
            WT_CORE_ERROR("PrefabSerializer: failed to open '{}'", resolvedPath.string());
            return false;
        }
        f << out.c_str();
        return true;
    }

    Entity SceneSerializer::DeserializePrefab(const std::filesystem::path& filepath,
        Scene* scene)
    {
        const std::filesystem::path resolvedPath = AssetPath::Resolve(filepath);
        YAML::Node data;
        try { data = YAML::LoadFile(resolvedPath.string()); }
        catch (const YAML::Exception& e) {
            WT_CORE_ERROR("PrefabSerializer: failed to load '{}': {}", resolvedPath.string(), e.what());
            return {};
        }
        if (!data["Prefab"] || !data["Entity"]) {
            WT_CORE_ERROR("PrefabSerializer: invalid file '{}'", resolvedPath.string());
            return {};
        }
        return DeserializeEntityFromNode(data["Entity"], scene, true);
    }

} // namespace Wheatear
