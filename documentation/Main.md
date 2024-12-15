<img src="https://i.imgur.com/GfJb3sQ.jpg" align="right" width="150px" />

# xProperties Documentation

Welcome to the xProperties documentation. This library provides a simple yet powerful reflection system and property system for C++ 20. It is designed to help developers manage properties in their C++ classes efficiently, making it easier to work with properties in game engines and applications.

## Getting Started

To get started with the xProperties library, follow the links below to explore various examples and detailed documentation.

### Simple Tutorial Examples

1. [Simple Example 01](SimpleExample01.md)
    - This example demonstrates how to define a class with properties, create an instance, set a property, and handle errors.

2. [Simple Example 02](SimpleExample02.md)
    - This example demonstrates how to use the `xproperty::sprop::collector` to collect and set multiple properties at once and print all properties as strings.

### Advanced Examples

Under the [Build](https://github.com/LIONant-depot/xproperty/tree/master/build) directory there is a solution for visual studio 2022 that contains a few examples on how to use the library. 
This example runs a basic unit-code and generates much of the documentation. Have a look!

## Documentation

- [Customization](Settings.md)
    - Detailed documentation on how to customize/setup xProperties library.

- [Documentation](DetailDocumentation.md)
    - Detailed documentation on how to use the xProperties library.

## Code folder overview

- **[Source [Dir]](../../tree/master/source)** - Main source code of the library
    - **[xproperty.h](https://github.com/LIONant-depot/xproperty/tree/master/source/xproperty.h)** - Single Header file for the library (**Customization needed**)
    - **[Examples [Dir]](https://github.com/LIONant-depot/xproperty/tree/master/source/examples)** - Here you will find examples on how to use the library
        - **[create_documentation [Dir]](https://github.com/LIONant-depot/xproperty/tree/master/source/examples/create_documentation)** - Generate most of the documentation found here, and a unitest.
        - **[imgui [Dir]](https://github.com/LIONant-depot/xproperty/tree/master/source/examples/imgui)** - Example on how to use the library with imgui
        - **[simple_examples [Dir]](https://github.com/LIONant-depot/xproperty/tree/master/source/examples/simple_examples)** - Simple examples to get started
        - **[xcore_sprop_serializer [Dir]](https://github.com/LIONant-depot/xproperty/tree/master/source/examples/xcore_sprop_serializer)** - Examples on how to serialize the properties using [xcore::textfile](https://gitlab.com/LIONant/xcore/-/blob/master/src/xcore_textfile.h)
    - **[sprop [Dir]](https://github.com/LIONant-depot/xproperty/tree/master/source/sprop)** - Extension to the xproperty library that adds more functionality (very useful)

## Feedback and Contributions

We welcome any feedback and contributions to the project. Please follow the links below to provide feedback or contribute to the project.

- [Feedback](https://github.com/LIONant-depot/xproperty/issues)
- [Contributions](https://github.com/LIONant-depot/xproperty)

### License

The xProperties library is licensed under the MIT License. For more details, please refer to the [license](https://opensource.org/licenses/MIT).

---

###### Thank you for using the xProperties library. We hope it helps you manage properties in your C++ projects efficiently. If you have any questions or need further assistance, please feel free to reach out.


