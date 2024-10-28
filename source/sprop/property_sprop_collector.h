#pragma once
namespace xproperty::sprop
{

    // Please note that the CALLBACK should follow this signature
    // [&]( const char* pPropertyName, xproperty::any&& Value, const xproperty::type::members& Member, bool isConst, const void* pInstance ) { ... }
    // * pPropertyName - is a view to the full property name including key for arrays and such
    // * Value         - is the value of the property
    // * Member        - is a convinient way to get the user data and whatever else you need
    // * isConst       - is true if the property is const
    // * pInstance     - is the instance of the object being processed, some times useful to do complex user based features...
    class collector
    {
    public:

        template<typename T>
        collector( T& Object, sprop::container& PropContainer, xproperty::settings::context& context, bool bForEditors = false ) noexcept
            : collector(Object, context, [&](const char* pPropertyName, xproperty::any&& Value, const xproperty::type::members&, bool, const void*) noexcept
                {
                    PropContainer.m_Properties.emplace_back(pPropertyName, std::move(Value));
                }, bForEditors ) {}

        template<typename T, typename T_CALLBACK>
        collector( T& Object, xproperty::settings::context& Context, T_CALLBACK&& CallBack, bool bForEditors = false ) noexcept
            : collector( &Object, *xproperty::getObject(Object), Context, std::move(CallBack), bForEditors ){}

        template<typename T_CALLBACK >
        collector( const void* pObject, const xproperty::type::object& PropertyObj, xproperty::settings::context& context, T_CALLBACK&& CallBack, bool bForEditors = false ) noexcept
            : m_pContext   (&context)
            , m_bForEditors(bForEditors)
        {
            PushPath(PropertyObj.m_pName);
            if(false && m_bForEditors)
            {
                xproperty::type::members Member
                { .m_GUID    = PropertyObj.m_GUID
                , .m_pName   = PropertyObj.m_pName
                , .m_Variant = xproperty::type::members::scope{}
                , .m_bConst  = false
                };
                CallBack(m_CurrentPath.data(), xproperty::any(), Member, false, pObject);
            }
            DumpObject( std::move(CallBack), const_cast<void*>(pObject), PropertyObj, false);
            PopPath();
        }

    protected:

        struct node
        {
            const char*     m_pName         {};
            std::size_t     m_iCurPath      {};
        };

        xproperty::settings::context*           m_pContext         {};
        std::size_t                             m_iCurrentPath     {0};
        std::vector<node>                       m_PathStack        {};
        bool                                    m_bForEditors      {false};
        std::array<char, 256>                   m_CurrentPath;

        //----------------------------------------------------------------------------------------------

        // Push a entry to the stack
        void PushPath(const char* pName)
        {
            const char* const pFmt = m_iCurrentPath ? "/%s" : "%s";

            m_PathStack.emplace_back( pName, m_iCurrentPath );
            m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], m_CurrentPath.size()-m_iCurrentPath, pFmt, pName);
            
            assert( m_iCurrentPath < m_CurrentPath.size() );
        }

        //----------------------------------------------------------------------------------------------

        void PopPath( void )
        {
            m_iCurrentPath = m_PathStack.back().m_iCurPath;
            m_PathStack.pop_back();
        }

        //----------------------------------------------------------------------------------------------

        template< typename T_CALLBACK, typename T >
        inline void DumpAtomicTypes
        ( T_CALLBACK&&                      CallBack
        , void* const                       pClass
        , const T&                          Info
        , const xproperty::type::members&   Members
        , const bool                        isConst 
        )
        {
            if (Info.m_pWrite == nullptr) assert( isConst == true );

            // if we don't have a write function we can't write anything
            if (Info.m_pRead == nullptr) return;

            // Read the actual value
            xproperty::any Value;
            Info.m_pRead(pClass, Value, Info.m_UnregisteredEnumSpan, *m_pContext);

            // Let the user know that we got properties
            CallBack( m_CurrentPath.data(), std::move(Value), Members, isConst, pClass);
        }

        //----------------------------------------------------------------------------------------------

        template< typename T_CALLBACK, typename T >
        inline void ProcessList
        ( T_CALLBACK&&                      CallBack
        , const std::size_t                 iDimension
        , void* const                       pClass
        , const T&                          List
        , const xproperty::type::members&   Members
        , const bool                        isConst
        )
        {
            xproperty::begin_iterator        StartIterator(pClass, List.m_Table[iDimension], *m_pContext);
            const xproperty::end_iterator    EndIterator  (pClass, List.m_Table[iDimension], *m_pContext);

            // If a size is 0 we completely ignore this xproperty
 //           if ( const auto Count = EndIterator.getSize(); Count == 0) return;
 //           else
            {
                const auto Count = EndIterator.getSize();

                // We will write the size for each dimension as a xproperty
                xproperty::any Value;
                Value.set<std::size_t>(Count);
                m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], m_CurrentPath.size() - m_iCurrentPath, "[]");

                // Let the user know that we got properties
                CallBack(m_CurrentPath.data(), std::move(Value), Members, isConst, pClass);
                m_iCurrentPath -= 2;

                // If we have zero entries there is nothing else to do...
                if( Count == 0 ) return;
            }

            auto iStringPop = m_iCurrentPath;
            do
            {
                void* const pObject = StartIterator.getObject();

                // Check if we have anything valid
                if (pObject == nullptr) continue;

                // Reset the string
                m_iCurrentPath = iStringPop;

                // Add the key for this particular dimension
                {
                    xproperty::any  Key;
                    if (StartIterator.getKey(Key) == false )
                    {
                        // This should not really be false as we got a valid pClazz object already
                        assert(false);
                    }

                    // we need to key into the path
                    m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], m_CurrentPath.size() - m_iCurrentPath, "[");
                    m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], m_CurrentPath.size() - m_iCurrentPath, "%c:", xproperty::settings::GUIDToCType(Key.getTypeGuid()));
                    m_iCurrentPath += xproperty::settings::AnyToString({ &m_CurrentPath[m_iCurrentPath], m_CurrentPath.size() - m_iCurrentPath }, Key );
                    m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], m_CurrentPath.size() - m_iCurrentPath, "]");
                }

                if (iDimension < (List.m_Table.size() - 1))
                {
                    // Keep processing more dimensions
                    ProcessList(CallBack, iDimension + 1, pObject, List, Members, isConst);
                }
                else
                {
                    if constexpr (std::is_same_v<T, xproperty::type::members::list_props>)
                    {
                        // Handle the printing of the object
                        auto [pInstance, pNewObject] = List.m_pCast(pObject, *m_pContext);
                        if (m_bForEditors) if(pNewObject->m_GroupGUID) CallBack(m_CurrentPath.data(), xproperty::any(pNewObject->m_GroupGUID), Members, isConst, pInstance);
                        if (pInstance) DumpObject(CallBack, pInstance, *pNewObject, isConst);
                    }
                    else
                    {
                        DumpAtomicTypes(CallBack, pObject, List, Members, isConst);
                    }
                }

            } while ( StartIterator.Next(EndIterator) );
        }

        //----------------------------------------------------------------------------------------------

        using scope      = const xproperty::type::members::scope;
        using var        = const xproperty::type::members::var;
        using props      = const xproperty::type::members::props;
        using func       = const xproperty::type::members::function;
        using list_props = const xproperty::type::members::list_props;
        using list_var   = const xproperty::type::members::list_var;

        template< typename T_CALLBACK >
        inline void DumpScope
        ( T_CALLBACK&&                      CallBack
        , void* const                       pClass
        , const scope* const                pScope
        , const bool                        isConst 
        )
        {
            for (auto& Member : pScope->m_Members)
            {
                const bool bConst = isConst || Member.m_bConst;

                PushPath(Member.m_pName);
                std::visit( [&]<typename T>( T&& Arg ) constexpr
                {
                         if constexpr (std::is_same_v<T, scope&> )      
                         {
                             // Let the user know that we are dumping an object
                             if (m_bForEditors) CallBack(m_CurrentPath.data(), xproperty::any(), Member, bConst, pClass);
                             DumpScope(CallBack, pClass, &Arg, bConst);
                         }
                    else if constexpr (std::is_same_v<T, var&>)         {DumpAtomicTypes(CallBack, pClass, Arg, Member, bConst);}
                    else if constexpr (std::is_same_v<T, props&>)       
                    { 
                        if(auto [pInstance, pObj] = Arg.m_pCast(pClass, *m_pContext); pInstance ) 
                        {
                            // Let the user know that we are dumping an object
                            if (m_bForEditors) CallBack( m_CurrentPath.data(), xproperty::any(pObj->m_GroupGUID), Member, bConst, pInstance );
                            DumpObject(CallBack, pInstance, *pObj, bConst);
                        }
                    }
                    else if constexpr (std::is_same_v<T, func&>)        {} // Nothing to do for functions
                    else if constexpr (std::is_same_v<T, list_props&> || std::is_same_v<T, list_var&> ) 
                    {
                        ProcessList( CallBack, 0, pClass, Arg, Member, bConst );
                    }
                    else static_assert( xproperty::always_false<T>::value, "Missing xproperty type" );
                }, Member.m_Variant );
                PopPath();
            }
        }

        //----------------------------------------------------------------------------------------------
        template< typename T_CALLBACK >
        inline void DumpObject( T_CALLBACK&& CallBack, void* const pClass, const xproperty::type::object& Object, const bool isConst )
        {
            assert(pClass);

            for (auto& Base : Object.m_BaseList)
            {
                const bool bConst      = isConst || Base.m_bConst;
                auto [pInstance, pObj] = Base.m_pCast(pClass, *m_pContext);

                // Unable to access some base? That should never happen... 
                assert(pInstance);

                // Push the name
                PushPath(pObj->m_pName);

                // If we are doing editor let the user know that we are dealing with a new scope
                if (m_bForEditors)
                {
                    xproperty::type::members Member
                    { .m_GUID    = pObj->m_GUID
                    , .m_pName   = pObj->m_pName
                    , .m_Variant = xproperty::type::members::scope{}
                    , .m_bConst  = bConst
                    };
                    CallBack(m_CurrentPath.data(), xproperty::any(), Member, bConst, pClass );
                }

                // Print all the members of the base class
                DumpObject(CallBack, pInstance, *pObj, bConst);
                PopPath();
            }
            DumpScope(CallBack, pClass, &Object, isConst);
        }
    };
}