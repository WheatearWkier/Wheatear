#include "wtpch.h"
#include "ActionDebugHistory.h"

#include <algorithm>
#include <deque>
#include <utility>

namespace Wheatear::WAO {

    namespace {

        constexpr size_t MaxRecords = 256;

        // deque so the overflow trim is a constant-time pop_front instead of an
        // O(n) vector front-erase.
        std::deque<ActionDebugRecord>& Records()
        {
            static std::deque<ActionDebugRecord> records;
            return records;
        }

        uint64_t& NextSequence()
        {
            static uint64_t sequence = 1;
            return sequence;
        }

    } // namespace

    void ActionDebugHistory::Push(ActionDebugRecord record)
    {
        record.Sequence = NextSequence()++;

        auto& records = Records();
        records.push_back(std::move(record));
        while (records.size() > MaxRecords)
            records.pop_front();
    }

    void ActionDebugHistory::Record(const EffectLedger& ledger,
        bool success,
        const std::string& detail)
    {
#ifndef WT_DEBUG
        // Debug-only telemetry: skip the ledger deep-copy entirely in release
        // builds so every action activation does not allocate on the hot path.
        (void)ledger;
        (void)success;
        (void)detail;
        return;
#else
        ActionDebugRecord record;
        record.Intent = ledger.CurrentIntent();
        record.Success = success;
        record.Detail = detail;
        record.Entries = ledger.Entries();
        Push(std::move(record));
#endif
    }

    void ActionDebugHistory::RecordSimple(const std::string& actionId,
        const std::string& source,
        const std::string& detail,
        bool success)
    {
#ifndef WT_DEBUG
        (void)actionId;
        (void)source;
        (void)detail;
        (void)success;
        return;
#else
        ActionDebugRecord record;
        record.Intent.ActionId = actionId;
        record.Intent.Source = source;
        record.Success = success;
        record.Detail = detail;
        record.Entries.push_back({
            actionId,
            EffectType::EmitSignal,
            0,
            0,
            detail,
            0.0f,
            success
        });
        Push(std::move(record));
#endif
    }

    std::vector<ActionDebugRecord> ActionDebugHistory::Recent(size_t limit)
    {
        const auto& records = Records();
        if (limit == 0 || records.empty())
            return {};

        const size_t count = std::min(limit, records.size());
        return {
            records.end() - static_cast<std::ptrdiff_t>(count),
            records.end()
        };
    }

    void ActionDebugHistory::Clear()
    {
        Records().clear();
    }

} // namespace Wheatear::WAO
