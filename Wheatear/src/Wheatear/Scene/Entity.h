#pragma once

#include "entt.hpp"

#include "Wheatear/Core/UUID.h"

#include <cstdint>
#include <string>

namespace Wheatear {

    class Scene;

    class Entity
    {
    public:
        Entity() = default;
        Entity(entt::entity handle, Scene* scene);
        Entity(const Entity&) = default;


        template<typename T, typename... Args>
        T& AddComponent(Args&&... args);

        template<typename T, typename... Args>
        T& AddOrReplaceComponent(Args&&... args);

        template<typename T>
        T& GetComponent();

        template<typename T>
        const T& GetComponent() const;

        template<typename T>
        bool HasComponent() const;

        template<typename T>
        void RemoveComponent();


        UUID               GetUUID() const;
        const std::string& GetName() const;
        Scene*             GetScene() const { return m_Scene; }


        operator bool()          const { return m_EntityHandle != entt::null; }
        operator entt::entity()  const { return m_EntityHandle; }

        explicit operator uint32_t() const
        {
            return static_cast<uint32_t>(m_EntityHandle);
        }


        bool operator==(const Entity& other) const
        {
            return m_EntityHandle == other.m_EntityHandle
                && m_Scene == other.m_Scene;
        }

        bool operator!=(const Entity& other) const
        {
            return !(*this == other);
        }

    private:
        entt::entity m_EntityHandle = entt::null;
        Scene* m_Scene = nullptr;
    };

} // namespace Wheatear
