#include "wtpch.h"
#include "EffectFormula.h"

#include "ActionTypes.h"
#include "Wheatear/Logic/LogicDiagnostics.h"
#include "Wheatear/Logic/LogicExpression.h"

namespace Wheatear::WAO {

    float EvaluateEffectFormula(const std::string& formula,
        const AttributeStore& vars,
        float fallback)
    {
        Logic::EvalContext context;
        context.Fallback = static_cast<double>(fallback);
        context.Resolve = [&vars, fallback](const std::string& name)
        {
            return static_cast<double>(vars.Get(name, fallback));
        };

        double value = fallback;
        std::string error;
        if (!Logic::Evaluate(formula, context, &value, &error))
            Logic::Diagnostics::Warn("wao", "Invalid effect formula '" + formula
                + "': " + error);
        return static_cast<float>(value);
    }

    bool ValidateEffectFormula(const std::string& formula, std::string* error)
    {
        const bool valid = Logic::Validate(formula, error);
        if (!valid && error)
            Logic::Diagnostics::Warn("wao",
                "Invalid effect formula '" + formula + "': " + *error);
        return valid;
    }

} // namespace Wheatear::WAO
