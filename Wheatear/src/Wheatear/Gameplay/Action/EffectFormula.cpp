#include "wtpch.h"
#include "EffectFormula.h"

#include "ActionTypes.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::WAO {

    namespace {

        // ------------------------------------------------------------------
        // Tokenizer
        // ------------------------------------------------------------------

        enum class TokenKind
        {
            End,
            Number,
            Identifier,
            Plus, Minus, Star, Slash, Percent,
            LParen, RParen, Comma,
            Eq, Ne, Lt, Le, Gt, Ge,
            And, Or, Not
        };

        struct Token
        {
            TokenKind Kind = TokenKind::End;
            float Number = 0.0f;
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
                case '=':
                    if (Match('=')) return { TokenKind::Eq };
                    return { TokenKind::Eq };
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
                    return { TokenKind::End };
                }
            }

            void SkipWhitespace()
            {
                while (m_Cursor < m_Source.size()
                    && std::isspace(static_cast<unsigned char>(m_Source[m_Cursor])))
                {
                    ++m_Cursor;
                }
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
                while (m_Cursor < m_Source.size()
                    && (std::isdigit(static_cast<unsigned char>(m_Source[m_Cursor]))
                        || m_Source[m_Cursor] == '.'))
                {
                    ++m_Cursor;
                }

                Token token;
                token.Kind = TokenKind::Number;
                token.Text = m_Source.substr(begin, m_Cursor - begin);
                try
                {
                    token.Number = std::stof(token.Text);
                }
                catch (...)
                {
                    token.Number = 0.0f;
                }
                return token;
            }

            Token ReadIdentifier()
            {
                const size_t begin = m_Cursor;
                while (m_Cursor < m_Source.size())
                {
                    const char c = m_Source[m_Cursor];
                    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'))
                        break;
                    ++m_Cursor;
                }

                Token token;
                token.Text = m_Source.substr(begin, m_Cursor - begin);

                // Keyword mapping (case-insensitive).
                std::string lower = token.Text;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (lower == "and")       token.Kind = TokenKind::And;
                else if (lower == "or")   token.Kind = TokenKind::Or;
                else if (lower == "not")  token.Kind = TokenKind::Not;
                else                      token.Kind = TokenKind::Identifier;
                return token;
            }

            const std::string& m_Source;
            size_t m_Cursor = 0;
            bool m_Peeked = false;
            Token m_Peek;
        };

        // ------------------------------------------------------------------
        // Parser
        // ------------------------------------------------------------------

        class Parser
        {
        public:
            Parser(const std::string& source, const AttributeStore& vars, float fallback)
                : m_Lexer(source)
                , m_Vars(vars)
                , m_Fallback(fallback)
            {
            }

            // Returns false when the expression did not parse cleanly.
            bool ParseAll(float& outValue)
            {
                outValue = ParseOr();
                const Token end = m_Lexer.Take();
                return !m_Error && end.Kind == TokenKind::End;
            }

        private:
            float ParseOr()
            {
                float value = ParseAnd();
                while (m_Lexer.Peek().Kind == TokenKind::Or)
                {
                    m_Lexer.Take();
                    const float rhs = ParseAnd();
                    value = (value != 0.0f || rhs != 0.0f) ? 1.0f : 0.0f;
                }
                return value;
            }

            float ParseAnd()
            {
                float value = ParseCompare();
                while (m_Lexer.Peek().Kind == TokenKind::And)
                {
                    m_Lexer.Take();
                    const float rhs = ParseCompare();
                    value = (value != 0.0f && rhs != 0.0f) ? 1.0f : 0.0f;
                }
                return value;
            }

            float ParseCompare()
            {
                float value = ParseAdd();
                for (;;)
                {
                    const TokenKind kind = m_Lexer.Peek().Kind;
                    if (kind != TokenKind::Eq && kind != TokenKind::Ne
                        && kind != TokenKind::Lt && kind != TokenKind::Le
                        && kind != TokenKind::Gt && kind != TokenKind::Ge)
                    {
                        break;
                    }
                    m_Lexer.Take();
                    const float rhs = ParseAdd();
                    switch (kind)
                    {
                    case TokenKind::Eq: value = (value == rhs) ? 1.0f : 0.0f; break;
                    case TokenKind::Ne: value = (value != rhs) ? 1.0f : 0.0f; break;
                    case TokenKind::Lt: value = (value < rhs) ? 1.0f : 0.0f; break;
                    case TokenKind::Le: value = (value <= rhs) ? 1.0f : 0.0f; break;
                    case TokenKind::Gt: value = (value > rhs) ? 1.0f : 0.0f; break;
                    case TokenKind::Ge: value = (value >= rhs) ? 1.0f : 0.0f; break;
                    default: break;
                    }
                }
                return value;
            }

            float ParseAdd()
            {
                float value = ParseMultiply();
                for (;;)
                {
                    const TokenKind kind = m_Lexer.Peek().Kind;
                    if (kind != TokenKind::Plus && kind != TokenKind::Minus)
                        break;
                    m_Lexer.Take();
                    const float rhs = ParseMultiply();
                    value = (kind == TokenKind::Plus) ? value + rhs : value - rhs;
                }
                return value;
            }

            float ParseMultiply()
            {
                float value = ParseUnary();
                for (;;)
                {
                    const TokenKind kind = m_Lexer.Peek().Kind;
                    if (kind != TokenKind::Star && kind != TokenKind::Slash
                        && kind != TokenKind::Percent)
                    {
                        break;
                    }
                    m_Lexer.Take();
                    const float rhs = ParseUnary();
                    switch (kind)
                    {
                    case TokenKind::Star:   value = value * rhs; break;
                    case TokenKind::Slash:  value = (rhs != 0.0f) ? value / rhs : m_Fallback; break;
                    case TokenKind::Percent: value = (rhs != 0.0f) ? std::fmod(value, rhs) : m_Fallback; break;
                    default: break;
                    }
                }
                return value;
            }

            float ParseUnary()
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
                    const float value = ParseUnary();
                    return (value == 0.0f) ? 1.0f : 0.0f;
                }
                return ParsePrimary();
            }

            float ParsePrimary()
            {
                const Token token = m_Lexer.Take();
                switch (token.Kind)
                {
                case TokenKind::Number:
                    return token.Number;

                case TokenKind::Identifier:
                    if (m_Lexer.Peek().Kind == TokenKind::LParen)
                        return ParseFunction(token.Text);
                    return m_Vars.Get(token.Text, 0.0f);

                case TokenKind::LParen:
                {
                    const float value = ParseOr();
                    m_Lexer.Take();   // closing ')'
                    return value;
                }

                default:
                    // Unexpected token (end of input, stray operator, ...):
                    // mark the expression invalid so the caller falls back.
                    m_Error = true;
                    return m_Fallback;
                }
            }

            float ParseFunction(const std::string& name)
            {
                m_Lexer.Take();   // '('

                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

                std::vector<float> args;
                if (m_Lexer.Peek().Kind != TokenKind::RParen)
                {
                    for (;;)
                    {
                        args.push_back(ParseOr());
                        if (m_Lexer.Peek().Kind == TokenKind::Comma)
                        {
                            m_Lexer.Take();
                            continue;
                        }
                        break;
                    }
                }
                m_Lexer.Take();   // ')'

                if (lower == "min" && args.size() >= 2)
                    return *std::min_element(args.begin(), args.end());
                if (lower == "max" && args.size() >= 2)
                    return *std::max_element(args.begin(), args.end());
                if (lower == "clamp" && args.size() >= 3)
                    return std::clamp(args[0], std::min(args[1], args[2]), std::max(args[1], args[2]));
                if (lower == "abs" && args.size() >= 1)
                    return std::abs(args[0]);
                if (lower == "round" && args.size() >= 1)
                    return std::round(args[0]);
                if (lower == "floor" && args.size() >= 1)
                    return std::floor(args[0]);
                if (lower == "ceil" && args.size() >= 1)
                    return std::ceil(args[0]);
                if (lower == "if" && args.size() >= 3)
                    return (args[0] != 0.0f) ? args[1] : args[2];
                m_Error = true;
                return m_Fallback;
            }

            Lexer m_Lexer;
            const AttributeStore& m_Vars;
            float m_Fallback;
            bool m_Error = false;
        };

    } // namespace

    float EvaluateEffectFormula(const std::string& formula,
        const AttributeStore& vars,
        float fallback)
    {
        if (formula.empty())
            return fallback;

        try
        {
            Parser parser(formula, vars, fallback);
            float value = fallback;
            return parser.ParseAll(value) ? value : fallback;
        }
        catch (...)
        {
            return fallback;
        }
    }

} // namespace Wheatear::WAO
