#include "wtpch.h"
#include "ActionSignalRouter.h"

#include <utility>

namespace Wheatear::WAO {

    namespace {

        bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return prefix.empty() || value.rfind(prefix, 0) == 0;
        }

    } // namespace

    std::vector<ActionSignalRouter::HandlerEntry>& ActionSignalRouter::Handlers()
    {
        static std::vector<HandlerEntry> handlers;
        return handlers;
    }

    void ActionSignalRouter::RegisterHandler(const std::string& prefix, Handler handler)
    {
        if (!handler)
            return;

        Handlers().push_back({ prefix, std::move(handler) });
    }

    void ActionSignalRouter::Emit(const ActionSignalContext& context)
    {
        if (context.SignalId.empty())
            return;

        for (const HandlerEntry& entry : Handlers())
        {
            if (StartsWith(context.SignalId, entry.Prefix))
                entry.Callback(context);
        }
    }

    void ActionSignalRouter::ClearHandlers()
    {
        Handlers().clear();
    }

} // namespace Wheatear::WAO
