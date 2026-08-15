#pragma once

#include "Wheatear/Scene/SceneSerializer.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Animation/AnimationClip.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <string>
#include <vector>

namespace YAML {

    template<> struct convert<glm::vec2> {
        static Node encode(const glm::vec2& value) {
            Node node; node.push_back(value.x); node.push_back(value.y); return node;
        }
        static bool decode(const Node& node, glm::vec2& value) {
            if (!node.IsSequence() || node.size() != 2) return false;
            value = { node[0].as<float>(), node[1].as<float>() }; return true;
        }
    };

    template<> struct convert<glm::vec3> {
        static Node encode(const glm::vec3& value) {
            Node node; node.push_back(value.x); node.push_back(value.y); node.push_back(value.z); return node;
        }
        static bool decode(const Node& node, glm::vec3& value) {
            if (!node.IsSequence() || node.size() != 3) return false;
            value = { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() }; return true;
        }
    };

    template<> struct convert<glm::vec4> {
        static Node encode(const glm::vec4& value) {
            Node node; node.push_back(value.x); node.push_back(value.y); node.push_back(value.z); node.push_back(value.w); return node;
        }
        static bool decode(const Node& node, glm::vec4& value) {
            if (!node.IsSequence() || node.size() != 4) return false;
            value = { node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>() }; return true;
        }
    };

} // namespace YAML

namespace Wheatear {

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& value)
    {
        return out << YAML::Flow << YAML::BeginSeq << value.x << value.y << YAML::EndSeq;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& value)
    {
        return out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& value)
    {
        return out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << value.w << YAML::EndSeq;
    }

    inline const char* BodyTypeToString(Rigidbody2DComponent::BodyType type)
    {
        switch (type)
        {
        case Rigidbody2DComponent::BodyType::Static:    return "Static";
        case Rigidbody2DComponent::BodyType::Dynamic:   return "Dynamic";
        case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
        }
        return "Static";
    }

    inline Rigidbody2DComponent::BodyType BodyTypeFromString(const std::string& value)
    {
        if (value == "Dynamic")   return Rigidbody2DComponent::BodyType::Dynamic;
        if (value == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;
        return Rigidbody2DComponent::BodyType::Static;
    }

    template<typename... Ts>
    void SerializeComponents(ComponentGroup<Ts...>, YAML::Emitter& out, Entity entity)
    {
        ([&]
        {
            if (entity.HasComponent<Ts>())
                ComponentSerializer<Ts>::Serialize(out, entity.GetComponent<Ts>());
        }(), ...);
    }

    template<typename... Ts>
    void DeserializeComponents(ComponentGroup<Ts...>, const YAML::Node& node, Entity entity)
    {
        ([&]
        {
            constexpr const char* key = ComponentSerializer<Ts>::Key;
            if (auto componentNode = node[key])
            {
                if (entity.HasComponent<Ts>())
                    ComponentSerializer<Ts>::Deserialize(componentNode, entity.GetComponent<Ts>());
                else
                    ComponentSerializer<Ts>::Deserialize(componentNode, entity.AddComponent<Ts>());
            }
        }(), ...);
    }

} // namespace Wheatear
