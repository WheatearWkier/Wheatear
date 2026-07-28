#pragma once

#include "Scene.h"
#include "ComponentGroup.h"
#include <filesystem>

namespace Wheatear {

    class SceneSerializer
    {
    public:
        explicit SceneSerializer(const Ref<Scene>& scene);

        // 序列化
        void SerializeYaml(const std::filesystem::path& filepath);

        // 反序列化，成功返回 true
        bool DeserializeYaml(const std::filesystem::path& filepath);

        // 供 PrefabSerializer 复用
        static bool SerializePrefab(Entity entity, const std::filesystem::path& filepath);
        static Entity DeserializePrefab(const std::filesystem::path& filepath, Scene* scene);

    private:
        Ref<Scene> m_Scene;
    };

    template<typename T>
    struct ComponentSerializer; // 只声明，不实现

} // namespace Wheatear