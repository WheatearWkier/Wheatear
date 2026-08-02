#include "wtpch.h"
#include "ActionDebugHistory.h"

#include <algorithm>
#include <utility>

namespace Wheatear::WAO {

    namespace {

        constexpr size_t MaxRecords = 256;

        std::vector<ActionDebugRecord>& Records()
        {
            static std::vector<ActionDebugRecord> records;
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
        if (records.size() > MaxRecords)
            records.erase(records.begin(), records.begin() + static_cast<std::ptrdiff_t>(records.size() - MaxRecords));
    }

    void ActionDebugHistory::Record(const EffectLedger& ledger,
        bool success,
        const std::string& detail)
    {
        ActionDebugRecord record;
        record.Intent = ledger.CurrentIntent();
        record.Success = success;
        record.Detail = detail;
        record.Entries = ledger.Entries();
        Push(std::move(record));
    }

    void ActionDebugHistory::RecordSimple(const std::string& actionId,
        const std::string& source,
        const std::string& detail,
        bool success)
    {
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
