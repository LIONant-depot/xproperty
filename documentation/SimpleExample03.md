
# Simple Example 03

This example demonstrates the *checked* API surface: every Try* function returns an
`xproperty::result<T>` instead of asserting or silently doing nothing on failure.

1.	xproperty::result<T> - a Rust-style result carrying either a value or an xproperty::error.
2.	TryRead / TryWrite - non-asserting equivalents of the legacy Read/Write.
3.	TrySetSize - and specifically how it honestly reports failure for a *fixed-size* list
	adapter (std::array, std::span, native C-arrays) instead of silently doing nothing, while
	still succeeding for a genuinely resizable one (std::vector).

By following this example, you can learn how to call the checked API directly instead of going
through the sprop convenience layer, and how to tell a real failure from a real success without
relying on an assert.

```cpp

// This is so we can print something in the screen
#include <iostream>

// includes the user settings used for other examples, this file will include the library as well
#include "../create_documentation/my_properties.h"

// Our example class
struct my_class
{
    int                     m_IntValue      = 42;
    std::vector<int>        m_Resizable     = { 1, 2, 3 };      // A real, resizable list adapter
    std::array<int, 4>      m_FixedSize     = { 1, 2, 3, 4 };    // A fixed-size list adapter

    XPROPERTY_DEF
    ("MyClass", my_class
    , obj_member<"IntValue",  &my_class::m_IntValue>
    , obj_member<"Resizable", &my_class::m_Resizable>
    , obj_member<"FixedSize", &my_class::m_FixedSize>
    )
};
XPROPERTY_REG(my_class)

int main()
{
    my_class                     Obj    {};   // Instance for the values
    xproperty::settings::context Context{};   // Context is used for more advance features, and not always required

    // getObject gives us the reflected type's own object descriptor - the same one XPROPERTY_REG registered.
    const auto& Info = *xproperty::getObject(Obj);

    // --- TryWrite: happy path ---
    if (auto Member = Info.requireMember(xproperty::settings::strguid("IntValue")))
    {
        xproperty::any Value; Value.set<int>(100);
        auto Result = Member.value()->TryWrite(&Obj, Value, Context);
        std::cout << "TryWrite(IntValue, 100)              -> " << (Result ? "ok" : "failed")
                   << ", m_IntValue is now " << Obj.m_IntValue << std::endl;
    }

    // --- TryWrite: a genuine type mismatch is reported, not asserted or silently ignored ---
    if (auto Member = Info.requireMember(xproperty::settings::strguid("IntValue")))
    {
        xproperty::any Value; Value.set<std::string>("not an int");
        auto Result = Member.value()->TryWrite(&Obj, Value, Context);
        std::cout << "TryWrite(IntValue, \"not an int\")     -> " << (Result ? "ok" : "failed, as expected") << std::endl;
    }

    // --- TrySetSize: succeeds on a real, resizable list adapter ---
    constexpr std::size_t FirstDimension = 0;   // both members here are single-dimension lists
    if (auto Member = Info.requireMember(xproperty::settings::strguid("Resizable")))
    {
        if (auto* pList = std::get_if<xproperty::type::members::list_var>(&Member.value()->m_Variant))
        {
            auto Result = pList->TrySetSize(&Obj, FirstDimension, 5, Context);
            std::cout << "TrySetSize(Resizable, 5)              -> " << (Result ? "ok" : "failed")
                       << ", new size is " << Obj.m_Resizable.size() << std::endl;
        }
    }

    // --- TrySetSize: honestly fails on a fixed-size adapter instead of silently doing nothing ---
    if (auto Member = Info.requireMember(xproperty::settings::strguid("FixedSize")))
    {
        if (auto* pList = std::get_if<xproperty::type::members::list_var>(&Member.value()->m_Variant))
        {
            auto Result = pList->TrySetSize(&Obj, FirstDimension, 8, Context);
            std::cout << "TrySetSize(FixedSize, 8)              -> " << (Result ? "ok" : "failed, as expected (fixed-size adapter)")
                       << ", size is still " << Obj.m_FixedSize.size() << std::endl;
        }
    }

    return 0;
}

```

