#pragma once

#include "Wheatear/Scene/Entity.h"

#include <functional>
#include <string>

namespace Wheatear {

    struct EditorComponentDescriptor
    {
        std::string Category;
        std::string Label;
        std::function<bool(Entity)> CanAdd;
        std::function<void(Entity)> Add;
        std::function<void(Entity)> Draw;
    };

    class EditorComponentRegistry
    {
    public:
        using Visitor = std::function<void(const EditorComponentDescriptor&)>;

        static void Register(EditorComponentDescriptor descriptor);
        static void Clear();
        static void ForEach(const Visitor& visitor);
    };

} // namespace Wheatear
