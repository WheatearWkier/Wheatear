#pragma once

#include "Scene.h"
#include "ComponentGroup.h"
#include <filesystem>

namespace Wheatear {

    class SceneSerializer
    {
    public:
        explicit SceneSerializer(const Ref<Scene>& scene);

        void SerializeYaml(const std::filesystem::path& filepath);

        bool DeserializeYaml(const std::filesystem::path& filepath);

        static bool SerializePrefab(Entity entity, const std::filesystem::path& filepath);
        static Entity DeserializePrefab(const std::filesystem::path& filepath, Scene* scene);

    private:
        Ref<Scene> m_Scene;
    };

    template<typename T>
    struct ComponentSerializer;

} // namespace Wheatear
