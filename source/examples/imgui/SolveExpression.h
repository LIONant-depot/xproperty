#include <stack>

namespace xproperty::ui::details::calculator
{
    // Function to check if a character is an operator
    static
    constexpr bool isOperator(char c)
    {
        return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
    }

    // Function to get the precedence of an operator
    static
    constexpr int precedence(char op)
    {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/') return 2;
        if (op == '^') return 3;
        return 0;
    }

    // Function to apply an operator to two operands
    static
    constexpr double applyOp(double a, double b, char op)
    {
        switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/':
            {
                const double epsilon = std::numeric_limits<double>::epsilon();
                if (abs(b) < epsilon) throw std::runtime_error("Division by nearly zero");
                return a / b;
            }
        case '^': return pow(a, b);
        default: throw std::runtime_error("Unknown operator");
        }
    }

    // Function to parse and evaluate a mathematical expression
    static
    double evaluateExpression( const std::string_view& expression )
    {
        std::stack<char> operators;
        std::stack<double> operands;

        std::string token;
        for (size_t i = 0; i < expression.length(); ++i) {
            char c = expression[i];

            if (isspace(c)) continue; // Skip spaces

            if (isdigit(c) || c == '.' || (c == '-' && (i == 0 || expression[i - 1] == '(' || isOperator(expression[i - 1])))) {
                token += c; // Building number token or handling unary minus
            }
            else {
                if (!token.empty()) {
                    operands.push(stod(token)); // Convert token to number and push to operand stack
                    token.clear();
                }

                if (isOperator(c)) {
                    while (!operators.empty() && precedence(operators.top()) >= precedence(c)) {
                        double b = operands.top(); operands.pop();
                        double a = operands.top(); operands.pop();
                        char op = operators.top(); operators.pop();
                        operands.push(applyOp(a, b, op));
                    }
                    operators.push(c);
                }
                else if (c == '(') {
                    operators.push(c);
                }
                else if (c == ')') {
                    while (!operators.empty() && operators.top() != '(') {
                        double b = operands.top(); operands.pop();
                        double a = operands.top(); operands.pop();
                        char op = operators.top(); operators.pop();
                        operands.push(applyOp(a, b, op));
                    }
                    if (!operators.empty() && operators.top() == '(') operators.pop();
                }
            }
        }

        // Handle any remaining number token
        if (!token.empty()) {
            operands.push(stod(token));
        }

        // Process remaining operators
        while (!operators.empty()) {
            double b = operands.top(); operands.pop();
            double a = operands.top(); operands.pop();
            char op = operators.top(); operators.pop();
            operands.push(applyOp(a, b, op));
        }

        if (operands.size() != 1) throw std::runtime_error("Invalid expression");
        return operands.top();
    }
}