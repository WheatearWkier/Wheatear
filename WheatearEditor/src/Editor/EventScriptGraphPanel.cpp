#include "EventScriptGraphPanel.h"

#include "Editor/CommandBuilder.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/GameplayEditorShell.h"

#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Wheatear {

    namespace {

        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;
        static std::string s_PendingOpenEvent;

        constexpr size_t SourceEditorCapacity = 512 * 1024;

        static void CopyToPathInput(std::array<char, 512>& buffer, const std::string& value)
        {
            buffer.fill('\0');
            if (buffer.empty())
                return;

            const size_t copyLength = std::min(value.size(), buffer.size() - 1);
            std::copy_n(value.begin(), copyLength, buffer.begin());
        }

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

        static std::string Trim(std::string value)
        {
            const size_t start = value.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                return {};

            const size_t end = value.find_last_not_of(" \t\r\n");
            return value.substr(start, end - start + 1);
        }

        static std::string FormatSeconds(float seconds)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2) << std::max(0.0f, seconds);
            return stream.str();
        }

        static std::vector<std::string> SplitWords(const std::string& value)
        {
            std::vector<std::string> words;
            std::istringstream stream(value);
            std::string word;
            while (stream >> word)
                words.push_back(word);
            return words;
        }

        static std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        enum class ConditionKind
        {
            Always, Never, StoryFlag, Skill,
            Dungeon, DungeonUnlocked, Completed,
            LastDungeon, LastResult, Equipment, Equipped,
            Chapter, Material, Raw
        };

        static const char* ConditionKindLabel(ConditionKind k)
        {
            switch (k)
            {
            case ConditionKind::Always: return "Always";
            case ConditionKind::Never: return "Never";
            case ConditionKind::StoryFlag: return "Story Flag";
            case ConditionKind::Skill: return "Skill Unlocked";
            case ConditionKind::Dungeon: return "Dungeon Unlocked";
            case ConditionKind::DungeonUnlocked: return "Dungeon Unlocked (explicit)";
            case ConditionKind::Completed: return "Dungeon Completed";
            case ConditionKind::LastDungeon: return "Last Dungeon";
            case ConditionKind::LastResult: return "Last Result";
            case ConditionKind::Equipment: return "Equipment Owned";
            case ConditionKind::Equipped: return "Equipment Equipped";
            case ConditionKind::Chapter: return "Chapter";
            case ConditionKind::Material: return "Material";
            case ConditionKind::Raw: return "Raw";
            }
            return "Raw";
        }

        static ConditionKind ConditionTokenToKind(const std::string& tokenLower)
        {
            if (tokenLower == "always") return ConditionKind::Always;
            if (tokenLower == "never") return ConditionKind::Never;
            if (tokenLower == "flag") return ConditionKind::StoryFlag;
            if (tokenLower == "skill") return ConditionKind::Skill;
            if (tokenLower == "dungeon") return ConditionKind::Dungeon;
            if (tokenLower == "dungeon_unlocked") return ConditionKind::DungeonUnlocked;
            if (tokenLower == "completed") return ConditionKind::Completed;
            if (tokenLower == "last_dungeon") return ConditionKind::LastDungeon;
            if (tokenLower == "last_result") return ConditionKind::LastResult;
            if (tokenLower == "equipment") return ConditionKind::Equipment;
            if (tokenLower == "equipped") return ConditionKind::Equipped;
            if (tokenLower == "chapter") return ConditionKind::Chapter;
            if (tokenLower == "material") return ConditionKind::Material;
            return ConditionKind::Raw;
        }

        static bool DrawConditionBuilder(std::string& condition)
        {
            bool changed = false;
            condition = Trim(condition);

            // Condition grammar is fixed in EventScriptSystem.cpp EvaluateCondition.
            // Kinds + exact runtime syntax (whitespace-separated):
            //   always | never | flag X | skill X | dungeon X | dungeon_unlocked X
            //   completed X | last_dungeon X | last_result X
            //   equipment X | equipped X | chapter OP N | material X[ OP N]
            // Optional leading "not " negates. Aliases (dungeon_unlocked==dungeon,
            // completed==dungeon_completed, last_result==last_dungeon) keep their
            // own spelling on edit; the raw form is preserved for Raw.

            // Split leading "not".
            bool negate = false;
            std::string body = condition;
            if (body.size() >= 4 && ToLower(body.substr(0, 4)) == "not ")
            {
                negate = true;
                body = Trim(body.substr(4));
            }

            std::vector<std::string> words = SplitWords(body);
            ConditionKind kind = words.empty() ? ConditionKind::Raw : ConditionTokenToKind(ToLower(words[0]));

            if (ImGui::BeginCombo("Condition Type", ConditionKindLabel(kind)))
            {
                static const ConditionKind kinds[] = {
                    ConditionKind::Always, ConditionKind::Never,
                    ConditionKind::StoryFlag, ConditionKind::Skill,
                    ConditionKind::Dungeon, ConditionKind::DungeonUnlocked,
                    ConditionKind::Completed, ConditionKind::LastDungeon,
                    ConditionKind::LastResult, ConditionKind::Equipment,
                    ConditionKind::Equipped, ConditionKind::Chapter,
                    ConditionKind::Material, ConditionKind::Raw
                };
                for (ConditionKind k : kinds)
                {
                    const bool selected = k == kind;
                    if (ImGui::Selectable(ConditionKindLabel(k), selected))
                    {
                        kind = k;
                        // Re-seed minimal default so the picker has something to edit.
                        switch (k)
                        {
                        case ConditionKind::Always: body = "always"; break;
                        case ConditionKind::Never: body = "never"; break;
                        case ConditionKind::StoryFlag: body = "flag FLAG_"; break;
                        case ConditionKind::Skill: body = "skill "; break;
                        case ConditionKind::Dungeon: body = "dungeon "; break;
                        case ConditionKind::DungeonUnlocked: body = "dungeon_unlocked "; break;
                        case ConditionKind::Completed: body = "completed "; break;
                        case ConditionKind::LastDungeon: body = "last_dungeon "; break;
                        case ConditionKind::LastResult: body = "last_result "; break;
                        case ConditionKind::Equipment: body = "equipment "; break;
                        case ConditionKind::Equipped: body = "equipped "; break;
                        case ConditionKind::Chapter: body = "chapter >= 1"; break;
                        case ConditionKind::Material: body = "material "; break;
                        case ConditionKind::Raw: body = {}; break;
                        }
                        changed = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            {
                bool neg = negate;
                if (ImGui::Checkbox("Negate (not)", &neg))
                {
                    negate = neg;
                    changed = true;
                }
            }

            // Helper: rebuild full condition from body + negate.
            auto rebuild = [&](const std::string& newBody)
            {
                body = newBody;
                condition = (negate && !newBody.empty() ? "not " + newBody : newBody);
                changed = true;
            };

            auto firstWordRest = [&]() -> std::string
            {
                // Returns words[1..] joined (the id argument) after the kind token.
                if (words.size() >= 2)
                {
                    std::string rest = body.substr(body.find(words[0]) + words[0].size());
                    return Trim(rest);
                }
                return {};
            };

            switch (kind)
            {
            case ConditionKind::Always:
                if (body != "always") rebuild("always");
                break;
            case ConditionKind::Never:
                if (body != "never") rebuild("never");
                break;
            case ConditionKind::StoryFlag:
            {
                std::string flag = words.size() >= 2 ? firstWordRest() : std::string{};
                if (EditorContentPickers::DrawStoryFlagField("Flag", flag, 256))
                    rebuild("flag " + flag);
                break;
            }
            case ConditionKind::Skill:
            {
                std::string id = words.size() >= 2 ? firstWordRest() : std::string{};
                if (EditorContentPickers::DrawProgressionIdField("Skill", id,
                    EditorContentPickers::ProgressionIdKind::Skill, 256))
                    rebuild("skill " + id);
                break;
            }
            case ConditionKind::Dungeon:
            case ConditionKind::DungeonUnlocked:
            {
                std::string id = words.size() >= 2 ? firstWordRest() : std::string{};
                if (EditorContentPickers::DrawProgressionIdField("Dungeon", id,
                    EditorContentPickers::ProgressionIdKind::Dungeon, 256))
                    rebuild((kind == ConditionKind::Dungeon ? "dungeon " : "dungeon_unlocked ") + id);
                break;
            }
            case ConditionKind::Completed:
            {
                std::string id = words.size() >= 2 ? firstWordRest() : std::string{};
                if (EditorContentPickers::DrawProgressionIdField("Dungeon", id,
                    EditorContentPickers::ProgressionIdKind::Dungeon, 256))
                    rebuild("completed " + id);
                break;
            }
            case ConditionKind::LastDungeon:
            case ConditionKind::LastResult:
            {
                std::string id = words.size() >= 2 ? firstWordRest() : std::string{};
                if (EditorContentPickers::DrawProgressionIdField("Dungeon", id,
                    EditorContentPickers::ProgressionIdKind::Dungeon, 256))
                    rebuild((kind == ConditionKind::LastDungeon ? "last_dungeon " : "last_result ") + id);
                break;
            }
            case ConditionKind::Equipment:
            {
                std::string id = words.size() >= 2 ? firstWordRest() : std::string{};
                if (EditorContentPickers::DrawProgressionIdField("Equipment", id,
                    EditorContentPickers::ProgressionIdKind::Equipment, 256))
                    rebuild("equipment " + id);
                break;
            }
            case ConditionKind::Equipped:
            {
                std::string id = words.size() >= 2 ? firstWordRest() : std::string{};
                if (EditorContentPickers::DrawProgressionIdField("Equipment", id,
                    EditorContentPickers::ProgressionIdKind::Equipment, 256))
                    rebuild("equipped " + id);
                break;
            }
            case ConditionKind::Chapter:
            {
                // grammar: chapter OP N  (OP in ==/!=/>/>=/</<=, N int)
                std::string op = words.size() >= 2 ? words[1] : std::string{">="};
                const char* ops[] = { "==", "!=", ">", ">=", "<", "<=" };
                int opIndex = 2; // default >=
                for (int i = 0; i < IM_ARRAYSIZE(ops); ++i)
                    if (op == ops[i]) { opIndex = i; break; }
                if (ImGui::Combo("Operator", &opIndex, ops, IM_ARRAYSIZE(ops)))
                {
                    op = ops[opIndex];
                    changed = true;
                }
                int n = 1;
                if (words.size() >= 3)
                {
                    try { n = std::stoi(words[2]); } catch (...) { n = 1; }
                }
                if (ImGui::DragInt("Chapter", &n, 1.0f, 0, 999))
                    changed = true;
                if (changed)
                {
                    std::ostringstream s; s << "chapter " << op << " " << n;
                    rebuild(s.str());
                }
                break;
            }
            case ConditionKind::Material:
            {
                std::string id = words.size() >= 2 ? firstWordRest() : std::string{};
                if (EditorContentPickers::DrawProgressionIdField("Material", id,
                    EditorContentPickers::ProgressionIdKind::Material, 256))
                    rebuild("material " + id);

                // Optional quantity comparison: "material <id> OP N" (runtime
                // CompareInt on material amount; bare form means amount > 0).
                std::string op = words.size() >= 3 ? words[2] : std::string{">"};
                const char* ops[] = { ">", ">=", "==", "!=", "<", "<=" };
                int opIndex = 0;
                for (int i = 0; i < IM_ARRAYSIZE(ops); ++i)
                    if (op == ops[i]) { opIndex = i; break; }
                if (ImGui::Combo("Operator", &opIndex, ops, IM_ARRAYSIZE(ops)))
                {
                    op = ops[opIndex];
                    changed = true;
                }
                int amount = 1;
                if (words.size() >= 4)
                {
                    try { amount = std::stoi(words[3]); } catch (...) { amount = 1; }
                }
                if (ImGui::DragInt("Amount", &amount, 1.0f, 0, 9999))
                    changed = true;
                if (changed)
                {
                    std::ostringstream s; s << "material " << id << " " << op << " " << amount;
                    rebuild(s.str());
                }
                EditorWidgets::HelpTooltip("Bare form means amount > 0; add an operator + amount for quantity comparisons.");
                break;
            }
            case ConditionKind::Raw:
                if (EditorWidgets::InputString("Condition", condition, 256))
                    changed = true;
                break;
            }

            return changed;
        }

        static void WriteIndented(std::ostringstream& stream, int indentLevel, const std::string& line)
        {
            for (int i = 0; i < indentLevel; ++i)
                stream << "    ";
            stream << line << "\n";
        }

        static std::string SerializeEventScript(const std::vector<EventScriptBlock>& events)
        {
            std::ostringstream stream;
            stream << "# Generated by Wheatear Event Script Editor.\n\n";

            for (const EventScriptBlock& event : events)
            {
                if (event.Name.empty())
                    continue;

                stream << "event " << event.Name << ":\n";
                int indent = 1;
                for (const EventScriptInstruction& instruction : event.Instructions)
                {
                    switch (instruction.Type)
                    {
                    case EventScriptInstructionType::Command:
                        if (!Trim(instruction.Text).empty())
                            WriteIndented(stream, indent, Trim(instruction.Text));
                        break;
                    case EventScriptInstructionType::Wait:
                        WriteIndented(stream, indent, "wait " + FormatSeconds(instruction.Seconds));
                        break;
                    case EventScriptInstructionType::If:
                        WriteIndented(stream, indent, "if " + Trim(instruction.Text));
                        ++indent;
                        break;
                    case EventScriptInstructionType::EndIf:
                        indent = std::max(1, indent - 1);
                        WriteIndented(stream, indent, "endif");
                        break;
                    }
                }
                stream << "end\n\n";
            }

            return stream.str();
        }

        static std::string UniqueEventName(const std::vector<EventScriptBlock>& events, const std::string& base)
        {
            auto exists = [&](const std::string& name)
            {
                return std::find_if(events.begin(), events.end(), [&](const EventScriptBlock& event)
                {
                    return event.Name == name;
                }) != events.end();
            };

            if (!exists(base))
                return base;

            for (int i = 2; i < 10000; ++i)
            {
                const std::string candidate = base + "_" + std::to_string(i);
                if (!exists(candidate))
                    return candidate;
            }

            return base + "_copy";
        }

        static void SetTextAssetBuffer(EditorUI::TextAssetEditorState& state,
            const std::string& sourcePath,
            const std::string& text,
            bool dirty,
            size_t defaultCapacity)
        {
            constexpr size_t maxEditorCapacity = 2 * 1024 * 1024;
            const size_t targetCapacity = std::min(maxEditorCapacity, std::max(defaultCapacity, text.size() + 4096));
            state.Buffer.assign(std::max<size_t>(targetCapacity, 1), '\0');
            const size_t copyLength = std::min(text.size(), state.Buffer.size() - 1);
            if (copyLength > 0)
                std::memcpy(state.Buffer.data(), text.data(), copyLength);

            state.Document.SetSourcePath(sourcePath);
            state.Document.SetText(text, dirty);
            EditorUI::SyncTextAssetMetadata(state);
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
        const bool sourceChanged = !sourcePath.empty() && sourcePath != m_SourcePath;
        const bool eventChanged = !eventName.empty() && eventName != m_SelectedEvent;
        if (!sourcePath.empty())
            m_SourcePath = sourcePath;
        if (!eventName.empty())
            m_SelectedEvent = eventName;

        m_Open = true;
        if (sourceChanged || eventChanged)
            m_SelectedInstruction = -1;
        SyncSourcePathInput();

        if (sourceChanged || !m_Loaded)
            Load();
        else if (!eventName.empty())
            RefreshScriptFromSource();
    }

    void EventScriptGraphPanel::Load()
    {
        EditorUI::LoadTextAsset(m_SourceEditorState, m_SourcePath, SourceEditorCapacity);
        SyncSourcePathInput();
        RefreshScriptFromSource();
        m_Loaded = true;
    }

    void EventScriptGraphPanel::SyncSourcePathInput()
    {
        CopyToPathInput(m_SourcePathInput, m_SourcePath);
    }

    void EventScriptGraphPanel::RefreshScriptFromSource()
    {
        const size_t textLength = EditorUI::GetTextLength(m_SourceEditorState.Buffer);
        const std::string sourceText(m_SourceEditorState.Buffer.data(), m_SourceEditorState.Buffer.data() + textLength);
        const std::string previousEvent = m_SelectedEvent;

        m_Script = EventScript::FromString(sourceText);
        SyncEditableEventsFromScript();

        if (m_EditableEvents.empty())
        {
            m_SelectedEvent.clear();
            m_SelectedInstruction = -1;
            m_Status = "No events found.";
            return;
        }

        const auto selected = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
            [&](const EventScriptBlock& block) { return block.Name == m_SelectedEvent; });
        if (m_SelectedEvent.empty() || selected == m_EditableEvents.end())
            m_SelectedEvent = m_EditableEvents.front().Name;

        if (m_SelectedEvent != previousEvent)
            m_SelectedInstruction = -1;
        else if (const EventScriptBlock* block = GetSelectedBlock();
                 block && m_SelectedInstruction >= static_cast<int>(block->Instructions.size()))
            m_SelectedInstruction = -1;

        m_Status = "Parsed " + std::to_string(m_EditableEvents.size()) + " event(s).";
    }

    void EventScriptGraphPanel::SyncEditableEventsFromScript()
    {
        m_EditableEvents = m_Script.GetEvents();
    }

    void EventScriptGraphPanel::SyncSourceFromEditableEvents(bool dirty)
    {
        SetTextAssetBuffer(m_SourceEditorState,
            m_SourcePath,
            SerializeEventScript(m_EditableEvents),
            dirty,
            SourceEditorCapacity);

        m_Script = EventScript::FromString(m_SourceEditorState.Document.GetText());
        SyncEditableEventsFromScript();
    }

    bool EventScriptGraphPanel::SaveGraph()
    {
        SyncSourceFromEditableEvents(true);
        if (!EditorUI::SaveTextAsset(m_SourceEditorState, m_SourcePath))
        {
            m_Status = m_SourceEditorState.Status;
            return false;
        }

        RefreshScriptFromSource();
        m_Status = "Saved " + std::to_string(m_EditableEvents.size()) + " event script event(s).";
        EditorCommandBuilder::RefreshAssetChoices();
        return true;
    }

    void EventScriptGraphPanel::AddEvent()
    {
        EventScriptBlock event;
        event.Name = UniqueEventName(m_EditableEvents, "new_event");
        m_EditableEvents.push_back(std::move(event));
        m_SelectedEvent = m_EditableEvents.back().Name;
        m_SelectedInstruction = -1;
        SyncSourceFromEditableEvents(true);
        m_Status = "Added event.";
    }

    void EventScriptGraphPanel::AddInstruction(EventScriptInstructionType type, const std::string& defaultText)
    {
        EventScriptBlock* block = GetSelectedBlockMutable();
        if (!block)
            return;

        EventScriptInstruction instruction;
        instruction.Type = type;
        switch (type)
        {
        case EventScriptInstructionType::Command:
            instruction.Text = defaultText.empty() ? "scene:assets/scenes/VerticalSliceHub.wt" : defaultText;
            break;
        case EventScriptInstructionType::Wait:
            instruction.Seconds = 0.10f;
            break;
        case EventScriptInstructionType::If:
            instruction.Text = defaultText.empty() ? "flag FLAG_HUB_UNLOCKED" : defaultText;
            break;
        case EventScriptInstructionType::EndIf:
            break;
        }

        const int insertAt = m_SelectedInstruction >= 0
            ? std::min(m_SelectedInstruction + 1, static_cast<int>(block->Instructions.size()))
            : static_cast<int>(block->Instructions.size());
        block->Instructions.insert(block->Instructions.begin() + insertAt, std::move(instruction));
        m_SelectedInstruction = insertAt;
        SyncSourceFromEditableEvents(true);
        m_Status = "Added instruction.";
    }

    const EventScriptBlock* EventScriptGraphPanel::GetSelectedBlock() const
    {
        if (m_SelectedEvent.empty())
            return nullptr;

        const auto it = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
            [&](const EventScriptBlock& block) { return block.Name == m_SelectedEvent; });
        return it == m_EditableEvents.end() ? nullptr : &(*it);
    }

    EventScriptBlock* EventScriptGraphPanel::GetSelectedBlockMutable()
    {
        if (m_SelectedEvent.empty())
            return nullptr;

        const auto it = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
            [&](const EventScriptBlock& block) { return block.Name == m_SelectedEvent; });
        return it == m_EditableEvents.end() ? nullptr : &(*it);
    }

    void EventScriptGraphPanel::DrawToolbar()
    {
        ImGui::SetNextItemWidth(340.0f);
        ImGui::InputText("Script Path", m_SourcePathInput.data(), m_SourcePathInput.size());
        const bool pathCommitted = ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SameLine();
        if (ImGui::Button("Load") || pathCommitted)
        {
            m_SourcePath = m_SourcePathInput.data();
            Load();
        }

        const auto& events = m_EditableEvents;
        if (ImGui::BeginCombo("Event", m_SelectedEvent.empty() ? "(none)" : m_SelectedEvent.c_str()))
        {
            for (size_t i = 0; i < events.size(); ++i)
            {
                const auto& event = events[i];
                const bool selected = event.Name == m_SelectedEvent;
                const std::string label = EditorWidgets::LabelWithId(
                    event.Name,
                    "event_script_event:" + std::to_string(i));
                if (ImGui::Selectable(label.c_str(), selected))
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
        if (ImGui::SmallButton("Add Event"))
            AddEvent();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedEvent.empty());
        if (ImGui::SmallButton("Delete Event"))
        {
            const auto it = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
                [&](const EventScriptBlock& event) { return event.Name == m_SelectedEvent; });
            if (it != m_EditableEvents.end())
            {
                m_EditableEvents.erase(it);
                m_SelectedEvent = m_EditableEvents.empty() ? std::string{} : m_EditableEvents.front().Name;
                m_SelectedInstruction = -1;
                SyncSourceFromEditableEvents(true);
                m_Status = "Deleted event.";
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::SmallButton("Save Graph"))
            SaveGraph();
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

    void EventScriptGraphPanel::DrawSourceEditor()
    {
        if (m_SourceEditorState.Buffer.empty())
            EditorUI::ResetBuffer(m_SourceEditorState, SourceEditorCapacity);

        if (ImGui::Button("Reload"))
            Load();

        if (m_SourceEditorState.Dirty)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.24f, 1.0f), "Graph has unsaved changes");
        }

        if (!m_SourceEditorState.Status.empty())
            ImGui::TextDisabled("%s", m_SourceEditorState.Status.c_str());
        if (!m_SourceEditorState.ResolvedPath.empty())
            ImGui::TextDisabled("Resolved: %s", m_SourceEditorState.ResolvedPath.generic_string().c_str());

        EditorWidgets::InlineStatus("Read-only generated source. Use Graph to edit and Save Asset to write the .wts file.",
            EditorWidgets::StatusKind::Info);

        ImGui::Spacing();
        std::string preview = SerializeEventScript(m_EditableEvents);
        EditorGameplayShell::DrawRawPreview(preview, "##EventScriptSourcePreview");
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
            AddInstruction(EventScriptInstructionType::Command, "scene:assets/scenes/VerticalSliceHub.wt");
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
            AddInstruction(EventScriptInstructionType::Command, "progression:set_flag FLAG_HUB_UNLOCKED");
            ImGui::EndChild();
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Dungeon"))
        {
            AddInstruction(EventScriptInstructionType::Command, "progression:set_active_dungeon CH02_MAIN_BearAwakening");
            ImGui::EndChild();
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Save"))
        {
            AddInstruction(EventScriptInstructionType::Command, "gamesave:open");
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
                            instruction.Text = "scene:assets/scenes/VerticalSliceHub.wt";
                        else if (type == EventScriptInstructionType::Wait)
                            instruction.Seconds = 0.10f;
                        else if (type == EventScriptInstructionType::If)
                            instruction.Text = "flag FLAG_HUB_UNLOCKED";
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
            ifInstruction.Text = "flag FLAG_HUB_UNLOCKED";
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

        if (EditorFloatingWindow::Begin("Event Script Editor", &m_Open, 0, { 1220.0f, 760.0f }))
        {
            EditorFloatingWindow::DrawToggleButton("Event Script Editor");
            ImGui::Separator();
            EditorGameplayShell::DrawDocumentStatus({
                EditorGameplayShell::DocumentKind::Asset,
                m_SourceEditorState.Dirty,
                true,
                m_SourcePath,
                m_Status
            });
            ImGui::Separator();
            DrawToolbar();
            ImGui::Separator();
            if (ImGui::BeginTabBar("##EventScriptEditorTabs"))
            {
                if (ImGui::BeginTabItem(EditorLocale::Text("Graph", "图形")))
                {
                    DrawGraph();
                    ImGui::EndTabItem();
                }

                if (EditorGameplayShell::BeginRawPreviewTab(EditorLocale::Text("Advanced Source Preview", "高级源码预览")))
                {
                    DrawSourceEditor();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        EditorFloatingWindow::End();
    }

} // namespace Wheatear
