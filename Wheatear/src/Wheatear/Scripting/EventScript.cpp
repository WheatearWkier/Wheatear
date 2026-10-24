#include "wtpch.h"
#include "EventScript.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Logic/LogicCommandRegistry.h"
#include "Wheatear/Logic/LogicExpression.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace Wheatear {

    using Wheatear::StringUtils::StartsWith;
    using Wheatear::StringUtils::ToLower;
    using Wheatear::StringUtils::Trim;

    namespace {

        static bool IsBareCommand(const std::string& line)
        {
            if (line == "quit")
                return true;

            // Any "prefix:payload" line is treated as a command so a typo
            // reaches validation and the editor instead of becoming a silently
            // ignored RawLine. .wts has no dialogue syntax that would collide.
            const size_t colon = line.find(':');
            if (colon == std::string::npos || colon == 0)
                return false;
            for (size_t i = 0; i < colon; ++i)
            {
                if (std::isspace(static_cast<unsigned char>(line[i])))
                    return false;
            }
            return true;
        }

        static std::string StripEventName(std::string payload)
        {
            payload = Trim(std::move(payload));
            if (!payload.empty() && payload.back() == ':')
                payload.pop_back();
            return Trim(payload);
        }

    } // namespace

    EventScript EventScript::FromFile(const std::filesystem::path& filepath)
    {
        EventScript script;
        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(filepath);
        script.m_SourcePath = resolved.generic_string();

        std::ifstream input(resolved, std::ios::binary);
        if (!input)
        {
            WT_CORE_WARN("EventScript: cannot open '{}'", resolved.string());
            return script;
        }

        std::stringstream buffer;
        buffer << input.rdbuf();
        script.Parse(buffer.str());
        return script;
    }

    EventScript EventScript::FromString(const std::string& text)
    {
        EventScript script;
        script.Parse(text);
        return script;
    }

    const EventScriptBlock* EventScript::FindEvent(const std::string& name) const
    {
        const auto it = std::find_if(m_Events.begin(), m_Events.end(),
            [&](const EventScriptBlock& block) { return block.Name == name; });
        return it == m_Events.end() ? nullptr : &(*it);
    }

    void EventScript::Parse(const std::string& text)
    {
        m_Events.clear();

        std::istringstream stream(text);
        std::string rawLine;
        EventScriptBlock* current = nullptr;
        int lineNumber = 0;
        // Comment lines ('#' / '//') pending attachment to the next block or
        // instruction; comments before the first event become the header.
        std::vector<std::string> pendingComments;

        auto attachPending = [&pendingComments](EventScriptBlock& block)
        {
            if (!pendingComments.empty())
            {
                block.LeadingComments = pendingComments;
                pendingComments.clear();
            }
        };
        auto attachPendingInstruction = [&pendingComments](EventScriptInstruction& instruction)
        {
            if (!pendingComments.empty())
            {
                instruction.LeadingComments = pendingComments;
                pendingComments.clear();
            }
        };

        while (std::getline(stream, rawLine))
        {
            ++lineNumber;
            std::string line = Trim(rawLine);
            if (lineNumber == 1 && StartsWith(line, "\xEF\xBB\xBF"))
                line = Trim(line.substr(3));
            if (line.empty())
                continue;
            if (StartsWith(line, "#") || StartsWith(line, "//"))
            {
                pendingComments.push_back(line);
                continue;
            }

            const std::string lower = ToLower(line);
            if (!current && StartsWith(lower, "event "))
            {
                const std::string eventName = StripEventName(line.substr(6));
                if (eventName.empty())
                {
                    WT_CORE_WARN("EventScript: empty event name at line {}", lineNumber);
                    current = nullptr;
                    pendingComments.clear();
                    continue;
                }

                m_Events.push_back({ eventName, lineNumber, {}, {} });
                current = &m_Events.back();
                if (m_Events.size() == 1)
                {
                    // Comments above the very first event are the file header.
                    m_HeaderComments = pendingComments;
                    pendingComments.clear();
                }
                else
                {
                    attachPending(*current);
                }
                continue;
            }

            if (!current && StartsWith(lower, "event:"))
            {
                const std::string eventName = StripEventName(line.substr(6));
                if (eventName.empty())
                {
                    WT_CORE_WARN("EventScript: empty event name at line {}", lineNumber);
                    current = nullptr;
                    pendingComments.clear();
                    continue;
                }

                m_Events.push_back({ eventName, lineNumber, {}, {} });
                current = &m_Events.back();
                if (m_Events.size() == 1)
                {
                    m_HeaderComments = pendingComments;
                    pendingComments.clear();
                }
                else
                {
                    attachPending(*current);
                }
                continue;
            }

            if (!current)
            {
                // Instruction outside any event: preserve the line verbatim
                // instead of dropping it, so editor round-trips never destroy
                // hand-written content. Comments stay pending (they may belong
                // to the next event or block).
                EventScriptInstruction orphan;
                orphan.Type = EventScriptInstructionType::RawLine;
                orphan.Text = line;
                orphan.SourceLine = lineNumber;
                m_OrphanLines.push_back(std::move(orphan));
                WT_CORE_WARN("EventScript: instruction outside event at line {}", lineNumber);
                continue;
            }

            if (lower == "end" || lower == "endevent")
            {
                // Comments after "end" belong to the next event, not to the
                // last instruction of the closed block.
                pendingComments.clear();
                current = nullptr;
                continue;
            }

            EventScriptInstruction instruction;
            instruction.SourceLine = lineNumber;
            attachPendingInstruction(instruction);

            if (StartsWith(lower, "command ") || StartsWith(lower, "cmd "))
            {
                const size_t offset = StartsWith(lower, "command ") ? 8 : 4;
                instruction.Type = EventScriptInstructionType::Command;
                instruction.Text = Trim(line.substr(offset));
            }
            else if (IsBareCommand(line))
            {
                instruction.Type = EventScriptInstructionType::Command;
                instruction.Text = line;
            }
            else if (StartsWith(lower, "wait "))
            {
                instruction.Type = EventScriptInstructionType::Wait;
                try
                {
                    const std::string payload = Trim(line.substr(5));
                    size_t consumed = 0;
                    const float seconds = std::stof(payload, &consumed);
                    if (consumed != payload.size())
                        throw std::invalid_argument("trailing characters");
                    if (!std::isfinite(seconds))
                        throw std::invalid_argument("wait must be finite");
                    instruction.Seconds = std::max(0.0f, seconds);
                }
                catch (...)
                {
                    WT_CORE_WARN("EventScript: invalid wait value at line {}", lineNumber);
                    instruction.Seconds = 0.0f;
                }
            }
            else if (StartsWith(lower, "if "))
            {
                instruction.Type = EventScriptInstructionType::If;
                instruction.Text = Trim(line.substr(3));
            }
            else if (lower == "endif" || lower == "end if")
            {
                instruction.Type = EventScriptInstructionType::EndIf;
            }
            else
            {
                // Unknown instruction: keep the line verbatim as RawLine so
                // the editor round-trip is lossless; the executor ignores it.
                WT_CORE_WARN("EventScript: unknown instruction '{}' at line {}", line, lineNumber);
                instruction.Type = EventScriptInstructionType::RawLine;
                instruction.Text = line;
            }

            current->Instructions.push_back(std::move(instruction));
        }
    }

    size_t EventScript::GetUnrecognizedLineCount() const
    {
        size_t count = m_OrphanLines.size();
        for (const EventScriptBlock& block : m_Events)
        {
            for (const EventScriptInstruction& instruction : block.Instructions)
            {
                if (instruction.Type == EventScriptInstructionType::RawLine)
                    ++count;
            }
        }
        return count;
    }

    std::vector<EventScriptIssue> EventScript::Validate() const
    {
        std::vector<EventScriptIssue> issues;
        const auto add = [&issues](int line, bool error, std::string message)
        {
            issues.push_back({ line, error, std::move(message) });
        };

        std::unordered_set<std::string> eventNames;
        for (const EventScriptBlock& block : m_Events)
        {
            if (!eventNames.insert(block.Name).second)
            {
                add(block.SourceLine, true,
                    "duplicate event '" + block.Name + "' (only the first is reachable)");
            }

            std::vector<int> openIfLines;
            for (const EventScriptInstruction& instruction : block.Instructions)
            {
                switch (instruction.Type)
                {
                case EventScriptInstructionType::Command:
                {
                    const std::string text = Trim(instruction.Text);
                    if (text.empty())
                    {
                        add(instruction.SourceLine, true, "empty command");
                        break;
                    }

                    std::string error;
                    if (!Logic::CommandRegistry::Validate(text, &error))
                        add(instruction.SourceLine, true, error);

                    if (text.rfind("event:", 0) == 0)
                    {
                        std::string payload = Trim(text.substr(6));
                        if (!payload.empty() && payload.front() == '@')
                        {
                            const size_t separator = payload.find(':');
                            payload = separator == std::string::npos
                                ? std::string()
                                : Trim(payload.substr(separator + 1));
                        }
                        if (payload.empty())
                            add(instruction.SourceLine, true,
                                "event command requires a non-empty event name");
                    }
                    break;
                }
                case EventScriptInstructionType::Wait:
                    if (!std::isfinite(instruction.Seconds)
                        || instruction.Seconds < 0.0f)
                        add(instruction.SourceLine, true,
                            "wait duration must be a finite non-negative number");
                    else if (instruction.Seconds > 3600.0f)
                        add(instruction.SourceLine, false,
                            "wait duration is longer than one hour");
                    break;
                case EventScriptInstructionType::If:
                {
                    const std::string condition = Trim(instruction.Text);
                    if (condition.empty())
                    {
                        add(instruction.SourceLine, true, "if requires a condition");
                    }
                    else
                    {
                        std::string error;
                        if (!Logic::Validate(
                            Logic::NormalizeCondition(condition), &error))
                            add(instruction.SourceLine, true,
                                "invalid condition '" + condition + "': " + error);
                    }
                    openIfLines.push_back(instruction.SourceLine);
                    break;
                }
                case EventScriptInstructionType::EndIf:
                    if (openIfLines.empty())
                        add(instruction.SourceLine, true, "unmatched endif");
                    else
                        openIfLines.pop_back();
                    break;
                case EventScriptInstructionType::RawLine:
                    add(instruction.SourceLine, false,
                        "unrecognized line will be skipped: " + Trim(instruction.Text));
                    break;
                }
            }

            if (!openIfLines.empty())
            {
                add(openIfLines.back(), true,
                    "if is missing a matching endif before end of event '"
                        + block.Name + "'");
            }
        }

        for (const EventScriptInstruction& orphan : m_OrphanLines)
        {
            add(orphan.SourceLine, false,
                "line outside any event will not run: " + Trim(orphan.Text));
        }

        return issues;
    }

} // namespace Wheatear
