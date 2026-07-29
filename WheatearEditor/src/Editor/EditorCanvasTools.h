#pragma once

#include "Wheatear/Scene/Entity.h"

#include <functional>
#include <utility>

#include <imgui/imgui.h>

namespace Wheatear::EditorCanvasTools {

    struct CanvasToolCallbacks
    {
        std::function<void(Entity)> OpenCanvasEditor;
        std::function<bool()> IsSceneViewportUIHidden;
        std::function<void(bool)> SetSceneViewportUIHidden;
    };

    inline CanvasToolCallbacks& Callbacks()
    {
        static CanvasToolCallbacks callbacks;
        return callbacks;
    }

    inline void Configure(CanvasToolCallbacks callbacks)
    {
        Callbacks() = std::move(callbacks);
    }

    inline void Reset()
    {
        Callbacks() = {};
    }

    inline void DrawCanvasInspectorTools(Entity canvasEntity)
    {
        auto& callbacks = Callbacks();
        if (!callbacks.OpenCanvasEditor)
            return;

        ImGui::Separator();
        ImGui::TextDisabled("Editor Tools");

        if (ImGui::Button("Edit Canvas"))
            callbacks.OpenCanvasEditor(canvasEntity);

        if (callbacks.IsSceneViewportUIHidden && callbacks.SetSceneViewportUIHidden)
        {
            bool hidden = callbacks.IsSceneViewportUIHidden();
            if (ImGui::Checkbox("Hide UI In Scene Viewport", &hidden))
                callbacks.SetSceneViewportUIHidden(hidden);
        }
    }

} // namespace Wheatear::EditorCanvasTools
