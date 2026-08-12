#include "wtpch.h"
#include "SceneSerializer.h"
#include "Entity.h"
#include "Components.h"
#include "SceneSerializerComponentGroups.h"

#include "Wheatear/Core/AssetPath.h"

#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

    static Entity DeserializeEntityFromNode(const YAML::Node& node,
        Scene* scene,
        bool newUUID,
        std::unordered_map<uint64_t, UUID>* idRemap = nullptr)
    {
        const uint64_t sourceUUID = node["Entity"].as<uint64_t>();
        const uint64_t uuid = newUUID ? static_cast<uint64_t>(UUID()) : sourceUUID;
        if (idRemap)
            (*idRemap)[sourceUUID] = UUID(uuid);

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

    static uint32_t EntityKey(Entity entity)
    {
        return static_cast<uint32_t>(static_cast<entt::entity>(entity));
    }

    static void CollectUIPrefabEntitiesRecursive(Entity root,
        std::vector<Entity>& entities,
        std::unordered_set<uint32_t>& visited)
    {
        if (!root || !root.HasComponent<IDComponent>())
            return;

        const uint32_t rootKey = EntityKey(root);
        if (!visited.insert(rootKey).second)
            return;

        entities.push_back(root);

        if (!root.HasComponent<UIWidgetComponent>())
            return;

        const UUID rootID = root.GetUUID();
        Scene* scene = root.GetScene();
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        std::vector<Entity> children;
        for (auto entityID : registry.view<IDComponent, UIWidgetComponent>())
        {
            Entity candidate{ entityID, scene };
            if (candidate == root)
                continue;

            const auto& widget = candidate.GetComponent<UIWidgetComponent>();
            if (widget.ParentEntity == rootID)
                children.push_back(candidate);
        }

        std::sort(children.begin(), children.end(), [](Entity a, Entity b)
        {
            const auto& aw = a.GetComponent<UIWidgetComponent>();
            const auto& bw = b.GetComponent<UIWidgetComponent>();
            if (aw.SortOrder != bw.SortOrder)
                return aw.SortOrder < bw.SortOrder;
            return a.GetName() < b.GetName();
        });

        for (Entity child : children)
            CollectUIPrefabEntitiesRecursive(child, entities, visited);
    }

    static std::vector<Entity> CollectPrefabEntities(Entity root)
    {
        std::vector<Entity> entities;
        std::unordered_set<uint32_t> visited;
        CollectUIPrefabEntitiesRecursive(root, entities, visited);

        if (entities.empty() && root)
            entities.push_back(root);

        return entities;
    }

    static UUID RemapUUID(UUID value, const std::unordered_map<uint64_t, UUID>& idRemap)
    {
        const uint64_t oldID = static_cast<uint64_t>(value);
        if (oldID == 0)
            return UUID(0);

        auto it = idRemap.find(oldID);
        return it == idRemap.end() ? UUID(0) : it->second;
    }

    static void ReplaceAll(std::string& text, const std::string& from, const std::string& to)
    {
        if (from.empty())
            return;

        size_t cursor = 0;
        while ((cursor = text.find(from, cursor)) != std::string::npos)
        {
            text.replace(cursor, from.size(), to);
            cursor += to.size();
        }
    }

    static void RemapPrefabEntityReferences(const std::vector<Entity>& entities,
        const std::unordered_map<uint64_t, UUID>& idRemap)
    {
        for (Entity entity : entities)
        {
            if (!entity)
                continue;

            if (entity.HasComponent<UIWidgetComponent>())
            {
                auto& widget = entity.GetComponent<UIWidgetComponent>();
                widget.ParentEntity = RemapUUID(widget.ParentEntity, idRemap);
            }

            if (entity.HasComponent<UIPageItemComponent>())
            {
                auto& pageItem = entity.GetComponent<UIPageItemComponent>();
                pageItem.PagerEntity = RemapUUID(pageItem.PagerEntity, idRemap);
            }

            if (entity.HasComponent<UIButtonComponent>())
            {
                auto& button = entity.GetComponent<UIButtonComponent>();
                for (const auto& [oldID, newID] : idRemap)
                {
                    const std::string oldSelector = "@" + std::to_string(oldID);
                    const std::string newSelector = "@" + std::to_string(static_cast<uint64_t>(newID));
                    ReplaceAll(button.OnClickFunction, oldSelector, newSelector);
                }
            }
        }
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
        const std::vector<Entity> prefabEntities = CollectPrefabEntities(entity);

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Prefab" << YAML::Value << entity.GetName();
        out << YAML::Key << "Version" << YAML::Value << 2;
        out << YAML::Key << "RootEntity" << YAML::Value << entity.GetUUID();
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
        for (Entity prefabEntity : prefabEntities)
            SerializeEntity(out, prefabEntity);
        out << YAML::EndSeq;
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

    std::vector<Entity> SceneSerializer::DeserializePrefabEntities(const std::filesystem::path& filepath, Scene* scene)
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

        if (!data["Prefab"] || !data["Version"] || data["Version"].as<int>(0) != 2 || !data["Entities"])
        {
            WT_CORE_ERROR("PrefabSerializer: '{}' must use Prefab Version 2 with an Entities array.", resolvedPath.string());
            return {};
        }

        std::vector<Entity> entities;
        std::unordered_map<uint64_t, UUID> idRemap;

        for (auto entityNode : data["Entities"])
            entities.push_back(DeserializeEntityFromNode(entityNode, scene, true, &idRemap));

        RemapPrefabEntityReferences(entities, idRemap);
        return entities;
    }

    Entity SceneSerializer::DeserializePrefab(const std::filesystem::path& filepath, Scene* scene)
    {
        const std::vector<Entity> entities = DeserializePrefabEntities(filepath, scene);
        return entities.empty() ? Entity{} : entities.front();
    }

    bool SceneSerializer::SerializeUITemplate(Entity entity,
        const std::filesystem::path& filepath,
        const std::string& displayName,
        const std::string& category,
        const std::string& description)
    {
        const std::vector<Entity> prefabEntities = CollectPrefabEntities(entity);

        YAML::Emitter out;
        out << YAML::BeginMap;

        // Metadata block - matches what UITemplateFactory::CreateFromAsset reads.
        out << YAML::Key << "UITemplate" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Version" << YAML::Value << 1;
        out << YAML::Key << "Kind" << YAML::Value << "Composite";
        out << YAML::Key << "DisplayName" << YAML::Value << displayName;
        out << YAML::Key << "Category" << YAML::Value << category;
        out << YAML::Key << "Description" << YAML::Value << description;
        out << YAML::EndMap;

        // Embedded prefab body - DeserializePrefabEntities reuses this verbatim,
        // so composites round-trip through the same V2 path as .wtprefab files.
        out << YAML::Key << "Prefab" << YAML::Value << entity.GetName();
        out << YAML::Key << "Version" << YAML::Value << 2;
        out << YAML::Key << "RootEntity" << YAML::Value << entity.GetUUID();
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
        for (Entity prefabEntity : prefabEntities)
            SerializeEntity(out, prefabEntity);
        out << YAML::EndSeq;

        out << YAML::EndMap;

        const std::filesystem::path resolvedPath = AssetPath::Resolve(filepath);
        const std::filesystem::path parentPath = resolvedPath.parent_path();
        if (!parentPath.empty())
            std::filesystem::create_directories(parentPath);

        std::ofstream file(resolvedPath);
        if (!file.is_open())
        {
            WT_CORE_ERROR("UITemplateSerializer: failed to open '{}'", resolvedPath.string());
            return false;
        }

        file << out.c_str();
        return true;
    }

} // namespace Wheatear
