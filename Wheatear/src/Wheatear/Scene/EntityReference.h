#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/UUID.h"

#include <string>

namespace Wheatear {

    class Entity;
    class Scene;

    struct EntityReference
    {
        UUID EntityID = 0;

        EntityReference() = default;
        explicit EntityReference(UUID id)
            : EntityID(id) {}

        bool IsEmpty() const
        {
            return static_cast<uint64_t>(EntityID) == 0;
        }
    };

    namespace EntityReferences {

        WHEATEAR_API bool IsUUIDSelector(const std::string& selector);
        WHEATEAR_API UUID ParseUUIDSelector(const std::string& selector);
        WHEATEAR_API std::string MakeUUIDSelector(UUID uuid);
        WHEATEAR_API Entity Resolve(Scene* scene, const EntityReference& reference);
        WHEATEAR_API Entity ResolveSelector(Scene* scene, const std::string& selector);

    } // namespace EntityReferences

} // namespace Wheatear
