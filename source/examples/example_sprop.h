#pragma once
#include "../sprop/property_sprop.h"

namespace xproperty::example::sprop
{
    template< typename T1 >
    struct example
    {
        template< typename T2 = T1 >
        static void DoExample(xproperty_doc::memfile& MemFile)
        {
            xproperty::settings::context Context;
            auto* pInfo     = xproperty::getObjectByType<T2>();

            // Create instance
            void* pInstance = pInfo->CreateInstance();

            // Set values
            static_cast<T2*>(pInstance)->setValues();

            // Create a container
            xproperty::sprop::container PropContainer;

            // Collect all the properties and put them into the container
            xproperty::sprop::collector(*static_cast<T1*>(pInstance), PropContainer, Context);

            // Destroy instance
            pInfo->DestroyInstance(pInstance);

            // Create a brand new instance
            pInstance = pInfo->CreateInstance();

            // Set properties
            {
                std::string             Error;
                std::array<char, 256>   Buffer;
                for (auto& E : PropContainer.m_Properties)
                {
                    // Print it to the Property
                    setProperty(Error, *static_cast<T1*>(pInstance), E, Context);
                    if( false == Error.empty() )
                    {
                        MemFile.print("%s = %s\n", E.m_Path.c_str(), Error.c_str());
                        Error.clear();
                    }
                    else
                    {
                        xproperty::settings::AnyToString(Buffer, E.m_Value);
                        if ( E.m_Value.getTypeGuid() == xproperty::settings::var_type<std::string>::guid_v )
                        {
                            MemFile.print("%s = \"%s\"\n", E.m_Path.c_str(), Buffer.data());
                        }
                        else
                        {
                            MemFile.print("%s = %s\n", E.m_Path.c_str(), Buffer.data());
                        }
                        
                    }
                }
            }

            // Check Values
            static_cast<T2*>(pInstance)->CheckValues();
        }
    };

    inline void Example( xproperty_doc::example_group& ExampleGroup )
    {
        ExampleGroup.m_GroupName = "SProps";

        // Run all the examples
        ExecuteExamples<example>(ExampleGroup);
    }
}



