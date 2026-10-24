#pragma once

#include "Editor/EditorWorkspace.h"
#include "Wheatear/Scene/Entity.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Wheatear {

    class Scene;

    struct EditorToolContext
    {
        Entity SelectedEntity;
        // The scene currently open in the editor (edit or play mode), when
        // one exists; gameplay tool panels use it for live scene inspection.
        Scene* ActiveScene = nullptr;
    };

    enum class EditorToolCategory
    {
        Gameplay = 0,
        UI,
        Assets,
        Diagnostics,
        Window
    };

    struct EditorToolDescriptor
    {
        std::string MenuLabel;
        EditorToolCategory Category = EditorToolCategory::Gameplay;
        std::function<void(const EditorToolContext&)> Open;
        std::function<void()> Draw;
        EditorDomain Domain = EditorDomain::Any;
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


        static void ForEach(const Visitor& visitor)
        {
            if (!visitor)
                return;

            for (const auto& tool : Tools())
            {
                if (EditorWorkspaceContext::IsVisible(ClassifyDomain(tool)))
                    visitor(tool);
            }
        }

    private:
        static EditorDomain ClassifyDomain(const EditorToolDescriptor& tool)
        {
            if (tool.Domain != EditorDomain::Any)
                return tool.Domain;
            const std::string& label = tool.MenuLabel;
            if (label.find("Sprite Sheet") != std::string::npos
                || label.find("Visual Novel") != std::string::npos
                || label.find("VN Script") != std::string::npos
                || label.find("Side Combat") != std::string::npos
                || label.find("Turn Combat") != std::string::npos
                || label.find("Arcade Combat") != std::string::npos
                || label.find("Tactical") != std::string::npos)
            {
                return EditorDomain::TwoD;
            }
            return EditorDomain::Any;
        }

        static std::vector<EditorToolDescriptor>& Tools()
        {
            static std::vector<EditorToolDescriptor> tools;
            return tools;
        }
    };

} // namespace Wheatear
