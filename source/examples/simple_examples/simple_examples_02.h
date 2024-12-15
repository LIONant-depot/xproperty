/*
# Simple Example

This example demonstrates how to:
1.	Define a class with properties using the xProperties library.
2.	Create an instance of the class.
3.	Use the xproperty::sprop::collector constructor to collect and set multiple properties at once.
4.	Print all the properties as strings

By following this example, you can learn how to use the `property_sprop_collector` to manage multiple properties in your C++ classes efficiently.

cpp */

// This is so we can print something in the screen
#include <iostream>

// includes the user settings used for other examples, this file will include the library as well
#include "../create_documentation/my_properties.h"

//  This include add very useful operation that while part of xproperty is more like an extension.
#include "../../sprop/property_sprop.h"

// Our example class
struct my_class
{
    int         m_IntValue      = 42;
    float       m_FloatValue    = 3.14f;
    std::string m_StringValue   = "Hello";

    // We can use this macro to define the properties. Note that we don't really need to use a macro
    // but it does minimize the amount of code we need to write.
    XPROPERTY_DEF
    ("MyClass", my_class                                       // String of the name of the class and the type of the class 
    , obj_member<"IntValue",    &my_class::m_IntValue>      // Define our interger property
    , obj_member<"FloatValue",  &my_class::m_FloatValue>    // Define our float property
    , obj_member<"StringValue", &my_class::m_StringValue>   // Define our string property
    )
};
XPROPERTY_REG(my_class)

int main()
{
    my_class                     Obj    {};     // Instance for the values
    std::string                  Error  {};     // Error message (if any)
    xproperty::settings::context Context{};     // Context is used for more advance features, and not always required
    xproperty::sprop::container  Container{};   // Container to hold all the collected properties

    // This will collect all the properties from Obj and put them in the Container
    xproperty::sprop::collector(Obj, Container, Context );

    // We print all the property inside the container.
    // We will print the names and values
    for( auto& E : Container.m_Properties )
    {
        // Place to store the string value of the property
        std::array<char, 128> Value;

        // Useful function to convert the value to a string
        xproperty::settings::AnyToString(Value, E.m_Value);

        // Print the name and the value
        std::cout << E.m_Path << " ==> Value: " << Value.data() << std::endl;
    }

    return 0;
}

/* cpp
*/