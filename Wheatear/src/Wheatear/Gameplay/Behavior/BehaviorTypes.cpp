#include "wtpch.h"
#include "BehaviorTypes.h"

#include <cmath>
#include <sstream>

namespace Wheatear::WAO {

    BehaviorValueType BehaviorValue::Type() const
    {
        if (std::holds_alternative<bool>(Value))
            return BehaviorValueType::Bool;
        if (std::holds_alternative<double>(Value))
            return BehaviorValueType::Number;
        if (std::holds_alternative<std::string>(Value))
            return BehaviorValueType::String;
        if (std::holds_alternative<UUID>(Value))
            return BehaviorValueType::UUID;
        return BehaviorValueType::None;
    }

    bool BehaviorValue::IsTruthy(bool fallback) const
    {
        if (const auto* value = std::get_if<bool>(&Value))
            return *value;
        if (const auto* value = std::get_if<double>(&Value))
            return std::abs(*value) > 0.000001;
        if (const auto* value = std::get_if<std::string>(&Value))
            return !value->empty();
        if (const auto* value = std::get_if<UUID>(&Value))
            return static_cast<uint64_t>(*value) != 0;
        return fallback;
    }

    double BehaviorValue::AsNumber(double fallback) const
    {
        if (const auto* value = std::get_if<double>(&Value))
            return *value;
        if (const auto* value = std::get_if<bool>(&Value))
            return *value ? 1.0 : 0.0;
        if (const auto* value = std::get_if<UUID>(&Value))
            return static_cast<double>(static_cast<uint64_t>(*value));
        if (const auto* value = std::get_if<std::string>(&Value))
        {
            try
            {
                size_t parsed = 0;
                const double result = std::stod(*value, &parsed);
                return parsed == value->size() ? result : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }
        return fallback;
    }

    bool BehaviorValue::AsBool(bool fallback) const
    {
        if (const auto* value = std::get_if<bool>(&Value))
            return *value;
        if (Type() == BehaviorValueType::Number)
            return AsNumber() != 0.0;
        if (Type() == BehaviorValueType::String)
        {
            const std::string text = AsString();
            if (text == "true" || text == "1" || text == "yes")
                return true;
            if (text == "false" || text == "0" || text == "no")
                return false;
        }
        return fallback;
    }

    std::string BehaviorValue::AsString(const std::string& fallback) const
    {
        if (const auto* value = std::get_if<std::string>(&Value))
            return *value;
        if (const auto* value = std::get_if<bool>(&Value))
            return *value ? "true" : "false";
        if (const auto* value = std::get_if<double>(&Value))
        {
            std::ostringstream stream;
            stream << *value;
            return stream.str();
        }
        if (const auto* value = std::get_if<UUID>(&Value))
            return std::to_string(static_cast<uint64_t>(*value));
        return fallback;
    }

    UUID BehaviorValue::AsUUID(UUID fallback) const
    {
        if (const auto* value = std::get_if<UUID>(&Value))
            return *value;
        if (const auto* value = std::get_if<double>(&Value))
            return UUID(static_cast<uint64_t>(std::max(0.0, *value)));
        if (const auto* value = std::get_if<std::string>(&Value))
        {
            try
            {
                size_t parsed = 0;
                const uint64_t result = std::stoull(*value, &parsed);
                return parsed == value->size() ? UUID(result) : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }
        return fallback;
    }

    const BehaviorEntry* BehaviorProgram::FindEntry(const std::string& id) const
    {
        for (const BehaviorEntry& entry : Entries)
        {
            if (entry.Id == id)
                return &entry;
        }
        return nullptr;
    }

} // namespace Wheatear::WAO
