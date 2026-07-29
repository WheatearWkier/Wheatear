#include "wtpch.h"
#include "SceneSerializer.h"
#include "Entity.h"
#include "Components.h"
#include "SceneSerializerComponentGroups.h"

#include "Wheatear/Core/AssetPath.h"

#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>

namespace Wheatear {

    static void SerializeEntity(YAML::Emitter& out, Entity entity)
    {
        WT_CORE_ASSERT(entity.HasComponent<IDComponent>(), "Entity missing IDComponent");

        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

        SerializeCoreSceneComponents(out, entity);
        SerializeAnimationSceneComponents(out, entity);
        SerializeScriptingSceneComponents(out, entity);
        SerializeModuleSceneComponents(out, entity);
        SerializeUISceneComponents(out, entity);

        out << YAML::EndMap;
    }

    static Entity DeserializeEntityFromNode(const YAML::Node& node, Scene* scene, bool newUUID)
    {
        const uint64_t uuid = newUUID ? static_cast<uint64_t>(UUID()) : node["Entity"].as<uint64_t>();
        std::string name;
        if (auto tagNode = node["TagComponent"])
            name = tagNode["Tag"].as<std::string>();

        Entity entity = scene->CreateEntityWithUUID(uuid, name);

        DeserializeCoreSceneComponents(node, entity);
        DeserializeAnimationSceneComponents(node, entity);
        DeserializeScriptingSceneComponents(node, entity);
        DeserializeModuleSceneComponents(node, entity);
        DeserializeUISceneComponents(node, entity);

        return entity;
    }

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
    }

    void SceneSerializer::SerializeYaml(const std::filesystem::path& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << "Untitled";
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
        for (auto id : m_Scene->m_Registry.view<IDComponent>())
            SerializeEntity(out, { id, m_Scene.get() });
        out << YAML::EndSeq << YAML::EndMap;

        const std::filesystem::path resolvedPath = AssetPath::Resolve(filepath);
        const std::filesystem::path parentPath = resolvedPath.parent_path();
        if (!parentPath.empty())
            std::filesystem::create_directories(parentPath);

        std::ofstream file(resolvedPath);
        WT_CORE_ASSERT(file.is_open(), "Failed to open file for serialization");
        file << out.c_str();
    }

    bool SceneSerializer::DeserializeYaml(const std::filesystem::path& filepath)
    {
        const std::filesystem::path resolvedPath = AssetPath::Resolve(filepath);
        YAML::Node data;
        try
        {
            data = YAML::LoadFile(resolvedPath.string());
        }
        catch (const YAML::Exception& e)
        {
            WT_CORE_ERROR("Failed to load '{}': {}", resolvedPath.string(), e.what());
            return false;
        }

        if (!data["Scene"])
            return false;

        if (auto entities = data["Entities"])
        {
            for (auto entityNode : entities)
                DeserializeEntityFromNode(entityNode, m_Scene.get(), false);
        }
        return true;
    }

    bool SceneSerializer::SerializePrefab(Entity entity, const std::filesystem::path& filepath)
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

        std::ofstream file(resolvedPath);
        if (!file.is_open())
        {
            WT_CORE_ERROR("PrefabSerializer: failed to open '{}'", resolvedPath.string());
            return false;
        }

        file << out.c_str();
        return true;
    }

    Entity SceneSerializer::DeserializePrefab(const std::filesystem::path& filepath, Scene* scene)
    {
        const std::filesystem::path resolvedPath = AssetPath::Resolve(filepath);
        YAML::Node data;
        try
        {
            data = YAML::LoadFile(resolvedPath.string());
        }
        catch (const YAML::Exception& e)
        {
            WT_CORE_ERROR("PrefabSerializer: failed to load '{}': {}", resolvedPath.string(), e.what());
            return {};
        }

        if (!data["Prefab"] || !data["Entity"])
        {
            WT_CORE_ERROR("PrefabSerializer: invalid file '{}'", resolvedPath.string());
            return {};
        }

        return DeserializeEntityFromNode(data["Entity"], scene, true);
    }

} // namespace Wheatear
