#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear::WAO {

    struct AttributeStore;

    // Evaluates a numeric effect formula against an attribute dictionary.
    //
    // Grammar (recursive descent):
    //   expr     := or
    //   or       := and ('or' and)*
    //   and      := cmp ('and' cmp)*
    //   cmp      := add (('==' | '!=' | '<' | '<=' | '>' | '>=') add)*
    //   add      := mul (('+' | '-') mul)*
    //   mul      := unary (('*' | '/' | '%') unary)*
    //   unary    := ('-' | 'not') unary | primary
    //   primary  := number | variable | '(' expr ')' | func '(' args ')'
    //   func     := min | max | clamp | abs | round | floor | ceil | if
    //
    // Variables are arbitrary dotted keys resolved from the store
    // ("target.health", "source.attack", "controller.mana"); unknown
    // variables evaluate to 0. Comparisons / logic yield 1.0 / 0.0.
    // On any parse or evaluation error the function returns `fallback`
    // (and never throws).
    WHEATEAR_API float EvaluateEffectFormula(const std::string& formula,
        const AttributeStore& vars,
        float fallback = 0.0f);

} // namespace Wheatear::WAO
