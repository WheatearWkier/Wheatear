#pragma once

#include "ActionTypes.h"
#include "Wheatear/Core/Core.h"

#include <functional>
#include <string>
#include <vector>

namespace Wheatear::WAO {

    struct ActionSignalContext
    {
        ActionIntent Intent;
        std::string ActionId;
        std::string SignalId;
        std::string Source;
        std::string Detail;
        const void* TransientPayload = nullptr; // Valid only during synchronous Emit.
    };

    class WHEATEAR_API ActionSignalRouter
    {
    public:
        using Handler = std::function<void(const ActionSignalContext&)>;

        static void RegisterHandler(const std::string& prefix, Handler handler);
        static void Emit(const ActionSignalContext& context);
        static void ClearHandlers();

    private:
        struct HandlerEntry
        {
            std::string Prefix;
            Handler Callback;
        };

        static std::vector<HandlerEntry>& Handlers();
    };

} // namespace Wheatear::WAO
