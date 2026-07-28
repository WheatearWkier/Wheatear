#include "wtpch.h"
#include "SceneSystemRegistry.h"

#include "Wheatear/Core/Log.h"

#include <vector>

namespace Wheatear {

    namespace {

        struct RegisteredSceneSystem
        {
            std::string Name;
            SceneSystemRegistry::SystemFactory Factory;
        };

        static std::vector<RegisteredSceneSystem>& RuntimeSystems()
        {
            static std::vector<RegisteredSceneSystem> systems;
            return systems;
        }

    } // namespace

    void SceneSystemRegistry::RegisterRuntimeSystem(const std::string& name, SystemFactory factory)
    {
        WT_CORE_ASSERT(!name.empty(), "Scene runtime system name cannot be empty.");
        WT_CORE_ASSERT(factory, "Scene runtime system '{}' has an empty factory.", name);

        auto& systems = RuntimeSystems();
        for (const auto& system : systems)
        {
            if (system.Name == name)
                return;
        }

        systems.push_back({ name, std::move(factory) });
    }

    void SceneSystemRegistry::ClearRuntimeSystems()
    {
        RuntimeSystems().clear();
    }

    void SceneSystemRegistry::ForEachRuntimeSystem(const SystemVisitor& visitor)
    {
        if (!visitor)
            return;

        for (const auto& system : RuntimeSystems())
            visitor(system.Name, system.Factory);
    }

} // namespace Wheatear
