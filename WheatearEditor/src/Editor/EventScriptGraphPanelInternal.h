#pragma once

// Shared file-internal helpers for the event script graph panel, extracted from
// EventScriptGraphPanel.cpp so per-view translation units can be split off.
// Inline so each TU compiles independently.

#include "Editor/CommandBuilder.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Scripting/EventScript.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace Wheatear::EventScriptGraphPanelInternal {


        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;
        static std::string s_PendingOpenEvent;

        constexpr size_t SourceEditorCapacity = 512 * 1024;

        inline static void CopyToPathInput(std::array<char, 512>& buffer, const std::string& value)
        {
            buffer.fill('\0');
            if (buffer.empty())
                return;

            const size_t copyLength = std::min(value.size(), buffer.size() - 1);
            std::copy_n(value.begin(), copyLength, buffer.begin());
        }

        inline static const char* InstructionTypeName(EventScriptInstructionType type)
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

        inline static ImU32 InstructionColor(EventScriptInstructionType type)
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

        inline static std::string InstructionText(const EventScriptInstruction& instruction)
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

        inline static std::string Trim(std::string value)
        {
            const size_t start = value.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                return {};

            const size_t end = value.find_last_not_of(" \t\r\n");
            return value.substr(start, end - start + 1);
        }

        inline static std::string FormatSeconds(float seconds)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2) << std::max(0.0f, seconds);
            return stream.str();
        }

        inline static std::vector<std::string> SplitWords(const std::string& value)
        {
            std::vector<std::string> words;
            std::istringstream stream(value);
            std::string word;
            while (stream >> word)
                words.push_back(word);
            return words;
        }

        inline static std::string ToLower(std::string value)
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

        inline static const char* ConditionKindLabel(ConditionKind k)
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

        inline static ConditionKind ConditionTokenToKind(const std::string& tokenLower)
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

        inline static bool DrawConditionBuilder(std::string& condition)
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
                if (ImGui::Combo(EditorLocale::Text("Operator", "运算符"), &opIndex, ops, IM_ARRAYSIZE(ops)))
                {
                    op = ops[opIndex];
                    changed = true;
                }
                int n = 1;
                if (words.size() >= 3)
                {
                    try { n = std::stoi(words[2]); } catch (...) { n = 1; }
                }
                if (ImGui::DragInt(EditorLocale::Text("Chapter", "章节"), &n, 1.0f, 0, 999))
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
                if (ImGui::Combo(EditorLocale::Text("Operator", "运算符"), &opIndex, ops, IM_ARRAYSIZE(ops)))
                {
                    op = ops[opIndex];
                    changed = true;
                }
                int amount = 1;
                if (words.size() >= 4)
                {
                    try { amount = std::stoi(words[3]); } catch (...) { amount = 1; }
                }
                if (ImGui::DragInt(EditorLocale::Text("Amount", "数量"), &amount, 1.0f, 0, 9999))
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

        inline static void WriteIndented(std::ostringstream& stream, int indentLevel, const std::string& line)
        {
            for (int i = 0; i < indentLevel; ++i)
                stream << "    ";
            stream << line << "\n";
        }

        inline static std::string SerializeEventScript(const std::vector<EventScriptBlock>& events)
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

        inline static std::string UniqueEventName(const std::vector<EventScriptBlock>& events, const std::string& base)
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

        inline static void SetTextAssetBuffer(EditorUI::TextAssetEditorState& state,
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

        inline static std::string Shorten(const std::string& text, size_t maxLength)
        {
            if (text.size() <= maxLength)
                return text;
            if (maxLength <= 3)
                return text.substr(0, maxLength);
            return text.substr(0, maxLength - 3) + "...";
        }

        inline static ImVec2 Add(const ImVec2& a, const ImVec2& b)
        {
            return { a.x + b.x, a.y + b.y };
        }

        inline static ImVec2 Scale(const ImVec2& value, float scale)
        {
            return { value.x * scale, value.y * scale };
        }

        inline static bool PointInRect(const ImVec2& point, const ImVec2& min, const ImVec2& max)
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

        inline static void DrawConnection(ImDrawList* drawList,
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

        inline static void DrawGrid(ImDrawList* drawList,
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


} // namespace Wheatear::EventScriptGraphPanelInternal
