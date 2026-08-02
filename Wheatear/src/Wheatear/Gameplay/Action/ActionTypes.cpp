#include "wtpch.h"
#include "ActionTypes.h"

namespace Wheatear::WAO {

    float AttributeStore::Get(const std::string& id, float fallback) const
    {
        const auto it = Values.find(id);
        return it == Values.end() ? fallback : it->second;
    }

    void AttributeStore::Set(const std::string& id, float value)
    {
        Values[id] = value;
    }

    float AttributeStore::Modify(const std::string& id, float delta, float fallback)
    {
        const float value = Get(id, fallback) + delta;
        Set(id, value);
        return value;
    }

    bool EffectBundle::Empty() const
    {
        return Effects.empty();
    }

    void EffectBundle::Add(const EffectSpec& effect)
    {
        Effects.push_back(effect);
    }

    void EffectLedger::BeginAction(const ActionIntent& intent)
    {
        m_CurrentIntent = intent;
        m_Entries.clear();
    }

    void EffectLedger::Record(const EffectLedgerEntry& entry)
    {
        m_Entries.push_back(entry);
    }

    void EffectLedger::Clear()
    {
        m_CurrentIntent = {};
        m_Entries.clear();
    }

} // namespace Wheatear::WAO
