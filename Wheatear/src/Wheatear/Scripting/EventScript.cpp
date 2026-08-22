#include "wtpch.h"
#include "EventScript.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Wheatear {

    using Wheatear::StringUtils::StartsWith;
    using Wheatear::StringUtils::ToLower;
    using Wheatear::StringUtils::Trim;

    namespace {

        static bool IsBareCommand(const std::string& line)
        {
            return line == "quit"
                || StartsWith(line, "scene:")
                || StartsWith(line, "newgame:")
                || StartsWith(line, "loadgame:")
                || StartsWith(line, "progression:")
                || StartsWith(line, "gamesave:")
                || StartsWith(line, "ui:")
                || StartsWith(line, "event:");
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
                    instruction.Seconds = std::max(0.0f, std::stof(Trim(line.substr(5))));
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

} // namespace Wheatear
