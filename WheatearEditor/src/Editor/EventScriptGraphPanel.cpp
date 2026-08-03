#include "EventScriptGraphPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorWidgets.h"

#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <vector>

namespace Wheatear {

    namespace {

        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;
        static std::string s_PendingOpenEvent;

        using EditorWidgets::InputString;

        static const char* InstructionTypeName(EventScriptInstructionType type)
        {
            switch (type)
            {
            case EventScriptInstructionType::Command: return "Command";
            case EventScriptInstructionType::Wait: return "Wait";
            case EventScriptInstructionType::If: return "If";
            case EventScriptInstructionType::EndIf: return "End If";
            }
            return "Unknown";
        }

        static ImU32 InstructionColor(EventScriptInstructionType type)
        {
            switch (type)
            {
            case EventScriptInstructionType::Command: return IM_COL32(44, 124, 130, 236);
            case EventScriptInstructionType::Wait: return IM_COL32(58, 89, 152, 236);
            case EventScriptInstructionType::If: return IM_COL32(155, 122, 42, 242);
            case EventScriptInstructionType::EndIf: return IM_COL32(72, 65, 46, 230);
            }
            return IM_COL32(78, 84, 90, 236);
        }

        static std::string InstructionText(const EventScriptInstruction& instruction)
        {
            if (instruction.Type == EventScriptInstructionType::Wait)
            {
                std::ostringstream stream;
                stream << "wait " << instruction.Seconds << "s";
                return stream.str();
            }

            if (instruction.Type == EventScriptInstructionType::EndIf)
                return "endif";

            return instruction.Text;
        }

        static std::string Shorten(const std::string& text, size_t maxLength)
        {
            if (text.size() <= maxLength)
                return text;
            if (maxLength <= 3)
                return text.substr(0, maxLength);
            return text.substr(0, maxLength - 3) + "...";
        }

        static ImVec2 Add(const ImVec2& a, const ImVec2& b)
        {
            return { a.x + b.x, a.y + b.y };
        }

        static ImVec2 Scale(const ImVec2& value, float scale)
        {
            return { value.x * scale, value.y * scale };
        }

        static bool PointInRect(const ImVec2& point, const ImVec2& min, const ImVec2& max)
        {
            return point.x >= min.x && point.x <= max.x
                && point.y >= min.y && point.y <= max.y;
        }

        struct GraphNode
        {
            int InstructionIndex = -1;
            ImVec2 Min;
            ImVec2 Max;
            EventScriptInstructionType Type = EventScriptInstructionType::Command;
            std::string Title;
            std::string Body;
            int SourceLine = 0;
        };

        static void DrawConnection(ImDrawList* drawList,
            const ImVec2& from,
            const ImVec2& to,
            ImU32 color,
            float thickness)
        {
            const float bend = std::max(38.0f, std::abs(to.y - from.y) * 0.32f);
            drawList->AddBezierCubic(
                from,
                { from.x, from.y + bend },
                { to.x, to.y - bend },
                to,
                color,
                thickness,
                24);
        }

        static void DrawGrid(ImDrawList* drawList,
            const ImVec2& min,
            const ImVec2& max,
            const ImVec2& pan,
            float zoom)
        {
            const float spacing = 48.0f * zoom;
            if (spacing < 8.0f)
                return;

            const ImU32 color = IM_COL32(74, 94, 100, 36);
            const float offsetX = std::fmod(pan.x, spacing);
            const float offsetY = std::fmod(pan.y, spacing);
            for (float x = min.x + offsetX; x < max.x; x += spacing)
                drawList->AddLine({ x, min.y }, { x, max.y }, color, 1.0f);
            for (float y = min.y + offsetY; y < max.y; y += spacing)
                drawList->AddLine({ min.x, y }, { max.x, y }, color, 1.0f);
        }

    } // namespace

    namespace EventScriptGraphRequests {

        void RequestOpenScript(const std::string& sourcePath, const std::string& eventName)
        {
            s_HasPendingOpen = true;
            s_PendingOpenPath = sourcePath;
            s_PendingOpenEvent = eventName;
        }

        bool ConsumeOpenScriptRequest(std::string& sourcePath, std::string& eventName)
        {
            if (!s_HasPendingOpen)
                return false;

            s_HasPendingOpen = false;
            sourcePath = s_PendingOpenPath;
            eventName = s_PendingOpenEvent;
            s_PendingOpenPath.clear();
            s_PendingOpenEvent.clear();
            return true;
        }

    } // namespace EventScriptGraphRequests

    void EventScriptGraphPanel::Open(const std::string& sourcePath, const std::string& eventName)
    {
        if (!sourcePath.empty())
            m_SourcePath = sourcePath;
        if (!eventName.empty())
            m_SelectedEvent = eventName;

        m_Open = true;
        m_Loaded = false;
        m_SelectedInstruction = -1;
        Load();
    }

    void EventScriptGraphPanel::Load()
    {
        m_Script = EventScript::FromFile(m_SourcePath);
        m_Loaded = true;

        const auto& events = m_Script.GetEvents();
        if (events.empty())
        {
            m_SelectedEvent.clear();
            m_Status = "No events found.";
            return;
        }

        const auto it = std::find_if(events.begin(), events.end(),
            [&](const EventScriptBlock& block) { return block.Name == m_SelectedEvent; });
        if (m_SelectedEvent.empty() || it == events.end())
            m_SelectedEvent = events.front().Name;

        m_Status = "Loaded " + std::to_string(events.size()) + " event(s).";
    }

    const EventScriptBlock* EventScriptGraphPanel::GetSelectedBlock() const
    {
        if (m_SelectedEvent.empty())
            return nullptr;
        return m_Script.FindEvent(m_SelectedEvent);
    }

    void EventScriptGraphPanel::DrawToolbar()
    {
        bool pathChanged = InputString("Script Path", m_SourcePath, 512);
        ImGui::SameLine();
        if (ImGui::Button("Reload") || pathChanged)
            Load();

        const auto& events = m_Script.GetEvents();
        if (ImGui::BeginCombo("Event", m_SelectedEvent.empty() ? "(none)" : m_SelectedEvent.c_str()))
        {
            for (const auto& event : events)
            {
                const bool selected = event.Name == m_SelectedEvent;
                if (ImGui::Selectable(event.Name.c_str(), selected))
                {
                    m_SelectedEvent = event.Name;
                    m_SelectedInstruction = -1;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Center"))
            m_GraphPan = { 18.0f, 18.0f };
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("Zoom", &m_Zoom, 0.65f, 1.45f, "%.2f");

        if (!m_Status.empty())
            ImGui::TextDisabled("%s", m_Status.c_str());
    }

    void EventScriptGraphPanel::DrawGraph()
    {
        const EventScriptBlock* block = GetSelectedBlock();
        if (!block)
        {
            ImGui::TextDisabled("No event selected.");
            return;
        }

        const float detailWidth = 300.0f;
        ImVec2 graphSize = ImGui::GetContentRegionAvail();
        graphSize.x = std::max(260.0f, graphSize.x - detailWidth - ImGui::GetStyle().ItemSpacing.x);

        ImGui::BeginChild("EventGraphCanvas", graphSize, true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        const ImVec2 canvasMax = { canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y };
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(12, 18, 21, 246), 4.0f);
        DrawGrid(drawList, canvasMin, canvasMax, m_GraphPan, m_Zoom);

        ImGui::InvisibleButton("##EventGraphSurface", canvasSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        const bool hovered = ImGui::IsItemHovered();
        if (hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
            m_GraphPan = Add(m_GraphPan, ImGui::GetIO().MouseDelta);

        std::vector<GraphNode> nodes;
        nodes.reserve(block->Instructions.size() + 1);

        const ImVec2 nodeSize = Scale({ 260.0f, 72.0f }, m_Zoom);
        const float gap = 34.0f * m_Zoom;
        const ImVec2 origin = Add(canvasMin, Add(m_GraphPan, { 38.0f * m_Zoom, 34.0f * m_Zoom }));

        nodes.push_back({
            -1,
            origin,
            Add(origin, nodeSize),
            EventScriptInstructionType::Command,
            "Event Start",
            block->Name,
            0
        });

        for (size_t i = 0; i < block->Instructions.size(); ++i)
        {
            const auto& instruction = block->Instructions[i];
            const ImVec2 min = Add(origin, { 0.0f, static_cast<float>(i + 1) * (nodeSize.y + gap) });
            nodes.push_back({
                static_cast<int>(i),
                min,
                Add(min, nodeSize),
                instruction.Type,
                InstructionTypeName(instruction.Type),
                InstructionText(instruction),
                instruction.SourceLine
            });
        }

        drawList->PushClipRect(canvasMin, canvasMax, true);

        for (size_t i = 1; i < nodes.size(); ++i)
        {
            const ImVec2 from = { (nodes[i - 1].Min.x + nodes[i - 1].Max.x) * 0.5f, nodes[i - 1].Max.y };
            const ImVec2 to = { (nodes[i].Min.x + nodes[i].Max.x) * 0.5f, nodes[i].Min.y };
            DrawConnection(drawList, from, to, IM_COL32(76, 178, 158, 150), 2.0f * m_Zoom);
        }

        std::vector<size_t> ifStack;
        for (size_t i = 0; i < block->Instructions.size(); ++i)
        {
            const auto& instruction = block->Instructions[i];
            if (instruction.Type == EventScriptInstructionType::If)
            {
                ifStack.push_back(i + 1);
            }
            else if (instruction.Type == EventScriptInstructionType::EndIf && !ifStack.empty())
            {
                const size_t start = ifStack.back();
                ifStack.pop_back();
                const size_t end = i + 1;
                const ImVec2 from = { nodes[start].Max.x, (nodes[start].Min.y + nodes[start].Max.y) * 0.5f };
                const ImVec2 to = { nodes[end].Max.x, (nodes[end].Min.y + nodes[end].Max.y) * 0.5f };
                drawList->AddBezierCubic(
                    from,
                    { from.x + 74.0f * m_Zoom, from.y },
                    { to.x + 74.0f * m_Zoom, to.y },
                    to,
                    IM_COL32(220, 178, 72, 140),
                    2.0f * m_Zoom,
                    24);
            }
        }

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        int clickedInstruction = m_SelectedInstruction;
        for (const GraphNode& node : nodes)
        {
            const bool selected = node.InstructionIndex == m_SelectedInstruction;
            const bool nodeHovered = hovered && PointInRect(mouse, node.Min, node.Max);
            const ImU32 fill = node.InstructionIndex < 0
                ? IM_COL32(38, 116, 94, 242)
                : InstructionColor(node.Type);
            const ImU32 border = selected
                ? IM_COL32(255, 216, 92, 255)
                : (nodeHovered ? IM_COL32(116, 226, 196, 255) : IM_COL32(82, 114, 118, 220));

            drawList->AddRectFilled(node.Min, node.Max, fill, 7.0f * m_Zoom);
            drawList->AddRect(node.Min, node.Max, border, 7.0f * m_Zoom, 0, selected ? 3.0f : 1.6f);
            drawList->AddCircleFilled({ node.Min.x + 18.0f * m_Zoom, (node.Min.y + node.Max.y) * 0.5f },
                6.0f * m_Zoom,
                IM_COL32(200, 248, 226, 240));

            const ImVec2 titlePos = { node.Min.x + 34.0f * m_Zoom, node.Min.y + 12.0f * m_Zoom };
            const ImVec2 bodyPos = { node.Min.x + 34.0f * m_Zoom, node.Min.y + 38.0f * m_Zoom };
            drawList->AddText(titlePos, IM_COL32(232, 246, 240, 255), node.Title.c_str());
            drawList->AddText(bodyPos, IM_COL32(178, 203, 196, 255), Shorten(node.Body, 38).c_str());

            if (node.SourceLine > 0)
            {
                const std::string line = "L" + std::to_string(node.SourceLine);
                drawList->AddText({ node.Max.x - 42.0f * m_Zoom, node.Min.y + 12.0f * m_Zoom },
                    IM_COL32(128, 154, 150, 255),
                    line.c_str());
            }

            if (nodeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                clickedInstruction = node.InstructionIndex;
        }
        m_SelectedInstruction = clickedInstruction;

        drawList->PopClipRect();
        ImGui::EndChild();

        ImGui::SameLine();
        DrawDetails(block);
    }

    void EventScriptGraphPanel::DrawDetails(const EventScriptBlock* block)
    {
        ImGui::BeginChild("EventGraphDetails", { 0.0f, 0.0f }, true);
        ImGui::TextUnformatted("Selection");
        ImGui::Separator();

        if (!block)
        {
            ImGui::TextDisabled("No event.");
            ImGui::EndChild();
            return;
        }

        ImGui::Text("Event: %s", block->Name.c_str());
        ImGui::Text("Instructions: %d", static_cast<int>(block->Instructions.size()));

        if (m_SelectedInstruction < 0)
        {
            ImGui::TextDisabled("Selected node: Event Start");
            ImGui::EndChild();
            return;
        }

        if (m_SelectedInstruction >= static_cast<int>(block->Instructions.size()))
        {
            ImGui::TextDisabled("Selected node is out of range.");
            ImGui::EndChild();
            return;
        }

        const auto& instruction = block->Instructions[static_cast<size_t>(m_SelectedInstruction)];
        ImGui::Spacing();
        ImGui::Text("Type: %s", InstructionTypeName(instruction.Type));
        ImGui::Text("Source Line: %d", instruction.SourceLine);
        if (instruction.Type == EventScriptInstructionType::Wait)
            ImGui::Text("Seconds: %.3f", instruction.Seconds);
        else
            ImGui::TextWrapped("%s", instruction.Text.c_str());

        ImGui::Spacing();
        ImGui::TextDisabled("This graph is a safe visual view. Edit and save script text from the Event Script component for now.");
        ImGui::EndChild();
    }

    void EventScriptGraphPanel::OnImGuiRender()
    {
        std::string pendingPath;
        std::string pendingEvent;
        if (EventScriptGraphRequests::ConsumeOpenScriptRequest(pendingPath, pendingEvent))
            Open(pendingPath, pendingEvent);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        if (EditorFloatingWindow::Begin("Event Script Graph", &m_Open, 0, { 1220.0f, 760.0f }))
        {
            EditorFloatingWindow::DrawToggleButton("Event Script Graph");
            ImGui::Separator();
            DrawToolbar();
            ImGui::Separator();
            DrawGraph();
        }
        EditorFloatingWindow::End();
    }

} // namespace Wheatear
