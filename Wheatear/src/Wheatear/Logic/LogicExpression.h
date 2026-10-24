#pragma once

#include "Wheatear/Core/Core.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Wheatear::Logic {

    // A single numeric expression language shared by WAO effect formulas,
    // Event Script conditions and Visual Novel conditions.
    //
    // Grammar:
    //   expr     := or
    //   or       := and ('or' and)*
    //   and      := cmp ('and' cmp)*
    //   cmp      := add (('==' | '!=' | '<' | '<=' | '>' | '>=') add)*
    //   add      := mul (('+' | '-') mul)*
    //   mul      := unary (('*' | '/' | '%') unary)*
    //   unary    := ('-' | 'not') unary | primary
    //   primary  := number | identifier | '(' expr ')' | function '(' args ')'
    //   function := min | max | clamp | abs | round | floor | ceil | if
    //
    // Identifiers may contain letters, digits, '_', '.' and ':' so domain
    // predicates such as flag:CH01 or material:iron can share the parser.
    using VariableResolver = std::function<double(const std::string&)>;

    struct EvalContext
    {
        VariableResolver Resolve;
        double Fallback = 0.0;
    };

    WHEATEAR_API bool Evaluate(const std::string& expression,
        const EvalContext& context,
        double* outValue,
        std::string* error = nullptr);

    WHEATEAR_API bool EvaluateBool(const std::string& expression,
        const EvalContext& context,
        bool* outValue,
        std::string* error = nullptr);

    WHEATEAR_API bool Validate(const std::string& expression,
        std::string* error = nullptr);

    // Converts the historical whitespace predicates used by .wts/.vn
    // ("flag X", "material iron >= 3", "always", ...) into the shared
    // expression syntax ("flag:X", "material:iron >= 3", "1", ...).
    WHEATEAR_API std::string NormalizeCondition(const std::string& expression);

    // Shared progression predicate resolver for identifiers produced by
    // NormalizeCondition. Returns 0 for unknown predicates.
    WHEATEAR_API double ResolveGameplayPredicate(const std::string& identifier);

    // Layered variable table shared by script systems. Resolution order is
    // local -> scene -> persistent -> gameplay predicates, so a script-local
    // variable shadows a scene value, which shadows save/progression data.
    //
    // Copies are cheap (shared state). MakeContext captures a live view of
    // that shared state; callers must not mutate the scope concurrently with
    // evaluation. Copy the scope value if an independent snapshot is needed.
    class WHEATEAR_API VariableScope
    {
    public:
        VariableScope();

        void SetLocal(const std::string& name, double value);
        void SetScene(const std::string& name, double value);
        void SetPersistent(const std::string& name, double value);
        void ClearLocal();
        void ClearScene();
        void ClearPersistent();

        bool TryGet(const std::string& name, double* value) const;
        size_t LocalCount() const;
        size_t SceneCount() const;
        size_t PersistentCount() const;

        // Builds an evaluation context that resolves through this scope and
        // optionally falls back to GameProgress predicates.
        EvalContext MakeContext(double fallback = 0.0,
            bool gameplayFallback = true) const;

    private:
        struct State
        {
            std::unordered_map<std::string, double> Local;
            std::unordered_map<std::string, double> Scene;
            std::unordered_map<std::string, double> Persistent;
        };

        std::shared_ptr<State> m_State;
    };

} // namespace Wheatear::Logic
