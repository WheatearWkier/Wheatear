#pragma once

#include "Entity.h"

#include "Components.h"
#include "Scene.h"
#include "Wheatear/Core/Log.h"

#include <utility>

namespace Wheatear {

    template<typename T, typename... Args>
    T& Entity::AddComponent(Args&&... args)
    {
        WT_CORE_ASSERT(!HasComponent<T>(), "Entity already has component");
        T& component = m_Scene->m_Registry.emplace<T>(
            m_EntityHandle, std::forward<Args>(args)...);
        return component;
    }

    template<typename T, typename... Args>
    T& Entity::AddOrReplaceComponent(Args&&... args)
    {
        T& component = m_Scene->m_Registry.emplace_or_replace<T>(
            m_EntityHandle, std::forward<Args>(args)...);
        return component;
    }

    template<typename T>
    T& Entity::GetComponent()
    {
        WT_CORE_ASSERT(HasComponent<T>(), "Entity does not have component");
        return m_Scene->m_Registry.get<T>(m_EntityHandle);
    }

    template<typename T>
    const T& Entity::GetComponent() const
    {
        WT_CORE_ASSERT(HasComponent<T>(), "Entity does not have component");
        return m_Scene->m_Registry.get<T>(m_EntityHandle);
    }

    template<typename T>
    bool Entity::HasComponent() const
    {
        return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
    }

    template<typename T>
    void Entity::RemoveComponent()
    {
        WT_CORE_ASSERT(HasComponent<T>(), "Entity does not have component");
        m_Scene->m_Registry.remove<T>(m_EntityHandle);
    }

} // namespace Wheatear
