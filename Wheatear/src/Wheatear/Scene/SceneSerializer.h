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

    private:
        Ref<Scene> m_Scene;
    };

    template<typename T>
    struct ComponentSerializer;

} // namespace Wheatear
