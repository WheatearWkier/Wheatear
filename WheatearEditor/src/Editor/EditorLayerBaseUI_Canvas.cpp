#include "wepch.h"
#include "EditorLayerBase.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/ImGui/ImGuiLayer.h"
#include "Wheatear/Renderer/Framebuffer.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIInputSystem.h"
#include "Wheatear/UI/UIWidgetLayout.h"
#include "Assets/AssetRegistry.h"
#include "Editor/EditorCanvasTools.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Panels/ContentBrowserPanel.h"
#include "Editor/EditorCommands.h"
#include "Panels/SceneHierarchy/SceneHierarchyPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    namespace {

        struct ViewportUIEntry
        {
            Entity EntityRef;
            UIWidgetLayout::Rect Rect;
            int SortOrder = 0;
            std::string Name;
        };


        static ImVec2 ToScreenPoint(const glm::vec2& viewportMin,
            const glm::vec2& viewportSize,
            float normalizedX,
            float normalizedY)
        {
            return {
                viewportMin.x + normalizedX * viewportSize.x,
                viewportMin.y + normalizedY * viewportSize.y
            };
        }

        static bool PointInScreenRect(const ImVec2& point, const ImVec2& min, const ImVec2& max)
        {
            return point.x >= min.x && point.x <= max.x
                && point.y >= min.y && point.y <= max.y;
        }

        static std::filesystem::path ResolveContentBrowserPayloadPath(const ImGuiPayload* payload)
        {
            if (!payload || !payload->Data || payload->DataSize <= 0)
                return {};

            const size_t charCount = static_cast<size_t>(payload->DataSize) / sizeof(wchar_t);
            if (charCount == 0)
                return {};

            const wchar_t* rawPath = static_cast<const wchar_t*>(payload->Data);
            std::wstring relativeText(rawPath, rawPath + charCount);
            if (!relativeText.empty() && relativeText.back() == L'\0')
                relativeText.pop_back();
            if (relativeText.empty())
                return {};

            std::filesystem::path relativePath(relativeText);
            if (relativePath.is_absolute())
                return {};

            for (const auto& part : relativePath)
            {
                if (part.generic_string() == "..")
                    return {};
            }

            return (GetEditorAssetPath() / relativePath).lexically_normal();
        }

        static bool PointInNormalizedRect(const UIWidgetLayout::Rect& rect, const glm::vec2& point)
        {
            return point.x >= rect.Left && point.x <= rect.Right
                && point.y >= rect.Top && point.y <= rect.Bottom;
        }

        static UIWidgetLayout::Rect Vec4ToRect(const glm::vec4& value)
        {
            return { value.x, value.y, value.z, value.w };
        }

        static glm::vec4 RectToVec4(const UIWidgetLayout::Rect& rect)
        {
            return { rect.Left, rect.Right, rect.Top, rect.Bottom };
        }

        static void ApplyLocalRectToWidget(UIWidgetComponent& widget, UIWidgetLayout::Rect rect)
        {
            constexpr float minSize = 0.001f;
            if (rect.Right < rect.Left + minSize)
                rect.Right = rect.Left + minSize;
            if (rect.Bottom < rect.Top + minSize)
                rect.Bottom = rect.Top + minSize;

            const float width = rect.Right - rect.Left;
            const float height = rect.Bottom - rect.Top;
            const float centerX = (rect.Left + rect.Right) * 0.5f;
            const float centerY = (rect.Top + rect.Bottom) * 0.5f;

            widget.Size = { width, height };
            switch (widget.Anchor)
            {
            case UIAnchor::TopLeft:      widget.Position = { rect.Left, rect.Top }; break;
            case UIAnchor::TopCenter:    widget.Position = { centerX, rect.Top }; break;
            case UIAnchor::TopRight:     widget.Position = { rect.Right, rect.Top }; break;
            case UIAnchor::MiddleLeft:   widget.Position = { rect.Left, centerY }; break;
            case UIAnchor::MiddleCenter: widget.Position = { centerX, centerY }; break;
            case UIAnchor::MiddleRight:  widget.Position = { rect.Right, centerY }; break;
            case UIAnchor::BottomLeft:   widget.Position = { rect.Left, rect.Bottom }; break;
            case UIAnchor::BottomCenter: widget.Position = { centerX, rect.Bottom }; break;
            case UIAnchor::BottomRight:  widget.Position = { rect.Right, rect.Bottom }; break;
            }
        }

        static int HitResizeHandle(const ImVec2& mouse, const ImVec2& min, const ImVec2& max, float size)
        {
            const float centerX = (min.x + max.x) * 0.5f;
            const float centerY = (min.y + max.y) * 0.5f;
            const float half = size * 0.5f;

            auto hit = [&](float x, float y)
            {
                return mouse.x >= x - half && mouse.x <= x + half
                    && mouse.y >= y - half && mouse.y <= y + half;
            };

            if (hit(min.x, min.y)) return UIEdit_TopLeft;
            if (hit(max.x, min.y)) return UIEdit_TopRight;
            if (hit(min.x, max.y)) return UIEdit_BottomLeft;
            if (hit(max.x, max.y)) return UIEdit_BottomRight;
            if (hit(centerX, min.y)) return UIEdit_Top;
            if (hit(centerX, max.y)) return UIEdit_Bottom;
            if (hit(min.x, centerY)) return UIEdit_Left;
            if (hit(max.x, centerY)) return UIEdit_Right;
            return UIEdit_None;
        }

        static ImGuiMouseCursor CursorForUIHandle(int handle)
        {
            switch (handle)
            {
            case UIEdit_Left:
            case UIEdit_Right:
                return ImGuiMouseCursor_ResizeEW;
            case UIEdit_Top:
            case UIEdit_Bottom:
                return ImGuiMouseCursor_ResizeNS;
            case UIEdit_TopLeft:
            case UIEdit_BottomRight:
                return ImGuiMouseCursor_ResizeNWSE;
            case UIEdit_TopRight:
            case UIEdit_BottomLeft:
                return ImGuiMouseCursor_ResizeNESW;
            case UIEdit_Move:
                return ImGuiMouseCursor_Hand;
            default:
                return ImGuiMouseCursor_Arrow;
            }
        }

        static void DrawUIHandle(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color)
        {
            const float half = size * 0.5f;
            drawList->AddRectFilled(
                { center.x - half, center.y - half },
                { center.x + half, center.y + half },
                color,
                1.5f);
            drawList->AddRect(
                { center.x - half, center.y - half },
                { center.x + half, center.y + half },
                IM_COL32(8, 20, 24, 230),
                1.5f,
                0,
                1.0f);
        }

        static void DrawDashedLine(ImDrawList* drawList,
            ImVec2 start,
            ImVec2 end,
            ImU32 color,
            float thickness,
            float dashLength = 7.0f,
            float gapLength = 5.0f)
        {
            const ImVec2 delta = { end.x - start.x, end.y - start.y };
            const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (length <= 0.001f)
                return;

            const ImVec2 direction = { delta.x / length, delta.y / length };
            float cursor = 0.0f;
            while (cursor < length)
            {
                const float segmentEnd = std::min(cursor + dashLength, length);
                drawList->AddLine(
                    { start.x + direction.x * cursor, start.y + direction.y * cursor },
                    { start.x + direction.x * segmentEnd, start.y + direction.y * segmentEnd },
                    color,
                    thickness);
                cursor += dashLength + gapLength;
            }
        }

        static void DrawDashedRect(ImDrawList* drawList,
            const ImVec2& min,
            const ImVec2& max,
            ImU32 color,
            float thickness)
        {
            DrawDashedLine(drawList, min, { max.x, min.y }, color, thickness);
            DrawDashedLine(drawList, { max.x, min.y }, max, color, thickness);
            DrawDashedLine(drawList, max, { min.x, max.y }, color, thickness);
            DrawDashedLine(drawList, { min.x, max.y }, min, color, thickness);
        }

        static bool UsesDashedUIOutline(Entity entity)
        {
            return entity
                && entity.HasComponent<UITextComponent>()
                && !entity.HasComponent<UIPanelComponent>()
                && !entity.HasComponent<UIImageComponent>()
                && !entity.HasComponent<UIButtonComponent>()
                && !entity.HasComponent<UIProgressBarComponent>()
                && !entity.HasComponent<UISliderComponent>()
                && !entity.HasComponent<UICheckboxComponent>();
        }

        static void DrawUIOutline(ImDrawList* drawList,
            const ImVec2& rectMin,
            const ImVec2& rectMax,
            Entity entity,
            ImU32 color,
            float thickness)
        {
            if (UsesDashedUIOutline(entity))
                DrawDashedRect(drawList, rectMin, rectMax, color, thickness);
            else
                drawList->AddRect(rectMin, rectMax, color, 0.0f, 0, thickness);
        }

        static void ProcessCanvasEditorHistoryShortcuts()
        {
            ImGuiIO& io = ImGui::GetIO();
            if (!io.KeyCtrl || io.WantTextInput || ImGui::IsAnyItemActive())
                return;

            if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                if (CommandHistory::Get().CanRedo())
                    CommandHistory::Get().Redo();
                return;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                if (CommandHistory::Get().CanUndo())
                    CommandHistory::Get().Undo();
                return;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
            {
                if (CommandHistory::Get().CanRedo())
                    CommandHistory::Get().Redo();
            }
        }

        static ImU32 ColorToImU32(const glm::vec4& color)
        {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a));
        }

        static Entity FindSingleCanvas(UIWidgetLayout::Context& layout)
        {
            if (!layout.ScenePtr)
                return {};

            auto& registry = layout.ScenePtr->GetRegistry();
            Entity found;
            for (auto entityID : registry.view<UICanvasComponent>())
            {
                if (found)
                    return {};
                found = Entity{ entityID, layout.ScenePtr };
            }

            return found;
        }

        static Entity FindOwningCanvas(Entity entity, UIWidgetLayout::Context& layout)
        {
            if (!entity || !layout.ScenePtr)
                return {};
            if (entity.HasComponent<UICanvasComponent>())
                return entity;
            if (!entity.HasComponent<UIWidgetComponent>())
                return {};

            auto& registry = layout.ScenePtr->GetRegistry();
            std::unordered_set<uint32_t> visited;
            Entity current = entity;
            while (current && current.HasComponent<UIWidgetComponent>())
            {
                const uint32_t currentKey = static_cast<uint32_t>(static_cast<entt::entity>(current));
                if (!visited.insert(currentKey).second)
                    return {};

                const auto& widget = current.GetComponent<UIWidgetComponent>();
                const entt::entity parentID = layout.ResolveReference(widget.ParentEntity);
                if (parentID == entt::null || !registry.valid(parentID))
                    return FindSingleCanvas(layout);

                Entity parent{ parentID, layout.ScenePtr };
                if (parent.HasComponent<UICanvasComponent>())
                    return parent;

                current = parent;
            }

            return {};
        }

        static bool BelongsToCanvas(Entity entity, Entity canvas, UIWidgetLayout::Context& layout)
        {
            if (!entity || !canvas)
                return false;
            if (entity == canvas)
                return true;
            return FindOwningCanvas(entity, layout) == canvas;
        }

        static bool IsEditorHidden(Entity entity)
        {
            return entity && entity.HasComponent<EditorHiddenComponent>();
        }

        static bool IsEditorUIHidden(Entity entity,
            UIWidgetLayout::Context& layout,
            std::unordered_set<uint32_t>& visiting)
        {
            if (!entity || !layout.ScenePtr)
                return false;

            if (entity.HasComponent<EditorHiddenComponent>())
                return true;

            if (!entity.HasComponent<UIWidgetComponent>())
                return false;

            const uint32_t key = static_cast<uint32_t>(static_cast<entt::entity>(entity));
            if (!visiting.insert(key).second)
                return false;

            auto& registry = layout.ScenePtr->GetRegistry();
            const auto& widget = entity.GetComponent<UIWidgetComponent>();
            const entt::entity parentID = layout.ResolveReference(widget.ParentEntity);
            const bool hidden = parentID != entt::null && registry.valid(parentID)
                ? IsEditorUIHidden(Entity{ parentID, layout.ScenePtr }, layout, visiting)
                : false;
            visiting.erase(key);
            return hidden;
        }

        static float EntityFrameRadius(Entity entity)
        {
            if (!entity)
                return 1.0f;

            const auto& transform = entity.GetComponent<TransformComponent>();
            float radius = 0.75f * std::max({
                std::abs(transform.Scale.x),
                std::abs(transform.Scale.y),
                std::abs(transform.Scale.z),
                1.0f
            });

            if (entity.HasComponent<SpriteRendererComponent>())
            {
                const auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                const glm::vec2 uvSize = glm::abs(sprite.UVMax - sprite.UVMin);
                const float drawScale = std::max(std::abs(sprite.DrawScale.x), std::abs(sprite.DrawScale.y));
                radius = std::max(radius, 0.5f * drawScale * std::max(uvSize.x, uvSize.y));
            }
            else if (entity.HasComponent<CircleRendererComponent>())
            {
                radius = std::max(radius, 0.5f);
            }
            else if (entity.HasComponent<MeshRendererComponent>())
            {
                radius = std::max(radius, 1.0f);
            }

            return radius;
        }

    } // namespace



    void EditorLayerBase::UI_CanvasEditor()
    {
        m_UIEditorMouseOverCanvas = false;

        if (!m_UIEditorOpen)
            return;

        if (m_FocusCanvasEditor)
        {
            ImGui::SetNextWindowFocus();
            m_FocusCanvasEditor = false;
        }

        EditorFloatingWindow::Begin("UI Canvas Editor", &m_UIEditorOpen, 0, { 1280.0f, 760.0f });

        auto isValidEntity = [this](Entity entity)
        {
            return entity
                && m_ActiveScene
                && entity.GetScene() == m_ActiveScene.get()
                && m_ActiveScene->GetRegistry().valid(
                    static_cast<entt::entity>(static_cast<uint32_t>(entity)));
        };

        if (!isValidEntity(m_UIEditingCanvas) || !m_UIEditingCanvas.HasComponent<UICanvasComponent>())
        {
            m_UIEditingCanvas = {};
            UIWidgetLayout::Context layout(m_ActiveScene.get());
            Entity selectedCanvas = FindOwningCanvas(m_SceneHierarchyPanel->GetSelectedEntity(), layout);
            if (selectedCanvas)
                m_UIEditingCanvas = selectedCanvas;

            if (m_ActiveScene)
            {
                auto& registry = m_ActiveScene->GetRegistry();
                for (auto entityID : registry.view<UICanvasComponent>())
                {
                    if (m_UIEditingCanvas)
                        break;
                    m_UIEditingCanvas = Entity{ entityID, m_ActiveScene.get() };
                }
            }
        }

        if (ImGui::Button("Use Selected Canvas"))
        {
            Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();
            UIWidgetLayout::Context layout(m_ActiveScene.get());
            if (Entity owningCanvas = FindOwningCanvas(selected, layout))
                m_UIEditingCanvas = owningCanvas;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(m_SceneState != SceneState::Edit || !CommandHistory::Get().CanUndo());
        if (ImGui::Button("Undo"))
            CommandHistory::Get().Undo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_SceneState != SceneState::Edit || !CommandHistory::Get().CanRedo());
        if (ImGui::Button("Redo"))
            CommandHistory::Get().Redo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_SceneState != SceneState::Edit);
        if (ImGui::Button("Save"))
            SaveScene();
        ImGui::EndDisabled();
        ImGui::SameLine();
        EditorFloatingWindow::DrawToggleButton("UI Canvas Editor");

        if (!m_UIEditingCanvas || !m_UIEditingCanvas.HasComponent<UICanvasComponent>())
        {
            ImGui::TextWrapped("Select or create a UI Canvas to edit UI in this dedicated view.");
            EditorFloatingWindow::End();
            return;
        }

        const auto& canvas = m_UIEditingCanvas.GetComponent<UICanvasComponent>();
        const float referenceWidth = std::max(canvas.ReferenceWidth, 1.0f);
        const float referenceHeight = std::max(canvas.ReferenceHeight, 1.0f);
        const float aspect = referenceWidth / referenceHeight;

        ImGui::TextDisabled("Canvas: %s  %.0fx%.0f",
            m_UIEditingCanvas.GetName().c_str(),
            referenceWidth,
            referenceHeight);

        ImVec2 available = ImGui::GetContentRegionAvail();
        available.x = std::max(available.x, 64.0f);
        available.y = std::max(available.y, 64.0f);

        ImVec2 canvasSize = available;
        if (canvasSize.x / canvasSize.y > aspect)
            canvasSize.x = canvasSize.y * aspect;
        else
            canvasSize.y = canvasSize.x / aspect;

        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMin = {
            cursor.x + (available.x - canvasSize.x) * 0.5f,
            cursor.y
        };

        ImGui::SetCursorScreenPos(canvasMin);
        ImGui::InvisibleButton("##UICanvasSurface", canvasSize);
        const ImVec2 canvasMax = { canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y };
        const bool canvasRectHovered = ImGui::IsMouseHoveringRect(canvasMin, canvasMax, true);
        const bool surfaceHovered = canvasRectHovered
            && (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)
                || ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem));
        m_UIEditorMouseOverCanvas = surfaceHovered;
        m_UIEditorCanvasBounds[0] = { canvasMin.x, canvasMin.y };
        m_UIEditorCanvasBounds[1] = { canvasMax.x, canvasMax.y };

        if (m_SceneState == SceneState::Edit
            && (surfaceHovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))
        {
            ProcessCanvasEditorHistoryShortcuts();
        }

        UI_DrawCanvasSceneReference(
            { canvasMin.x, canvasMin.y },
            { canvasSize.x, canvasSize.y });

        UI_DrawCanvasOverlay(
            { canvasMin.x, canvasMin.y },
            { canvasSize.x, canvasSize.y },
            surfaceHovered,
            false,
            m_UIEditingCanvas);

        EditorFloatingWindow::End();
    }

    void EditorLayerBase::UI_DrawCanvasSceneReference(const glm::vec2& regionMin,
        const glm::vec2& regionSize)
    {
        if (!m_Framebuffer || regionSize.x <= 1.0f || regionSize.y <= 1.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = { regionMin.x, regionMin.y };
        const ImVec2 max = { regionMin.x + regionSize.x, regionMin.y + regionSize.y };
        drawList->AddRectFilled(min, max, IM_COL32(28, 32, 38, 255));
        drawList->AddImage(
            reinterpret_cast<void*>(static_cast<uintptr_t>(m_UIReferenceFramebuffer
                ? m_UIReferenceFramebuffer->GetColorAttachmentRendererID()
                : m_Framebuffer->GetColorAttachmentRendererID())),
            min,
            max,
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f),
            IM_COL32(255, 255, 255, 255));
    }

    void EditorLayerBase::UI_DrawCanvasOverlay(const glm::vec2& regionMin,
        const glm::vec2& regionSize,
        bool surfaceHovered,
        bool drawBackdrop,
        Entity canvasEntity)
    {
        if (m_SceneState != SceneState::Edit || !m_ActiveScene)
            return;
        if (regionSize.x <= 1.0f || regionSize.y <= 1.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasMin = { regionMin.x, regionMin.y };
        const ImVec2 canvasMax = { regionMin.x + regionSize.x, regionMin.y + regionSize.y };
        UIWidgetLayout::Context layout(m_ActiveScene.get());
        Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();
        std::unordered_set<uint32_t> selectedHiddenVisiting;
        const bool selectedHiddenInEditor = IsEditorUIHidden(selected, layout, selectedHiddenVisiting);
        const bool selectedInsideThisCanvas = selected && selected != canvasEntity
            && !selectedHiddenInEditor
            && BelongsToCanvas(selected, canvasEntity, layout);
        const bool canvasIsSelectionContext = selected == canvasEntity || selectedInsideThisCanvas;

        if (drawBackdrop)
        {
            const ImU32 background = canvasIsSelectionContext
                ? IM_COL32(20, 38, 35, 48)
                : IM_COL32(18, 23, 29, 42);
            const ImU32 border = canvasIsSelectionContext
                ? IM_COL32(87, 226, 188, 255)
                : IM_COL32(78, 115, 127, 255);
            drawList->AddRectFilled(canvasMin, canvasMax, background);
            drawList->AddRect(canvasMin, canvasMax, border, 0.0f, 0, canvasIsSelectionContext ? 2.5f : 1.5f);
        }

        std::vector<ViewportUIEntry> entries;

        auto& registry = m_ActiveScene->GetRegistry();
        auto view = registry.view<TagComponent, UIWidgetComponent>();
        for (auto entityID : view)
        {
            Entity entity{ entityID, m_ActiveScene.get() };
            if (!BelongsToCanvas(entity, canvasEntity, layout))
                continue;
            std::unordered_set<uint32_t> hiddenVisiting;
            if (IsEditorUIHidden(entity, layout, hiddenVisiting))
                continue;
            if (!UIWidgetLayout::ResolveEditorVisible(layout, entityID))
                continue;

            const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, entityID);
            if (rect.Right <= rect.Left || rect.Bottom <= rect.Top)
                continue;

            const auto& tag = view.get<TagComponent>(entityID).Tag;
            const auto& widget = view.get<UIWidgetComponent>(entityID);
            entries.push_back({ entity, rect, widget.SortOrder, tag });
        }

        std::sort(entries.begin(), entries.end(), [](const ViewportUIEntry& a, const ViewportUIEntry& b)
        {
            if (a.SortOrder != b.SortOrder)
                return a.SortOrder < b.SortOrder;
            return static_cast<uint32_t>(static_cast<entt::entity>(a.EntityRef))
                < static_cast<uint32_t>(static_cast<entt::entity>(b.EntityRef));
        });

        drawList->PushClipRect(canvasMin, canvasMax, true);

        const ImVec2 mouse = ImGui::GetMousePos();
        const bool mouseInCanvas = surfaceHovered
            || ImGui::IsMouseHoveringRect(canvasMin, canvasMax, true);
        const glm::vec2 mouseNorm = {
            (mouse.x - canvasMin.x) / std::max(1.0f, regionSize.x),
            (mouse.y - canvasMin.y) / std::max(1.0f, regionSize.y)
        };
        constexpr float handleSize = 9.0f;
        int selectedHandleUnderMouse = UIEdit_None;
        if (selectedInsideThisCanvas)
        {
            const entt::entity selectedHandleID = static_cast<entt::entity>(selected);
            if (registry.valid(selectedHandleID) && registry.all_of<UIWidgetComponent>(selectedHandleID))
            {
                const UIWidgetLayout::Rect selectedHitRect = UIWidgetLayout::ResolveRect(layout, selectedHandleID);
                const ImVec2 selectedHitMin = ToScreenPoint(regionMin, regionSize, selectedHitRect.Left, selectedHitRect.Top);
                const ImVec2 selectedHitMax = ToScreenPoint(regionMin, regionSize, selectedHitRect.Right, selectedHitRect.Bottom);
                selectedHandleUnderMouse = HitResizeHandle(mouse, selectedHitMin, selectedHitMax, handleSize + 6.0f);
            }
        }
        Entity clickedUI;
        if (mouseInCanvas
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            if (selectedHandleUnderMouse != UIEdit_None)
            {
                clickedUI = selected;
            }
            else
            {
                int bestSort = std::numeric_limits<int>::min();
                float bestArea = std::numeric_limits<float>::max();
                for (const ViewportUIEntry& entry : entries)
                {
                    if (entry.EntityRef == canvasEntity || !PointInNormalizedRect(entry.Rect, mouseNorm))
                        continue;

                    const float area = std::max(entry.Rect.Right - entry.Rect.Left, 0.0f)
                        * std::max(entry.Rect.Bottom - entry.Rect.Top, 0.0f);
                    if (entry.SortOrder > bestSort
                        || (entry.SortOrder == bestSort && area <= bestArea))
                    {
                        clickedUI = entry.EntityRef;
                        bestSort = entry.SortOrder;
                        bestArea = area;
                    }
                }
            }

            if (clickedUI)
            {
                SelectEditorEntity(clickedUI, false);
                selected = clickedUI;
            }
        }

        for (const ViewportUIEntry& entry : entries)
        {
            const ImVec2 rectMin = ToScreenPoint(regionMin, regionSize, entry.Rect.Left, entry.Rect.Top);
            const ImVec2 rectMax = ToScreenPoint(regionMin, regionSize, entry.Rect.Right, entry.Rect.Bottom);

            if (drawBackdrop)
            {
                Entity entity = entry.EntityRef;
                if (entity.HasComponent<UIPanelComponent>())
                {
                    const auto& panel = entity.GetComponent<UIPanelComponent>();
                    drawList->AddRectFilled(rectMin, rectMax, ColorToImU32(panel.BackgroundColor));
                    if (panel.BorderThickness > 0.0f)
                        drawList->AddRect(rectMin, rectMax, ColorToImU32(panel.BorderColor), 0.0f, 0, panel.BorderThickness);
                }
                if (entity.HasComponent<UIButtonComponent>())
                {
                    const auto& button = entity.GetComponent<UIButtonComponent>();
                    drawList->AddRectFilled(rectMin, rectMax, ColorToImU32(button.NormalColor), 4.0f);
                }
                if (entity.HasComponent<UIProgressBarComponent>())
                {
                    const auto& bar = entity.GetComponent<UIProgressBarComponent>();
                    drawList->AddRectFilled(rectMin, rectMax, ColorToImU32(bar.BackgroundColor), 2.0f);
                    const float fillRight = rectMin.x + (rectMax.x - rectMin.x) * bar.GetNormalized();
                    drawList->AddRectFilled(rectMin, { fillRight, rectMax.y }, ColorToImU32(bar.ForegroundColor), 2.0f);
                }
                if (entity.HasComponent<UIImageComponent>())
                {
                    const auto& image = entity.GetComponent<UIImageComponent>();
                    drawList->AddRectFilled(rectMin, rectMax, ColorToImU32(image.Color), 2.0f);
                }
                if (entity.HasComponent<UITextComponent>())
                {
                    const auto& text = entity.GetComponent<UITextComponent>();
                    drawList->AddText({ rectMin.x + 4.0f, rectMin.y + 4.0f },
                        ColorToImU32(text.Color),
                        text.Text.c_str());
                }
            }

            const bool isSelected = selected == entry.EntityRef;
            const ImU32 color = isSelected
                ? IM_COL32(82, 230, 244, 245)
                : IM_COL32(112, 185, 196, drawBackdrop ? 145 : 105);
            if (isSelected || m_ShowUIOutlines || drawBackdrop)
                DrawUIOutline(drawList, rectMin, rectMax, entry.EntityRef, color, isSelected ? 2.0f : 1.0f);

            if (isSelected)
            {
                const std::string label = entry.Name.empty() ? "UI Widget" : entry.Name;
                const ImVec2 labelPos = {
                    rectMin.x + 4.0f,
                    rectMin.y - 18.0f >= canvasMin.y ? rectMin.y - 18.0f : rectMin.y + 4.0f
                };
                drawList->AddText(labelPos, IM_COL32(160, 244, 250, 235), label.c_str());
            }
            else if (m_ShowUIOutlines)
            {
                const std::string label = entry.Name.empty() ? "UI Widget" : entry.Name;
                const ImVec2 labelPos = {
                    rectMin.x + 4.0f,
                    rectMin.y - 18.0f >= canvasMin.y ? rectMin.y - 18.0f : rectMin.y + 4.0f
                };
                drawList->AddText(labelPos, IM_COL32(150, 192, 198, 170), label.c_str());
            }
        }

        if (m_UIEditHandle != UIEdit_None && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            CommitPendingUIEdit();
            drawList->PopClipRect();
            return;
        }

        if (!selected || !selected.HasComponent<UIWidgetComponent>())
        {
            drawList->PopClipRect();
            return;
        }

        std::unordered_set<uint32_t> handleHiddenVisiting;
        if (IsEditorUIHidden(selected, layout, handleHiddenVisiting))
        {
            drawList->PopClipRect();
            return;
        }

        const entt::entity selectedID = static_cast<entt::entity>(selected);
        if (!registry.valid(selectedID) || !registry.all_of<UIWidgetComponent>(selectedID))
        {
            drawList->PopClipRect();
            return;
        }

        if (selected == canvasEntity)
        {
            drawList->PopClipRect();
            return;
        }

        UIWidgetLayout::Rect selectedRect = UIWidgetLayout::ResolveRect(layout, selectedID);
        ImVec2 rectMin = ToScreenPoint(regionMin, regionSize, selectedRect.Left, selectedRect.Top);
        ImVec2 rectMax = ToScreenPoint(regionMin, regionSize, selectedRect.Right, selectedRect.Bottom);

        auto& widget = selected.GetComponent<UIWidgetComponent>();
        const ImU32 handleColor = IM_COL32(86, 230, 244, 245);
        const float centerX = (rectMin.x + rectMax.x) * 0.5f;
        const float centerY = (rectMin.y + rectMax.y) * 0.5f;
        DrawUIHandle(drawList, rectMin, handleSize, handleColor);
        DrawUIHandle(drawList, { centerX, rectMin.y }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMax.x, rectMin.y }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMin.x, centerY }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMax.x, centerY }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMin.x, rectMax.y }, handleSize, handleColor);
        DrawUIHandle(drawList, { centerX, rectMax.y }, handleSize, handleColor);
        DrawUIHandle(drawList, rectMax, handleSize, handleColor);

        const int hoveredHandle = HitResizeHandle(mouse, rectMin, rectMax, handleSize + 6.0f);
        const int hoveredOperation = hoveredHandle != UIEdit_None
            ? hoveredHandle
            : (PointInScreenRect(mouse, rectMin, rectMax) ? UIEdit_Move : UIEdit_None);
        if (hoveredOperation != UIEdit_None)
            ImGui::SetMouseCursor(CursorForUIHandle(hoveredOperation));

        if (mouseInCanvas
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && (selected == clickedUI || hoveredOperation == UIEdit_Move || hoveredHandle != UIEdit_None)
            && hoveredOperation != UIEdit_None
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            m_UIEditHandle = hoveredOperation;
            m_UIEditEntity = selected;
            m_UIEditStartMouse = mouseNorm;
            m_UIEditStartRect = RectToVec4(UIWidgetLayout::WidgetToLocalRect(widget));
            m_UIEditStartWidget = std::make_unique<UIWidgetComponent>(widget);
            m_UIEditStartHadText = selected.HasComponent<UITextComponent>();
            m_UIEditStartText.reset();
            if (m_UIEditStartHadText)
                m_UIEditStartText = std::make_unique<UITextComponent>(selected.GetComponent<UITextComponent>());
        }

        if (m_UIEditHandle != UIEdit_None && m_UIEditEntity == selected)
        {
            float parentWidth = 1.0f;
            float parentHeight = 1.0f;
            const entt::entity parentID = layout.ResolveReference(widget.ParentEntity);
            if (parentID != entt::null)
            {
                if (parentID != entt::null && registry.valid(parentID) && registry.all_of<UIWidgetComponent>(parentID))
                {
                    const UIWidgetLayout::Rect parentRect = UIWidgetLayout::ResolveRect(layout, parentID);
                    parentWidth = std::max(parentRect.Right - parentRect.Left, 0.0001f);
                    parentHeight = std::max(parentRect.Bottom - parentRect.Top, 0.0001f);
                }
            }

            const glm::vec2 delta = {
                (mouseNorm.x - m_UIEditStartMouse.x) / parentWidth,
                (mouseNorm.y - m_UIEditStartMouse.y) / parentHeight
            };

            UIWidgetLayout::Rect localRect = Vec4ToRect(m_UIEditStartRect);
            switch (m_UIEditHandle)
            {
            case UIEdit_Move: localRect.Left += delta.x; localRect.Right += delta.x; localRect.Top += delta.y; localRect.Bottom += delta.y; break;
            case UIEdit_Left: localRect.Left += delta.x; break;
            case UIEdit_Right: localRect.Right += delta.x; break;
            case UIEdit_Top: localRect.Top += delta.y; break;
            case UIEdit_Bottom: localRect.Bottom += delta.y; break;
            case UIEdit_TopLeft: localRect.Left += delta.x; localRect.Top += delta.y; break;
            case UIEdit_TopRight: localRect.Right += delta.x; localRect.Top += delta.y; break;
            case UIEdit_BottomLeft: localRect.Left += delta.x; localRect.Bottom += delta.y; break;
            case UIEdit_BottomRight: localRect.Right += delta.x; localRect.Bottom += delta.y; break;
            default: break;
            }

            constexpr float minSize = 0.0025f;
            if (localRect.Right < localRect.Left + minSize)
            {
                if (m_UIEditHandle == UIEdit_Left || m_UIEditHandle == UIEdit_TopLeft || m_UIEditHandle == UIEdit_BottomLeft)
                    localRect.Left = localRect.Right - minSize;
                else
                    localRect.Right = localRect.Left + minSize;
            }
            if (localRect.Bottom < localRect.Top + minSize)
            {
                if (m_UIEditHandle == UIEdit_Top || m_UIEditHandle == UIEdit_TopLeft || m_UIEditHandle == UIEdit_TopRight)
                    localRect.Top = localRect.Bottom - minSize;
                else
                    localRect.Bottom = localRect.Top + minSize;
            }

            ApplyLocalRectToWidget(widget, localRect);
            UpdateUITextFontDuringUIResize(selected);
        }

        drawList->PopClipRect();
    }

} // namespace Wheatear
