
#include "my_properties.h"
#include "generate_documentation.h"
#include "example_properties.h"

//--------------------------------------------------------------------------------------------
// Get the type name as a string from a template argument
//--------------------------------------------------------------------------------------------
namespace internal
{
    // https://blog.molecular-matters.com/2015/12/11/getting-the-type-of-a-template-argument-as-string-without-rtti/

    inline constexpr unsigned int FRONT_SIZE = sizeof("internal::GetTypeNameHelper<") - 1u;
    inline constexpr unsigned int BACK_SIZE = sizeof(">::GetTypeName") - 1u;

    template <typename T>
    struct GetTypeNameHelper
    {
        consteval static auto GetTypeName(void)
        {
            constexpr auto size = sizeof(__FUNCTION__) - FRONT_SIZE - BACK_SIZE;
            std::array<char, size> Data = {};
            for (int i = 0; i < (size - 1); ++i) Data[i] = __FUNCTION__[i + FRONT_SIZE];
            Data[size - 1] = '\0';
            return Data;
        }
    };
}

template <typename T>
consteval auto GetTypeName(void)
{
    return internal::GetTypeNameHelper<T>::GetTypeName();
}

//--------------------------------------------------------------------------------------------
// Execute all the examples
//--------------------------------------------------------------------------------------------

template< template< typename > class TT, typename T1, typename T2 = T1 >
void DoExample( xproperty_doc::memfile& MemFile )
{
    MemFile.m_Title = std::string("[[") + std::string(GetTypeName<T2>().data()) + std::string("]]");
    printf("\n\n##[%s] \n", MemFile.m_Title.c_str());

    TT<T1>::template DoExample<T2>(MemFile);

    // Print it to the console
    MemFile.output();
}

template< template< typename > class TT >
void ExecuteExamples(xproperty_doc::example_group& ExampleGroup)
{
    std::vector<xproperty_doc::memfile>& Examples = ExampleGroup.m_Examples;

    //
    // Different types of xproperty definitions
    //
    if (true)
    {
        printf("\n\n");
        printf("------------------------------------------------------------------\n");
        printf("Example of: Different types of xproperty definitions\n");
        printf("GROUP: %s\n", ExampleGroup.m_GroupName.c_str());
        printf("------------------------------------------------------------------\n");
        if (true) DoExample< TT, my_object1>(Examples.emplace_back());
        if (true) DoExample< TT, my_object2>(Examples.emplace_back());
        if (true) DoExample< TT, my_object3>(Examples.emplace_back());
        if (true) DoExample< TT, base1>     (Examples.emplace_back());
    }

    //
    // Hierarchical properties
    //
    if (true)
    {
        printf("\n\n");
        printf("------------------------------------------------------------------\n");
        printf("Example of: Hierarchical properties\n");
        printf("GROUP: %s\n", ExampleGroup.m_GroupName.c_str());
        printf("------------------------------------------------------------------\n");
        if (true) DoExample< TT, derived1>          (Examples.emplace_back());
        if (true) DoExample< TT, derived2>          (Examples.emplace_back());
        if (true) DoExample< TT, base1, derived2>   (Examples.emplace_back());
    }

    //
    // Types of variables
    //
    if (true)
    {
        printf("\n\n");
        printf("------------------------------------------------------------------\n");
        printf("Example of: Types of variables\n");
        printf("GROUP: %s\n", ExampleGroup.m_GroupName.c_str());
        printf("------------------------------------------------------------------\n");
        if (true) DoExample< TT, common_types>                          (Examples.emplace_back());
        if (true) DoExample< TT, enums_unregistered>                    (Examples.emplace_back());
        if (true) DoExample< TT, enums_registered>                      (Examples.emplace_back());
        if (true) DoExample< TT, pointer_and_reference_c_style_values>  (Examples.emplace_back());
        if (true) DoExample< TT, pointer_and_reference_c_style_props>   (Examples.emplace_back());
        if (true) DoExample< TT, pointers_and_references_cpp_style>     (Examples.emplace_back());
        if (true) DoExample< TT, list_c_arrays>                         (Examples.emplace_back());
        if (true) DoExample< TT, lists_cpp>                             (Examples.emplace_back());
        if (true) DoExample< TT, lists_advance>                         (Examples.emplace_back());
    }

    //
    // Virtual properties
    //
    if (true)
    {
        printf("\n\n");
        printf("------------------------------------------------------------------\n");
        printf("Example of: Virtual properties\n");
        printf("GROUP: %s\n", ExampleGroup.m_GroupName.c_str());
        printf("------------------------------------------------------------------\n");
        if (true) DoExample< TT, virtual_properties>(Examples.emplace_back());
        if (true) DoExample< TT, user_data_object>  (Examples.emplace_back());
    }
}

//--------------------------------------------------------------------------------------------
// Includes examples
//--------------------------------------------------------------------------------------------
#include "example_printing.h"
#include "example_sprop.h"

//--------------------------------------------------------------------------------------------
// main
//--------------------------------------------------------------------------------------------
int main()
{
    std::vector<xproperty_doc::example_group> Examples;

    // Execute all the examples
    if constexpr (true) xproperty::example::printing::Example(Examples.emplace_back());
    if constexpr (true) xproperty::example::sprop::Example(Examples.emplace_back());

    // Generate the documentation
    if constexpr (true) xproperty_doc::Generate(Examples);

    return 0;
}