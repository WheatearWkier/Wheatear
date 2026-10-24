#include "wtpch.h"
#include "BehaviorDatabase.h"

#include "Wheatear/Core/Log.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace Wheatear::WAO {

    std::unordered_map<std::string, std::shared_ptr<const BehaviorProgram>>& BehaviorDatabase::Programs()
    {
        static std::unordered_map<std::string, std::shared_ptr<const BehaviorProgram>> programs;
        return programs;
    }

    uint64_t& BehaviorDatabase::RevisionCounter()
    {
        static uint64_t revision = 0;
        return revision;
    }

    bool BehaviorDatabase::Register(const std::shared_ptr<BehaviorProgram>& program,
        std::string* error)
    {
        if (!program || program->Entries.empty())
        {
            if (error)
                *error = "Behavior program has no entries.";
            return false;
        }

        ++RevisionCounter();
        program->Revision = RevisionCounter();
        const std::shared_ptr<const BehaviorProgram> immutable = program;

        for (const BehaviorEntry& entry : program->Entries)
        {
            if (entry.Id.empty())
                continue;

            auto it = Programs().find(entry.Id);
            if (it != Programs().end() && it->second->SourcePath != program->SourcePath)
            {
                WT_CORE_WARN("BehaviorDatabase: replacing behavior '{}' from '{}' with '{}'.",
                    entry.Id,
                    it->second->SourcePath,
                    program->SourcePath);
            }
            Programs()[entry.Id] = immutable;
        }

        return true;
    }

    bool BehaviorDatabase::Replace(
        const std::vector<std::shared_ptr<BehaviorProgram>>& programs,
        std::string* error)
    {
        if (programs.empty())
        {
            if (error)
                *error = "Behavior replacement is empty; keeping the current database.";
            return false;
        }

        std::unordered_map<std::string, std::shared_ptr<const BehaviorProgram>> next;
        const uint64_t nextRevision = RevisionCounter() + 1;

        for (const std::shared_ptr<BehaviorProgram>& program : programs)
        {
            if (!program || program->Entries.empty())
            {
                if (error)
                    *error = "Behavior replacement contains an empty program.";
                return false;
            }

            for (const BehaviorEntry& entry : program->Entries)
            {
                if (entry.Id.empty())
                {
                    if (error)
                        *error = "Behavior replacement contains an entry without an id.";
                    return false;
                }

                if (!next.emplace(entry.Id, std::shared_ptr<const BehaviorProgram>(program)).second)
                {
                    if (error)
                        *error = "Duplicate behavior entry in replacement: " + entry.Id;
                    return false;
                }
            }
        }

        for (const std::shared_ptr<BehaviorProgram>& program : programs)
            program->Revision = nextRevision;

        Programs().swap(next);
        RevisionCounter() = nextRevision;
        return true;
    }

    std::shared_ptr<const BehaviorProgram> BehaviorDatabase::Find(const std::string& behaviorId)
    {
        const auto it = Programs().find(behaviorId);
        return it == Programs().end() ? nullptr : it->second;
    }

    bool BehaviorDatabase::Has(const std::string& behaviorId)
    {
        return !behaviorId.empty() && Programs().find(behaviorId) != Programs().end();
    }

    std::vector<std::string> BehaviorDatabase::AllIds()
    {
        std::vector<std::string> ids;
        ids.reserve(Programs().size());
        for (const auto& [id, program] : Programs())
            ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    void BehaviorDatabase::Clear()
    {
        Programs().clear();
        ++RevisionCounter();
    }

    uint64_t BehaviorDatabase::Revision()
    {
        return RevisionCounter();
    }

} // namespace Wheatear::WAO
