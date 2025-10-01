#ifndef XCORE_PROPERTIES_H
#define XCORE_PROPERTIES_H
#pragma once

#ifdef XPROPERTY_IMGUI_INSPECTOR_H
    #error "Please include dependencies/xproperty/xcore/my_properties.h before the imgui inspector"
#endif

// Tell the IMGUID Inspector to ignore the default properties
#define MY_PROPERTIES_H

// ------------------------------------------------------------------------------
// This header file is a convinient way to centralize the standard properties
// that will be use commonly in xcore. If you are looking for examples or
// tutorials look into the example directory.
// ------------------------------------------------------------------------------
#pragma once
#include<string>
#include<vector>
#include<map>
#include<unordered_map>
#include<memory>
#include<array>

// ------------------------------------------------------------------------------
// USER PRE-CONFIGURATION
// ------------------------------------------------------------------------------
namespace xproperty::settings
{
    // Here are a bunch of useful tools to help us configure xproperty. 
    namespace details
    {
        constexpr auto force_minimum_case_alignment = 16;

        // This is the structure which is align and as big as our worse alignment and worse size
        template< typename...T >
        struct alignas(force_minimum_case_alignment) atomic_type_worse_alignment_and_memory
        {
            alignas(std::max({ alignof(T)... })) std::uint8_t m_Data[std::max({ sizeof(T)... })];
        };
    }

    using data_memory = details::atomic_type_worse_alignment_and_memory
        < std::string
        , std::wstring      
        , std::size_t       
        , std::uint64_t     
        >;

    using iterator_memory = details::atomic_type_worse_alignment_and_memory
        < std::vector<data_memory>::iterator
        , std::map<std::string, data_memory>::iterator
        , std::uint64_t 
        , std::array<char, 4>
        >;
}

// ------------------------------------------------------------------------------
// ADDING THE ACTUAL LIBRARY
// ------------------------------------------------------------------------------

#include "dependencies\xproperty\source\xproperty.h"
#include "dependencies\xproperty\source\sprop\property_sprop_container.h"

// ------------------------------------------------------------------------------
// POST-MEMORY-CONFIGURATION
// ------------------------------------------------------------------------------
namespace xproperty::settings
{
    struct context
    {
    };
}

//
// ATOMIC TYPES
//
namespace xproperty::settings
{
    template<>
    struct var_type<std::int64_t> : var_defaults<"s64", std::int64_t >
    {
        constexpr static inline char ctype_v                = 'D';
        constexpr static inline auto serialization_type_v   = "D";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::int64_t>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::int64_t>());
        }
    };

    template<>
    struct var_type<std::uint64_t> : var_defaults<"u64", std::uint64_t >
    {
        constexpr static inline char ctype_v                = 'G';
        constexpr static inline auto serialization_type_v   = "G";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::uint64_t>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::uint64_t>());
        }
    };

    template<>
    struct var_type<std::int32_t> : var_defaults<"s32", std::int32_t >
    {
        constexpr static inline char ctype_v                = 'd';
        constexpr static inline auto serialization_type_v   = "d";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::int32_t>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::int32_t>());
        }
    };

    template<>
    struct var_type<std::uint32_t> : var_defaults<"u32", std::uint32_t >
    {
        constexpr static inline char ctype_v                = 'g';
        constexpr static inline auto serialization_type_v   = "g";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::uint32_t>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::uint32_t>());
        }
    };

    template<>
    struct var_type<std::int16_t> : var_defaults<"s16", std::int16_t >
    {
        constexpr static inline char ctype_v                = 'C';
        constexpr static inline auto serialization_type_v   = "C";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::int16_t>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::int16_t>());
        }
    };

    template<>
    struct var_type<std::uint16_t> : var_defaults<"u16", std::uint16_t >
    {
        constexpr static inline char ctype_v                = 'H';
        constexpr static inline auto serialization_type_v   = "H";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::uint16_t>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::uint16_t>());
        }
    };

    template<>
    struct var_type<std::int8_t> : var_defaults<"s8", std::int8_t >
    {
        constexpr static inline char ctype_v                = 'c';
        constexpr static inline auto serialization_type_v   = "c";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::int8_t>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::int8_t>());
        }
    };

    template<>
    struct var_type<std::uint8_t> : var_defaults<"u8", std::uint8_t >
    {
        constexpr static inline char ctype_v                = 'h';
        constexpr static inline auto serialization_type_v   = "h";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::uint8_t>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::uint8_t>());
        }
    };

    template<>
    struct var_type<char> : var_type<std::int8_t>
    {
    };

    template<>
    struct var_type<bool> : var_defaults<"bool", bool >
    {
        constexpr static inline char ctype_v                = 'b';
        constexpr static inline auto serialization_type_v   = "c";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<bool>();
            if constexpr (sizeof(bool) == sizeof(std::int8_t)) return FileStream.Field(CRC_V, "Value:?", reinterpret_cast<std::int8_t&>(Any.get<bool>()));
            else
            {
                std::int8_t x = static_cast<std::int8_t>(Any.get<bool>());
                auto Err = FileStream.Field(CRC_V, "Value:?", x);
                Any.set<bool>(static_cast<bool>(x));
                return Err;
            }
        }
    };

    template<>
    struct var_type<float> : var_defaults<"f32", float >
    {
        constexpr static inline char ctype_v                = 'f';
        constexpr static inline auto serialization_type_v   = "f";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<float>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<float>());
        }
    };

    template<>
    struct var_type<double> : var_defaults<"f64", double >
    {
        constexpr static inline char ctype_v                = 'F';
        constexpr static inline auto serialization_type_v   = "F";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<double>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<double>());
        }
    };

    template<>
    struct var_type<std::string> : var_defaults<"string", std::string >
    {
        constexpr static inline char ctype_v                = 's';
        constexpr static inline auto serialization_type_v   = "s";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::string>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::string>());
        }
    };

    template<>
    struct var_type<std::wstring> : var_defaults<"wstring", std::wstring >
    {
        constexpr static inline char ctype_v                = 'S';
        constexpr static inline auto serialization_type_v   = "S";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != guid_v) Any.Reset<std::wstring>();
            return FileStream.Field(CRC_V, "Value:?", Any.get<std::wstring>());
        }
    };
}

// 
// SUPPORT FOR ADVANCE REFERENCES
// 
/*
 // #include "dependencies/xresource_guid/source/xresource_guid.h"      // because of xresource::guid_def
// #include "dependencies/xresource_mgr/source/xresource_mgr.h"        // because we need to get the guid from the guid_def (this could be moved to a cpp)
//#include<inttypes.h> // for the PRIX64

namespace xproperty::settings
{
    template< typename T_INSTANCE_TYPE, xresource::type_guid T_TYPE_GUID_V >
    struct var_type<xresource::def_guid_t<T_INSTANCE_TYPE, T_TYPE_GUID_V>> 
    {
        inline constexpr static bool is_list_v      = false;
        inline constexpr static bool is_pointer_v   = true;
        inline constexpr static auto name_v         = "rsc_ref";
        inline constexpr static auto guid_v         = T_TYPE_GUID_V.m_Value;
        using                        type           = xresource::def_guid_t<T_INSTANCE_TYPE, T_TYPE_GUID_V>;
        using                        atomic_type    = type;
        using                        specializing_t = type;
        inline constexpr static auto is_const_v     = false;

        // Any of the following can be overriden
        constexpr static void              Write    (       type& MemberVar, const atomic_type& Data,  context& C ) noexcept { if( MemberVar != nullptr ) var_type<specializing_t>::Write( *MemberVar, Data, C ); }
        constexpr static void              Read     ( const type& MemberVar,       atomic_type& Data,  context& C ) noexcept { if( MemberVar != nullptr ) var_type<specializing_t>::Read ( *MemberVar, Data, C ); }
        constexpr static specializing_t*   getObject(       type& MemberVar,                           context&   ) noexcept { return (MemberVar) ? &(*MemberVar) : nullptr; }
        constexpr static atomic_type*      getAtomic(       type& MemberVar,                           context& C ) noexcept { return (MemberVar) ? var_type<specializing_t>::getAtomic(const_cast<specializing_t&>(*MemberVar), C ) : nullptr; }
    };

    template< typename T_INSTANCE_TYPE, xresource::type_guid T_TYPE_GUID_V >
    struct var_type<xresource::def_guid_t<T_INSTANCE_TYPE, T_TYPE_GUID_V>> : var_defaults<"full_guid", full_guid >
    {
        inline constexpr static auto guid_v                 = T_TYPE_GUID_V.m_Value;
        constexpr static inline auto serialization_type_v   = "GG";
        template< typename T, auto CRC_V >
        constexpr static auto XCoreTextFile(T& FileStream, xproperty::any& Any)
        {
            using parent = var_defaults<"rsc_ref", xresource::def_guid_t<T_INSTANCE_TYPE, T_TYPE_GUID_V> >;
            using t      = xresource::def_guid_t<T_INSTANCE_TYPE, T_TYPE_GUID_V>;

            // If any is trash... let us reset it to our type
            if (Any.m_pType == nullptr || Any.m_pType->m_GUID != parent::guid_v) Any.Reset<t>();

            xresource::full_guid FullGuid;

            // If we are writting make sure we have set the value of the resource GUID to an actual GUID
            if (not FileStream.isReading()) 
            {
                const auto& RscRef = Any.get<t>();
                if(RscRef.isPointer()) FullGuid = xresource::g_Mgr.getFullGuid(RscRef);
                else                   FullGuid = RscRef;
            }

            auto Err = FileStream.Field(CRC_V, "Value:?", FullGuid.m_Instance.m_Value, FullGuid.m_Type.m_Value);

            // If we are reading lets check for errors and set the final value
            if( not Err && FileStream.isReading() )
            {
                // Let the user know if somehow the types of the resource has changed...
                // So let us clear the value to minimize chances of errors...
                if ( not FullGuid.m_Instance.empty() && FullGuid.m_Type != T_TYPE_GUID_V)
                {
                    printf("ERROR: The resoure reference is not longer of the same type, in the file is %016" PRIX64 " the runtime is % %016" PRIX64 " we will set the value to zero\n", FullGuid.m_Type.m_Value, T_TYPE_GUID_V.m_Value);
                    FullGuid.m_Instance.clear();
                }
                Any.get<t>().m_Instance = FullGuid.m_Instance;
            }
            return Err;
        }
    };
}
    */

//
// USEFULL GROUPS 
// 
namespace xproperty::settings
{
    using vector3_group = obj_group<"Vector3">;
    using vector2_group = obj_group<"Vector2">;
}

//
// SUPPORTING C-POINTERS TYPES
// 
namespace xproperty::settings
{
    template<typename T>
    requires(std::is_function_v<T> == false)
    struct var_type<T*> : var_ref_defaults<"pointer", T*> {};

    template<typename T>
    requires(std::is_function_v<T> == false)
    struct var_type<T**> : var_ref_defaults<"ppointer", T**> {};
}

//
// SUPPORTING CPP POINTERS TYPES
//
namespace xproperty::settings
{
    template< typename T>
    requires(std::is_function_v<T> == false)
    struct var_type<std::unique_ptr<T>> : var_ref_defaults<"unique_ptr", std::unique_ptr<T>> {};

    template< typename T>
    requires(std::is_function_v<T> == false)
    struct var_type<std::shared_ptr<T>> : var_ref_defaults<"shared_ptr", std::shared_ptr<T>> {};
}

//
// SUPPORTING CPP LISTS TYPES
//
namespace xproperty::settings
{
    template< typename T >
    struct var_type<std::span<T>> : var_list_defaults< "span", std::span<T>, T > {};

    template< typename T, auto N >
    struct var_type<std::array<T, N>> : var_list_defaults< "array", std::array<T, N>, T> {};

    namespace details
    {
        template <typename...> struct is_std_unique_ptr final : std::false_type {};
        template<class T, typename... Args>
        struct is_std_unique_ptr<std::unique_ptr<T, Args...>> final : std::true_type {};
    }

    template< typename T >
    struct var_type<std::vector<T>> : var_list_defaults< "vector", std::vector<T>, T>
    {
        using type = typename var_list_defaults< "vector", std::vector<T>, T>::type;
        constexpr static void setSize(type& MemberVar, const std::size_t Size, context&) noexcept
        {
            MemberVar.resize(Size);

            // If it is a vector of a unique pointers we can initialize them right away...
            // Otherwise, when we try to access the nullptr pointer, and it will blow up... 
            if constexpr (details::is_std_unique_ptr<T>::value)
            {
                for (auto& E : MemberVar)
                    E = std::make_unique<typename T::element_type>();
            }
        }
    };
}

//
// SUPPORTING C LISTS TYPES
//
namespace xproperty::settings
{
    template< typename T, std::size_t N >
    struct var_type<T[N]> : var_list_native_defaults< "C-Array1D", N, T[N], T, std::size_t > {};

    template< typename T, std::size_t N1, std::size_t N2 >
    struct var_type<T[N1][N2]> : var_list_native_defaults< "C-Array2D", N1, T[N1][N2], T[N2], std::size_t > {};

    template< typename T, std::size_t N1, std::size_t N2, std::size_t N3>
    struct var_type<T[N1][N2][N3]> : var_list_native_defaults< "C-Array3D", N1, T[N1][N2][N3], T[N2][N3], std::size_t > {};
}

//
// SUPPORTING ADVANCED CPP LISTS TYPES
//
namespace xproperty::settings
{
    template< typename T_KEY, typename T_VALUE_TYPE >
    struct var_type<std::map<T_KEY, T_VALUE_TYPE>> : var_list_defaults< "map", std::map<T_KEY, T_VALUE_TYPE>, T_VALUE_TYPE, typename std::map<T_KEY, T_VALUE_TYPE>::iterator, T_KEY >
    {
        using base              = var_list_defaults< "map", std::map<T_KEY, T_VALUE_TYPE>, T_VALUE_TYPE, typename std::map<T_KEY, T_VALUE_TYPE>::iterator, T_KEY >;
        using type              = typename base::type;
        using specializing_t    = typename base::specializing_t;
        using begin_iterator    = typename base::begin_iterator;
        using atomic_key        = typename base::atomic_key;

        constexpr static void             IteratorToKey(const type& MemberVar, any& Key, const begin_iterator& I, context&) noexcept { Key.set<atomic_key>(I->first); }
        constexpr static specializing_t* getObject(type& MemberVar, const any& Key, context&) noexcept
        {
            auto p = MemberVar.find(Key.get<atomic_key>());

            // if we don't find it then we are going to add it!
            if (p == MemberVar.end())
            {
                MemberVar[Key.get<atomic_key>()] = T_VALUE_TYPE{};
                p = MemberVar.find(Key.get<atomic_key>());
            }

            return &p->second;
        }
        constexpr static specializing_t* IteratorToObject(type& MemberVar, begin_iterator& I, context&) noexcept { return &(*I).second; }
    };

    template< typename T_KEY, typename T_VALUE_TYPE >
    struct var_type<std::unordered_map<T_KEY, T_VALUE_TYPE>> : var_list_defaults< "umap", std::unordered_map<T_KEY, T_VALUE_TYPE>, T_VALUE_TYPE, typename std::unordered_map<T_KEY, T_VALUE_TYPE>::iterator, T_KEY >
    {
        using base              = var_list_defaults< "umap", std::unordered_map<T_KEY, T_VALUE_TYPE>, T_VALUE_TYPE, typename std::unordered_map<T_KEY, T_VALUE_TYPE>::iterator, T_KEY >;
        using type              = typename base::type;
        using specializing_t    = typename base::specializing_t;
        using begin_iterator    = typename base::begin_iterator;
        using atomic_key        = typename base::atomic_key;

        constexpr static void            IteratorToKey(const type& MemberVar, any& Key, const begin_iterator& I, context&) noexcept { Key.set<atomic_key>(I->first); }
        constexpr static specializing_t* getObject(type& MemberVar, const any& Key, context&) noexcept
        {
            auto p = MemberVar.find(Key.get<atomic_key>());

            // if we don't find it then we are going to add it!
            if (p == MemberVar.end())
            {
                MemberVar[Key.get<atomic_key>()] = T_VALUE_TYPE{};
                p = MemberVar.find(Key.get<atomic_key>());
            }

            return &p->second;
        }
        constexpr static specializing_t* IteratorToObject(type& MemberVar, begin_iterator& I, context&) noexcept { return &(*I).second; }
    };
}

// ------------------------------------------------------------------------------
// CUSTOM USER DATA PER PROPERTY
// ------------------------------------------------------------------------------
namespace xproperty::settings
{
    struct member_help_t : xproperty::member_user_data<"help">
    {
        const char* m_pHelp;
    };
}

// We put what the user interface in a different namespace
// to make it less verbose for the user.
namespace xproperty
{
    template< xproperty::details::fixed_string HELP >
    struct member_help : settings::member_help_t
    {
        constexpr  member_help() noexcept
            : member_help_t{ .m_pHelp = HELP.m_Value } {
        }
    };
}

// ------------------------------------------------------------------------------
// GROUPING ALL THE ATOMIC TYPES AS A TUPLE
// ------------------------------------------------------------------------------
namespace xproperty::settings
{
    using atomic_types_tuple = std::tuple
    < std::int32_t
    , std::uint32_t
    , std::int16_t
    , std::uint16_t
    , std::int8_t
    , std::uint8_t
    , float
    , double
    , std::string
    , std::wstring
    , std::uint64_t
    , std::int64_t
    , bool
    >;
}

// ------------------------------------------------------------------------------
// USEFUL FUNCTIONS
// ------------------------------------------------------------------------------
namespace xproperty::settings
{
    inline int AnyToString(std::span<char> String, const xproperty::any& Value) noexcept
    {
        switch (Value.getTypeGuid())
        {
        case xproperty::settings::var_type<std::int32_t>::guid_v:    return sprintf_s(String.data(), String.size(), "%d", Value.get<std::int32_t>());
        case xproperty::settings::var_type<std::uint32_t>::guid_v:   return sprintf_s(String.data(), String.size(), "%u", Value.get<std::uint32_t>());
        case xproperty::settings::var_type<std::int16_t>::guid_v:    return sprintf_s(String.data(), String.size(), "%d", Value.get<std::int16_t>());
        case xproperty::settings::var_type<std::uint16_t>::guid_v:   return sprintf_s(String.data(), String.size(), "%u", Value.get<std::uint16_t>());
        case xproperty::settings::var_type<std::int8_t>::guid_v:     return sprintf_s(String.data(), String.size(), "%d", Value.get<std::int8_t>());
        case xproperty::settings::var_type<std::uint8_t>::guid_v:    return sprintf_s(String.data(), String.size(), "%u", Value.get<std::uint8_t>());
        case xproperty::settings::var_type<float>::guid_v:           return sprintf_s(String.data(), String.size(), "%f", Value.get<float>());
        case xproperty::settings::var_type<double>::guid_v:          return sprintf_s(String.data(), String.size(), "%f", Value.get<double>());
        case xproperty::settings::var_type<std::string>::guid_v:     return sprintf_s(String.data(), String.size(), "%s", Value.get<std::string>().c_str());
        case xproperty::settings::var_type<std::wstring>::guid_v:    return sprintf_s(String.data(), String.size(), "%ls", Value.get<std::wstring>().c_str());
        case xproperty::settings::var_type<std::uint64_t>::guid_v:   return sprintf_s(String.data(), String.size(), "%llu", Value.get<std::uint64_t>());
        case xproperty::settings::var_type<std::int64_t>::guid_v:    return sprintf_s(String.data(), String.size(), "%lld", Value.get<std::int64_t>());
        case xproperty::settings::var_type<bool>::guid_v:            return sprintf_s(String.data(), String.size(), "%s", Value.get<bool>() ? "true" : "false");
        default: assert(false); break;
        }

        return 0;
    }

    #pragma warning(push)
    #pragma warning(disable : 4996)
    inline
    std::wstring convert_span_to_wstring(std::span<char> span) {
        std::wstring wstr(span.size(), L'\0');
        std::mbstowcs(wstr.data(), span.data(), span.size());
        return wstr;
    }
    // Re-enable warning C4996
    #pragma warning(pop)

    inline bool StringToAny( xproperty::any& Value, std::uint32_t TypeGUID, const std::span<char> String) noexcept
    {
        switch (TypeGUID)
        {
        case xproperty::settings::var_type<std::int32_t>::guid_v:    Value.set<std::int32_t>    (static_cast<std::int32_t>(std::stol(String.data())));  return true;
        case xproperty::settings::var_type<std::uint32_t>::guid_v:   Value.set<std::uint32_t>   (static_cast<std::uint32_t>(std::stoul(String.data())));  return true;
        case xproperty::settings::var_type<std::int16_t>::guid_v:    Value.set<std::int16_t>    (static_cast<std::int16_t>(std::atoi(String.data())));  return true;
        case xproperty::settings::var_type<std::uint16_t>::guid_v:   Value.set<std::uint16_t>   (static_cast<std::uint16_t>(std::atoi(String.data())));  return true;
        case xproperty::settings::var_type<std::int8_t>::guid_v:     Value.set<std::int8_t>     (static_cast<std::int8_t>(std::atoi(String.data())));  return true;
        case xproperty::settings::var_type<std::uint8_t>::guid_v:    Value.set<std::uint8_t>    (static_cast<std::uint8_t>(std::atoi(String.data())));  return true;
        case xproperty::settings::var_type<float>::guid_v:           Value.set<float>           (static_cast<float>(std::stof(String.data())));  return true;
        case xproperty::settings::var_type<double>::guid_v:          Value.set<double>          (static_cast<float>(std::stof(String.data())));  return true;
        case xproperty::settings::var_type<std::string>::guid_v:     Value.set<std::string>     (std::string(String.data(), String.size()));  return true;
        case xproperty::settings::var_type<std::wstring>::guid_v:    Value.set<std::wstring>    (convert_span_to_wstring(String)); return true;
        case xproperty::settings::var_type<std::uint64_t>::guid_v:   Value.set<std::uint64_t>   (static_cast<std::uint64_t>(std::stoull(String.data())));  return true;
        case xproperty::settings::var_type<std::int64_t>::guid_v:    Value.set<std::int64_t>    (static_cast<std::int64_t>(std::stoll(String.data())));  return true;
        case xproperty::settings::var_type<bool>::guid_v:            Value.set<bool>            ((String[0]=='t' || String[0]=='1' || String[0]=='T')?true:false);  return true;
        default: assert(false); break;
        }
        return false; 
    }

    //
    // Example convert from ctype to guid (Thanks to the atomic_types_tuple)
    //
    namespace details
    {
        // Collisionless hash map creation
        static constexpr auto ctype_to_guid_hashtable_v = []<typename...T>(std::tuple<T...>*) consteval
        {
            std::array<std::uint32_t, 256> CType2GUID{};
            ((CType2GUID[xproperty::settings::var_type<T>::ctype_v] = xproperty::settings::var_type<T>::guid_v), ...);

            return CType2GUID;

        }(static_cast<atomic_types_tuple*>(0));

        struct entry
        {
            std::uint32_t m_GUID{};
            char          m_CType{};
        };

        // Collision based hash map creation
        static constexpr auto guid_to_ctype_hash_size_v = 256;
        static constexpr auto guid_to_ctype_hashtable_v = []<typename...T>(std::tuple<T...>*) consteval
        {
            std::array<entry, guid_to_ctype_hash_size_v> GUID2XTypeHash{};

            // Fill the hash
            ([&]<typename X>(X*)
            {
                auto Index = xproperty::settings::var_type<X>::guid_v % guid_to_ctype_hash_size_v;
                for (; GUID2XTypeHash[Index].m_CType; ) Index = (Index + 1) % guid_to_ctype_hash_size_v;
                GUID2XTypeHash[Index].m_GUID = xproperty::settings::var_type<X>::guid_v;
                GUID2XTypeHash[Index].m_CType = xproperty::settings::var_type<X>::ctype_v;
            }(static_cast<T*>(0)), ...);

            return GUID2XTypeHash;

        }(static_cast<atomic_types_tuple*>(0));
    }
    
    constexpr std::uint32_t CTypeToGUID(const char Type) noexcept
    {
        // Convert to GUID
        auto E = details::ctype_to_guid_hashtable_v[Type];
        assert(E != 0);
        return E;
    }
    

    constexpr char GUIDToCType(std::uint32_t GUID) noexcept
    {
        // Hash map look up
        auto Index = GUID % details::guid_to_ctype_hash_size_v;
        do
        {
            auto& E = details::guid_to_ctype_hashtable_v[Index];
            assert(E.m_GUID);
            if (E.m_GUID == GUID) break;
            Index = (Index + 1) % details::guid_to_ctype_hash_size_v;
        } while (true);

        return details::guid_to_ctype_hashtable_v[Index].m_CType;
    }
}

// ------------------------------------------------------------------------------
//  ADD SUPPORT FOR IMGUI UI/EDITOR
// ------------------------------------------------------------------------------
#include "dependencies\xproperty\source\examples\imgui\my_property_ui.h"

/////////////////////////////////////////////////////////////////
// DONE
/////////////////////////////////////////////////////////////////
#endif