#include "wepch.h"
#include "EventScriptGraphPanel.h"
#include "EventScriptGraphPanelInternal.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Wheatear {

    using namespace EventScriptGraphPanelInternal;

    void EventScriptGraphPanel::DrawGraph()
    {
        const EventScriptBlock* block = GetSelectedBlock();
        if (!block)
        {
            ImGui::TextDisabled(EditorLocale::Text("No event selected.", "未选择事件。"));
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
        // Ctrl + wheel zooms the canvas (matches the zoom slider range).
        if (hovered && ImGui::GetIO().MouseWheel != 0.0f && ImGui::GetIO().KeyCtrl)
            m_Zoom = std::clamp(m_Zoom * (1.0f + ImGui::GetIO().MouseWheel * 0.1f), 0.65f, 1.45f);

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
                const ImVec2 branchMin = {
                    nodes[start].Min.x - 12.0f * m_Zoom,
                    nodes[start].Min.y - 10.0f * m_Zoom
                };
                const ImVec2 branchMax = {
                    nodes[end].Max.x + 92.0f * m_Zoom,
                    nodes[end].Max.y + 10.0f * m_Zoom
                };
                drawList->AddRectFilled(branchMin,
                    branchMax,
                    IM_COL32(126, 98, 32, 34),
                    8.0f * m_Zoom);
                drawList->AddRect(branchMin,
                    branchMax,
                    IM_COL32(226, 180, 76, 120),
                    8.0f * m_Zoom,
                    0,
                    1.5f * m_Zoom);
                drawList->AddText({ nodes[start].Max.x + 12.0f * m_Zoom, nodes[start].Min.y + 8.0f * m_Zoom },
                    IM_COL32(246, 215, 122, 210),
                    "true");
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
        ImGui::TextUnformatted("Event Graph");
        ImGui::Separator();

        EventScriptBlock* mutableBlock = GetSelectedBlockMutable();
        if (!block || !mutableBlock)
        {
            ImGui::TextDisabled("No event.");
            ImGui::EndChild();
            return;
        }

        bool changed = false;

        std::string eventName = mutableBlock->Name;
        if (EditorWidgets::InputString("Event Name", eventName, 128))
        {
            eventName = Trim(eventName);
            if (eventName.empty())
            {
                m_Status = "Event name cannot be empty.";
            }
            else
            {
                const bool duplicate = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
                    [&](const EventScriptBlock& event)
                    {
                        return &event != mutableBlock && event.Name == eventName;
                    }) != m_EditableEvents.end();

                if (duplicate)
                {
                    m_Status = "Event name already exists.";
                }
                else
                {
                    mutableBlock->Name = eventName;
                    m_SelectedEvent = eventName;
                    changed = true;
                }
            }
        }

        ImGui::Text("Instructions: %d", static_cast<int>(mutableBlock->Instructions.size()));

        int ifDepth = 0;
        int unmatchedEndIf = 0;
        for (const EventScriptInstruction& current : mutableBlock->Instructions)
        {
            if (current.Type == EventScriptInstructionType::If)
                ++ifDepth;
            else if (current.Type == EventScriptInstructionType::EndIf)
            {
                if (ifDepth <= 0)
                    ++unmatchedEndIf;
                else
                    --ifDepth;
            }
        }
        if (ifDepth > 0)
            EditorWidgets::InlineStatus(("Missing EndIf for " + std::to_string(ifDepth) + " If node(s).").c_str(),
                EditorWidgets::StatusKind::Warning);
        if (unmatchedEndIf > 0)
            EditorWidgets::InlineStatus(("Unmatched EndIf node(s): " + std::to_string(unmatchedEndIf)).c_str(),
                EditorWidgets::StatusKind::Error);
        if (ifDepth > 0 && ImGui::SmallButton("Append Missing EndIf"))
        {
            for (int i = 0; i < ifDepth; ++i)
            {
                EventScriptInstruction endIf;
                endIf.Type = EventScriptInstructionType::EndIf;
                mutableBlock->Instructions.push_back(endIf);
            }
            changed = true;
            SyncSourceFromEditableEvents(true);
            ImGui::EndChild();
            return;
        }
        if (unmatchedEndIf > 0)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove Unmatched EndIf"))
            {
                int depth = 0;
                for (auto it = mutableBlock->Instructions.begin(); it != mutableBlock->Instructions.end();)
                {
                    if (it->Type == EventScriptInstructionType::If)
                    {
                        ++depth;
                        ++it;
                    }
                    else if (it->Type == EventScriptInstructionType::EndIf && depth <= 0)
                    {
                        it = mutableBlock->Instructions.erase(it);
                        changed = true;
                    }
                    else
                    {
                        if (it->Type == EventScriptInstructionType::EndIf)
                            --depth;
                        ++it;
                    }
                }
                m_SelectedInstruction = std::min(m_SelectedInstruction, static_cast<int>(mutableBlock->Instructions.size()) - 1);
                SyncSourceFromEditableEvents(true);
                ImGui::EndChild();
                return;
            }
        }

        EditorWidgets::SectionHeader(
            EditorLocale::Text("Node Palette", "节点面板"),
            EditorLocale::Text("Adds a node after the current selection.", "在当前选择后添加节点。"));
        if (ImGui::SmallButton("Scene"))
        {
            AddInstruction(EventScriptInstructionType::Command, "scene:assets/scenes/");
            ImGui::EndChild();
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Event Call"))
        {
            AddInstruction(EventScriptInstructionType::Command, "event:" + mutableBlock->Name);
            ImGui::EndChild();
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Set Flag"))
        {
            AddInstruction(EventScriptInstructionType::Command, "progression:set_flag ");
            ImGui::EndChild();
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Dungeon"))
        {
            AddInstruction(EventScriptInstructionType::Command, "progression:set_active_dungeon ");
            ImGui::EndChild();
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Save"))
        {
            AddInstruction(EventScriptInstructionType::Command, "gamesave:open_save_menu");
            ImGui::EndChild();
            return;
        }

        if (ImGui::SmallButton("Raw Command"))
        {
            AddInstruction(EventScriptInstructionType::Command);
            ImGui::EndChild();
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Add Wait"))
        {
            AddInstruction(EventScriptInstructionType::Wait);
            ImGui::EndChild();
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Add If"))
        {
            AddInstruction(EventScriptInstructionType::If);
            ImGui::EndChild();
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Add EndIf"))
        {
            AddInstruction(EventScriptInstructionType::EndIf);
            ImGui::EndChild();
            return;
        }

        if (m_SelectedInstruction < 0)
        {
            ImGui::TextDisabled("Selected node: Event Start");
            if (changed)
                SyncSourceFromEditableEvents(true);
            ImGui::EndChild();
            return;
        }

        if (m_SelectedInstruction >= static_cast<int>(mutableBlock->Instructions.size()))
        {
            ImGui::TextDisabled("Selected node is out of range.");
            if (changed)
                SyncSourceFromEditableEvents(true);
            ImGui::EndChild();
            return;
        }

        auto& instruction = mutableBlock->Instructions[static_cast<size_t>(m_SelectedInstruction)];
        ImGui::Spacing();
        ImGui::Text("Selected Instruction: %d", m_SelectedInstruction + 1);
        ImGui::Text("Source Line: %d", instruction.SourceLine);

        static const EventScriptInstructionType kEditableTypes[] = {
            EventScriptInstructionType::Command,
            EventScriptInstructionType::Wait,
            EventScriptInstructionType::If,
            EventScriptInstructionType::EndIf
        };

        if (ImGui::BeginCombo("Type", InstructionTypeName(instruction.Type)))
        {
            for (EventScriptInstructionType type : kEditableTypes)
            {
                const bool selected = instruction.Type == type;
                if (ImGui::Selectable(InstructionTypeName(type), selected))
                {
                    if (instruction.Type != type)
                    {
                        instruction.Type = type;
                        instruction.Text.clear();
                        instruction.Seconds = 0.0f;
                        if (type == EventScriptInstructionType::Command)
                            instruction.Text = "scene:assets/scenes/";
                        else if (type == EventScriptInstructionType::Wait)
                            instruction.Seconds = 0.10f;
                        else if (type == EventScriptInstructionType::If)
                            instruction.Text = "flag ";
                        changed = true;
                    }
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (instruction.Type == EventScriptInstructionType::Wait)
        {
            float seconds = instruction.Seconds;
            if (ImGui::DragFloat("Seconds", &seconds, 0.01f, 0.0f, 60.0f, "%.2f"))
            {
                instruction.Seconds = std::max(0.0f, seconds);
                changed = true;
            }
        }
        else if (instruction.Type == EventScriptInstructionType::EndIf)
        {
            ImGui::TextUnformatted("endif");
        }
        else if (instruction.Type == EventScriptInstructionType::If)
        {
            std::string condition = instruction.Text;
            if (DrawConditionBuilder(condition))
            {
                instruction.Text = condition;
                changed = true;
            }
        }
        else
        {
            std::string command = instruction.Text;
            if (EditorCommandBuilder::DrawCommandBuilder("Command", command, 512))
            {
                instruction.Text = command;
                changed = true;
            }
        }

        ImGui::Separator();
        const bool canMoveUp = m_SelectedInstruction > 0;
        const bool canMoveDown = m_SelectedInstruction + 1 < static_cast<int>(mutableBlock->Instructions.size());
        ImGui::BeginDisabled(!canMoveUp);
        if (ImGui::SmallButton("Move Up"))
        {
            std::swap(mutableBlock->Instructions[static_cast<size_t>(m_SelectedInstruction)],
                mutableBlock->Instructions[static_cast<size_t>(m_SelectedInstruction - 1)]);
            --m_SelectedInstruction;
            changed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!canMoveDown);
        if (ImGui::SmallButton("Move Down"))
        {
            std::swap(mutableBlock->Instructions[static_cast<size_t>(m_SelectedInstruction)],
                mutableBlock->Instructions[static_cast<size_t>(m_SelectedInstruction + 1)]);
            ++m_SelectedInstruction;
            changed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedInstruction < 0);
        if (ImGui::SmallButton("Wrap In If"))
        {
            const int insertIndex = std::clamp(m_SelectedInstruction, 0, static_cast<int>(mutableBlock->Instructions.size()));
            EventScriptInstruction ifInstruction;
            ifInstruction.Type = EventScriptInstructionType::If;
            ifInstruction.Text = "flag ";
            EventScriptInstruction endIfInstruction;
            endIfInstruction.Type = EventScriptInstructionType::EndIf;
            mutableBlock->Instructions.insert(mutableBlock->Instructions.begin() + insertIndex, ifInstruction);
            mutableBlock->Instructions.insert(mutableBlock->Instructions.begin() + insertIndex + 2, endIfInstruction);
            m_SelectedInstruction = insertIndex;
            changed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(instruction.Type != EventScriptInstructionType::If);
        if (ImGui::SmallButton("Insert EndIf After"))
        {
            EventScriptInstruction endIfInstruction;
            endIfInstruction.Type = EventScriptInstructionType::EndIf;
            mutableBlock->Instructions.insert(mutableBlock->Instructions.begin() + m_SelectedInstruction + 1, endIfInstruction);
            changed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        // Delete key removes the selected instruction while the details panel
        // is focused (guarded so typing in fields is untouched).
        if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive()
            && ImGui::IsKeyPressed(ImGuiKey_Delete)
            && m_SelectedInstruction >= 0
            && m_SelectedInstruction < static_cast<int>(mutableBlock->Instructions.size()))
        {
            mutableBlock->Instructions.erase(mutableBlock->Instructions.begin() + m_SelectedInstruction);
            m_SelectedInstruction = -1;
            changed = true;
        }
        if (ImGui::SmallButton("Delete Node"))
        {
            mutableBlock->Instructions.erase(mutableBlock->Instructions.begin() + m_SelectedInstruction);
            m_SelectedInstruction = -1;
            changed = true;
        }

        if (changed)
            SyncSourceFromEditableEvents(true);

        ImGui::EndChild();
    }
} // namespace Wheatear
