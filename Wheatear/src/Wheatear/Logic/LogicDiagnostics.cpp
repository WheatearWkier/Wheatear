#include "wtpch.h"
#include "LogicDiagnostics.h"

#include <algorithm>
#include <mutex>

namespace Wheatear::Logic {

    namespace {

        constexpr size_t kMaxDiagnostics = 512;

        std::vector<LogicDiagnostic>& Store()
        {
            static std::vector<LogicDiagnostic> diagnostics;
            return diagnostics;
        }

        std::mutex& StoreMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

    } // namespace

    void Diagnostics::Report(DiagnosticSeverity severity,
        const std::string& source,
        const std::string& message)
    {
        if (message.empty())
            return;

        std::lock_guard<std::mutex> lock(StoreMutex());
        auto& diagnostics = Store();
        if (!diagnostics.empty())
        {
            const LogicDiagnostic& last = diagnostics.back();
            if (last.Severity == severity && last.Source == source
                && last.Message == message)
                return;
        }

        diagnostics.push_back({ severity, source, message });
        if (diagnostics.size() > kMaxDiagnostics)
        {
            diagnostics.erase(diagnostics.begin(),
                diagnostics.begin()
                    + static_cast<std::ptrdiff_t>(diagnostics.size() - kMaxDiagnostics));
        }
    }

    void Diagnostics::Info(const std::string& source, const std::string& message)
    {
        Report(DiagnosticSeverity::Info, source, message);
    }

    void Diagnostics::Warn(const std::string& source, const std::string& message)
    {
        Report(DiagnosticSeverity::Warning, source, message);
    }

    void Diagnostics::Error(const std::string& source, const std::string& message)
    {
        Report(DiagnosticSeverity::Error, source, message);
    }

    std::vector<LogicDiagnostic> Diagnostics::Snapshot()
    {
        std::lock_guard<std::mutex> lock(StoreMutex());
        return Store();
    }

    size_t Diagnostics::Count(DiagnosticSeverity severity)
    {
        std::lock_guard<std::mutex> lock(StoreMutex());
        return static_cast<size_t>(std::count_if(Store().begin(), Store().end(),
            [severity](const LogicDiagnostic& diagnostic)
            {
                return diagnostic.Severity == severity;
            }));
    }

    size_t Diagnostics::TotalCount()
    {
        std::lock_guard<std::mutex> lock(StoreMutex());
        return Store().size();
    }

    void Diagnostics::Clear()
    {
        std::lock_guard<std::mutex> lock(StoreMutex());
        Store().clear();
    }

} // namespace Wheatear::Logic
