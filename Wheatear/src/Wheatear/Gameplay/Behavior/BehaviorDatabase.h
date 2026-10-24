#pragma once

#include "BehaviorTypes.h"
#include "Wheatear/Core/Core.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::WAO {

    class WHEATEAR_API BehaviorDatabase
    {
    public:
        static bool Register(const std::shared_ptr<BehaviorProgram>& program,
            std::string* error = nullptr);
        static bool Replace(
            const std::vector<std::shared_ptr<BehaviorProgram>>& programs,
            std::string* error = nullptr);
        static std::shared_ptr<const BehaviorProgram> Find(const std::string& behaviorId);
        static bool Has(const std::string& behaviorId);
        static std::vector<std::string> AllIds();
        static void Clear();
        static uint64_t Revision();

    private:
        static std::unordered_map<std::string, std::shared_ptr<const BehaviorProgram>>& Programs();
        static uint64_t& RevisionCounter();
    };

} // namespace Wheatear::WAO
