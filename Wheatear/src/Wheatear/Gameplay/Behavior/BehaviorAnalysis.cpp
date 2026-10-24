#include "wtpch.h"
#include "BehaviorAnalysis.h"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear::WAO {

    namespace {

        struct EntryRange
        {
            const BehaviorEntry* Entry = nullptr;
            size_t Begin = 0;
            size_t End = 0;
        };

        std::vector<int> Successors(const BehaviorInstruction& instruction)
        {
            std::vector<int> result;
            switch (instruction.Opcode)
            {
            case BehaviorOpcode::End:
                break;
            case BehaviorOpcode::BranchIfFalse:
            case BehaviorOpcode::WaitEvent:
                result.push_back(instruction.TargetInstruction);
                result.push_back(instruction.AlternateInstruction);
                break;
            default:
                result.push_back(instruction.TargetInstruction);
                break;
            }
            return result;
        }

        bool IsSuccessorInRange(int index, size_t begin, size_t end)
        {
            return index >= static_cast<int>(begin) && index < static_cast<int>(end);
        }

    } // namespace

    std::vector<BehaviorAnalysisIssue> AnalyzeProgram(
        const BehaviorProgram& program)
    {
        std::vector<BehaviorAnalysisIssue> issues;
        std::vector<EntryRange> ranges;
        ranges.reserve(program.Entries.size());
        for (const BehaviorEntry& entry : program.Entries)
            ranges.push_back({ &entry, entry.InstructionBegin, program.Instructions.size() });

        std::sort(ranges.begin(), ranges.end(),
            [](const EntryRange& left, const EntryRange& right)
            {
                return left.Begin < right.Begin;
            });
        for (size_t i = 0; i + 1 < ranges.size(); ++i)
            ranges[i].End = ranges[i + 1].Begin;

        for (const EntryRange& range : ranges)
        {
            if (!range.Entry || range.Begin >= range.End)
                continue;
            if (range.Entry->FirstInstruction < range.Begin
                || range.Entry->FirstInstruction >= range.End)
                continue;

            const auto report = [&](bool error, size_t index, std::string message)
            {
                BehaviorAnalysisIssue issue;
                issue.Error = error;
                issue.BehaviorId = range.Entry->Id;
                issue.EntryId = range.Entry->Id;
                issue.NodeId = index < program.Instructions.size()
                    ? program.Instructions[index].DebugNodeId
                    : std::string();
                issue.Message = std::move(message);
                issues.push_back(std::move(issue));
            };

            // Reachability from the entry instruction.
            std::vector<bool> reachable(range.End - range.Begin, false);
            std::vector<size_t> stack = { range.Entry->FirstInstruction };
            reachable[range.Entry->FirstInstruction - range.Begin] = true;
            while (!stack.empty())
            {
                const size_t index = stack.back();
                stack.pop_back();
                for (int successor : Successors(program.Instructions[index]))
                {
                    if (!IsSuccessorInRange(successor, range.Begin, range.End))
                        continue;
                    const size_t offset = static_cast<size_t>(successor) - range.Begin;
                    if (!reachable[offset])
                    {
                        reachable[offset] = true;
                        stack.push_back(static_cast<size_t>(successor));
                    }
                }
            }

            for (size_t index = range.Begin; index < range.End; ++index)
            {
                if (!reachable[index - range.Begin])
                {
                    report(false, index, "unreachable node '"
                        + program.Instructions[index].DebugNodeId
                        + "' (no control-flow path from the entry)");
                }
            }

            // Cycle detection over the reachable control-flow graph.
            std::vector<int> color(range.End - range.Begin, 0);
            std::vector<size_t> path;
            const auto visit = [&](const auto& self, size_t index) -> void
            {
                const size_t offset = index - range.Begin;
                color[offset] = 1;
                path.push_back(index);
                for (int successor : Successors(program.Instructions[index]))
                {
                    if (!IsSuccessorInRange(successor, range.Begin, range.End))
                        continue;
                    const size_t nextOffset = static_cast<size_t>(successor) - range.Begin;
                    if (color[nextOffset] == 1)
                    {
                        bool hasSuspension = false;
                        const auto cycleStart = std::find(path.begin(), path.end(),
                            static_cast<size_t>(successor));
                        for (auto it = cycleStart; it != path.end(); ++it)
                        {
                            const BehaviorOpcode opcode =
                                program.Instructions[*it].Opcode;
                            if (opcode == BehaviorOpcode::WaitSeconds
                                || opcode == BehaviorOpcode::WaitEvent)
                            {
                                hasSuspension = true;
                                break;
                            }
                        }
                        report(!hasSuspension, static_cast<size_t>(successor),
                            hasSuspension
                                ? "control-flow cycle detected; it includes a wait/event suspension"
                                : std::string("control-flow cycle has no wait/event suspension; ")
                                    + "the VM will hit its per-tick instruction limit");
                        continue;
                    }
                    if (color[nextOffset] == 0)
                        self(self, static_cast<size_t>(successor));
                }
                path.pop_back();
                color[offset] = 2;
            };
            visit(visit, range.Entry->FirstInstruction);

            // Branch locals must be produced before the branch, and each local
            // should have a single obvious writer.
            std::unordered_map<std::string, std::vector<size_t>> producers;
            for (size_t index = range.Begin; index < range.End; ++index)
            {
                const BehaviorInstruction& instruction = program.Instructions[index];
                if (instruction.Opcode == BehaviorOpcode::SetLocal
                    && !instruction.LocalName.empty())
                {
                    producers[instruction.LocalName].push_back(index);
                }
                for (const auto& [outputName, localName] : instruction.OutputLocals)
                {
                    (void)outputName;
                    if (!localName.empty())
                        producers[localName].push_back(index);
                }

                if (instruction.Opcode == BehaviorOpcode::BranchIfFalse
                    && (instruction.TargetInstruction < 0
                        || instruction.AlternateInstruction < 0))
                {
                    report(false, index,
                        "branch node is missing a true or false target; the missing side ends the behavior");
                }
                if (instruction.Opcode == BehaviorOpcode::WaitEvent
                    && instruction.TargetInstruction < 0
                    && instruction.AlternateInstruction < 0)
                {
                    report(false, index,
                        "waitEvent node has neither a next node nor a timeout target");
                }
            }

            for (size_t index = range.Begin; index < range.End; ++index)
            {
                const BehaviorInstruction& instruction = program.Instructions[index];
                if (instruction.Opcode == BehaviorOpcode::BranchIfFalse
                    && !instruction.LocalName.empty()
                    && instruction.LocalName.rfind("event.", 0) != 0)
                {
                    const auto producer = producers.find(instruction.LocalName);
                    const bool hasEarlierProducer = producer != producers.end()
                        && std::any_of(producer->second.begin(), producer->second.end(),
                            [index](size_t writer) { return writer < index; });
                    if (!hasEarlierProducer)
                    {
                        report(false, index, "branch local '$" + instruction.LocalName
                            + "' has no producer before this node");
                    }
                }
            }

            for (const auto& [local, writers] : producers)
            {
                if (writers.size() > 1)
                {
                    report(false, writers.front(), "local '$" + local
                        + "' is written by " + std::to_string(writers.size())
                        + " nodes; the last executor wins");
                }
            }
        }

        std::sort(issues.begin(), issues.end(),
            [](const BehaviorAnalysisIssue& left, const BehaviorAnalysisIssue& right)
            {
                if (left.BehaviorId != right.BehaviorId)
                    return left.BehaviorId < right.BehaviorId;
                if (left.NodeId != right.NodeId)
                    return left.NodeId < right.NodeId;
                return left.Message < right.Message;
            });
        return issues;
    }

} // namespace Wheatear::WAO
