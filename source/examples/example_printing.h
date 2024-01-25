#pragma once
namespace xproperty::example::printing
{
    namespace details
    {
        inline const char* getSpaces( int nSpaces )
        {
            static constexpr char Spaces[] = "                                                                                                                                    ";
            return &Spaces[sizeof(Spaces) - nSpaces - 1];
        }

        /*
        inline int AnyToString( std::span<char> String, const xproperty::any& Value )
        {
            switch (Value.getTypeGuid())
            {
            case xproperty::settings::var_type<std::int32_t>::guid_v:    return sprintf_s( String.data(), String.size(), "%d",   Value.get<std::int32_t>());  
            case xproperty::settings::var_type<std::uint32_t>::guid_v:   return sprintf_s( String.data(), String.size(), "%u",   Value.get<std::uint32_t>()); 
            case xproperty::settings::var_type<std::int16_t>::guid_v:    return sprintf_s( String.data(), String.size(), "%d",   Value.get<std::int16_t>());  
            case xproperty::settings::var_type<std::uint16_t>::guid_v:   return sprintf_s( String.data(), String.size(), "%u",   Value.get<std::uint16_t>()); 
            case xproperty::settings::var_type<std::int8_t>::guid_v:     return sprintf_s( String.data(), String.size(), "%d",   Value.get<std::int8_t>());   
            case xproperty::settings::var_type<std::uint8_t>::guid_v:    return sprintf_s( String.data(), String.size(), "%u",   Value.get<std::uint8_t>());  
            case xproperty::settings::var_type<float>::guid_v:           return sprintf_s( String.data(), String.size(), "%f",   Value.get<float>());         
            case xproperty::settings::var_type<double>::guid_v:          return sprintf_s( String.data(), String.size(), "%f",   Value.get<double>());        
            case xproperty::settings::var_type<std::string>::guid_v:     return sprintf_s( String.data(), String.size(), "\"%s\"", Value.get<std::string>().c_str()); 
            case xproperty::settings::var_type<std::uint64_t>::guid_v:   return sprintf_s( String.data(), String.size(), "%llu", Value.get<std::uint64_t>());
            case xproperty::settings::var_type<std::int64_t>::guid_v:    return sprintf_s( String.data(), String.size(), "%lld", Value.get<std::int64_t>()); 
            default: assert(false); break;
            }
            return 0;
        }
        */

        template< typename T >
        inline void DumpAtomicTypes( xproperty_doc::memfile& MemFile, void* pClass, const T& Info, xproperty::settings::context& Context, const bool isConst )
        {
            if (Info.m_pWrite == nullptr) assert( isConst == true );
            if (Info.m_pRead == nullptr)
            {
                MemFile.print("( type: %s (has no read function) )", Info.m_AtomicType.m_pName );
                return;
            }

            // Read the actual value
            xproperty::any Value;
            Info.m_pRead(pClass, Value, Info.m_UnregisteredEnumSpan, Context);

            // Print the type
            MemFile.print
            ( "( type: %s%s, value: "
            , isConst ? "const " : ""
            , Info.m_AtomicType.m_pName
            );

            // Print the value
            if ( Info.m_AtomicType.m_IsEnum )
            {
                const auto pString = Value.getEnumString();
                MemFile.print
                ( "{ %s, (%d) } )"
                , pString ? pString : "failed to find its String"
                , Value.getEnumValue()
                );
            }
            else
            {
                std::array< char, 256 > String;
                xproperty::settings::AnyToString(String, Value);
                MemFile.print("%s )", String.data());
            }
        }

        inline void DumpObject(xproperty_doc::memfile& MemFile, void* pClass, const xproperty::type::object& Object, xproperty::settings::context& Context, int nSpaces, bool isConst);

        template< typename T >
        inline void ProcessList
        ( xproperty_doc::memfile&             MemFile
        , const std::size_t             iDimension
        , void* const                   pClass
        , const T&                      List
        , const std::size_t             Size
        , const int                     nSpaces
        , xproperty::settings::context&  Context
        , const bool                    isConst
        )
        {
            xproperty::begin_iterator        StartIterator(pClass, List.m_Table[iDimension], Context);
            const xproperty::end_iterator    EndIterator  (pClass, List.m_Table[iDimension], Context);

            if( EndIterator.getSize() == 0 )
            {
                MemFile.print(" empty ]\n");
                return;
            }

            std::array< char, 256 >         KeyString;
            do
            {
                void* const pObject = StartIterator.getObject();

                if( pObject == nullptr )
                {
                    MemFile.print(" empty ]\n");
                    continue;
                }

                // Handle the printing of the dimensions
                if (StartIterator.getIndex()) MemFile.print("%s[ ", getSpaces(nSpaces));

                {
                    xproperty::any  Key;
                    if(StartIterator.getKey(Key) == false )
                    {
                        // This should not really be false as we got a valid pClazz object already
                        assert(false);
                    }
                    xproperty::settings::AnyToString(KeyString, Key);
                    MemFile.print("%d ] key(%s) = ", StartIterator.getIndex(), KeyString.data() );
                }

                if( iDimension < (List.m_Table.size()-1) ) 
                {
                    std::size_t NextSize = List.m_Table[iDimension+1].m_pGetSize(pObject, Context);

                    MemFile.print("count( %llu )\n", NextSize );
                    MemFile.print("%s[ ", getSpaces(nSpaces + 4 ));

                    // Keep processing more dimensions
                    ProcessList(MemFile, iDimension + 1, pObject, List, NextSize, nSpaces + 4, Context, isConst );
                }
                else
                {
                    if constexpr ( std::is_same_v<T, xproperty::type::members::list_props> )
                    {
                        // Handle the printing of the object
                        auto [pInstance, pObj] = List.m_pCast(pObject, Context);
                        if (pInstance) DumpObject(MemFile, pInstance, *pObj, Context, nSpaces, isConst);
                    }
                    else
                    {
                        static_assert(std::is_same_v<T, xproperty::type::members::list_var>);
                        // Handle the printing of the object
                        DumpAtomicTypes(MemFile, pObject, List, Context, isConst);
                        MemFile.print("\n");
                    }
                }

            } while( StartIterator.Next(EndIterator) );
        }

        using scope      = const xproperty::type::members::scope;
        using var        = const xproperty::type::members::var;
        using props      = const xproperty::type::members::props;
        using func       = const xproperty::type::members::function;
        using list_props = const xproperty::type::members::list_props;
        using list_var   = const xproperty::type::members::list_var;

        inline void DumpScope
        ( xproperty_doc::memfile&             MemFile
        , void* const                   pClass
        , const char* const             pScopeName
        , const scope* const            pScope
        , xproperty::settings::context&  Context
        , const int                     nSpaces
        , const bool                    isConst 
        )
        {
            for (auto& Member : pScope->m_Members)
            {
                const bool bConst = isConst || Member.m_bConst;

                std::visit( [&]<typename T>( T&& Arg ) constexpr
                {
                    MemFile.print("%s", getSpaces(nSpaces) );
                    if constexpr (std::is_same_v<T, scope&> )
                    {
                        MemFile.print("SCOPE[ %s ]\n", pScopeName);
                        DumpScope(MemFile, pClass, Member.m_pName, &Arg, Context, nSpaces + 4, bConst );
                    }
                    else if constexpr (std::is_same_v<T, var&>)
                    {
                        MemFile.print("MEMBER_VARS[ %s ] = ", Member.m_pName);
                        DumpAtomicTypes(MemFile, pClass, Arg, Context, bConst);
                        MemFile.print("\n");
                    }
                    else if constexpr (std::is_same_v<T, props&>)
                    {
                        auto [pInstance, pObj] = Arg.m_pCast(pClass, Context);
                        // These pointers could be null so check for the state before moving on
                        if (pInstance)
                        {
                            MemFile.print("MEMBER_PROPS[ %s ] = %s", Member.m_pName, bConst ?"( Const ) " : "" );
                            DumpObject(MemFile, pInstance, *pObj, Context, nSpaces, bConst);
                        }
                    }
                    else if constexpr (std::is_same_v<T, func&>)
                    {
                        MemFile.print("MEMBER_FUNCTION[ %s ]\n", Member.m_pName);
                    }
                    else if constexpr (std::is_same_v<T, list_props&> || std::is_same_v<T, list_var&>)
                    {
                        //
                        // Start the recursive process
                        //
                        const std::size_t Size = Arg.m_Table[0].m_pGetSize(pClass, Context);
                        MemFile.print("MEMBER_LIST[ %s ] = dimensions( %llu ), count( %llu )\n", Member.m_pName, Arg.m_Table.size(), Size);

                        if( Size )
                        {
                            MemFile.print("%s[ ", getSpaces(nSpaces+4));
                            ProcessList(MemFile, 0, pClass, Arg, Size, nSpaces + 4, Context, bConst );
                        }
                    }
                    else
                    {
                        MemFile.print("MEMBER[ %s ]\n", Member.m_pName );
                    }
                }, Member.m_Variant );

                // Print the help of the member if any
                if (auto pHelp = Member.getUserData<xproperty::settings::member_help_t>(); pHelp)
                {
                    MemFile.print("%s", getSpaces(nSpaces+4));
                    MemFile.print("Help: \"%s\"\n", pHelp->m_pHelp);
                }
            }
        }

        inline void DumpObject(xproperty_doc::memfile& MemFile, void* pClass, const xproperty::type::object& Object, xproperty::settings::context& Context, int nSpaces, const bool isConst)
        {
            assert(pClass);

            MemFile.print("OBJECT[ %s ]\n", Object.m_pName);

            for( auto& Base : Object.m_BaseList )
            {
                const bool bConst = isConst || Base.m_bConst;
                auto[pInstance, pObj] = Base.m_pCast(pClass, Context);
                if( pInstance )
                {
                    MemFile.print("%s", getSpaces(nSpaces+4));
                    DumpObject(MemFile, pInstance, *pObj, Context, nSpaces + 4, bConst);
                }
                else
                {
                    // Unable to access some base? That should never happen... 
                    assert(false);
                }
            }
            DumpScope(MemFile, pClass, Object.m_pName, &Object, Context, nSpaces + 4, isConst);
        }
    };

    template< typename T >
    void DumpObject( xproperty_doc::memfile& MemFile, T& Object, xproperty::settings::context& C )
    {
        details::DumpObject(MemFile , &Object, *xproperty::getObject(Object), C, 0, false );
    }

    template< typename T1 >
    struct example
    {
        template< typename T2 = T1 >
        static void DoExample( xproperty_doc::memfile& MemFile )
        {
            xproperty::settings::context Context;
            auto* pInfo     = xproperty::getObjectByType<T2>();
            void* pInstance = pInfo->CreateInstance();
            static_cast<T2*>(pInstance)->setValues();
            DumpObject(MemFile, *static_cast<T1*>(pInstance), Context);
        }
    };

    //------------------------------------------------------------------------------

    void Example( xproperty_doc::example_group& ExampleGroup )
    {
        ExampleGroup.m_GroupName = "Printing";

        // Run all the examples
        ExecuteExamples<example>(ExampleGroup);
    }
}