#pragma once

#include "Wheatear/Scene/Entity.h"

#include "Editor/EditorLocale.h"
#include <functional>
#include <utility>

#include <imgui/imgui.h>

namespace Wheatear::EditorCanvasTools {

    struct CanvasToolCallbacks
    {
        std::function<void(Entity)> OpenCanvasEditor;
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
        ImGui::TextDisabled(EditorLocale::Text("Editor Tools", "编辑器工具"));

        if (ImGui::Button(EditorLocale::Text("Edit Canvas", "编辑画布")))
            callbacks.OpenCanvasEditor(canvasEntity);
    }

} // namespace Wheatear::EditorCanvasTools
