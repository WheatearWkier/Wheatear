#pragma once

#include "entt.hpp"

#include "Wheatear/Core/Log.h"
#include "Scene.h"
#include "Components.h"
#include "Wheatear/Core/UUID.h"

namespace Wheatear {

    class Entity
    {
    public:
        Entity() = default;
        Entity(entt::entity handle, Scene* scene);
        Entity(const Entity&) = default;

        // ── 组件操作 ──────────────────────────────────────

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            WT_CORE_ASSERT(!HasComponent<T>(), "Entity already has component");
            T& component = m_Scene->m_Registry.emplace<T>(
                m_EntityHandle, std::forward<Args>(args)...);
            m_Scene->OnComponentAdded<T>(*this, component);
            return component;
        }

        template<typename T, typename... Args>
        T& AddOrReplaceComponent(Args&&... args)
        {
            T& component = m_Scene->m_Registry.emplace_or_replace<T>(
                m_EntityHandle, std::forward<Args>(args)...);
            m_Scene->OnComponentAdded<T>(*this, component);
            return component;
        }

        template<typename T>
        T& GetComponent()
        {
            WT_CORE_ASSERT(HasComponent<T>(), "Entity does not have component");
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }

        template<typename T>
        const T& GetComponent() const
        {
            WT_CORE_ASSERT(HasComponent<T>(), "Entity does not have component");
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }

        template<typename T>
        bool HasComponent() const
        {
            return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
        }

        template<typename T>
        void RemoveComponent()
        {
            WT_CORE_ASSERT(HasComponent<T>(), "Entity does not have component");
            m_Scene->m_Registry.remove<T>(m_EntityHandle);
        }

        // ── 快捷访问 ──────────────────────────────────────

        UUID               GetUUID() const { return GetComponent<IDComponent>().ID; }
        const std::string& GetName() const { return GetComponent<TagComponent>().Tag; }
        Scene*             GetScene() const { return m_Scene; }

        // ── 类型转换 ──────────────────────────────────────

        operator bool()          const { return m_EntityHandle != entt::null; }
        operator entt::entity()  const { return m_EntityHandle; }

        // explicit 防止 Entity 被意外当成整数使用
        explicit operator uint32_t() const
        {
            return static_cast<uint32_t>(m_EntityHandle);
        }

        // ── 比较 ──────────────────────────────────────────

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
