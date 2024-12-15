
# Simple Example 01

In this example:
1. We define a class my_class with two member variables m_IntValue and m_FloatValue.
2. We define a member function printValues to print the values of these members.
3. We use the XPROPERTY_DEF macro to define the properties for my_class.
4. We register the class with XPROPERTY_REG(my_class).
5. In the main function, we create an instance of MyClass, create a property to set, and use the setProperty function to set the m_IntValue property.
6. If there is an error, it is printed to the standard error output. Otherwise, the printValues function is called to display the values.

This demonstrates how to use the xproperty::sprop::setProperty function to set a property in a class defined using the xProperties library.

```cpp

// This is so we can print something in the screen
#include <iostream>

// includes the user settings used for other examples, this file will include the library as well
#include "../create_documentation/my_properties.h"

//  This include add very useful operation that while part of xproperty is more like an extension.
#include "../../sprop/property_sprop.h"

// Our example class
struct my_class
{
    int     m_IntValue      = 42;
    float   m_FloatValue    = 3.14f;

    // We can use this macro to define the properties. Note that we don't really need to use a macro
    // but it does minimize the amount of code we need to write.
    XPROPERTY_DEF
    ("MyClass", my_class                                       // String of the name of the class and the type of the class 
    , obj_member<"IntValue",    &my_class::m_IntValue>      // Define our interger property
    , obj_member<"FloatValue",  &my_class::m_FloatValue>    // Define our float property
    )
};
XPROPERTY_REG(my_class)

int main()
{
    my_class                     Obj    {};  // Instance for the values
    std::string                  Error  {};  // Error message (if any)
    xproperty::settings::context Context{};  // Context is used for more advance features, and not always required

    // Create a property to set
    xproperty::container::prop Property{};
    property.m_Path  = "MyClass/IntValue";   // Note that it needs to be the full name (which looks like a file path)
    property.m_Value.set<int>(100);          // Value can handle any of the atomic types provided in the xproperty::settings

    // Set the property using setProperty
    xproperty::sprop::setProperty(Error, Obj, Property, Context);

    // If the Error string is empty there was not errors otherwise we should print the error
    if (false == Error.empty())
    {
        std::cerr << "Error: " << Error << std::endl;
    }
    else
    {
        std::cout << "Int Value: " << Obj.m_IntValue << ", Float Value: " << Obj.m_FloatValue << std::endl;
    }

    return 0;
}

```
