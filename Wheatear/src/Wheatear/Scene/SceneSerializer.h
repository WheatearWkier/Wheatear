#pragma once

#include "Scene.h"
#include "ComponentGroup.h"
#include <filesystem>
#include <vector>

namespace Wheatear {

    class SceneSerializer
    {
    public:
        explicit SceneSerializer(const Ref<Scene>& scene);

        void SerializeYaml(const std::filesystem::path& filepath);

        bool DeserializeYaml(const std::filesystem::path& filepath);

        static bool SerializePrefab(Entity entity, const std::filesystem::path& filepath);
        static std::vector<Entity> DeserializePrefabEntities(const std::filesystem::path& filepath, Scene* scene);
        static Entity DeserializePrefab(const std::filesystem::path& filepath, Scene* scene);

        // Author a designer-composable UI template (.wtuit) by serializing the
        // selected widget subtree as a Prefab Version 2 block alongside a
        // UITemplate metadata block. CreateFromAsset reuses DeserializePrefabEntities
        // on the embedded Prefab body, so the format stays a single round-trippable file.
        static bool SerializeUITemplate(Entity entity,
            const std::filesystem::path& filepath,
            const std::string& displayName,
            const std::string& category,
            const std::string& description);

    private:
        Ref<Scene> m_Scene;
    };

    template<typename T>
    struct ComponentSerializer;

} // namespace Wheatear
