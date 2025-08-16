#ifndef XPROPERTY_XCORE_SERIALIZER_H
#define XPROPERTY_XCORE_SERIALIZER_H
#pragma once

#include "xcore.h"
#include "../imgui/my_properties.h"
#include "../../sprop/property_sprop.h"

namespace xproperty::sprop::serializer
{
    //----------------------------------------------------------------------------

    template< typename T_TYPES_TUPLE >
    inline constexpr static auto user_defined_type_table_v =
        []< typename...T>(std::tuple<T...>*) noexcept
    {
        return std::array
        {
            xcore::textfile::user_defined_types
            { xproperty::settings::var_type<T>::name_v
            , xproperty::settings::var_type<T>::serialization_type_v
            } ...
            , xcore::textfile::user_defined_types
            { "enum"
            , "s"
            }
        };
    }(static_cast<T_TYPES_TUPLE*>(0u));

    //----------------------------------------------------------------------------

    namespace details
    {
        using fn_type = xcore::err(xcore::textfile::stream& Stream, xproperty::any& Any);

        struct hash_entry
        {
            std::uint32_t m_Hash        {};
            fn_type*      m_pCallback   {};
        };

        static inline constexpr auto hash_size_v = 512;

        //----------------------------------------------------------------------------

        template< typename T_TYPES_TUPLE >
        inline constexpr static auto file_crc_to_callback_v =
            []< typename...T>( std::tuple<T...>* ) noexcept
            {
                using tu = std::tuple<T...>;
                std::array<hash_entry, hash_size_v> HashTable = {};

                ( [&]< typename A>(A*) noexcept
                {
                    constexpr auto  CRC        = user_defined_type_table_v<T_TYPES_TUPLE>[xcore::types::tuple_t2i_v<A, tu>].m_CRC;
                    auto            iHashEntry = CRC.m_Value % hash_size_v;
                    while( HashTable[iHashEntry].m_pCallback ) iHashEntry = (iHashEntry+1)% hash_size_v;
                    HashTable[iHashEntry].m_Hash      = CRC.m_Value;
                    HashTable[iHashEntry].m_pCallback = xproperty::settings::var_type<A>::template XCoreTextFile<xcore::textfile::stream, CRC>;
                }(static_cast<T*>(nullptr)), ... );

                return HashTable;
            }(static_cast<T_TYPES_TUPLE*>(nullptr));

        //----------------------------------------------------------------------------

        template< typename T_TYPES_TUPLE >
        inline constexpr static auto property_guid_to_callback_v =
            []< typename...T>( std::tuple<T...>* ) noexcept
            {
                using tu = std::tuple<T...>;
                std::array<hash_entry, hash_size_v> HashTable = {};

                ( [&]< typename A>(A*) noexcept
                {
                    constexpr auto CRC        = user_defined_type_table_v<T_TYPES_TUPLE>[xcore::types::tuple_t2i_v<A, tu>].m_CRC;
                    constexpr auto GUID       = xproperty::settings::var_type<A>::guid_v;
                    auto           iHashEntry = GUID % hash_size_v;
                    while( HashTable[iHashEntry].m_pCallback ) iHashEntry = (iHashEntry+1)% hash_size_v;
                    HashTable[iHashEntry].m_Hash      = GUID;
                    HashTable[iHashEntry].m_pCallback = &xproperty::settings::var_type<A>::template XCoreTextFile<xcore::textfile::stream, CRC>;
                }(static_cast<T*>(0)), ... );

                return HashTable;
            }(static_cast<T_TYPES_TUPLE*>(0));
        
        //----------------------------------------------------------------------------

        constexpr static const hash_entry* FindEntryInHash( const std::span<const hash_entry> HashMap, std::uint32_t Value ) noexcept
        {
            auto Index = Value % hash_size_v;
            for( ; HashMap[Index].m_Hash != Value; Index = (Index+1)% hash_size_v)
            {
                if( HashMap[Index].m_pCallback == nullptr ) return nullptr;
            }
            return &HashMap[Index];
        }

        //----------------------------------------------------------------------------
        template< typename T_TUPLE_TYPES >
        xcore::err IO( xcore::textfile::stream& Stream, std::vector<container::prop>& Properties ) noexcept
        {
            xcore::err Error;

            if(Stream.Record( Error, "xProperties"
                ,   [&]( std::size_t& C, xcore::err& )
                    {
                        if(Stream.isReading()) Properties.resize(C);
                        else                   C = Properties.size();
                    }
                ,   [&]( std::size_t i, xcore::err& Error)
                    {
                        auto& E = Properties[i];

                        // Just save some integers to show that we can still safe normal fields at any point
                        if(Stream.Field( "Name", E.m_Path ).isError(Error) ) return;

                        // CRC for enum types...
                        constexpr auto ENUM_CRC = user_defined_type_table_v<T_TUPLE_TYPES>.back().m_CRC;

                        // Handle the data
                        if(Stream.isReading())
                        {
                            xcore::crc<32> Type;
                            if( Stream.ReadFieldUserType( Type, "Value:?" ).isError(Error)) return;

                            if( Type == ENUM_CRC )
                            {
                                // Read enums as strings...
                                Error = xproperty::settings::var_type<std::string>::template XCoreTextFile<xcore::textfile::stream, ENUM_CRC>(Stream, E.m_Value);
                            }
                            else
                            {
                                auto pEntry = details::FindEntryInHash(details::file_crc_to_callback_v<T_TUPLE_TYPES>, Type.m_Value);
                                if (pEntry == nullptr) Error = xerr_code(Error, xcore::textfile::error_state::FIELD_NOT_FOUND, "Unable to find property type while reading the file");
                                else                   Error = pEntry->m_pCallback(Stream, E.m_Value);
                            }
                        }
                        else
                        {
                            if( E.m_Value.isEnum() )
                            {
                                auto pEntry = details::FindEntryInHash(details::property_guid_to_callback_v<T_TUPLE_TYPES>, xproperty::settings::var_type<std::string>::guid_v);

                                xproperty::any Value;
                                Value.set<std::string>( E.m_Value.getEnumString() );

                                Error = xproperty::settings::var_type<std::string>::template XCoreTextFile<xcore::textfile::stream, ENUM_CRC>(Stream, Value);
                            }
                            else if( E.m_Value.m_pType == nullptr ) Error = xerr_code(Error, xcore::textfile::error_state::FIELD_NOT_FOUND, "Trying the serialize a property but it contained no type or value");
                            else
                            {
                                auto pEntry = details::FindEntryInHash( details::property_guid_to_callback_v<T_TUPLE_TYPES>, E.m_Value.m_pType->m_GUID );
                                if (pEntry == nullptr) Error = xerr_code(Error, xcore::textfile::error_state::FIELD_NOT_FOUND, "Trying the serialize a property type by unable to find the type in the list");
                                else                   Error = pEntry->m_pCallback(Stream, E.m_Value);
                            }
                        }
                    }
                ))
            {
                return Error;
            }

            return Error;
        }
    }

    //----------------------------------------------------------------------------
    // Stream
    //----------------------------------------------------------------------------
    template< typename T_TUPLE_TYPES = xproperty::settings::atomic_types_tuple >
    xcore::err Stream(xcore::textfile::stream& Stream, void* pObject, const xproperty::type::object& PropertyObj, xproperty::settings::context& Context) noexcept
    {
        xproperty::sprop::container  Container;

        if( Stream.isReading())
        {
            // First read all the properties
            auto Err = details::IO<T_TUPLE_TYPES>(Stream, Container.m_Properties);
            if( Err ) return Err;

            for(auto& E : Container.m_Properties)
            {
                std::string Error;
                xproperty::sprop::setProperty(Error, pObject, PropertyObj, E, Context);
                if( Err )
                {
                    // May be here should be a warning... fail to set property but should not be a failure?
                    assert(false);
                }
            }

            return Err;
        }

        // Only required when writing... when reading we can get it from the file itself
        if( Stream.getUserTypeCount() == 0 )
        {
            Stream.AddUserTypes(xproperty::sprop::serializer::user_defined_type_table_v<T_TUPLE_TYPES>);
        }

        struct scope
        {
            std::uint32_t m_GUID;
            std::uint32_t m_Length;
            bool          m_Ignore;
        };
        std::uint32_t           ScopeIndex = 0;
        std::array< scope, 32 > ScopeList;

        ScopeList[0].m_Ignore = false;
        ScopeList[0].m_Length = static_cast<std::uint32_t>(std::strlen(PropertyObj.m_pName));
        ScopeList[0].m_GUID   = PropertyObj.m_GUID;

        xproperty::sprop::collector(pObject, PropertyObj, Context, [&](const char* pPropertyName, xproperty::any&& Value, const xproperty::type::members& Member, bool isConst, const void* pInstance)
        {
            const xproperty::flags::type Flags = [&]
            {
                if (auto* pDynamicFlags = Member.getUserData<xproperty::settings::member_dynamic_flags_t>(); pDynamicFlags)
                {
                    return pDynamicFlags->m_pCallback(pInstance, Context);
                }
                else if (auto* pStaticFlags = Member.getUserData<xproperty::settings::member_flags_t>(); pStaticFlags)
                {
                    return pStaticFlags->m_Flags;
                }
                else
                {
                    return xproperty::flags::type{ .m_Value = 0 };
                }
            }();

            const bool bScope = std::holds_alternative<xproperty::type::members::scope>(Member.m_Variant)
                             || std::holds_alternative<xproperty::type::members::props>(Member.m_Variant);

            const bool bArray = std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant);

            /*
            if( std::holds_alternative<xproperty::type::members::props>(Member.m_Variant) 
             || std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant) )
            {
                // GUIDs for groups are marked as u32... vs sizes are mark as u64
                if( Value.m_pType->m_GUID == xproperty::settings::var_type<std::uint32_t>::guid_v )
                {
                    GroupGUID = Value.get<std::uint32_t>();
                }
            }
            */

            // Pop any irrelevant scopes
            while (ScopeIndex && ScopeList[ScopeIndex].m_GUID != xproperty::settings::strguid({ pPropertyName, ScopeList[ScopeIndex].m_Length })) ScopeIndex--;

            if(bScope || bArray)
            {
                std::uint32_t l = static_cast<std::uint32_t>(std::strlen(pPropertyName));
                if(std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant))
                {
                    l -= 2;
                }

                auto& E = ScopeList[++ScopeIndex];
                E.m_GUID   = xproperty::settings::strguid({ pPropertyName, l });
                E.m_Length = l;
                E.m_Ignore = Flags.m_bDontSave || isConst || ScopeList[ScopeIndex-1].m_Ignore;

                // We don't save non-array headers
                if( false == bArray ) return;

                // If it is not a u32 then it is a group other wise it would have been u64 (the size of the array)
                if (Value.m_pType->m_GUID == xproperty::settings::var_type<std::uint32_t>::guid_v) 
                    return;
            }

            // If the user asked us to ignore this then lets us do so...
            if( Flags.m_bDontSave || isConst || ScopeList[ScopeIndex].m_Ignore ) return;

            // Store properties
            if( std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant) )
            {
                // For array counts let us convert it to int64 since we will never use the full range of the 64bit values
                // This makes the file format look nicer...
                Container.m_Properties.emplace_back(pPropertyName, xproperty::any{ static_cast<std::int64_t>(Value.getCastValue<std::uint64_t>()) });
            }
            else if ( std::holds_alternative<xproperty::type::members::list_var>(Member.m_Variant) 
                      && Value.m_pType->m_GUID == xproperty::settings::var_type<std::uint64_t>::guid_v 
                      && [&]{ std::uint32_t l = static_cast<std::uint32_t>(std::strlen(pPropertyName)); return pPropertyName[l - 1] == ']' && pPropertyName[l - 2] == '[';}() )
            {
                // For array counts let us convert it to int64 since we will never use the full range of the 64bit values
                // This makes the file format look nicer...
                Container.m_Properties.emplace_back(pPropertyName, xproperty::any{ static_cast<std::int64_t>(Value.getCastValue<std::uint64_t>()) });
            }
            else
            {
                Container.m_Properties.emplace_back(pPropertyName, Value);
            }

        }, true);

        return details::IO<T_TUPLE_TYPES>(Stream, Container.m_Properties );
    }

    //----------------------------------------------------------------------------

    template< typename T_TUPLE_TYPES = xproperty::settings::atomic_types_tuple, typename T >
    constexpr xcore::err Stream(xcore::textfile::stream& Stream, T& Object, xproperty::settings::context& Context) noexcept
    {
        return xproperty::sprop::serializer::Stream<T_TUPLE_TYPES>(Stream, &Object, *xproperty::getObject(Object), Context);
    }
}

#endif