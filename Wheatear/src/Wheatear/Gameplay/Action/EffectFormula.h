#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear::WAO {

    struct AttributeStore;

    // Evaluates a numeric effect formula against an attribute dictionary.
    // The syntax, precedence and functions are owned by the shared logic
    // runtime (Wheatear/Logic/LogicExpression.h); WAO only supplies the
    // variable resolver.
    //
    // Variables are arbitrary dotted keys resolved from the store
    // ("target.health", "source.attack", "controller.mana"); unknown
    // variables evaluate to `fallback`. Comparisons / logic yield 1.0 / 0.0.
    // On any parse or evaluation error the function returns `fallback`
    // (and never throws).
    WHEATEAR_API float EvaluateEffectFormula(const std::string& formula,
        const AttributeStore& vars,
        float fallback = 0.0f);

    // Parses the expression without evaluating it against real data. Used by
    // the Action Editor to reject broken formulas before saving.
    WHEATEAR_API bool ValidateEffectFormula(const std::string& formula,
        std::string* error = nullptr);

} // namespace Wheatear::WAO
