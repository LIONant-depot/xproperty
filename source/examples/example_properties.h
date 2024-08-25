/*
# Define properties

Creating a way to define properties can be an complex issue because you need to navigate correctly the following issues:
* Member variable access issues (protected, private, etc...)
* Access to the source code of the object (Some times the code is from a 3rd party)
* Code distance... been able to keep xproperty definitions close to the class
* Linkage issues
* Debug-ability issues. If there is a bug... how quickly can you detected it? compile-time/runtime/etc...
* Complex syntax, keeping it simple is hard...

The solution provided seems to be the most flexible as well as finding a fair compromise with all the above points.<br>

There are two kinds of xproperty definitions:

* opt-in definitions - (Optional In) This properties are the most noninvasive and flexible, but it comes with the limitations that are unaware of its descendents.
* base class definitions - (xproperty::base). This definition works similar to virtual functions by having a base class you can have access to the entire stack.
    But they are more restricted on how they get define.

## <a name="OptInProps.1"></a> Example 1 - Introduction to opt-in properties

However if you want don't care so much about object oriented hierarchies and want to have just
have properties for your class you can do the following. This specially works well for objects
that is not your code and you don't want to pollute it with other stuff.
cpp */
struct my_object1
{
    int m_Var = 0;
    void setValues()         { m_Var = 20; }
    void CheckValues() const { assert(m_Var == 20); }

    // Create the propery definition for this object
    // We can use this helpful macro if we like...
    XPROPERTY_DEF
    ( "My Object 1", my_object1     // String name of the object name and the actual type

    // member variables. Note that first is the name of the
    // variable and the second is a pointer to the variable
    , xproperty::obj_member<"var",          &my_object1::m_Var>

    // member functions are also supported
    , xproperty::obj_member<"setValues",    &my_object1::setValues>
    , xproperty::obj_member<"CheckValues",  &my_object1::CheckValues>
    )
};
// Once we have create the definition of our properties we need to register them
// again we can use another helpful macro for this (Note that both macros must be used together)
XPROPERTY_REG(my_object1)


/* [[my_object1]]

## <a name="Example0.2"></a> Example 2 - opt in, resolving friendship issues

Sometime we don't want to use macros because it may not help us debug things clearly.
This is how we will use just playing C++.
Note that friendship issues for variables are not a problem for properties.
cpp */
class my_object2
{
public:

    void setValues()         { m_Var = 20; }
    void CheckValues() const { assert(m_Var == 20); }

protected:

    int m_Var = 0;

    // This function can have any name you want
    // But should follow the same pattern
    // The main job of this function is to give us the 
    // property definition type
    public: static auto PropertiesDefinition() 
    {
        // note that this function should never be called anyways.
        // we only care about the return type
        assert(false);

        // We could add some name space here to help us with our 
        // property definitions to make it less verbose...
        using namespace xproperty; 

        // here we are going to define the return type and just return one object of it
        // note that this code should never execute...
        return xproperty::def 
        < "My Object 2", my_object2 
        , obj_member<"var",         &my_object2::m_Var>
        , obj_member<"setValues",   &my_object2::setValues>
        , obj_member<"CheckValues", &my_object2::CheckValues>
        >{};
    }
};
// We use this namespace to hide our global variable that we use for the actual registration
// NOTE: No one should have to accessing this variable directly!
namespace my_object2_props { inline const decltype(my_object2::PropertiesDefinition()) v{}; }

/* [[my_object2]]

## <a name="Example0.3"></a> Example 3 - opt in, resolving friendship issues when no access to the base class

There may be times that you may not have access to the source code of the class
that you wish to create properties for, or there may some friend-ship issues. 
Generating a fake object that derives from the original is a possible solution to both.
cpp */

class my_object3
{
public:
    void setValues()            { m_Var = 20; }
    void CheckValues() const    { assert(m_Var == 20); }

protected:
    int m_Var = 0;
};

// We will never instantiate this class
// can also be named anything you want
// and can be a class or a struct...
class my_object_3_fake_friend : public my_object3
{    
    XPROPERTY_DEF
    ( "My Object 3", my_object3  // Note that we need to specify the original type
    , obj_member<"var",           &my_object_3_fake_friend::m_Var>
    , obj_member<"setValues",     &my_object_3_fake_friend::setValues>
    , obj_member<"CheckValues",   &my_object_3_fake_friend::CheckValues>
    )
};
XPROPERTY_REG(my_object_3_fake_friend)

/* [[my_object3]]
<br>

# Hierarchical properties

## <a name="Example1.1"></a> Example 1 - How to specify the base classes for opt-in properties

This example shows the very basics of adding properties using base classes to your object.
As long as you know the final derived classes the xproperty system can walk its base class properties.

cpp */
// Note that the base object is from the previous example
struct derived1 : my_object1, my_object2            
{
    int m_Var2 = 0;

    void setValues()
    {
        m_Var2 = 20;
        my_object1::setValues();
        my_object2::setValues();
    }

    void CheckValues() const
    {
        assert(m_Var2 == 20);
        my_object1::CheckValues();
        my_object2::CheckValues();
    }

    XPROPERTY_DEF
    ( "Derived1", derived1

    // Note that we are adding the base class here
    // We could add as many bases as we want...
    // However usually matches the object itself.
    , obj_base<my_object1>
    , obj_base<my_object2>

    // Only need to add our own member variables
    , obj_member<"var2",        &derived1::m_Var2>

    // And/Or member functions
    , obj_member<"setValues",   &derived1::setValues>
    , obj_member<"CheckValues", &derived1::CheckValues>
    )
};
XPROPERTY_REG(derived1)

/* [[derived1]]

## <a name="Example1.2"></a> Example 2 - Defining properties with xproperty::base

(Virtual Hierarchical properties)
In C++ we have the concept of base object with virtual functions. This means that we
should support only having a base class and still recognizance the entire hierarchy. This is
done in cpp with virtual functions. We can also support that but your base class must include
the 'xproperty::base' in your base class. This will indicate that we are going to be using 
virtual functions. Also you need to override the 'getProperties' function.

cpp */
struct base1 : xproperty::base
{
    int m_Var = 0;

    void setValues()
    {
        m_Var = 20;
    }

    void CheckValues() const
    {
        assert(m_Var == 20);
    }

    // Since we are dealing with virtual properties we must specidy the right macro;
    // Stead of using 'DEF' we will use 'VDEF' at the end of the macro name.
    // This will generate the require virtual functions to make the system work correctly
    XPROPERTY_VDEF
    ( "Base1", base1
    // We do not need to add: obj_base<xproperty::base>
    // because xproperty::base has no properties at all...
    , obj_member<"var",         &base1::m_Var>
    , obj_member<"setValues",   &base1::setValues>
    , obj_member<"CheckValues", &base1::CheckValues>
    )
};
// NOTE: Since we are dealing with a virtual properties we need to change the name of the 
// registration macro. This one has the 'VREG' stead of the 'REG' at the end.
XPROPERTY_VREG(base1)


/* End of the definition of base1
```
Now the example of a derived class
cpp */
struct derived2 : base1
{
    int m_Var2 = 0;

    void setValues()
    {
        m_Var2 = 40;
        base1::setValues();
    }

    void CheckValues() const
    {
        assert(m_Var2 == 40);
        base1::CheckValues();
    }

    // This class still part of the virtual property hirarchy so we must use the 'VDEF' macro
    XPROPERTY_VDEF
    ( "Derived2", derived2

    // NOTR: that we are adding the base class here
    // since base1 does have properties that we need to know about
    , obj_base<base1>

    , obj_member<"var2",        &derived2::m_Var2>
    , obj_member<"setValues",   &derived2::setValues>
    , obj_member<"CheckValues", &derived2::CheckValues>
    )
};
XPROPERTY_VREG(derived2)


/* [[derived2]]
<br>

# PropertyObject Types

## <a name="Example3.1"></a> Example 1 - Categories of xproperty variables & const

There are two main groups of variables that we support. Variables that hold values and
variables that hold more properties. The first group is the most common and is the one,
but sometimes you want to have a list of objects that have properties. 

cpp */
struct common_types
{
    // Holds values
    int     m_ValueHoldingVar       = 0;

    // Holds properties (base1 is from the previous example)
    base1   m_PropertyHoldingVar    = {};   

    // Holds values but since it is const you can not change this var
    const int m_ReadOnlyValue       = 100;  

    // Holds properties but since it is const you can not change these values
    const base1 m_ReadOnlyProps     = []{ base1 a; a.setValues(); return a; }();

    void setValues()
    {
        // We just set the value of this xproperty
        m_ValueHoldingVar = 40;

        // We set the values of the properties of this object
        m_PropertyHoldingVar.setValues();   
    }

    void CheckValues() const
    {
        assert(m_ValueHoldingVar == 40);
        m_PropertyHoldingVar.CheckValues();
        assert(m_ReadOnlyValue == 100);
        m_ReadOnlyProps.CheckValues();
    }

    XPROPERTY_DEF
    ( "CommonTypes", common_types
    , obj_member<"m_ValueHoldingVar",       &common_types::m_ValueHoldingVar>
    , obj_member<"m_PropertyHoldingVar",    &common_types::m_PropertyHoldingVar >

    // Properties that are const will be automatically be read-only
    , obj_member<"m_ReadOnlyValue",         &common_types::m_ReadOnlyValue >
    , obj_member<"m_ReadOnlyProps",         &common_types::m_ReadOnlyProps >

    // However we can also force the issue if we want
    , obj_member_ro<"ForceReadOnlyVar",     &common_types::m_ValueHoldingVar>
    , obj_member_ro<"ForceReadOnlyProps",   &common_types::m_PropertyHoldingVar>

    // We can also add member functions
    , obj_member<"setValues",               &common_types::setValues>
    , obj_member<"CheckValues",             &common_types::CheckValues>
    );
};
XPROPERTY_REG(common_types)

/* [common_types]]
<br>

# Enums

## <a name="Example.Enum.1"></a> Example 1 - Unregistered enums

Enums are a special case because you can have many types of enums in your classes yet you may not want
to make them an Atomic type every time... For that we have the concept of unregistered enums. These are
enums that have not been register by the system manually and so the system has to deal with them in
another way. To solve this problem the system will register their types and value pairs (string, value)
as you define the properties. This simplifies the usage of enums allot.
cpp */

struct enums_unregistered
{
    enum example1
    {
        A1, A2, A3
    };

    enum example2 : std::uint16_t
    {
        B1, B2
    };

    enum class example3 : std::uint8_t
    {
        C1, C2, C3
    };

    example1    m_EnumA = {};
    example2    m_EnumB = {};
    example3    m_EnumC = {};

    void setValues()
    {
        m_EnumA = A2;
        m_EnumB = example2::B2;
        m_EnumC = example3::C3;
    }

    void CheckValues() const
    {
        assert(m_EnumA == A2);
        assert(m_EnumB == example2::B2);
        assert(m_EnumC == example3::C3);
    }
    
    // We can also create a list of items for unregistered enums and
    // pass this to the xproperty. This is useful when you have multiple
    // variables in a class that use the same enum. That way you don't
    // need to list all the items again and again...
    // MAKE sure that it is marked as constexpr static and that it is
    // a std::array of enum_item.
    static constexpr auto enum_b_list_v = std::array
    { xproperty::settings::enum_item{ "B1",  example2::B1 }
    , xproperty::settings::enum_item{ "B2",  example2::B2 }
    };

    XPROPERTY_DEF
    ( "Enum Unregistered", enums_unregistered

    // This is the first way to handle unregistered enums.
    // It is simple and direct and solves the most common use case.
    , obj_member<"m_EnumA", &enums_unregistered::m_EnumA
                          , member_enum_value<"A1", A1>
                          , member_enum_value<"A2", A2>
                          , member_enum_value<"A3", A3>
                          >

    // This is the second way to handle unregistered enums
    // we use the previous constexpr static array and pass it
    // to the xproperty. Note that you can not mix both methods!!!
    , obj_member<"m_EnumB", &enums_unregistered::m_EnumB
                          , member_enum_span<enum_b_list_v>
                          >

    // This is another enum..l for fun... 
    , obj_member<"m_EnumC", &enums_unregistered::m_EnumC
                          , member_enum_value<"C1", example3::C1>
                          , member_enum_value<"C2", example3::C2>
                          , member_enum_value<"C3", example3::C3>
                          >

    // We can also add member functions
    , obj_member<"setValues",   &enums_unregistered::setValues>
    , obj_member<"CheckValues", &enums_unregistered::CheckValues>
    );
};
XPROPERTY_REG(enums_unregistered)

/* [[enums_unregistered]]

## <a name="Example.Enum.2"></a> Example 2 - Registered Enums

Sometimes an enum is used across many classes or members and becomes important enough to be registered
as an official atomic type. This is when registered enums make sense. This must be done inside the
name space of the xproperty system for it to know about the type as seem in the example.

cpp */

// Here is an example of an enum that could be located somewhere else
// The name space is of no relevance and just to show it could be place any where...
namespace registered_enum
{
    // This enum can look like anything you like...
    // but we want to register this enum as an official atomic type
    enum class example : std::uint8_t
    { INVALID
    , VALID_VALUE_1
    , VALID_VALUE_2
    , VALID_VALUE_3
    };
}

// Here is where we define our enum... notice the patterns
// the first thing we define is an specialization for our var_type template
// (Note that is inside the name space xproperty::settings)
// this allows the system to find the type.
// The var_defaults is a helper class that helps us define it.
// The first thing you will see is the name of the enum in a string form...
// this could be anything you like. the second thing is our type again...
template<>
struct xproperty::settings::var_type<registered_enum::example>
    : var_defaults< "example", registered_enum::example >
{
    // inside our structure we must define our trait enum_list_v
    // array of string and values pairs
    // please note the exact definition and mirror it.
    inline static constexpr auto enum_list_v = std::array
    { enum_item{ "INVALID",         registered_enum::example::INVALID       }
    , enum_item{ "VALID_VALUE_1",   registered_enum::example::VALID_VALUE_1 }
    , enum_item{ "VALID_VALUE_2",   registered_enum::example::VALID_VALUE_2 }
    , enum_item{ "VALID_VALUE_3",   registered_enum::example::VALID_VALUE_3 }
    };
};

/*
```
Now we define our class so we can use our new register enum
cpp */

struct enums_registered
{
    // We define anohter unregistered enum here to mix things up...
    enum unreg
    { UNREG_V1
    , UNREG_V2
    , UNREG_V3
    };

    // Note that we can create the list of the unregistered items
    // here if we wish too.
    inline static constexpr auto unreg_enum_list_v = std::array
    { xproperty::settings::enum_item{ "V1-Invalid",  unreg::UNREG_V1 }
    , xproperty::settings::enum_item{ "V2",          unreg::UNREG_V2 }
    , xproperty::settings::enum_item{ "V3",          unreg::UNREG_V3 }
    };


          registered_enum::example  m_Value01   = registered_enum::example::INVALID;
    const registered_enum::example  m_CValue01  = registered_enum::example::VALID_VALUE_1;

          unreg                     m_Value02   = {};
    const unreg                     m_CValue02  = UNREG_V2;

    void setValues()
    {
        m_Value01 = registered_enum::example::VALID_VALUE_2;
        m_Value02 = UNREG_V3;
    }

    void CheckValues() const
    {
        assert(m_Value01 == registered_enum::example::VALID_VALUE_2);
        assert(m_Value02 == UNREG_V3);

        assert(m_CValue01 == registered_enum::example::VALID_VALUE_1);
        assert(m_CValue02 == UNREG_V2);
    }

    XPROPERTY_DEF
    ( "Enum Registered", enums_registered

    // These are the registered enums as you can see there is
    // no need to add the enum values here... 
    , obj_member<"m_Value01", &enums_registered::m_Value01 >
    , obj_member<"m_CValue",  &enums_registered::m_CValue01 >

    // Unregistered enums
    , obj_member<"m_Value02", &enums_registered::m_Value02
                            , member_enum_span<unreg_enum_list_v>
                            >

    // Here we would need to repeat the enum again... but since 
    // it is a const and can only really have one value 
    , obj_member<"m_CValue", &enums_registered::m_CValue02
                           , member_enum_span<unreg_enum_list_v>
                           >

    // We can also add member functions
    , obj_member<"setValues",   &enums_registered::setValues>
    , obj_member<"CheckValues", &enums_registered::CheckValues>
    )
};
XPROPERTY_REG(enums_registered)

/* [[enums_registered]]
<br>

# Pointers and references

## <a name="Example4.1"></a> Example 1 - C - Style pointers and references Values

We have seem so far we support basic variables however we also support pointers and references.
The user can also define pointer types beyond what the standard provides. But we will talk about
that in another section.<br>

Since in C++ you can not get the address of a reference we have to use a lambda to solve this problem.
Note that we use the operator '+' with the lambda to force the lambda object into a function pointer.
cpp */

struct pointer_and_reference_c_style_values
{
    using enum_t   = registered_enum::example;
    using unenum_t = enums_registered::unreg;

          int         m_Int     = 0;
          int*        m_pInt    = &m_Int;
          int**       m_ppInt   = &m_pInt;
          int&        m_IntRef  = m_Int;

    const int*        m_CpInt   = &m_Int;
    const int**       m_CppInt  = &m_CpInt;
    const int&        m_CIntRef = m_Int;
    const int* const  m_CpCInt  = &m_Int;
    const int** const m_CppCInt = &m_CpInt;

          enum_t      m_Enum    = {};
          enum_t*     m_pEnum   = &m_Enum;
          enum_t&     m_EnumRef = m_Enum;
    const enum_t*     m_CpEnum  = &m_Enum;

          unenum_t    m_UnEnum    = {};
          unenum_t*   m_pUnEnum   = &m_UnEnum;
          unenum_t&   m_RefUnEnum = m_UnEnum;

    void setValues()
    {
        m_Int       = 100;
        m_Enum      = enum_t::VALID_VALUE_2;
        m_UnEnum    = unenum_t::UNREG_V3;
    }

    void CheckValues() const
    {
        assert(  m_Int     == 100 );
        assert(  m_pInt    == &m_Int );
        assert(  m_ppInt   == &m_pInt );
        assert(  m_CpInt   == &m_Int );
        assert(  m_CppInt  == &m_CpInt);
        assert(  m_Enum    == enum_t::VALID_VALUE_2);
        assert(  m_pEnum   == &m_Enum );
        assert(  m_CpEnum  == &m_Enum );

        assert(m_UnEnum == unenum_t::UNREG_V3);
        assert(m_pUnEnum == &m_UnEnum);
    }

    XPROPERTY_DEF
    ( "Pointer and References C Style Values"
    , pointer_and_reference_c_style_values
    , obj_member<"m_Int",    &pointer_and_reference_c_style_values::m_Int >

    // C-Pointer properties look the same
    , obj_member<"m_pInt",   &pointer_and_reference_c_style_values::m_pInt >
    , obj_member<"m_ppInt",  &pointer_and_reference_c_style_values::m_ppInt>

    // Notice the reference lambda here. We use the '+' operator to force 
    // the lambda into a function pointer. Inside the function is very simple.
    // Note that the constexpr is optional but recommended
    , obj_member<"m_IntRef", +[](pointer_and_reference_c_style_values& O)
                             constexpr ->auto& { return O.m_IntRef; } >
    , obj_member<"m_CpInt",  &pointer_and_reference_c_style_values::m_CpInt >
    , obj_member<"m_CppInt", &pointer_and_reference_c_style_values::m_CppInt >
    , obj_member<"m_CIntRef", +[]( pointer_and_reference_c_style_values& O)
                              constexpr ->auto& { return O.m_CIntRef; } >
    , obj_member<"m_CpCInt",  &pointer_and_reference_c_style_values::m_CpCInt >
    , obj_member<"m_CppCInt", &pointer_and_reference_c_style_values::m_CppCInt>

    , obj_member<"m_Enum",  &pointer_and_reference_c_style_values::m_Enum >
    , obj_member<"m_pEnum", &pointer_and_reference_c_style_values::m_pEnum>
    , obj_member<"m_EnumRef", +[](pointer_and_reference_c_style_values& O)
                              constexpr ->auto& { return O.m_EnumRef; } >
    , obj_member<"m_CpEnum", &pointer_and_reference_c_style_values::m_CpEnum>

    , obj_member<"m_UnEnum", &pointer_and_reference_c_style_values::m_UnEnum
                           , member_enum_span<enums_registered::unreg_enum_list_v>
                           >
    , obj_member<"m_pUnEnum", &pointer_and_reference_c_style_values::m_pUnEnum
                            , member_enum_span<enums_registered::unreg_enum_list_v>
                            >
    , obj_member < "m_RefUnEnum", +[](pointer_and_reference_c_style_values& O)
                                  constexpr ->auto& { return O.m_RefUnEnum; }
                                , member_enum_span<enums_registered::unreg_enum_list_v>
                                >
    , obj_member<"setValues",    &pointer_and_reference_c_style_values::setValues>
    , obj_member<"CheckValues",  &pointer_and_reference_c_style_values::CheckValues>
    )
};
XPROPERTY_REG(pointer_and_reference_c_style_values)

/* [[pointer_and_reference_c_style_values]]

## <a name="Example4.2"></a> Example 2 - C - Style pointers and references Props

We have seem so far we support basic variables however we also support pointers and references.
The user can also define pointer types beyond what the standard provides. But we will talk about
that in another section.<br>

Since in C++ you can not get the address of a reference we have to use a lambda to solve this problem.
Note that we use the operator '+' with the lambda to force the lambda object into a function pointer.
cpp */
struct pointer_and_reference_c_style_props
{
          base1     m_Other         = {};
          base1&    m_OtherRef      = m_Other;
          base1*    m_pOther        = &m_Other;
          base1*&   m_ppOtherRef    = m_pOther;
          base1**   m_ppOther       = &m_pOther;

    const base1&    m_COtherRef     = m_Other;
    const base1*    m_CpOther       = &m_Other;
    const base1**   m_CppOther      = &m_CpOther;

    void setValues()
    {
        m_Other.setValues();
    }

    void CheckValues() const
    {
        m_Other.CheckValues();
        assert( m_pOther   == &m_Other   );
        assert( m_ppOther  == &m_pOther  );
        assert( m_CpOther  == &m_Other   );
        assert( m_CppOther == &m_CpOther );
    }

    XPROPERTY_DEF
    ( "Pointer and References C Style Props"
    , pointer_and_reference_c_style_props
    , obj_member<"Other",    &pointer_and_reference_c_style_props::m_Other>

    // Another reference... the pattern is the same as before
    , obj_member<"OtherRef", +[](pointer_and_reference_c_style_props& O)
                             constexpr ->auto& { return O.m_OtherRef;} >
    , obj_member<"m_pOther", &pointer_and_reference_c_style_props::m_pOther>
    , obj_member<"m_ppOther",&pointer_and_reference_c_style_props::m_ppOther>

    // Reference of a pointer... 
    , obj_member<"m_ppOtherRef", +[](pointer_and_reference_c_style_props& O )
                                 constexpr ->auto& { return O.m_ppOtherRef; }>
    , obj_member<"m_COtherRef", +[](pointer_and_reference_c_style_props& O)
                                constexpr ->auto& { return O.m_COtherRef; } >
    , obj_member<"m_CpOther", &pointer_and_reference_c_style_props::m_CpOther>
    , obj_member<"m_CppOther",&pointer_and_reference_c_style_props::m_CppOther>

    , obj_member<"setValues", &pointer_and_reference_c_style_props::setValues>
    , obj_member<"CheckValues", &pointer_and_reference_c_style_props::CheckValues>
    )
};
XPROPERTY_REG(pointer_and_reference_c_style_props)

/* [[pointer_and_reference_c_style_props]]

## <a name="Example4.3"></a> Example 3 - C++ Style of pointers

We can also support pointers and references to objects that have properties. They look very similar as before.
cpp */
struct pointers_and_references_cpp_style
{
    using enum_t   = registered_enum::example;
    using unenum_t = enums_registered::unreg;

    std::unique_ptr<int>                    m_upInt         = std::make_unique<int>();
    std::unique_ptr<std::unique_ptr<int>>   m_uupInt        = std::make_unique<std::unique_ptr<int>>(std::make_unique<int>());
    std::shared_ptr<std::unique_ptr<int>>   m_supInt        = std::make_shared<std::unique_ptr<int>>(std::make_unique<int>());

    std::unique_ptr<base1>                  m_upOther       = std::make_unique<base1>();
    std::unique_ptr<std::unique_ptr<base1>> m_uupOther      = std::make_unique<std::unique_ptr<base1>>(std::make_unique<base1>());
    std::shared_ptr<std::unique_ptr<base1>> m_supOther      = std::make_shared<std::unique_ptr<base1>>(std::make_unique<base1>());

    std::unique_ptr<base1>&                 m_RefupOther    = m_upOther;
    std::unique_ptr<const base1>            m_CupOther      = []{ auto v = std::make_unique<base1>(); v->setValues(); return v; }();
    std::unique_ptr<std::unique_ptr<const base1>> m_CCupOther = []{ base1 x; x.setValues(); auto v = std::make_unique<std::unique_ptr<const base1>>(std::make_unique<base1>(x)); return v; }();

    std::unique_ptr<enum_t>                  m_upEnum       = std::make_unique<enum_t>();
    std::unique_ptr<std::unique_ptr<enum_t>> m_uupEnum      = std::make_unique<std::unique_ptr<enum_t>>(std::make_unique<enum_t>());
    std::unique_ptr<unenum_t>                m_UupEnum      = std::make_unique<unenum_t>();
    std::unique_ptr<std::unique_ptr<unenum_t>> m_UuupEnum   = std::make_unique<std::unique_ptr<unenum_t>>(std::make_unique<unenum_t>());

    void setValues()
    {
        *m_upInt = 64;
        *(*m_uupInt) = 646;
        *(*m_supInt) = 1646;

        m_upOther->setValues();
        (*m_uupOther)->setValues();
        (*m_supOther)->setValues();

        *m_upEnum       = enum_t::VALID_VALUE_3;
        *(*m_uupEnum)   = enum_t::VALID_VALUE_2;
        *m_UupEnum      = unenum_t::UNREG_V3;
        *(*m_UuupEnum)  = unenum_t::UNREG_V2;
    }

    void CheckValues() const
    {
        assert( *m_upInt           == 64 );
        assert( *(*m_uupInt.get()) == 646);
        assert( *(*m_supInt.get()) == 1646);

        m_upOther->CheckValues();
        m_uupOther->get()->CheckValues();
        m_supOther->get()->CheckValues();
        m_CupOther->CheckValues();
        m_CCupOther->get()->CheckValues();

        assert(*m_upEnum == enum_t::VALID_VALUE_3);
    }

    XPROPERTY_DEF
    ( "Pointers and Reference C++ Style"
    , pointers_and_references_cpp_style
    // Unique/Share pointers are also supported
    , obj_member<"m_upInt", &pointers_and_references_cpp_style::m_upInt >
    , obj_member<"m_uupInt", &pointers_and_references_cpp_style::m_uupInt>
    , obj_member<"m_supInt", &pointers_and_references_cpp_style::m_supInt>

    , obj_member<"m_upOther", &pointers_and_references_cpp_style::m_upOther>
    , obj_member<"m_uupOther", &pointers_and_references_cpp_style::m_uupOther>

    // Mixing unique and share pointers... 
    , obj_member<"m_supOther", &pointers_and_references_cpp_style::m_supOther>
    , obj_member < "m_RefupOther", +[](pointers_and_references_cpp_style& O)
                                   constexpr ->auto& { return O.m_RefupOther; } >
    , obj_member<"m_CupOther", &pointers_and_references_cpp_style::m_CupOther>
    , obj_member<"m_CCupOther", &pointers_and_references_cpp_style::m_CCupOther>

    , obj_member<"m_upEnum", &pointers_and_references_cpp_style::m_upEnum>
    , obj_member<"m_uupEnum", &pointers_and_references_cpp_style::m_uupEnum>
    , obj_member<"m_UupEnum", &pointers_and_references_cpp_style::m_UupEnum
                            , member_enum_span<enums_registered::unreg_enum_list_v> 
                            >
    , obj_member<"m_UuupEnum", &pointers_and_references_cpp_style::m_UuupEnum
                             , member_enum_span<enums_registered::unreg_enum_list_v>
                             >

    , obj_member<"setValues", &pointers_and_references_cpp_style::setValues>
    , obj_member<"CheckValues", &pointers_and_references_cpp_style::CheckValues>
    )
};
XPROPERTY_REG(pointers_and_references_cpp_style)

/* [[pointers_and_references_cpp_style]]
<br>

# Lists

## <a name="Example5.1"></a> Example 1 - C-Style arrays

Having a list of things is very common in C++. We support this by having a list of properties.
There are many types of lists possible... C++ include things like std::vector, std::array, std::map, std::list, etc.
But there are also 'C' style arrays. We support all of them.

cpp */

struct list_c_arrays
{
    using enum_t   = registered_enum::example;
    using unenum_t = enums_registered::unreg;

          float     m_c1ListA [4]           = {};
          float     m_c2ListA [2][3]        = {};
          float     m_c3ListA [2][3][1]     = {};
    const float     m_Cc1ListA[2]           = { 2,5 };

    // Reference to a C-array
          float   (&m_Refc1ListA)[4]        = m_c1ListA;

          enum_t    m_EnumList [2]          = {};
          unenum_t  m_UEnumList[2]          = {};

          base1     m_c1ListB [2]           = {};
          base1     m_c2ListB [2][3]        = {};
          base1     m_c3ListB[1][1][1]      = {};
    const base1     m_Cc1ListB[2]           = 
                    {[]{base1 a; a.setValues(); return a; }()
                    ,[]{base1 a; a.setValues(); return a; }() };

          base1   (&m_Refc1ListB)[2]        = m_c1ListB;

    void setValues()
    {
        m_c1ListA[0]        = 0.13f; m_c1ListA[1]       = 0.14f;
        m_c1ListA[2]        = 0.15f; m_c1ListA[3]       = 0.16f;

        m_c2ListA[0][0]     = 0.13f; m_c2ListA[0][1]    = 0;
        m_c2ListA[0][2]     = 0;     m_c2ListA[1][0]    = 0.14f;
        m_c2ListA[1][1]     = 0.15f; m_c2ListA[1][2]    = 0.16f;

        m_c3ListA[0][0][0]  = 0.13f; m_c3ListA[0][1][0] = 0;
        m_c3ListA[0][2][0]  = 0;     m_c3ListA[1][0][0] = 0.14f;
        m_c3ListA[1][1][0]  = 0.15f; m_c3ListA[1][2][0] = 0.16f;

        m_EnumList[0] = enum_t::VALID_VALUE_1;
        m_EnumList[1] = enum_t::VALID_VALUE_2;

        m_UEnumList[0] = enums_registered::UNREG_V2;
        m_UEnumList[1] = enums_registered::UNREG_V3;

        for( auto& E : m_c1ListB ) 
            E.setValues();

        for( auto& E : m_c2ListB ) for( auto& E2 : E ) 
            E2.setValues();

        for( auto& E : m_c3ListB ) for( auto& E2 : E ) for( auto& E3 : E2 ) 
            E3.setValues();
    }

    void CheckValues() const
    {
        list_c_arrays Ref;
        Ref.setValues();

        for( int i=0; i< 4; ++i) 
            assert(m_c1ListA[i] == Ref.m_c1ListA[i]);

        for( int i=0; i< 2; ++i) for( int j=0; j< 3; ++j ) 
            assert(m_c2ListA[i][j] == Ref.m_c2ListA[i][j]);

        for( int i=0; i< 2; ++i) for( int j=0; j< 3; ++j ) for( int k=0; k< 1; ++k ) 
            assert(m_c3ListA[i][j][k] == Ref.m_c3ListA[i][j][k]);

        for (int i = 0; i < 2; ++i)
            assert(m_EnumList[i] == Ref.m_EnumList[i]);

        for (int i = 0; i < 2; ++i)
            assert(m_UEnumList[i] == Ref.m_UEnumList[i]);

        for (auto& E : m_c1ListB) 
            E.CheckValues();

        for (auto& E : m_c2ListB) for (auto& E2 : E) 
            E2.CheckValues();

        for (auto& E : m_c3ListB) for (auto& E2 : E) for (auto& E3 : E2) 
            E3.CheckValues();
    }

    XPROPERTY_DEF
    ( "Lists - C Arrays", list_c_arrays
    , obj_member<"m_c1ListAC",  &list_c_arrays::m_c1ListA >
    , obj_member<"m_c2ListAC",  &list_c_arrays::m_c2ListA >
    , obj_member<"m_c3ListAC",  &list_c_arrays::m_c3ListA >
    , obj_member<"m_Refc1ListA", +[](list_c_arrays& O) constexpr
                                 ->auto& { return O.m_Refc1ListA; } >
    , obj_member<"m_Cc1ListA", &list_c_arrays::m_Cc1ListA >
    , obj_member<"m_EnumList", &list_c_arrays::m_EnumList >
    , obj_member<"m_UEnumList",&list_c_arrays::m_UEnumList
                              , member_enum_span<enums_registered::unreg_enum_list_v>
                              >

    , obj_member<"m_c1ListBC",  &list_c_arrays::m_c1ListB >
    , obj_member<"m_c2ListBC",  &list_c_arrays::m_c2ListB >
    , obj_member<"m_c3ListBC",  &list_c_arrays::m_c3ListB >
    , obj_member<"m_Cc1ListB", &list_c_arrays::m_Cc1ListB >
    , obj_member<"m_Refc1ListB", +[](list_c_arrays& O) constexpr
                                 ->auto& { return O.m_Refc1ListB; } >

    , obj_member<"setValues",    &list_c_arrays::setValues >
    , obj_member<"CheckValues",  &list_c_arrays::CheckValues >
    )
};
XPROPERTY_REG(list_c_arrays)

/* [[list_c_arrays]]

## <a name="Example5.2"></a> Example 2 - C++ Style Lists

Having a list of things is very common in C++. We support this by having a list of properties.
There are many types of lists possible... C++ include things like std::vector, std::array, std::map, std::list, etc.
But there are also 'C' style arrays. We support all of them.

cpp */

struct lists_cpp
{
    using enum_t   = registered_enum::example;
    using unenum_t = enums_registered::unreg;

          std::vector<base1>                    m_vListA    = {};
          std::vector<std::vector<base1>>       m_vvListA   = {};
          std::vector<base1>&                   m_vListARef = m_vListA;
          std::vector<std::unique_ptr<base1>>   m_vListAup  = {};
          std::array<std::vector<base1>, 3>     m_avListA   = {};
    const std::vector<base1>                    m_CvListA   = 
            { []{ base1 a; a.setValues(); return a;}() };

          std::vector<float>                    m_vListB    = {};
          std::vector<std::vector<float>>       m_vvListB   = {};
          std::vector<float>&                   m_vListBRef = m_vListB;
          std::vector<std::unique_ptr<float>>   m_vListBup  = {};
          std::array<std::vector<float>,3>      m_avListB   = {};
          std::vector<enum_t>                   m_Enum      = {};
          std::vector<unenum_t>                 m_UEnum     = {};

    const std::array<std::vector<float>, 1>     m_CavListB  = {{{0.5f}}};
          std::array<const float, 2>            m_CvListB   = { 0.5f, 9.6f };

    void setValues()
    {
        m_vListA.resize(4);
        for (auto& E : m_vListA) 
            E.setValues();

        m_vvListA.resize(3);
        for( auto& E : m_vvListA ) 
        { E.resize(2);
          for( auto& E2 : E ) 
             E2.setValues();
        }

        m_vListAup.resize(3);
        for (auto& E : m_vListAup) 
        { E = std::make_unique<base1>();
          E->setValues();
        }

        for (auto& E : m_avListA)  
        { E.resize(2);
          for( auto& E2 : E ) 
            E2.setValues();
        }

        m_vListB   = { 0.3f, 0.4f, 0.5f, 0.6f };
        m_vvListB  = { { 0.3f, 0.4f }
                     , { 0.88f }
                     , { 0.05f, 0.06f }
                     };

        m_vListBup.resize(3);
        for( auto& E : m_vListBup ) 
           E = std::make_unique<float>(0.3f);

        m_avListB = { std::vector<float>{ 0.3f, 0.4f }
                    , std::vector<float>{ 0.88f }
                    , std::vector<float>{ 0.05f, 0.06f }
                    };

        m_Enum = { enum_t::VALID_VALUE_1
                 , enum_t::VALID_VALUE_2
                 , enum_t::VALID_VALUE_3 };

        m_UEnum = { unenum_t::UNREG_V3
                  , unenum_t::UNREG_V1
                  , unenum_t::UNREG_V2 };

    }

    void CheckValues() const
    {
        lists_cpp Ref;
        Ref.setValues();

        assert(Ref.m_vListA.size() == m_vListA.size());
        for (auto& E : m_vListA) E.CheckValues();

        assert(Ref.m_vvListA.size() == m_vvListA.size());
        for (auto& E : m_vvListA) 
        { assert(E.size() == 2);
            for( auto& E2 : E ) E2.CheckValues();
        }

        assert(Ref.m_vListAup.size() == m_vListAup.size());
        for( auto& E : m_vListAup )
            E->setValues();

        for(auto& E : m_avListA)   
        { assert(E.size() == 2);
            for( auto& E2 : E ) E2.CheckValues();
        }

        for(auto i=0u; i< m_vListB.size(); ++i) 
            assert(m_vListB[i] == Ref.m_vListB[i]);

        for( auto i=0u; i< m_vvListB.size(); ++i) 
        { assert(m_vvListB[i].size() == Ref.m_vvListB[i].size());
          for( auto j=0u; j< m_vvListB[i].size(); ++j )
             assert(m_vvListB[i][j] == Ref.m_vvListB[i][j]);
        }

        for( auto i=0u; i< m_vListBup.size(); ++i) 
            assert( *Ref.m_vListBup[i] == *m_vListBup[i] );

        for( auto i=0u; i< m_avListB.size(); ++i) 
        { assert(m_avListB[i].size() == Ref.m_avListB[i].size());
          for( auto j=0u; j< m_avListB[i].size(); ++j ) 
             assert(m_avListB[i][j] == Ref.m_avListB[i][j]);
        }

        for (auto i = 0u; i < m_Enum.size(); ++i)
            assert(m_Enum[i] == Ref.m_Enum[i]);

        for (auto i = 0u; i < m_UEnum.size(); ++i)
            assert(m_UEnum[i] == Ref.m_UEnum[i]);
    }

    XPROPERTY_DEF
    ( "Lists C++", lists_cpp
    , obj_member<"m_vListA",    &lists_cpp::m_vListA >
    , obj_member<"m_vvListA",   &lists_cpp::m_vvListA >
    , obj_member<"m_vListARef", +[](lists_cpp& O) constexpr
                                ->auto& { return O.m_vListARef; } >
    , obj_member<"m_vListAup",  &lists_cpp::m_vListAup >
    , obj_member<"m_CvListA",   &lists_cpp::m_CvListA >

    , obj_member<"m_avListA",   &lists_cpp::m_avListA >
    , obj_member<"m_vListB",    &lists_cpp::m_vListB >
    , obj_member<"m_vvListB",   &lists_cpp::m_vvListB >
    , obj_member<"m_vListBRef", +[](lists_cpp& O) constexpr
                                ->auto& { return O.m_vListBRef; } >
    , obj_member<"m_vListBup",  &lists_cpp::m_vListBup >
    , obj_member<"m_avListB",   &lists_cpp::m_avListB >
    , obj_member<"m_CavListB",  &lists_cpp::m_CavListB >
    , obj_member<"m_CvListB",   &lists_cpp::m_CvListB >

    , obj_member<"m_Enum",     &lists_cpp::m_Enum >
    , obj_member<"m_UEnum",    &lists_cpp::m_UEnum
                ,               member_enum_span<enums_registered::unreg_enum_list_v>>
    , obj_member<"setValues",   &list_c_arrays::setValues >
    , obj_member<"CheckValues", &list_c_arrays::CheckValues >
    );
};
XPROPERTY_REG(lists_cpp)

/* [[lists_cpp]]

## <a name="Example5.3"></a> Example 3 - Advance List types

Having a list of things is very common in C++. We support this by having a list of properties.
There are many types of lists possible... C++ include things like std::vector, std::array, std::map, std::list, etc.
But there are also 'C' style arrays. We support all of them.

cpp */

struct lists_advance
{
    std::map<std::uint32_t, base1>          m_mListA    = {};
    std::map<std::string, int>              m_mListB    = {};
    xproperty::settings::llist<std::string>  m_llListC   = {};

    void setValues()
    {
        m_mListA[xproperty::settings::strguid("A")].setValues();
        m_mListA[xproperty::settings::strguid("B")].setValues();
        m_mListA[xproperty::settings::strguid("C")].setValues();
        m_mListA[xproperty::settings::strguid("D")].setValues();

        m_mListB["A"] = { 1 };
        m_mListB["B"] = { 2 };
        m_mListB["C"] = { 3 };

        m_llListC.push_front("A");
        m_llListC.push_front("B");
        m_llListC.push_front("C");
        m_llListC.push_front("D");
    }

    void CheckValues() const
    {
        lists_advance Ref;
        Ref.setValues();

        assert(Ref.m_mListA.size() == m_mListA.size());
        for(auto& E : m_mListA) E.second.CheckValues();
        for(auto& E : Ref.m_mListA) assert(m_mListA.find(E.first) != m_mListA.end());

        assert(Ref.m_mListB.size() == m_mListB.size());
        for(auto& E : m_mListB) assert(Ref.m_mListB.at(E.first) == E.second);

        assert(Ref.m_llListC.size() == m_llListC.size());
        for (const auto& E : m_llListC)
        {
            bool bFound = false;
            for(auto& E2 : Ref.m_llListC)
            {
                if( E == E2)
                {
                    bFound = true;
                    break;
                }
            }
            assert(bFound);
        }
    }
    XPROPERTY_DEF
    ( "List Advance", lists_advance
    , obj_member<"m_mListA", &lists_advance::m_mListA >
    , obj_member<"m_mListB", &lists_advance::m_mListB >
    , obj_member<"m_llListC", &lists_advance::m_llListC >

    , obj_member<"setValues", &list_c_arrays::setValues >
    , obj_member<"CheckValues", &list_c_arrays::CheckValues >
    )
};
XPROPERTY_REG(lists_advance)

/* [[lists_advance]]
<br>

# Virtual Properties

## <a name="Example6.1"></a> Example 1 - Virtual properties

Some times you may want to do more than just read or write properties. Some times you may want to do additional work when reading or writing a xproperty.
To solve this issue the concept of virtual properties was created. This is done by using a lambda function to override the read or write of a xproperty
in the case of atomic variables, but in the case of objects with properties we can enable them or disable them.
Remember we always use the'+' operator in our lambdas. <br>

You can also use virtual properties to call specific getters and setters from your object if you wish.

Also note that the context variable is optional... you can choose to added or not.

Properties that contain more properties their lambdas look similar to the reference lambdas however they return a pointer. This means that they are
able to turn on and off access to those properties.

Also note the const usage in the lambdas. By using const you can disable write or read access to properties.

cpp */

struct virtual_properties
{
    using enum_t   = registered_enum::example;
    using unenum_t = enums_registered::unreg;

    int                         m_Int       = 0;
    std::vector<base1>          m_vListA    = {};
    base1                       m_Other     = {};
    base1*                      m_pOther    = &m_Other;

    enum_t                      m_Enum      = {};   
    unenum_t                    m_UEnum     = {};

    int  getInt(void)   { return m_Int; }
    void setInt(int a)  { m_Int = a; }

    void setValues()
    {
        m_Int       = 20;
        m_Other.setValues();
        m_Enum      = enum_t::VALID_VALUE_2;
        m_UEnum     = unenum_t::UNREG_V2;
        m_vListA.resize(3);
        for(auto& E : m_vListA) E.setValues();
    }

    void CheckValues() const
    {
        assert(m_Int == 20);
        m_Other.CheckValues();
        assert(m_Enum == enum_t::VALID_VALUE_2);
        assert(m_UEnum == unenum_t::UNREG_V2);
    }

    XPROPERTY_DEF 
    ( "Virtual Properties", virtual_properties
    // This is the default lambda for xproperty that hold values
    // Notice the plus operator in front of the lambda
    // There is not need to return anything from the lambda for these types 
    // of properties. Also while the constexpr is optional it is recommended
    , xproperty::obj_member
        < "m_VirtualInt"
        , +[]( virtual_properties& O, bool bRead, int& InOutValue ) constexpr
        {
            if (bRead) InOutValue = O.m_Int; else O.m_Int = InOutValue;
        }>

    // We can also do enums...
    , xproperty::obj_member
        < "m_Enum"
        , +[]( virtual_properties& O, bool bRead, enum_t& InOutValue ) constexpr
        {
            if (bRead) InOutValue = O.m_Enum; else O.m_Enum = InOutValue;
        }>

    // Or unregistered enums...
    , xproperty::obj_member
        < "m_UEnum"
        , +[]( virtual_properties& O, bool bRead, unenum_t& InOutValue ) constexpr
        {
            if (bRead) InOutValue = O.m_UEnum; else O.m_UEnum = InOutValue;
        }
        , xproperty::member_enum_span<enums_registered::unreg_enum_list_v>
        >

    // We can also do readonly virtual properties...
    , xproperty::obj_member_ro
        < "m_CEnum"
        , +[]( virtual_properties& O, bool bRead, enum_t& InOutValue ) constexpr
        {
            assert(bRead==true);
            InOutValue = O.m_Enum;
        }>

    // If we want to be always a read only xproperty we can make the class instance
    // const and the lambda will be called with bRead = true always... which you
    // can ignore if you want.
    , xproperty::obj_member
        < "m_ReadOnlyVirtualInt"
        , +[](const virtual_properties& O, bool, int& InOutValue) constexpr
        {
            InOutValue = 222;
        }>

    // You can also specify the read only with the _ro postfix as before
    // Also note how we can always add the context variable if we want to...
    , xproperty::obj_member_ro
        < "m_ForceVirtualInt"
        , +[](virtual_properties& O, bool, int& InOutValue, xproperty::settings::context& C)
        {
            InOutValue = 222;
        }>

    // You can also have a write only xproperty... which is very strange...
    // but it is possible and there may be a use for it... you will make
    // the InOutValue const in this case. (bRead will always be false).
    // Finally Making the class and the InOutValue const would not 
    // make much sense so we don't allow it.
    , xproperty::obj_member
        < "m_WriteOnlyVirtualInt"
        , +[](virtual_properties& O, bool, const int& InOutValue) constexpr
        {
            O.m_Int = InOutValue;
        }>

    // We can use virtual properties to call specific getters and setters
    , xproperty::obj_member
        < "m_VirtualInt3"
        , +[]( virtual_properties& O, bool bRead, int& InOutValue) constexpr
        {
            if (bRead) InOutValue = O.getInt(); else O.setInt(InOutValue);
        }>

    // We can disable virtual properties that have properties
    // by returning nullptr
    , xproperty::obj_member
        < "m_VirtualProps"
        , +[]( virtual_properties& O, xproperty::settings::context& C) constexpr
            ->auto*
        {
            return (O.m_Int==20) ? &O.m_Other : nullptr;
        }>

    // We can also make the properties read_only by having a
    // return const pointer
    , xproperty::obj_member
        < "m_ReadOnlyVirtualProps"
        , +[]( virtual_properties& O, xproperty::settings::context& C) constexpr
            ->const auto*
        {
            return (O.m_Int==20) ? &O.m_Other : nullptr;
        }>

    // Or we can do the _ro postfix
    , xproperty::obj_member_ro
        < "m_ForceReadOnlyVirtualProps"
        , +[]( virtual_properties& O, xproperty::settings::context& C) constexpr
            ->auto*
        {
            return (O.m_Int==20) ? &O.m_Other : nullptr;
        }>

    // We also support lists...
    , xproperty::obj_member
        < "m_VirtualPropsList"
        , +[]( virtual_properties& O, xproperty::settings::context& C)
            ->auto*
        {
            return &O.m_vListA;
        }>

    // We also support pointer and such...
    , xproperty::obj_member
        < "m_VirtualPointer"
        , +[]( virtual_properties& O, xproperty::settings::context& C)
            ->auto*
        {
            return &O.m_pOther;
        }>

    , xproperty::obj_member<"setValues",   &virtual_properties::setValues >
    , xproperty::obj_member<"CheckValues", &virtual_properties::CheckValues >
    )
};
XPROPERTY_REG(virtual_properties)

/* [[virtual_properties]]
<br>

# User Data

## <a name="Example7.1"></a> Example 1 - Adding Member Help

Sometime we may want to have extra data for each member in this case we can
use the member_user_data. In this example we leverage this feature to extend the
xproperty system to allow us to add a string for each member to help the
end user.

cpp */

struct user_data_object
{
    int m_Var = 0;
    void setValues() { m_Var = 20; }
    void CheckValues() const { assert(m_Var == 20); }

    XPROPERTY_DEF
    ( "Member User Data Object"
    , user_data_object
    , xproperty::obj_member<"var"
                , &user_data_object::m_Var
                , xproperty::member_help<"Some help for the variable">
                >
    , xproperty::obj_member<"setValues"
                , &user_data_object::setValues
                , xproperty::member_help<"This function help set the default"
                                "Values for the class">
                >
    , xproperty::obj_member<"CheckValues"
                , &user_data_object::CheckValues
                , xproperty::member_help<"Check if the values have been"
                                " set properly">
                >
    )
};
XPROPERTY_REG(user_data_object)

/* [[user_data_object]]
<br>

# Property Storage

Sometimes is it important to store properties as just generic data. The purpose for
that could be like serializing an object to a file or sometimes you may want to
copy and paste some of the values, or you may want to collect a bunch of values
for other reasons. To accomplish this we can try to express a xproperty with the minimum amount of data.
The least amount of data per xproperty are 3 things:

* The name/path of the xproperty
* The type of the xproperty
* The value of the xproperty

## <a name="Example8.1"></a> Example 1 - Store as a pair of string and value

We can combine the xproperty system give us a way to store the type and the value
with a structure call **xproperty::any**. This structure is a generic way to store
any type of xproperty values. To Store the name/path of the xproperty we can use
a string, which means that we can store the xproperty as a pair of **std::string** and **xproperty::any**.
Since any object can have many properties we can create a container that has a vector of such properties.
It is very convenient because every xproperty is fully contain in one entry.
This method is very easy to manipulate individual properties.
But it has the overhead of parsing out key keys and the paths from the string and
Since every path is a string it means one allocation per xproperty which is no so perfect.

cpp */

struct sprop_container
{
    struct prop
    {
        std::string     m_Path;         // Path to the xproperty and all keys encoded
        xproperty::any   m_Value;        // Value + type associated with this xproperty
    };

    std::vector<prop>   m_Properties;
};

/* ```
## <a name="Example8.2"></a> Example 2 - Store as two vectors

We can improve the above method by separating the list keys from the path. This allows
us to store the keys are regular xproperty values. Which means shorter strings and
less parsing. However, it now pollutes which values belong to which properties.

cpp */

struct sprop_container_v2
{
    struct prop
    {
        std::string     m_Path;         // Path to the xproperty
        std::uint16_t   m_Index;        // Index to the first value 
        std::uint16_t   m_Count;        // Number of values associated with this xproperty
    };

    std::vector<prop>           m_Properties;
    std::vector<xproperty::any>  m_Values;       // Values + Keys encoded in the values
};

/* ```
## <a name="Example8.2"></a> Example 3 - Store as two vectors

This method is identical to the above method but removes the need to
have strings. Stead it encodes the path using the string guid.
The additional negative side of this method is when there is an
error is no easy to track. But has the advantage of allocating only 3 times.

cpp */

struct gprop_container_v3
{
    struct prop
    {
        std::uint16_t   m_iPath;        // Offset to the Path where path is encoded.
        std::uint16_t   m_nPaths;       // Number of paths associated with this xproperty
        std::uint16_t   m_iValue;       // Index to the first value (Keys are encoded in the values)
        std::uint16_t   m_nValues;      // Number of values associated with this xproperty
    };

    std::vector<prop>           m_Properties;
    std::vector<std::uint32_t>  m_Path;       // Path represented by GUIDs
    std::vector<xproperty::any>  m_Values;     // Values + Keys encoded in the values
};

/* ```
## <a name="Example8.2"></a> Example 4 - Fastest way to encode properties

This is by most optimum way to store properties. Here the properties are merge
with the path itself. The encoding is a relative path to the xproperty, where the
actual number indicates the actual index of the xproperty and negative numbers
means how many paths we should pop out from the stack. The only reason to store
properties this way is to minimize the memory footprint and maximize perforce.
Very useful when exporting a final scene level for instance. Of course assumes
you won't change the code (adding or deleting properties).

cpp */

struct iprop_container_v4
{
    std::vector<std::int32_t>   m_Encoding;     // Encoding props paths
    std::vector<xproperty::any>  m_Values;       // Values + Keys encoded in the values
};

/* ```
END
<br>
*/
