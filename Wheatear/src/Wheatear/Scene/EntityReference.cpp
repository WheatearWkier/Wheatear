#include "wtpch.h"
#include "EntityReference.h"

#include "Entity.h"
#include "Scene.h"

#include <charconv>

namespace Wheatear::EntityReferences {

    namespace {

        static std::string StripPrefix(const std::string& selector)
        {
            if (selector.empty())
                return {};

            if (selector.front() == '@')
                return selector.substr(1);

            constexpr const char* uuidPrefix = "uuid:";
            constexpr const char* idPrefix = "id:";
            if (selector.rfind(uuidPrefix, 0) == 0)
                return selector.substr(std::char_traits<char>::length(uuidPrefix));
            if (selector.rfind(idPrefix, 0) == 0)
                return selector.substr(std::char_traits<char>::length(idPrefix));

            return {};
        }

        static UUID ParseU64(const std::string& text)
        {
            if (text.empty())
                return UUID(0);

            uint64_t value = 0;
            const char* begin = text.data();
            const char* end = begin + text.size();
            const auto result = std::from_chars(begin, end, value);
            if (result.ec != std::errc{} || result.ptr != end)
                return UUID(0);
            return UUID(value);
        }

    } // namespace

    bool IsUUIDSelector(const std::string& selector)
    {
        return static_cast<uint64_t>(ParseUUIDSelector(selector)) != 0;
    }

    UUID ParseUUIDSelector(const std::string& selector)
    {
        return ParseU64(StripPrefix(selector));
    }

    std::string MakeUUIDSelector(UUID uuid)
    {
        const uint64_t value = static_cast<uint64_t>(uuid);
        return value == 0 ? std::string{} : "@" + std::to_string(value);
    }

    Entity Resolve(Scene* scene, const EntityReference& reference)
    {
        if (!scene)
            return {};

        if (static_cast<uint64_t>(reference.EntityID) != 0)
        {
            if (Entity entity = scene->GetEntityByUUID(reference.EntityID))
                return entity;
        }

        return {};
    }

    Entity ResolveSelector(Scene* scene, const std::string& selector)
    {
        if (!scene || selector.empty())
            return {};

        const UUID uuid = ParseUUIDSelector(selector);
        if (static_cast<uint64_t>(uuid) != 0)
            return scene->GetEntityByUUID(uuid);

        return {};
    }

} // namespace Wheatear::EntityReferences
