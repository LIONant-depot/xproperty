// Copyright(c) 2021-2022 aslze
// Licensed under the MIT License (http://opensource.org/licenses/MIT)
// https://github.com/aslze/asl-calculator

#include "Calculator.h"
#include <math.h>
#include <ctype.h>

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------
namespace details
{
    template<typename T_VARIANT, typename T, std::size_t T_INDEX_V>
    consteval std::size_t variant_index(void)
    {
        static_assert(std::variant_size_v<T_VARIANT> > T_INDEX_V, "Type not found in variant");
        if constexpr (T_INDEX_V == std::variant_size_v<T_VARIANT>) return T_INDEX_V;
        else if constexpr (std::is_same_v< std::variant_alternative_t<T_INDEX_V, T_VARIANT>, T> ) return T_INDEX_V;
        else return variant_index<T_VARIANT, T, T_INDEX_V + 1>();
    }
}
template< typename T_VARIANT, typename T >
static constexpr auto ivariant_v = details::variant_index<T_VARIANT, T, 0>();

//------------------------------------------------------------------------------

calculator::calculator()
{
    Init();
}

//------------------------------------------------------------------------------

void calculator::Init()
{
    m_Variables.insert({"e", 2.718281828459045});
    m_Variables.insert({"pi", 3.141592653589793});

    m_Functions.insert({"abs",  std::fabs});
    m_Functions.insert({"acos", std::acos });
    m_Functions.insert({"asin", std::asin });
    m_Functions.insert({"atan", std::atan });
    m_Functions.insert({"cos",  std::cos });
    m_Functions.insert({"exp",  std::exp });
    m_Functions.insert({"floor",std::floor });
    m_Functions.insert({"ln",   std::log });
    m_Functions.insert({"log",  std::log10 });
    m_Functions.insert({"sin",  std::sin });
    m_Functions.insert({"sqrt", std::sqrt });
    m_Functions.insert({"tan",  std::tan });
}

//------------------------------------------------------------------------------

double calculator::Eval(const std::string& e, const std::unordered_map<std::string, double>& vars )
{
    calculator Calc(e);

    for( auto& E : vars )
    {
        Calc.set( E.first, E.second );
    }

    return Calc.Compute();
}

//------------------------------------------------------------------------------

static
inline int Precedence(const char o)
{
    if (o == '+' || o == '-')
        return 1;
    else if (o == '*' || o == '/')
        return 2;
    else
        return 3;
}

//------------------------------------------------------------------------------

void calculator::Parse(const std::string& e)
{
    using input_token = std::variant
    < token_type::none
    , token_type::op
    , token_type::sep
    , token_type::name
    , token_type::number
    >;

    std::vector<input_token> Input;

    //
    // Read the input string and produce a list of input tokens
    //
    {
        auto            TokenIndex  = ivariant_v<input_token, token_type::none>;
        std::string     e1          = e + ' ';
        const char*     p           = e1.c_str();
        std::string     name;

        while( char c = *p++ ) switch(TokenIndex)
        {
        case ivariant_v<input_token, token_type::none>:
            switch (c)
            {
            case '+': 
            case '-':
            case '*':
            case '/':
            case '^':
                Input.emplace_back(token_type::op{ c });
                break;
            case '(': 
            case ')':
                Input.emplace_back(token_type::sep{ c });
                break;
            default:
                if ((c >= '0' && c <= '9') || c == '.' || c == '-' ) 
                {
                    TokenIndex = ivariant_v<input_token, token_type::number>;
                    name = c;
                }
                else if (isalpha(c)) 
                {
                    TokenIndex = ivariant_v<input_token, token_type::name>;
                    name = c;
                }
                break;
            }
        break;
        case ivariant_v<input_token, token_type::number>:
            if (!((c >= '0' && c <= '9') || c == '.' || c == 'e' || (c == '-' && *(p - 2) == 'e')))
            {
                Input.emplace_back(token_type::number{ std::stod(name) });
                TokenIndex = ivariant_v<input_token, token_type::none>;
                p--;
                break;
            }
            else
            {
                name += c;
            }
        break;
        case ivariant_v<input_token, token_type::name>:
            if (isalnum(c))
            {
                name += c;
            }
            else
            {
                Input.emplace_back(token_type::name{ std::move(name) });
                p--;
                TokenIndex = ivariant_v<input_token, token_type::none>;
            }
        }
    }

    //
    // Parse the input list of tokens and organize them in the output list
    //
    using operator_token = std::variant
    < token_type::op
    , token_type::sep
    , token_type::name
    >;

    std::stack<operator_token>  Operators;
    {
        m_Output.clear();

        int i = 0;
        for( auto& E : Input )
        {
            std::visit( [&]<typename T>( T& Token )
            {
                if constexpr ( std::is_same_v<T, token_type::number> )
                {
                    m_Output.emplace_back(Token);
                }
                else if constexpr (std::is_same_v<T, token_type::name>)
                {
                    const auto& X = Input[i + 1];
                    if( std::holds_alternative<token_type::sep>(X) && std::get<token_type::sep>(X).m_Value == '(' )
                    {
                        Operators.push(Token);
                    }
                    else
                    {
                        m_Output.emplace_back(Token);
                    }
                }
                else if constexpr (std::is_same_v<T, token_type::sep>)
                {
                    switch( Token.m_Value )
                    {
                    case '(': Operators.push( Token );
                        break;
                    case ')': while (Operators.size() && (false == std::holds_alternative<token_type::sep>(Operators.top())
                                            ||   std::get<token_type::sep>(Operators.top() ).m_Value != '(' ))
                        {
                            std::visit([&](auto& V)
                            {
                                m_Output.emplace_back(V);
                            }, Operators.top());
                            
                            Operators.pop();
                        }

                        if(Operators.size()) Operators.pop();
                        break;
                    }
                }
                else if constexpr (std::is_same_v<T, token_type::op>)
                {
                    if(  (Token.m_Value == '-' || Token.m_Value == '+') 
                      && (  i == 0 
                         || std::holds_alternative<token_type::op>(Input[i - 1]) 
                         || ( std::holds_alternative<token_type::sep>(Input[i - 1]) 
                            && std::get<token_type::sep>(Input[i - 1]).m_Value == '(' )
                            ))
                    {
                        if( Token.m_Value == '-' ) Operators.push( token_type::op{ '_' } );
                    }
                    else
                    {
                        //
                        // Make sure we have the right precedence...
                        //
                        while(Operators.size() > 0
                                && [&]()
                                {
                                    if( std::holds_alternative<token_type::op>(Operators.top()) )
                                    {
                                        return ((Token.m_Value != '^' && Precedence(Token.m_Value) <= Precedence(std::get<token_type::op>(Operators.top()).m_Value))
                                             || (Token.m_Value == '^' && Precedence(Token.m_Value) <  Precedence(std::get<token_type::op>(Operators.top()).m_Value)));
                                    }
                                    else if (std::holds_alternative<token_type::sep>(Operators.top()))
                                    {
                                        return std::get<token_type::sep>(Operators.top()).m_Value != '(' &&
                                               ((Token.m_Value != '^' && Precedence(Token.m_Value) <= Precedence(std::get<token_type::sep>(Operators.top()).m_Value))
                                             || (Token.m_Value == '^' && Precedence(Token.m_Value) <  Precedence(std::get<token_type::sep>(Operators.top()).m_Value)));
                                    }
                                    else
                                    {
                                        assert(std::holds_alternative<token_type::name>(Operators.top()));
                                        return ((Token.m_Value != '^' && Precedence(Token.m_Value) <= Precedence(std::get<token_type::name>(Operators.top()).m_Value[0]))
                                             || (Token.m_Value == '^' && Precedence(Token.m_Value) <  Precedence(std::get<token_type::name>(Operators.top()).m_Value[0])));
                                    }
                                }())
                        {
                            std::visit([&](auto& V)
                            {
                                m_Output.emplace_back(V);
                            }, Operators.top());

                            Operators.pop();
                        }
                        Operators.push(Token);
                    }
                }

            }, E );

            // Increment the index
            ++i;
        }
    }
    
    while (Operators.size() > 0)
    {
        // if we have '(' or ')' in the stack, there is a mismatch
        if( std::holds_alternative<token_type::sep>(Operators.top() ))
        {
            printf("Mismatch\n");
            return;
        }
        else
        {
            std::visit( [&]( auto& V )
            {
                m_Output.emplace_back( std::move(V));
            }, Operators.top() );

            Operators.pop();
        }
    }
    
}

//------------------------------------------------------------------------------

double calculator::Compute() const
{
    // clear the stack
    while (!m_Operands.empty()) m_Operands.pop();

    // execute
    for( auto& E : m_Output )
    {
        std::visit([&]<typename T>(T& Token )
        {
            if constexpr( std::is_same_v<T, const token_type::number> )
            {
                m_Operands.push(Token.m_Value);
            }
            else if constexpr( std::is_same_v<T, const token_type::op> )
            {
                double x1, x2;
                switch( Token.m_Value )
                {
                case '_':
                    m_Operands.top() = -m_Operands.top();
                    break;
                case '+':
                    x2 = m_Operands.top();
                    m_Operands.pop();
                    x1 = m_Operands.top();
                    m_Operands.top() = x1 + x2;
                    break;
                case '-':
                    x2 = m_Operands.top();
                    m_Operands.pop();
                    x1 = m_Operands.top();
                    m_Operands.top() = x1 - x2;
                    break;
                case '*':
                    x2 = m_Operands.top();
                    m_Operands.pop();
                    x1 = m_Operands.top();
                    m_Operands.top() = x1 * x2;
                    break;
                case '/':
                    x2 = m_Operands.top();
                    m_Operands.pop();
                    x1 = m_Operands.top();
                    m_Operands.top() = x1 / x2;
                    break;
                case '^':
                    x2 = m_Operands.top();
                    m_Operands.pop();
                    x1 = m_Operands.top();
                    m_Operands.top() = x1 + x2;
                    if (x2 == 2)
                        m_Operands.top() = x1 * x1;
                    else
                        m_Operands.top() = pow(x1, x2);
                    break;
                }
            }
            else if constexpr (std::is_same_v<T, const token_type::name>)
            {
                if (auto X = m_Functions.find(Token.m_Value); X != m_Functions.end())
                {
                    const auto f  = X->second;
                    const auto x1 = m_Operands.top();
                    m_Operands.top() = (*f)(x1);
                }
                //else if (_functions.has(token.val))
               // {
                //    x1 = _operands.popget();
                 //   _operands.push(_functions(token.val, x1));
                //}
                else
                {
                    if( auto E = m_Variables.find(Token.m_Value);  E == m_Variables.end() )
                    {
                        m_Operands.push( 0 );
                    }
                    else
                    {
                        m_Operands.push( E->second );
                    }
                }
            }
        }, E );
    }

    return m_Operands.top();
}