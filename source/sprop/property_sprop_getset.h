#pragma once
namespace xproperty::sprop
{
    // A path looks like this: "pepe/somearray[key_type:key_value]/someprop"
    // So it will look physically like this: "pepe/somearray[s:string][i:2132]/someprop"
    // TODO: Maybe we should require quotes around strings that are keys else could run into the issue of treating a ']' inside the string and a terminator
    template< bool IS_SET_V >
    class io_property
    {
    protected:

        using prop_t     = std::conditional_t< IS_SET_V, const container::prop, container::prop>;
        using inst_t     = std::conditional_t< IS_SET_V, void*, const void* >;

        using scope      = const xproperty::type::members::scope;
        using var        = const xproperty::type::members::var;
        using props      = const xproperty::type::members::props;
        using func       = const xproperty::type::members::function;
        using list_props = const xproperty::type::members::list_props;
        using list_var   = const xproperty::type::members::list_var;

    protected:

        bool PropertyMember(inst_t pClass, const xproperty::type::members::scope& Scope, std::uint32_t GUID, bool isPath )
        {
            // Check with its members
            if( auto pMember = Scope.findMember(GUID); pMember )
            {
                // If the member is const we are not going to be able to set it!
                if constexpr (IS_SET_V) if (pMember->m_bConst) 
                {
                    m_Error = std::format("ERROR: Fail to set a constant xproperty! {} Location {}", m_Property.m_Path, m_iPath);
                    return false;
                }

                // If the path says is a path we will be expecting that the member
                // contains properties or else something is definitely wrong... 
                return std::visit([&]<typename T>(T&& Arg) constexpr -> bool
                {
                    if constexpr (std::is_same_v<T, scope&>)
                    {
                        if (isPath == false)
                        {
                            m_Error = std::format("ERROR: Fail to find the property! {} Location {}", m_Property.m_Path, m_iPath);
                            return false;
                        }
                        assert(isPath);
                        GUID = getNextGuid(isPath);
                        return PropertyMember(pClass, Arg, GUID, isPath);
                    }
                    else if constexpr (std::is_same_v<T, props&>)
                    {
                        if (isPath == false)
                        {
                            printf("[ERROR]: Unable to find this get/set this [%s] property\n", m_Property.m_Path.c_str());
                            return false;
                        }

                        auto [pInstance, pObj] = Arg.m_pCast(pClass, m_Context);
                        //GUID = getNextGuid(isPath);
                        return PropertyObject(pInstance, *pObj, pObj->m_GUID, isPath, false);
                    }
                    else if constexpr (std::is_same_v<T, list_props&>)
                    {
                        // Access the right dimension
                        void* pObject = pClass;
                        for (auto i = 0; i < m_KeyCount; ++i)
                        {
                            if(m_Keys[i].getTypeGuid() != Arg.m_Table[i].m_KeyAtomicType.m_GUID )
                            {
                                m_Error = std::format("ERROR: Key Types don't match expecting to have {} but the xproperty path has {}. From {} Location {}"
                                    , xproperty::settings::GUIDToCType(Arg.m_Table[i].m_KeyAtomicType.m_GUID)
                                    , xproperty::settings::GUIDToCType(m_Keys[i].getTypeGuid())
                                    , m_Property.m_Path
                                    , m_iPath
                                    );
                                return false;
                            }

                            pObject = Arg.m_Table[i].m_pGetObject(pObject, m_Keys[i], m_Context);
                            if (pObject == nullptr)
                            {
                                m_Error = std::format("ERROR: Failed to access one of the dimensions [{}], From {} Location {}"
                                    , i, m_Property.m_Path, m_iPath);
                                return false;
                            }
                        }

                        // Check if we are trying to deal with the size of a dimension or the actual xproperty value
                        if (m_isSize)
                        {
                            // Means we are trying to access the size rather than
                            if constexpr (IS_SET_V) Arg.m_Table[m_KeyCount].m_pSetSize(pObject, m_Property.m_Value.template getCastValue<std::size_t>(), m_Context);
                            else                    m_Property.m_Value.set(Arg.m_Table[m_KeyCount].m_pGetSize(pObject, m_Context));
                        }
                        else
                        {
                            auto [pInstance, pObj] = Arg.m_pCast(pObject, m_Context);
                           // GUID = getNextGuid(isPath);
                            return PropertyObject(pInstance, *pObj, pObj->m_GUID, isPath, false);
                        }
                        return true;
                    }
                    else if constexpr (std::is_same_v<T, var&>)
                    {
                        if(isPath == true)
                        {
                            printf("[ERROR]: Unable to find this get/set this [%s] property\n", m_Property.m_Path.c_str() );
                            return false;
                        }

                        if constexpr (IS_SET_V) Arg.m_pWrite(pClass, m_Property.m_Value, Arg.m_UnregisteredEnumSpan, m_Context);
                        else                    Arg.m_pRead (pClass, m_Property.m_Value, Arg.m_UnregisteredEnumSpan, m_Context);

                        return true;
                    }
                    else if constexpr (std::is_same_v<T, list_var&>)
                    {
                        assert(isPath == false);

                        // Access the right dimension
                        void* pObject = pClass;
                        for( auto i=0; i<m_KeyCount; ++i )
                        {
                            if (m_Keys[i].getTypeGuid() != Arg.m_Table[i].m_KeyAtomicType.m_GUID)
                            {
                                m_Error = std::format("ERROR: Key Types don't match expecting to have {} but the xproperty path has {}. From {} Location {}"
                                    , xproperty::settings::GUIDToCType(Arg.m_Table[i].m_KeyAtomicType.m_GUID)
                                    , xproperty::settings::GUIDToCType(m_Keys[i].getTypeGuid())
                                    , m_Property.m_Path
                                    , m_iPath
                                );
                                return false;
                            }

                            pObject = Arg.m_Table[i].m_pGetObject(pObject, m_Keys[i], m_Context);
                            if( pObject == nullptr )
                            {
                                m_Error = std::format("ERROR: Failed to access one of the dimensions [{}], From {} Location {}"
                                    , i, m_Property.m_Path, m_iPath);
                                return false;
                            }
                        }
                        
                        // Check if we are trying to deal with the size of a dimension or the actual xproperty value
                        if(m_isSize) 
                        {
                            // Means we are trying to access the size rather than
                            if constexpr (IS_SET_V) Arg.m_Table[m_KeyCount].m_pSetSize(pObject, m_Property.m_Value.getCastValue<std::size_t>(), m_Context);
                            else                    m_Property.m_Value.set( Arg.m_Table[m_KeyCount].m_pGetSize(pObject, m_Context));
                        }
                        else
                        {
                            if( m_KeyCount != Arg.m_Table.size() )
                            {
                                m_Error = std::format("ERROR: Expecting {} Keys but found {}! {} Location {}"
                                                     , Arg.m_Table.size(), m_KeyCount, m_Property.m_Path, m_iPath);
                                return false;
                            }

                            if constexpr (IS_SET_V) Arg.m_pWrite( pObject, m_Property.m_Value, Arg.m_UnregisteredEnumSpan, m_Context);
                            else                    Arg.m_pRead ( pObject, m_Property.m_Value, Arg.m_UnregisteredEnumSpan, m_Context);
                        }
                        return true;
                    }
                    else
                    {
                        assert(false);
                        // This is not a xproperty (it is a scope)!!
                        m_Error = std::format("ERROR: Expecting a xproperty but found a Scope! {} Location {}", m_Property.m_Path, m_iPath);
                        return false;
                    }

                }, pMember->m_Variant);
                
            }

            return false;
        }

        bool PropertyObject(inst_t pClass, const xproperty::type::object& Object, std::uint32_t GUID, bool isPath, bool isConst )
        {
            // Make sure the GUID matches
            if( Object.m_GUID != GUID ) return false;
            if constexpr (IS_SET_V) if( isConst )
            {
                m_Error = std::format("ERROR: Fail to set a constant xproperty!! {} Location {}", m_Property.m_Path, m_iPath);
                return false;
            }

            // Time to check the next GUID
            GUID = getNextGuid(isPath);

            // Check with its parents...
            if(isPath)
            {
                for (auto& Base : Object.m_BaseList)
                {
                    auto [pInstance, pObj] = Base.m_pCast(pClass, m_Context);

                    // Unable to access some base? That should never happen... 
                    assert(pInstance);

                    if( true == PropertyObject(pInstance, *pObj, GUID, isPath, Base.m_bConst) ) return true;
                    if (Base.m_bConst) return false;
                }
            }

            return PropertyMember(pClass, Object, GUID, isPath);
        }

        std::uint32_t getNextGuid(bool& isPath)
        {
            isPath = false;
            const auto iBegin = m_iPath;
            const auto end    = m_Property.m_Path.size();
            for( ; m_iPath < end; m_iPath++)
            {
                if (m_Property.m_Path[m_iPath] == '/' ) 
                {
                    // Skip '/'
                    m_iPath++;

                    isPath = true;
                    break;
                }

                // Handle Lists
                if (m_Property.m_Path[m_iPath] == '[' )
                {
                    const auto iOldPath = m_iPath;

                    m_KeyCount = 0;
                    m_isSize  = false;

                    // Collect all the keys
                    do 
                    {
                        // Skip '['
                        m_iPath++;

                        if (m_Property.m_Path[m_iPath] == ']')
                        {
                            m_iPath ++;
                            m_isSize = true;
                            isPath = false;
                            break;
                        }

                        // Next expected thing is the type as a single character
                        const auto CTypeGUID = xproperty::settings::CTypeToGUID(m_Property.m_Path[m_iPath++]);
                        if( CTypeGUID == 0 )
                        {
                            // Unknown key type
                            m_Error = std::format("ERROR: Could not find the type of a Key. {} Location {}", m_Property.m_Path, m_iPath );
                            return 0;
                        }

                        // Then we should be reading a ':'
                        if( m_Property.m_Path[m_iPath++] != ':' ) 
                        {
                            // Bad formatted path
                            m_Error = std::format("ERROR: Bad formatted path {} Location {} Expecting a ':' but did not find it.", m_Property.m_Path, m_iPath );
                            return 0;
                        }

                        // Now we can copy the actual key into a separate string
                        std::array<char, 128> Buff;
                        int i = 0;

                        // Copy the string into a temp buffer
                        // also find the length of the string
                        for (; (Buff[i] = m_Property.m_Path[m_iPath]) != 0; ++i, ++m_iPath)
                        {
                            if (Buff[i] == ']')
                            {
                                Buff[i] = 0;
                                m_iPath++;
                                break;
                            }
                        }

                        // Convert the Key from string to an actual value
                        if( false == xproperty::settings::StringToAny(m_Keys[m_KeyCount], CTypeGUID, { Buff.data(), static_cast<std::uint32_t>(i) }) )
                        {
                            // failed to read the key
                            m_Error = std::format("ERROR: Bad formatted path {} Location {} Failed to read a key.", m_Property.m_Path, m_iPath);
                            return 0;
                        }

                        // Done parsing this key
                        m_KeyCount++;

                    } while( m_Property.m_Path[m_iPath] == '[' );

                    // Check if we are a path or not
                    if (m_Property.m_Path[m_iPath] == '/')
                    {
                        isPath = true;
                        m_iPath++;
                    }
                    else
                    {
                        assert( m_iPath == end );
                    }

                    // OK we are done with the keys
                    return xproperty::settings::strguid({ m_Property.m_Path.data() + iBegin, static_cast<std::uint32_t>(iOldPath - iBegin + 1) });
                }
            }

            // Since we don't have a null terminated string we must compensate for it...
            if( end == m_iPath ) m_iPath++;
            return xproperty::settings::strguid({ m_Property.m_Path.data() + iBegin, static_cast<std::uint32_t>(m_iPath - iBegin) });
        }

    protected:

        prop_t&                         m_Property;
        xproperty::settings::context&   m_Context;
        std::string&                    m_Error;
        std::uint32_t                   m_iPath     {0};
        std::array<xproperty::any, 8>   m_Keys      {};
        int                             m_KeyCount  {0};
        bool                            m_isSize   {false};

    public:

        template<typename T>
        io_property(std::string& Error, T& Object, prop_t& Property, xproperty::settings::context& Context)
            : m_Property    { Property }
            , m_Context     { Context }
            , m_Error       { Error }
        {
            assert(Error.empty());

            bool isPath;
            auto GUID = getNextGuid(isPath);

            if( false == PropertyObject(&Object, *xproperty::getObject(Object), GUID, isPath, std::is_const_v<T>) )
            {
                if(m_Error.empty()) m_Error = std::format("ERROR: Could not find the xproperty {}", m_Property.m_Path);
            }
        }

        io_property(std::string& Error, void* pObject, const xproperty::type::object& PropObject, prop_t& Property, xproperty::settings::context& Context)
            : m_Property    { Property }
            , m_Context     { Context }
            , m_Error       { Error }
        {
            assert(Error.empty());

            bool isPath;
            auto GUID = getNextGuid(isPath);

            if( false == PropertyObject(pObject, PropObject, GUID, isPath, false ) )
            {
                if(m_Error.empty()) m_Error = std::format("ERROR: Could not find the xproperty {}", m_Property.m_Path);
            }
        }

        constexpr operator bool() const { return m_Error.empty(); }
    };

    template<typename T>
    void setProperty(std::string& Error, T& Object, const container::prop& Property, xproperty::settings::context& Context) noexcept
    {
        io_property<true>(Error, Object, Property, Context);
    }

    inline
    void setProperty(std::string& Error, void* pObject, const xproperty::type::object& PropObject, const container::prop& Property, xproperty::settings::context& Context) noexcept
    {
        io_property<true>(Error, pObject, PropObject, Property, Context);
    }
}
