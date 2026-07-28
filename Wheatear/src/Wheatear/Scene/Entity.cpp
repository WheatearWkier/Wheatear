#include "wtpch.h"
#include "Entity.h"

namespace Wheatear {

    Entity::Entity(entt::entity handle, Scene* scene)
        : m_EntityHandle(handle)
        , m_Scene(scene)
    {
        
    }

} // namespace Wheatear