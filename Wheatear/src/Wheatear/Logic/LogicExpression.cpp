#include "wtpch.h"
#include "LogicExpression.h"

#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace Wheatear::Logic {

    namespace {

        enum class TokenKind
        {
            End, Invalid, Number, Identifier,
            Plus, Minus, Star, Slash, Percent,
            LParen, RParen, Comma,
            Eq, Ne, Lt, Le, Gt, Ge,
            And, Or, Not
        };

        struct Token
        {
            TokenKind Kind = TokenKind::End;
            double Number = 0.0;
            std::string Text;
        };

        struct Lexer
        {
            explicit Lexer(const std::string& source) : m_Source(source) {}

            Token Peek()
            {
                if (!m_Peeked)
                {
                    m_Peeked = true;
                    m_Peek = Next();
                }
                return m_Peek;
            }

            Token Take()
            {
                const Token token = Peek();
                m_Peeked = false;
                return token;
            }

            size_t Position() const { return m_Cursor; }

        private:
            Token Next()
            {
                SkipWhitespace();
                if (m_Cursor >= m_Source.size())
                    return { TokenKind::End };

                const char c = m_Source[m_Cursor];
                if (std::isdigit(static_cast<unsigned char>(c)) || c == '.')
                    return ReadNumber();
                if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
                    return ReadIdentifier();

                ++m_Cursor;
                switch (c)
                {
                case '+': return { TokenKind::Plus };
                case '-': return { TokenKind::Minus };
                case '*': return { TokenKind::Star };
                case '/': return { TokenKind::Slash };
                case '%': return { TokenKind::Percent };
                case '(': return { TokenKind::LParen };
                case ')': return { TokenKind::RParen };
                case ',': return { TokenKind::Comma };
                case '=': return { TokenKind::Eq };
                case '!':
                    if (Match('=')) return { TokenKind::Ne };
                    return { TokenKind::Not };
                case '<':
                    if (Match('=')) return { TokenKind::Le };
                    return { TokenKind::Lt };
                case '>':
                    if (Match('=')) return { TokenKind::Ge };
                    return { TokenKind::Gt };
                default:
                    return { TokenKind::Invalid };
                }
            }

            void SkipWhitespace()
            {
                while (m_Cursor < m_Source.size()
                    && std::isspace(static_cast<unsigned char>(m_Source[m_Cursor])))
                    ++m_Cursor;
            }

            bool Match(char expected)
            {
                if (m_Cursor < m_Source.size() && m_Source[m_Cursor] == expected)
                {
                    ++m_Cursor;
                    return true;
                }
                return false;
            }

            Token ReadNumber()
            {
                const size_t begin = m_Cursor;
                size_t digits = 0;
                size_t dots = 0;
                while (m_Cursor < m_Source.size()
                    && (std::isdigit(static_cast<unsigned char>(m_Source[m_Cursor]))
                        || m_Source[m_Cursor] == '.'))
                {
                    if (m_Source[m_Cursor] == '.')
                        ++dots;
                    else
                        ++digits;
                    ++m_Cursor;
                }

                Token token;
                token.Text = m_Source.substr(begin, m_Cursor - begin);
                if (digits == 0 || dots > 1)
                {
                    token.Kind = TokenKind::Invalid;
                    return token;
                }

                token.Kind = TokenKind::Number;
                try
                {
                    size_t consumed = 0;
                    token.Number = std::stod(token.Text, &consumed);
                    if (consumed != token.Text.size())
                        token.Kind = TokenKind::Invalid;
                }
                catch (...)
                {
                    token.Kind = TokenKind::Invalid;
                    token.Number = 0.0;
                }
                return token;
            }

            Token ReadIdentifier()
            {
                const size_t begin = m_Cursor;
                while (m_Cursor < m_Source.size())
                {
                    const char c = m_Source[m_Cursor];
                    if (!(std::isalnum(static_cast<unsigned char>(c))
                        || c == '_' || c == '.' || c == ':'))
                        break;
                    ++m_Cursor;
                }

                Token token;
                token.Text = m_Source.substr(begin, m_Cursor - begin);
                std::string lower = token.Text;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (lower == "and") token.Kind = TokenKind::And;
                else if (lower == "or") token.Kind = TokenKind::Or;
                else if (lower == "not") token.Kind = TokenKind::Not;
                else token.Kind = TokenKind::Identifier;
                return token;
            }

            const std::string& m_Source;
            size_t m_Cursor = 0;
            bool m_Peeked = false;
            Token m_Peek;
        };

        class Parser
        {
        public:
            Parser(const std::string& source, const EvalContext& context, std::string* error)
                : m_Lexer(source), m_Context(context), m_Error(error)
            {
            }

            bool ParseAll(double& outValue)
            {
                outValue = ParseOr();
                const Token end = m_Lexer.Take();
                if (m_Failed)
                {
                    Fail("Invalid expression near byte "
                        + std::to_string(m_Lexer.Position()));
                    return false;
                }
                if (end.Kind != TokenKind::End)
                {
                    Fail("Unexpected token after expression");
                    return false;
                }
                return true;
            }

        private:
            double Resolve(const std::string& name) const
            {
                return m_Context.Resolve ? m_Context.Resolve(name) : 0.0;
            }

            void Fail(const std::string& message)
            {
                m_Failed = true;
                if (m_Error && m_Error->empty())
                    *m_Error = message;
            }

            double ParseOr()
            {
                double value = ParseAnd();
                while (m_Lexer.Peek().Kind == TokenKind::Or)
                {
                    m_Lexer.Take();
                    const double rhs = ParseAnd();
                    value = (value != 0.0 || rhs != 0.0) ? 1.0 : 0.0;
                }
                return value;
            }

            double ParseAnd()
            {
                double value = ParseCompare();
                while (m_Lexer.Peek().Kind == TokenKind::And)
                {
                    m_Lexer.Take();
                    const double rhs = ParseCompare();
                    value = (value != 0.0 && rhs != 0.0) ? 1.0 : 0.0;
                }
                return value;
            }

            double ParseCompare()
            {
                double value = ParseAdd();
                for (;;)
                {
                    const TokenKind kind = m_Lexer.Peek().Kind;
                    if (kind != TokenKind::Eq && kind != TokenKind::Ne
                        && kind != TokenKind::Lt && kind != TokenKind::Le
                        && kind != TokenKind::Gt && kind != TokenKind::Ge)
                        break;
                    m_Lexer.Take();
                    const double rhs = ParseAdd();
                    switch (kind)
                    {
                    case TokenKind::Eq: value = value == rhs ? 1.0 : 0.0; break;
                    case TokenKind::Ne: value = value != rhs ? 1.0 : 0.0; break;
                    case TokenKind::Lt: value = value < rhs ? 1.0 : 0.0; break;
                    case TokenKind::Le: value = value <= rhs ? 1.0 : 0.0; break;
                    case TokenKind::Gt: value = value > rhs ? 1.0 : 0.0; break;
                    case TokenKind::Ge: value = value >= rhs ? 1.0 : 0.0; break;
                    default: break;
                    }
                }
                return value;
            }

            double ParseAdd()
            {
                double value = ParseMultiply();
                for (;;)
                {
                    const TokenKind kind = m_Lexer.Peek().Kind;
                    if (kind != TokenKind::Plus && kind != TokenKind::Minus)
                        break;
                    m_Lexer.Take();
                    const double rhs = ParseMultiply();
                    value = kind == TokenKind::Plus ? value + rhs : value - rhs;
                }
                return value;
            }

            double ParseMultiply()
            {
                double value = ParseUnary();
                for (;;)
                {
                    const TokenKind kind = m_Lexer.Peek().Kind;
                    if (kind != TokenKind::Star && kind != TokenKind::Slash
                        && kind != TokenKind::Percent)
                        break;
                    m_Lexer.Take();
                    const double rhs = ParseUnary();
                    switch (kind)
                    {
                    case TokenKind::Star: value = value * rhs; break;
                    case TokenKind::Slash:
                        value = rhs != 0.0 ? value / rhs : m_Context.Fallback;
                        break;
                    case TokenKind::Percent:
                        value = rhs != 0.0 ? std::fmod(value, rhs) : m_Context.Fallback;
                        break;
                    default: break;
                    }
                }
                return value;
            }

            double ParseUnary()
            {
                const TokenKind kind = m_Lexer.Peek().Kind;
                if (kind == TokenKind::Minus)
                {
                    m_Lexer.Take();
                    return -ParseUnary();
                }
                if (kind == TokenKind::Not)
                {
                    m_Lexer.Take();
                    return ParseUnary() == 0.0 ? 1.0 : 0.0;
                }
                return ParsePrimary();
            }

            double ParsePrimary()
            {
                const Token token = m_Lexer.Take();
                switch (token.Kind)
                {
                case TokenKind::Number:
                    return token.Number;
                case TokenKind::Identifier:
                    if (m_Lexer.Peek().Kind == TokenKind::LParen)
                        return ParseFunction(token.Text);
                    return Resolve(token.Text);
                case TokenKind::LParen:
                {
                    const double value = ParseOr();
                    if (m_Lexer.Take().Kind != TokenKind::RParen)
                        Fail("Missing closing parenthesis");
                    return value;
                }
                case TokenKind::Invalid:
                    Fail("Unexpected character in expression");
                    return m_Context.Fallback;
                default:
                    Fail("Unexpected token");
                    return m_Context.Fallback;
                }
            }

            double ParseFunction(const std::string& name)
            {
                m_Lexer.Take();
                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

                std::vector<double> args;
                if (m_Lexer.Peek().Kind != TokenKind::RParen)
                {
                    for (;;)
                    {
                        args.push_back(ParseOr());
                        if (m_Lexer.Peek().Kind != TokenKind::Comma)
                            break;
                        m_Lexer.Take();
                    }
                }
                if (m_Lexer.Take().Kind != TokenKind::RParen)
                {
                    Fail("Missing closing parenthesis in function call");
                    return m_Context.Fallback;
                }

                if (lower == "min" && args.size() >= 2)
                    return *std::min_element(args.begin(), args.end());
                if (lower == "max" && args.size() >= 2)
                    return *std::max_element(args.begin(), args.end());
                if (lower == "clamp" && args.size() >= 3)
                    return std::clamp(args[0], std::min(args[1], args[2]),
                        std::max(args[1], args[2]));
                if (lower == "abs" && !args.empty()) return std::abs(args[0]);
                if (lower == "round" && !args.empty()) return std::round(args[0]);
                if (lower == "floor" && !args.empty()) return std::floor(args[0]);
                if (lower == "ceil" && !args.empty()) return std::ceil(args[0]);
                if (lower == "if" && args.size() >= 3)
                    return args[0] != 0.0 ? args[1] : args[2];

                Fail("Unknown function '" + name + "'");
                return m_Context.Fallback;
            }

            Lexer m_Lexer;
            const EvalContext& m_Context;
            std::string* m_Error = nullptr;
            bool m_Failed = false;
        };

    } // namespace

    bool Evaluate(const std::string& expression,
        const EvalContext& context,
        double* outValue,
        std::string* error)
    {
        if (outValue)
            *outValue = context.Fallback;
        if (error)
            error->clear();
        if (expression.empty())
        {
            if (error)
                *error = "Expression is empty.";
            return false;
        }

        try
        {
            Parser parser(expression, context, error);
            double value = context.Fallback;
            if (!parser.ParseAll(value))
                return false;
            if (outValue)
                *outValue = value;
            return true;
        }
        catch (...)
        {
            if (error)
                *error = "Invalid expression.";
            return false;
        }
    }

    bool EvaluateBool(const std::string& expression,
        const EvalContext& context,
        bool* outValue,
        std::string* error)
    {
        double value = 0.0;
        if (!Evaluate(expression, context, &value, error))
        {
            if (outValue)
                *outValue = false;
            return false;
        }
        if (outValue)
            *outValue = value != 0.0;
        return true;
    }

    bool Validate(const std::string& expression, std::string* error)
    {
        EvalContext context;
        context.Resolve = [](const std::string&) { return 0.0; };
        return Evaluate(expression, context, nullptr, error);
    }

    std::string NormalizeCondition(const std::string& expression)
    {
        using Wheatear::StringUtils::ToLower;
        using Wheatear::StringUtils::Trim;

        std::string text = Trim(expression);
        if (text.empty())
            return text;

        std::istringstream stream(text);
        std::vector<std::string> words;
        std::string word;
        while (stream >> word)
            words.push_back(word);
        if (words.empty())
            return text;

        const auto stripQuotes = [](std::string value)
        {
            value = Trim(value);
            if (value.size() >= 2
                && ((value.front() == '"' && value.back() == '"')
                    || (value.front() == '\'' && value.back() == '\'')))
                return value.substr(1, value.size() - 2);
            return value;
        };
        const auto isComparison = [](const std::string& token)
        {
            return token == "==" || token == "=" || token == "!="
                || token == ">" || token == ">=" || token == "<"
                || token == "<=";
        };
        const auto isConnector = [](const std::string& token)
        {
            const std::string lower = ToLower(token);
            return lower == "and" || lower == "or" || lower == "not"
                || lower == "(" || lower == ")";
        };

        static const std::set<std::string> kPredicates = {
            "flag", "skill", "dungeon", "dungeon_unlocked",
            "completed", "dungeon_completed", "last_dungeon", "last_result",
            "equipment", "equipped", "material"
        };

        std::vector<std::string> normalized;
        normalized.reserve(words.size());
        for (size_t i = 0; i < words.size(); ++i)
        {
            const std::string lower = ToLower(words[i]);
            if (lower == "always")
            {
                normalized.push_back("1");
                continue;
            }
            if (lower == "never")
            {
                normalized.push_back("0");
                continue;
            }
            if (kPredicates.count(lower) > 0
                && i + 1 < words.size()
                && !isComparison(words[i + 1])
                && !isConnector(words[i + 1]))
            {
                const std::string argument = stripQuotes(words[i + 1]);
                if (!argument.empty())
                {
                    std::string combined = lower + ":" + argument;
                    if (lower == "material" && (i + 2 >= words.size()
                        || !isComparison(words[i + 2])))
                    {
                        combined += " > 0";
                    }
                    normalized.push_back(std::move(combined));
                    ++i;
                    continue;
                }
            }
            normalized.push_back(words[i]);
        }

        std::string result;
        for (const std::string& token : normalized)
        {
            if (!result.empty())
                result += ' ';
            result += token;
        }

        if (normalized.size() > 1 && ToLower(normalized.front()) == "not")
        {
            std::string rest;
            for (size_t i = 1; i < normalized.size(); ++i)
            {
                if (!rest.empty())
                    rest += ' ';
                rest += normalized[i];
            }
            return "not (" + rest + ")";
        }
        return result;
    }

    double ResolveGameplayPredicate(const std::string& identifier)
    {
        const auto& state = GameProgress::GetState();
        const size_t separator = identifier.find(':');
        const std::string kind = separator == std::string::npos
            ? identifier
            : identifier.substr(0, separator);
        const std::string value = separator == std::string::npos
            ? std::string()
            : identifier.substr(separator + 1);

        if (kind == "chapter")
            return static_cast<double>(state.CurrentChapter);
        if (kind == "flag" && !value.empty())
            return state.StoryFlags.count(value) > 0 ? 1.0 : 0.0;
        if (kind == "skill" && !value.empty())
            return GameProgress::IsSkillUnlocked(value) ? 1.0 : 0.0;
        if ((kind == "dungeon" || kind == "dungeon_unlocked") && !value.empty())
            return GameProgress::IsDungeonUnlocked(value) ? 1.0 : 0.0;
        if ((kind == "completed" || kind == "dungeon_completed") && !value.empty())
            return state.CompletedDungeons.count(value) > 0 ? 1.0 : 0.0;
        if ((kind == "last_dungeon" || kind == "last_result") && !value.empty())
            return state.LastDungeonResult.Valid
                && state.LastDungeonResult.DungeonId == value ? 1.0 : 0.0;
        if (kind == "equipment" && !value.empty())
            return GameProgress::IsEquipmentOwned(value) ? 1.0 : 0.0;
        if (kind == "equipped" && !value.empty())
            return GameProgress::IsEquipmentEquipped(value) ? 1.0 : 0.0;
        if (kind == "material" && !value.empty())
            return static_cast<double>(GameProgress::GetMaterialAmount(value));
        return 0.0;
    }

    VariableScope::VariableScope()
        : m_State(std::make_shared<State>())
    {
    }

    void VariableScope::SetLocal(const std::string& name, double value)
    {
        if (!name.empty())
            m_State->Local[name] = value;
    }

    void VariableScope::SetScene(const std::string& name, double value)
    {
        if (!name.empty())
            m_State->Scene[name] = value;
    }

    void VariableScope::SetPersistent(const std::string& name, double value)
    {
        if (!name.empty())
            m_State->Persistent[name] = value;
    }

    void VariableScope::ClearLocal()
    {
        m_State->Local.clear();
    }

    void VariableScope::ClearScene()
    {
        m_State->Scene.clear();
    }

    void VariableScope::ClearPersistent()
    {
        m_State->Persistent.clear();
    }

    bool VariableScope::TryGet(const std::string& name, double* value) const
    {
        const auto find = [&name](const std::unordered_map<std::string, double>& map,
            double* out) -> bool
        {
            const auto it = map.find(name);
            if (it == map.end())
                return false;
            if (out)
                *out = it->second;
            return true;
        };

        return find(m_State->Local, value)
            || find(m_State->Scene, value)
            || find(m_State->Persistent, value);
    }

    size_t VariableScope::LocalCount() const
    {
        return m_State->Local.size();
    }

    size_t VariableScope::SceneCount() const
    {
        return m_State->Scene.size();
    }

    size_t VariableScope::PersistentCount() const
    {
        return m_State->Persistent.size();
    }

    EvalContext VariableScope::MakeContext(double fallback,
        bool gameplayFallback) const
    {
        EvalContext context;
        context.Fallback = fallback;
        const std::shared_ptr<State> state = m_State;
        context.Resolve = [state, gameplayFallback](const std::string& name)
        {
            const std::unordered_map<std::string, double>* scopes[] = {
                &state->Local, &state->Scene, &state->Persistent
            };
            for (const auto* scope : scopes)
            {
                const auto it = scope->find(name);
                if (it != scope->end())
                    return it->second;
            }
            return gameplayFallback ? ResolveGameplayPredicate(name) : 0.0;
        };
        return context;
    }

} // namespace Wheatear::Logic
