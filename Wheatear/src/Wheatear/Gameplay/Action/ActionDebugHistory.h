#pragma once

#include "ActionTypes.h"
#include "Wheatear/Core/Core.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Wheatear::WAO {

    struct ActionDebugRecord
    {
        uint64_t Sequence = 0;
        ActionIntent Intent;
        bool Success = false;
        std::string Detail;
        std::vector<EffectLedgerEntry> Entries;
    };

    class WHEATEAR_API ActionDebugHistory
    {
    public:
        static void Record(const EffectLedger& ledger,
            bool success,
            const std::string& detail = {});
        static void RecordSimple(const std::string& actionId,
            const std::string& source,
            const std::string& detail,
            bool success = true);
        static std::vector<ActionDebugRecord> Recent(size_t limit = 128);
        static void Clear();

    private:
        static void Push(ActionDebugRecord record);
    };

} // namespace Wheatear::WAO
