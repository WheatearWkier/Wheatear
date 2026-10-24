#include "wtpch.h"
#include "BehaviorCompiler.h"

#include "BehaviorPrimitiveRegistry.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Utils/StringUtils.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace Wheatear::WAO {

    namespace {

        using Wheatear::StringUtils::ToLower;

        struct PendingLink
        {
            size_t InstructionIndex = 0;
            bool Alternate = false;
            std::string TargetNode;
        };

        static void SetError(std::string* error, const std::string& message)
        {
            if (error)
                *error = message;
        }

        static std::string ReadString(const YAML::Node& node,
            std::initializer_list<const char*> keys,
            const std::string& fallback = {})
        {
            for (const char* key : keys)
            {
                const YAML::Node value = node[key];
                if (value && value.IsScalar())
                    return value.as<std::string>(fallback);
            }
            return fallback;
        }

        static double ReadNumber(const YAML::Node& node,
            std::initializer_list<const char*> keys,
            double fallback = 0.0)
        {
            for (const char* key : keys)
            {
                const YAML::Node value = node[key];
                if (value && value.IsScalar())
                    return value.as<double>(fallback);
            }
            return fallback;
        }

        static BehaviorValue ParseValue(const YAML::Node& node)
        {
            if (!node || !node.IsScalar())
                return {};

            const std::string text = node.as<std::string>("");
            const std::string lower = ToLower(text);
            if (lower == "true" || lower == "yes")
                return BehaviorValue(true);
            if (lower == "false" || lower == "no")
                return BehaviorValue(false);

            char* end = nullptr;
            const double number = std::strtod(text.c_str(), &end);
            if (end && *end == '\0' && end != text.c_str())
                return BehaviorValue(number);
            return BehaviorValue(text);
        }

        static BehaviorArgument ParseArgument(const YAML::Node& node)
        {
            BehaviorArgument argument;
            if (!node || !node.IsScalar())
                return argument;

            const std::string text = node.as<std::string>("");
            if (!text.empty() && text.front() == '$')
            {
                argument.IsLocal = true;
                argument.LocalName = text.substr(1);
                return argument;
            }

            argument.Literal = ParseValue(node);
            return argument;
        }

        static void ParseArguments(const YAML::Node& node,
            std::unordered_map<std::string, BehaviorArgument>* arguments)
        {
            if (!node || !node.IsMap() || !arguments)
                return;

            for (const auto& item : node)
            {
                if (!item.first.IsScalar())
                    continue;
                (*arguments)[item.first.as<std::string>()] = ParseArgument(item.second);
            }
        }

        static void ParseOutputs(const YAML::Node& node,
            std::unordered_map<std::string, std::string>* outputs)
        {
            if (!node || !node.IsMap() || !outputs)
                return;

            for (const auto& item : node)
            {
                if (!item.first.IsScalar() || !item.second.IsScalar())
                    continue;
                (*outputs)[item.first.as<std::string>()] =
                    item.second.as<std::string>("");
            }
        }

        static const BehaviorPrimitivePort* FindPort(
            const std::vector<BehaviorPrimitivePort>& ports,
            const std::string& name)
        {
            const auto it = std::find_if(ports.begin(), ports.end(),
                [&name](const BehaviorPrimitivePort& port)
                {
                    return port.Name == name;
                });
            return it == ports.end() ? nullptr : &(*it);
        }

        static bool ValidatePrimitiveInstruction(
            const BehaviorPrimitiveDefinition& definition,
            const BehaviorInstruction& instruction,
            std::string* error)
        {
            for (const auto& [name, argument] : instruction.Arguments)
            {
                const BehaviorPrimitivePort* port =
                    FindPort(definition.Inputs, name);
                if (!port)
                {
                    SetError(error,
                        "Primitive '" + definition.Id +
                        "' does not declare input '" + name + "'.");
                    return false;
                }

                if (!argument.IsLocal &&
                    port->Type != BehaviorValueType::None &&
                    argument.Literal.Type() != BehaviorValueType::None &&
                    argument.Literal.Type() != port->Type)
                {
                    SetError(error,
                        "Primitive '" + definition.Id +
                        "' input '" + name + "' has an incompatible literal type.");
                    return false;
                }
            }

            for (const BehaviorPrimitivePort& port : definition.Inputs)
            {
                if (port.Required &&
                    instruction.Arguments.find(port.Name) == instruction.Arguments.end())
                {
                    SetError(error,
                        "Primitive '" + definition.Id +
                        "' is missing required input '" + port.Name + "'.");
                    return false;
                }
            }

            for (const auto& [name, localName] : instruction.OutputLocals)
            {
                if (!FindPort(definition.Outputs, name))
                {
                    SetError(error,
                        "Primitive '" + definition.Id +
                        "' does not declare output '" + name + "'.");
                    return false;
                }
                if (localName.empty())
                {
                    SetError(error,
                        "Primitive '" + definition.Id +
                        "' maps output '" + name + "' to an empty local.");
                    return false;
                }
            }

            return true;
        }

        static bool ResolveLinks(BehaviorProgram* program,
            const std::unordered_map<std::string, int>& labels,
            const std::vector<PendingLink>& pending,
            std::string* error)
        {
            for (const PendingLink& link : pending)
            {
                const auto it = labels.find(link.TargetNode);
                if (it == labels.end())
                {
                    SetError(error, "Behavior link targets unknown node '" + link.TargetNode + "'.");
                    return false;
                }

                BehaviorInstruction& instruction = program->Instructions[link.InstructionIndex];
                if (link.Alternate)
                    instruction.AlternateInstruction = it->second;
                else
                    instruction.TargetInstruction = it->second;
            }
            return true;
        }

        static bool ParseBehavior(const YAML::Node& node,
            const std::filesystem::path& sourcePath,
            BehaviorProgram* program,
            std::string* error)
        {
            if (!node || !node.IsMap() || !program)
                return false;

            const std::string id = ReadString(node, { "id", "behaviorId", "name" });
            if (id.empty())
            {
                SetError(error, "Behavior entry has no id in '" + sourcePath.string() + "'.");
                return false;
            }

            const YAML::Node nodes = node["nodes"];
            if (!nodes || !nodes.IsSequence() || nodes.size() == 0)
            {
                SetError(error, "Behavior '" + id + "' has no nodes.");
                return false;
            }

            BehaviorEntry entry;
            entry.Id = id;
            entry.Version = static_cast<uint32_t>(std::max(
                1.0,
                ReadNumber(node, { "version", "behaviorVersion" }, 1.0)));
            entry.FirstInstruction = program->Instructions.size();
            entry.InstructionBegin = entry.FirstInstruction;

            std::unordered_map<std::string, int> labels;
            std::vector<PendingLink> pending;
            std::string firstNode;

            for (const YAML::Node& nodeDefinition : nodes)
            {
                if (!nodeDefinition || !nodeDefinition.IsMap())
                {
                    SetError(error, "Behavior '" + id + "' contains a non-map node.");
                    return false;
                }

                const std::string nodeId = ReadString(nodeDefinition, { "id", "name" });
                if (nodeId.empty())
                {
                    SetError(error, "Behavior '" + id + "' contains a node without id.");
                    return false;
                }
                if (!labels.emplace(nodeId, static_cast<int>(program->Instructions.size())).second)
                {
                    SetError(error, "Behavior '" + id + "' contains duplicate node '" + nodeId + "'.");
                    return false;
                }
                if (firstNode.empty())
                    firstNode = nodeId;

                BehaviorInstruction instruction;
                instruction.DebugNodeId = nodeId;

                if (nodeDefinition["call"])
                {
                    instruction.Opcode = BehaviorOpcode::CallPrimitive;
                    instruction.PrimitiveId = nodeDefinition["call"].as<std::string>("");
                    if (instruction.PrimitiveId.empty())
                    {
                        SetError(error, "Behavior node '" + nodeId + "' has an empty primitive id.");
                        return false;
                    }
                    const BehaviorPrimitiveDefinition* definition =
                        BehaviorPrimitiveRegistry::Find(instruction.PrimitiveId);
                    if (!definition)
                    {
                        SetError(error, "Behavior node '" + nodeId + "' references unknown primitive '" +
                            instruction.PrimitiveId + "'.");
                        return false;
                    }
                    instruction.PrimitiveVersion = static_cast<uint32_t>(std::max(
                        0.0,
                        ReadNumber(nodeDefinition, { "primitiveVersion", "primitive_version" }, 0.0)));
                    if (instruction.PrimitiveVersion > 0 &&
                        instruction.PrimitiveVersion != definition->Version)
                    {
                        SetError(error,
                            "Behavior node '" + nodeId + "' requires primitive '" +
                            instruction.PrimitiveId + "' version " +
                            std::to_string(instruction.PrimitiveVersion) +
                            ", loaded version " +
                            std::to_string(definition->Version) + ".");
                        return false;
                    }
                    instruction.PrimitiveVersion = definition->Version;
                    ParseArguments(nodeDefinition["inputs"], &instruction.Arguments);
                    ParseOutputs(nodeDefinition["outputs"], &instruction.OutputLocals);
                    if (!ValidatePrimitiveInstruction(*definition, instruction, error))
                        return false;
                    const std::string next = ReadString(nodeDefinition, { "next" });
                    if (!next.empty())
                        pending.push_back({ program->Instructions.size(), false, next });
                }
                else if (nodeDefinition["branch"])
                {
                    instruction.Opcode = BehaviorOpcode::BranchIfFalse;
                    instruction.LocalName = nodeDefinition["branch"].as<std::string>("");
                    if (!instruction.LocalName.empty() && instruction.LocalName.front() == '$')
                        instruction.LocalName.erase(instruction.LocalName.begin());
                    if (instruction.LocalName.empty())
                    {
                        SetError(error, "Behavior node '" + nodeId + "' has an empty branch local.");
                        return false;
                    }
                    const std::string onTrue =
                        ReadString(nodeDefinition, { "true", "then", "next" });
                    const std::string onFalse =
                        ReadString(nodeDefinition, { "false", "else", "onFalse" });
                    if (!onTrue.empty())
                        pending.push_back({ program->Instructions.size(), false, onTrue });
                    if (!onFalse.empty())
                        pending.push_back({ program->Instructions.size(), true, onFalse });
                }
                else if (nodeDefinition["jump"])
                {
                    instruction.Opcode = BehaviorOpcode::Jump;
                    const std::string target = nodeDefinition["jump"].as<std::string>("");
                    if (!target.empty())
                        pending.push_back({ program->Instructions.size(), false, target });
                }
                else if (nodeDefinition["wait"])
                {
                    instruction.Opcode = BehaviorOpcode::WaitSeconds;
                    instruction.WaitSeconds = static_cast<float>(std::max(
                        0.0,
                        nodeDefinition["wait"].as<double>(0.0)));
                    const std::string next = ReadString(nodeDefinition, { "next" });
                    if (!next.empty())
                        pending.push_back({ program->Instructions.size(), false, next });
                }
                else if (nodeDefinition["waitEvent"])
                {
                    instruction.Opcode = BehaviorOpcode::WaitEvent;
                    instruction.EventId = nodeDefinition["waitEvent"].as<std::string>("");
                    if (instruction.EventId.empty())
                    {
                        SetError(error, "Behavior node '" + nodeId + "' has an empty event id.");
                        return false;
                    }
                    instruction.WaitSeconds = static_cast<float>(std::max(
                        0.0,
                        ReadNumber(nodeDefinition, { "timeout" }, 0.0)));
                    const std::string next = ReadString(nodeDefinition, { "next" });
                    const std::string onTimeout =
                        ReadString(nodeDefinition, { "onTimeout", "timeoutNode" });
                    if (!next.empty())
                        pending.push_back({ program->Instructions.size(), false, next });
                    if (!onTimeout.empty())
                        pending.push_back({ program->Instructions.size(), true, onTimeout });
                }
                else if (nodeDefinition["set"])
                {
                    instruction.Opcode = BehaviorOpcode::SetLocal;
                    instruction.LocalName = nodeDefinition["set"].as<std::string>("");
                    if (!instruction.LocalName.empty() && instruction.LocalName.front() == '$')
                        instruction.LocalName.erase(instruction.LocalName.begin());
                    instruction.LocalValue = ParseArgument(nodeDefinition["value"]);
                    if (instruction.LocalName.empty())
                    {
                        SetError(error, "Behavior node '" + nodeId + "' has an empty local name.");
                        return false;
                    }
                    const std::string next = ReadString(nodeDefinition, { "next" });
                    if (!next.empty())
                        pending.push_back({ program->Instructions.size(), false, next });
                }
                else if (nodeDefinition["end"] || ToLower(ReadString(nodeDefinition, { "op" })) == "end")
                {
                    instruction.Opcode = BehaviorOpcode::End;
                }
                else
                {
                    SetError(error, "Behavior node '" + nodeId + "' has no supported operation.");
                    return false;
                }

                program->Instructions.push_back(std::move(instruction));
            }

            const std::string entryNode = ReadString(node, { "entry", "start" }, firstNode);
            const auto entryIt = labels.find(entryNode);
            if (entryIt == labels.end())
            {
                SetError(error, "Behavior '" + id + "' entry node '" + entryNode + "' was not found.");
                return false;
            }
            entry.FirstInstruction = static_cast<size_t>(entryIt->second);

            if (!ResolveLinks(program, labels, pending, error))
                return false;

            for (const BehaviorEntry& existing : program->Entries)
            {
                if (existing.Id == entry.Id)
                {
                    SetError(error, "Duplicate behavior id '" + entry.Id
                        + "' in '" + sourcePath.string() + "'.");
                    return false;
                }
            }

            program->Entries.push_back(std::move(entry));
            return true;
        }

    } // namespace

    bool BehaviorCompiler::Compile(const YAML::Node& root,
        const std::filesystem::path& sourcePath,
        BehaviorProgram* program,
        std::string* error)
    {
        if (!program)
            return false;

        *program = {};
        program->SourcePath = sourcePath.generic_string();
        program->ModuleId = ReadString(root, { "module", "id" }, sourcePath.stem().string());
        program->SchemaVersion = static_cast<uint32_t>(std::max(
            1.0,
            ReadNumber(root, { "version", "schemaVersion" }, 1.0)));
        constexpr uint32_t kCurrentSchemaVersion = 1;
        if (program->SchemaVersion > kCurrentSchemaVersion)
        {
            SetError(error,
                "Behavior file '" + sourcePath.string() +
                "' requires unsupported schema version " +
                std::to_string(program->SchemaVersion) +
                " (runtime supports " +
                std::to_string(kCurrentSchemaVersion) + ").");
            *program = {};
            return false;
        }

        const YAML::Node behaviors = root["behaviors"];
        if (!behaviors || !behaviors.IsSequence())
        {
            SetError(error, "Behavior file '" + sourcePath.string() + "' has no behaviors sequence.");
            return false;
        }

        for (const YAML::Node& behavior : behaviors)
        {
            if (!ParseBehavior(behavior, sourcePath, program, error))
            {
                *program = {};
                return false;
            }
        }

        if (program->Entries.empty())
        {
            SetError(error, "Behavior file '" + sourcePath.string() + "' contains no entries.");
            return false;
        }

        return true;
    }

} // namespace Wheatear::WAO
