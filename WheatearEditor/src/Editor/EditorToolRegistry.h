#pragma once

#include "Wheatear/Scene/Entity.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Wheatear {

    struct EditorToolContext
    {
        Entity SelectedEntity;
    };

    struct EditorToolDescriptor
    {
        std::string MenuLabel;
        std::function<void(const EditorToolContext&)> Open;
        std::function<void()> Draw;
    };

    class EditorToolRegistry
    {
    public:
        using Visitor = std::function<void(const EditorToolDescriptor&)>;

        static void Register(EditorToolDescriptor descriptor)
        {
            if (descriptor.MenuLabel.empty())
                return;

            auto& tools = Tools();
            for (const auto& existing : tools)
            {
                if (existing.MenuLabel == descriptor.MenuLabel)
                    return;
            }

            tools.push_back(std::move(descriptor));
        }

        static void Clear()
        {
            Tools().clear();
        }

        static void ForEach(const Visitor& visitor)
        {
            if (!visitor)
                return;

            for (const auto& tool : Tools())
                visitor(tool);
        }

    private:
        static std::vector<EditorToolDescriptor>& Tools()
        {
            static std::vector<EditorToolDescriptor> tools;
            return tools;
        }
    };

} // namespace Wheatear
