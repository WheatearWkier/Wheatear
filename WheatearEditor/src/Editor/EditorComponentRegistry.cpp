#include "wtpch.h"
#include "EditorComponentRegistry.h"

#include "Wheatear/Core/Log.h"

#include <utility>
#include <vector>

namespace Wheatear {

    namespace {

        static std::vector<EditorComponentDescriptor>& Descriptors()
        {
            static std::vector<EditorComponentDescriptor> descriptors;
            return descriptors;
        }

    } // namespace

    void EditorComponentRegistry::Register(EditorComponentDescriptor descriptor)
    {
        WT_CORE_ASSERT(!descriptor.Category.empty(), "Editor component category cannot be empty.");
        WT_CORE_ASSERT(!descriptor.Label.empty(), "Editor component label cannot be empty.");
        WT_CORE_ASSERT(descriptor.CanAdd, "Editor component '{}' has no CanAdd function.", descriptor.Label);
        WT_CORE_ASSERT(descriptor.Add, "Editor component '{}' has no Add function.", descriptor.Label);
        WT_CORE_ASSERT(descriptor.Draw, "Editor component '{}' has no Draw function.", descriptor.Label);

        auto& descriptors = Descriptors();
        for (const auto& existing : descriptors)
        {
            if (existing.Category == descriptor.Category && existing.Label == descriptor.Label)
                return;
        }

        descriptors.push_back(std::move(descriptor));
    }

    void EditorComponentRegistry::Clear()
    {
        Descriptors().clear();
    }

    void EditorComponentRegistry::ForEach(const Visitor& visitor)
    {
        if (!visitor)
            return;

        for (const auto& descriptor : Descriptors())
            visitor(descriptor);
    }

} // namespace Wheatear
