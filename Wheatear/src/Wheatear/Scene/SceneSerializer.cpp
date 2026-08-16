#include "wtpch.h"
#include "SceneSerializer.h"
#include "Entity.h"
#include "Components.h"
#include "Wheatear/Scene/Serialization/SceneSerializerComponentGroups.h"

#include "Wheatear/Assets/AssetPath.h"

#include <yaml-cpp/yaml.h>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    namespace {

        static void SerializeSavePolicy(YAML::Emitter& out, const SavePolicy& policy)
        {
            out << YAML::Key << "SavePolicy" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "CanSave" << YAML::Value << policy.CanSave;
            out << YAML::Key << "CanLoad" << YAML::Value << policy.CanLoad;
            out << YAML::Key << "SaveDirectory" << YAML::Value << policy.SaveDirectory;
            out << YAML::Key << "AutoLoadSlot" << YAML::Value << policy.AutoLoadSlot;
            out << YAML::EndMap;
        }

        static void DeserializeSavePolicy(const YAML::Node& node, SavePolicy& policy)
        {
            if (!node)
                return;

            policy.CanSave = node["CanSave"].as<bool>(policy.CanSave);
            policy.CanLoad = node["CanLoad"].as<bool>(policy.CanLoad);
            policy.SaveDirectory = node["SaveDirectory"].as<std::string>(policy.SaveDirectory);
            policy.AutoLoadSlot = node["AutoLoadSlot"].as<int>(policy.AutoLoadSlot);
        }

    } // namespace

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
        Entity entity;
        try
        {
            // Hand-edited or older files may omit fields; every as<T>() in the
            // component deserializers carries a default, and this catch drops
            // any entity that still fails to parse instead of terminating the
            // whole process.
            const uint64_t sourceUUID = node["Entity"].as<uint64_t>(0);
            const uint64_t uuid = (newUUID || sourceUUID == 0)
                ? static_cast<uint64_t>(UUID())
                : sourceUUID;
            if (idRemap && sourceUUID != 0)
                (*idRemap)[sourceUUID] = UUID(uuid);

            std::string name;
            if (auto tagNode = node["TagComponent"])
                name = tagNode["Tag"].as<std::string>("");

            entity = scene->CreateEntityWithUUID(uuid, name);

            DeserializeCoreSceneComponents(node, entity);
            DeserializeAnimationSceneComponents(node, entity);
            DeserializeScriptingSceneComponents(node, entity);
            DeserializeModuleSceneComponents(node, entity);
            DeserializeUISceneComponents(node, entity);

            return entity;
        }
        catch (const YAML::Exception& e)
        {
            WT_CORE_ERROR("SceneSerializer: failed to deserialize entity: {}", e.what());
            if (entity)
            {
                scene->GetRegistry().destroy(static_cast<entt::entity>(entity));
                scene->InvalidateEntityLookupCache();
            }
            return {};
        }
    }

    static uint32_t EntityKey(Entity entity)
    {
        return static_cast<uint32_t>(static_cast<entt::entity>(entity));
    }

    using UIChildrenByParent = std::unordered_map<uint64_t, std::vector<Entity>>;

    static UIChildrenByParent BuildUIChildrenIndex(Scene* scene)
    {
        UIChildrenByParent childrenByParent;
        if (!scene)
            return childrenByParent;

        auto& registry = scene->GetRegistry();
        for (auto entityID : registry.view<IDComponent, UIWidgetComponent>())
        {
            Entity child{ entityID, scene };
            const auto& widget = child.GetComponent<UIWidgetComponent>();
            const uint64_t parentID = static_cast<uint64_t>(widget.ParentEntity);
            if (parentID != 0)
                childrenByParent[parentID].push_back(child);
        }

        for (auto& [parentID, children] : childrenByParent)
        {
            std::sort(children.begin(), children.end(), [](Entity a, Entity b)
            {
                const auto& aw = a.GetComponent<UIWidgetComponent>();
                const auto& bw = b.GetComponent<UIWidgetComponent>();
                if (aw.SortOrder != bw.SortOrder)
                    return aw.SortOrder < bw.SortOrder;
                return a.GetName() < b.GetName();
            });
        }

        return childrenByParent;
    }

    static void CollectUIPrefabEntitiesRecursive(Entity root,
        const UIChildrenByParent& childrenByParent,
        std::vector<Entity>& entities,
        std::unordered_set<uint32_t>& visited,
        int depth = 0)
    {
        if (!root || !root.HasComponent<IDComponent>())
            return;

        // Guard against hand-authored arbitrarily deep parent chains: the
        // recursion must not blow the stack.
        if (depth > 512)
        {
            WT_CORE_WARN("SceneSerializer: UI hierarchy deeper than 512 levels; stopping prefab collection at '{}'",
                root.GetName());
            return;
        }

        const uint32_t rootKey = EntityKey(root);
        if (!visited.insert(rootKey).second)
            return;

        entities.push_back(root);

        if (!root.HasComponent<UIWidgetComponent>())
            return;

        const UUID rootID = root.GetUUID();
        auto childrenIt = childrenByParent.find(static_cast<uint64_t>(rootID));
        if (childrenIt == childrenByParent.end())
            return;

        for (Entity child : childrenIt->second)
            CollectUIPrefabEntitiesRecursive(child, childrenByParent, entities, visited, depth + 1);
    }

    static std::vector<Entity> CollectPrefabEntities(Entity root)
    {
        std::vector<Entity> entities;
        std::unordered_set<uint32_t> visited;
        const UIChildrenByParent childrenByParent = BuildUIChildrenIndex(root ? root.GetScene() : nullptr);
        CollectUIPrefabEntitiesRecursive(root, childrenByParent, entities, visited);

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

    static bool IsSelectorPrefixBoundary(const std::string& text, size_t position)
    {
        if (position == 0)
            return true;

        const unsigned char previous = static_cast<unsigned char>(text[position - 1]);
        return !std::isalnum(previous) && previous != '_';
    }

    static bool TryReadU64(const std::string& text, size_t begin, size_t end, uint64_t& value)
    {
        if (begin >= end)
            return false;

        const char* first = text.data() + begin;
        const char* last = text.data() + end;
        const auto result = std::from_chars(first, last, value);
        return result.ec == std::errc{} && result.ptr == last;
    }

    static void RemapUUIDSelectorsInText(std::string& text,
        const std::unordered_map<uint64_t, UUID>& idRemap)
    {
        if (text.empty() || idRemap.empty())
            return;

        std::string remapped;
        remapped.reserve(text.size());

        size_t cursor = 0;
        while (cursor < text.size())
        {
            size_t prefixLength = 0;
            if (text[cursor] == '@')
            {
                prefixLength = 1;
            }
            else if (IsSelectorPrefixBoundary(text, cursor)
                && text.compare(cursor, 5, "uuid:") == 0)
            {
                prefixLength = 5;
            }
            else if (IsSelectorPrefixBoundary(text, cursor)
                && text.compare(cursor, 3, "id:") == 0)
            {
                prefixLength = 3;
            }

            if (prefixLength == 0 ||
                cursor + prefixLength >= text.size() ||
                !std::isdigit(static_cast<unsigned char>(text[cursor + prefixLength])))
            {
                remapped.push_back(text[cursor++]);
                continue;
            }

            const size_t digitsBegin = cursor + prefixLength;
            size_t digitsEnd = digitsBegin;
            while (digitsEnd < text.size() &&
                std::isdigit(static_cast<unsigned char>(text[digitsEnd])))
            {
                ++digitsEnd;
            }

            uint64_t oldID = 0;
            if (TryReadU64(text, digitsBegin, digitsEnd, oldID))
            {
                auto it = idRemap.find(oldID);
                if (it != idRemap.end())
                {
                    remapped.append(text, cursor, prefixLength);
                    remapped += std::to_string(static_cast<uint64_t>(it->second));
                    cursor = digitsEnd;
                    continue;
                }
            }

            remapped.append(text, cursor, digitsEnd - cursor);
            cursor = digitsEnd;
        }

        text = std::move(remapped);
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
                RemapUUIDSelectorsInText(button.OnClickFunction, idRemap);
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
        SerializeSavePolicy(out, m_Scene->GetSavePolicy());
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

        try
        {
            if (!data["Scene"])
                return false;

            if (auto savePolicy = data["SavePolicy"])
            {
                SavePolicy policy = m_Scene->GetSavePolicy();
                DeserializeSavePolicy(savePolicy, policy);
                m_Scene->SetSavePolicy(policy);
            }

            if (auto entities = data["Entities"])
            {
                // Malformed entities are reported (and rolled back) inside
                // DeserializeEntityFromNode; the rest of the scene still loads.
                for (auto entityNode : entities)
                    DeserializeEntityFromNode(entityNode, m_Scene.get(), false);
            }
            return true;
        }
        catch (const YAML::Exception& e)
        {
            WT_CORE_ERROR("Failed to deserialize '{}': {}", resolvedPath.string(), e.what());
            return false;
        }
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

        try
        {
            for (auto entityNode : data["Entities"])
            {
                Entity entity = DeserializeEntityFromNode(entityNode, scene, true, &idRemap);
                if (entity)
                    entities.push_back(entity);
            }
        }
        catch (const YAML::Exception& e)
        {
            WT_CORE_ERROR("PrefabSerializer: failed to deserialize '{}': {}", resolvedPath.string(), e.what());
            for (Entity entity : entities)
                scene->GetRegistry().destroy(static_cast<entt::entity>(entity));
            scene->InvalidateEntityLookupCache();
            return {};
        }

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
