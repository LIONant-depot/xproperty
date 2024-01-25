#pragma once
namespace xproperty::sprop
{
    class collector
    {
    public:

        template<typename T>
        collector( T& Object, sprop::container& PropContainer, xproperty::settings::context& context )
            : m_pContext(&context)
            , m_pPropContainer(&PropContainer)
        {
            DumpObject(&Object, *xproperty::getObject(Object), false);
        }

    protected:
        struct node
        {
            const char*     m_pName;
            std::size_t     m_iCurPath;
        };

        xproperty::settings::context*             m_pContext         {};
        sprop::container*                        m_pPropContainer   {};
        std::size_t                              m_iCurrentPath     {0};
        std::vector<node>                        m_PathStack        {};
        std::array<char, 256>                    m_CurrentPath;

        //----------------------------------------------------------------------------------------------

        // Push a entry to the stack
        void PushPath(const char* pName)
        {
            const char* const pFmt = m_iCurrentPath ? "/%s" : "%s";

            m_PathStack.emplace_back( pName, m_iCurrentPath );
            m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], 256, pFmt, pName);
            
            assert( m_iCurrentPath < m_CurrentPath.size() );
        }

        //----------------------------------------------------------------------------------------------

        void PopPath()
        {
            m_iCurrentPath = m_PathStack.back().m_iCurPath;
            m_PathStack.pop_back();
        }

        //----------------------------------------------------------------------------------------------

        template< typename T >
        inline void DumpAtomicTypes( void* pClass, const T& Info, const bool isConst )
        {
            if (Info.m_pWrite == nullptr) assert( isConst == true );

            // if we don't have a write function we can't write anything
            if (Info.m_pRead == nullptr) return;

            // Read the actual value
            xproperty::any Value;
            Info.m_pRead(pClass, Value, Info.m_UnregisteredEnumSpan, *m_pContext);
            m_pPropContainer->m_Properties.emplace_back(m_CurrentPath.data(), std::move(Value));
        }

        //----------------------------------------------------------------------------------------------

        template< typename T >
        inline void ProcessList
        ( const std::size_t             iDimension
        , void* const                   pClass
        , const T&                      List
        , const bool                    isConst
        )
        {
            xproperty::begin_iterator        StartIterator(pClass, List.m_Table[iDimension], *m_pContext);
            const xproperty::end_iterator    EndIterator  (pClass, List.m_Table[iDimension], *m_pContext);

            // If a size is 0 we completely ignore this xproperty
            if ( const auto Count = EndIterator.getSize(); Count == 0) return;
            else
            {
                // We will write the size for each dimension as a xproperty
                xproperty::any Value;
                Value.set<std::size_t>(Count);
                m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], 235, "[]");
                m_pPropContainer->m_Properties.emplace_back(m_CurrentPath.data(), std::move(Value));
                m_iCurrentPath -= 2;
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
                    m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], 235, "[");
                    m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], 235, "%c:", xproperty::settings::GUIDToCType(Key.getTypeGuid()));
                    m_iCurrentPath += xproperty::settings::AnyToString({ &m_CurrentPath[m_iCurrentPath], m_CurrentPath.size() - m_iCurrentPath }, Key );
                    m_iCurrentPath += sprintf_s(&m_CurrentPath[m_iCurrentPath], 235, "]");
                }

                if (iDimension < (List.m_Table.size() - 1))
                {
                    // Keep processing more dimensions
                    ProcessList(iDimension + 1, pObject, List, isConst);
                }
                else
                {
                    if constexpr (std::is_same_v<T, xproperty::type::members::list_props>)
                    {
                        // Handle the printing of the object
                        auto [pInstance, pNewObject] = List.m_pCast(pObject, *m_pContext);
                        if (pInstance) DumpObject(pInstance, *pNewObject, isConst);
                    }
                    else
                    {
                        DumpAtomicTypes(pObject, List, isConst);
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

        inline void DumpScope
        ( void* const                   pClass
        , const scope* const            pScope
        , const bool                    isConst 
        )
        {
            for (auto& Member : pScope->m_Members)
            {
                const bool bConst = isConst || Member.m_bConst;

                PushPath(Member.m_pName);
                std::visit( [&]<typename T>( T&& Arg ) constexpr
                {
                         if constexpr (std::is_same_v<T, scope&> )      {DumpScope( pClass, &Arg, bConst);}
                    else if constexpr (std::is_same_v<T, var&>)         {DumpAtomicTypes( pClass, Arg, bConst);}
                    else if constexpr (std::is_same_v<T, props&>)       {if(auto [pInstance, pObj] = Arg.m_pCast(pClass, *m_pContext); pInstance ) DumpObject( pInstance, *pObj, bConst);}
                    else if constexpr (std::is_same_v<T, func&>)        {} // Nothing to do for functions
                    else if constexpr (std::is_same_v<T, list_props&> 
                                    || std::is_same_v<T, list_var&>)    {ProcessList( 0, pClass, Arg, bConst);}
                    else static_assert( xproperty::always_false<T>::value, "Missing xproperty type" );
                }, Member.m_Variant );
                PopPath();
            }
        }

        //----------------------------------------------------------------------------------------------

        inline void DumpObject( void* pClass, const xproperty::type::object& Object, const bool isConst )
        {
            assert(pClass);

            PushPath(Object.m_pName);
            for (auto& Base : Object.m_BaseList)
            {
                const bool bConst      = isConst || Base.m_bConst;
                auto [pInstance, pObj] = Base.m_pCast(pClass, *m_pContext);

                // Unable to access some base? That should never happen... 
                assert(pInstance);

                DumpObject(pInstance, *pObj, bConst);
            }
            DumpScope( pClass, &Object, isConst);
            PopPath();
        }
    };
}