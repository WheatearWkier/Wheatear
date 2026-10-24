#include "wtpch.h"
#include "BehaviorPrimitiveRegistry.h"

#include "Wheatear/Core/Log.h"

#include <algorithm>
#include <utility>

namespace Wheatear::WAO {

    const BehaviorValue* BehaviorPrimitiveContext::FindInput(const std::string& name) const
    {
        if (!Inputs)
            return nullptr;
        const auto it = Inputs->find(name);
        return it == Inputs->end() ? nullptr : &it->second;
    }

    BehaviorValue BehaviorPrimitiveContext::Input(const std::string& name,
        const BehaviorValue& fallback) const
    {
        const BehaviorValue* value = FindInput(name);
        return value ? *value : fallback;
    }

    void BehaviorPrimitiveContext::SetOutput(const std::string& name,
        const BehaviorValue& value)
    {
        if (Outputs)
            (*Outputs)[name] = value;
    }

    void BehaviorPrimitiveContext::Fail(const std::string& reason)
    {
        FailureReason = reason;
    }

    namespace {

        std::unordered_map<std::string, BehaviorPrimitiveDefinition>& Registry()
        {
            static std::unordered_map<std::string, BehaviorPrimitiveDefinition> registry;
            return registry;
        }

    } // namespace

    void BehaviorPrimitiveRegistry::Register(BehaviorPrimitiveDefinition definition)
    {
        if (definition.Id.empty() || !definition.Callback)
            return;
        Registry()[definition.Id] = std::move(definition);
    }

    void BehaviorPrimitiveRegistry::Clear()
    {
        Registry().clear();
    }

    bool BehaviorPrimitiveRegistry::Has(const std::string& id)
    {
        return !id.empty() && Registry().find(id) != Registry().end();
    }

    const BehaviorPrimitiveDefinition* BehaviorPrimitiveRegistry::Find(const std::string& id)
    {
        const auto it = Registry().find(id);
        return it == Registry().end() ? nullptr : &it->second;
    }

    std::vector<BehaviorPrimitiveDefinition> BehaviorPrimitiveRegistry::All()
    {
        std::vector<BehaviorPrimitiveDefinition> definitions;
        definitions.reserve(Registry().size());
        for (const auto& [id, definition] : Registry())
            definitions.push_back(definition);
        std::sort(definitions.begin(), definitions.end(),
            [](const BehaviorPrimitiveDefinition& left, const BehaviorPrimitiveDefinition& right)
            {
                return left.Id < right.Id;
            });
        return definitions;
    }

    bool BehaviorPrimitiveRegistry::Execute(const std::string& id,
        BehaviorPrimitiveContext& context)
    {
        const BehaviorPrimitiveDefinition* definition = Find(id);
        if (!definition)
        {
            context.Fail("Unknown behavior primitive: " + id);
            WT_CORE_WARN("Behavior VM: unknown primitive '{}'.", id);
            return false;
        }

        for (const BehaviorPrimitivePort& port : definition->Inputs)
        {
            const BehaviorValue* input = context.FindInput(port.Name);
            if (port.Required && !input)
            {
                context.Fail("Primitive '" + id + "' is missing input '" +
                    port.Name + "'.");
                return false;
            }
            if (!input ||
                port.Type == BehaviorValueType::None ||
                input->Type() == BehaviorValueType::None)
            {
                continue;
            }
            if (input->Type() != port.Type)
            {
                context.Fail("Primitive '" + id + "' input '" + port.Name +
                    "' has type " + std::to_string(static_cast<int>(input->Type())) +
                    ", expected " + std::to_string(static_cast<int>(port.Type)) + ".");
                return false;
            }
        }

        if (!definition->Callback(context))
        {
            if (context.FailureReason.empty())
                context.Fail("Primitive declined execution: " + id);
            return false;
        }

        if (context.Outputs)
        {
            for (const auto& [name, value] : *context.Outputs)
            {
                const auto outputIt = std::find_if(
                    definition->Outputs.begin(),
                    definition->Outputs.end(),
                    [&name](const BehaviorPrimitivePort& port)
                    {
                        return port.Name == name;
                    });
                if (outputIt == definition->Outputs.end())
                {
                    context.Fail("Primitive '" + id + "' wrote undeclared output '" +
                        name + "'.");
                    return false;
                }
                if (outputIt->Type != BehaviorValueType::None &&
                    value.Type() != BehaviorValueType::None &&
                    value.Type() != outputIt->Type)
                {
                    context.Fail("Primitive '" + id + "' output '" + name +
                        "' has an incompatible type.");
                    return false;
                }
            }
        }
        return true;
    }

} // namespace Wheatear::WAO
