// Copyright(c) 2021-2022 aslze
// Licensed under the MIT License (http://opensource.org/licenses/MIT)
// https://github.com/aslze/asl-calculator

#ifndef ASL_CALCULATOR_H
#define ASL_CALCULATOR_H

#include <stack>

//-------------------------------------------------------------------------------------
// A Calculator to compute the value of math expressions including functions and variables.
// 
// ~~~
// Calculator calc("-3.5*cos(i*5)+pi*(-2+sqrt(i*pi^2))/3");
// double y = calc.set("i", 25.5).compute();
// ~~~
//-------------------------------------------------------------------------------------

class calculator
{
public:
    /**
    Creates a calculator
    */
    calculator();
    /**
    Creates a calculator and parses the given expression
    */
    calculator(const std::string& e) { Init(); Parse(e); }
    /**
    Parses an expression to prepare for computation
    */
    void Parse(const std::string& e);
    /**
    Computes the value of a previously parsed expression
    */
    double Compute() const;
    /**
    Parses and computes the value of an expression, optionally using a set of variable values
    */
    static double Eval(const std::string& e, const std::unordered_map<std::string,double>& vars = std::unordered_map<std::string, double>() );
    /**
    Sets the value of a variable for use in computation of an expression
    */
    calculator& set(const std::string& var, double val) { m_Variables[var] = val; return *this; }

protected:

    void Init();

protected:

    using function = double(*)(double);

    struct token_type
    {
        struct none      {};
        struct op        { char         m_Value; };
        struct sep       { char         m_Value; };
        struct name      { std::string  m_Value; };
        struct number    { double       m_Value; };
    };

    using output_token = std::variant
    < token_type::op
    , token_type::sep
    , token_type::name
    , token_type::number
    >;

    std::vector<output_token>                       m_Output;
    mutable std::stack<double>                      m_Operands;
    std::unordered_map<std::string,  double>        m_Variables;
    std::unordered_map< std::string, function>      m_Functions;
};

#endif