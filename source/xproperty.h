#ifndef XPROPERTY_H
#define XPROPERTY_H
#pragma once

#include<array>
#include<tuple>
#include<ranges>
#include<assert.h>
#include<variant>
#include<format>

namespace xproperty
{
    //
    // Dependent type which is always is false
    //
    template< typename T > struct always_false : std::false_type {};

    //
    // Check is a type is 
    //
    namespace details
    {
        namespace details
        {
            template< template< typename... > typename T_ORIGINAL, typename    T_SPECIALIZED > struct   is_specialized : std::false_type {};
            template< template< typename... > typename T_ORIGINAL, typename... T_ARGS        > struct   is_specialized<T_ORIGINAL, T_ORIGINAL<T_ARGS...>> : std::true_type {};
        }
        template< template< typename... > typename T_ORIGINAL, typename    T_SPECIALIZED > constexpr static bool is_specialized_v = details::is_specialized<T_ORIGINAL, T_SPECIALIZED>::value;
    }

    //
    // Remove all const from a stype
    //
    namespace details
    {
        template<typename T, typename T2 >
        struct delay_linkage
        {
            using type = T;
        };
        
        namespace details
        {
            template<bool IS_POINTER_V, typename T, typename T_CLEAN >
            struct remove_all_const;

            template<typename T, typename T_CLEAN >
            struct remove_all_const<true, T*, T_CLEAN>
            {
                using base = typename remove_all_const<std::is_pointer_v<T>, T, T_CLEAN* >::base;
                using type = typename remove_all_const<std::is_pointer_v<T>, T, T_CLEAN* >::type;
                inline static constexpr bool value = std::is_const_v<T> || remove_all_const<std::is_pointer_v<T>, T, T_CLEAN* >::value;
            };

            template<typename T, typename T_CLEAN >
            struct remove_all_const<true, T* const, T_CLEAN> : remove_all_const<true, T*, T_CLEAN>
            {
                inline static constexpr bool value = true;
            };

            template<typename T, typename T_CLEAN, typename...T_ARGS >
            struct remove_all_const<false, T(T_ARGS...)const, T_CLEAN>
            {
                using base = T(T_ARGS...);
                using type = T_CLEAN;
                inline static constexpr bool value = false;
            };

            template<typename T, typename T_CLEAN >
            struct remove_all_const<false, T, T_CLEAN>
            {
                using base = std::remove_const_t<T>;
                using type = T_CLEAN;
                inline static constexpr bool value = std::is_const_v<T>;
            };
        }

        template<typename T>
        using remove_all_const_t = typename details::remove_all_const
        < std::is_pointer_v<T>
        , T
        , typename details::remove_all_const<std::is_pointer_v<T>, T, void>::base
        >::type;

        template< typename T>
        inline constexpr bool has_const_v = details::remove_all_const< std::is_pointer_v<T>, T, void>::value;

        static_assert( std::is_same_v<remove_all_const_t<const void** const>, void**> );
        static_assert( has_const_v< void* const> );
        static_assert(has_const_v< const char >);
    }


    //
    // TAG
    // Used to member_help with templates by easily identifying its type...
    //
    struct base_tag{};

    template<typename T_TAG>
    struct tag : base_tag
    {
        using type = T_TAG;
    };

    template< typename...T_ARGS >
    inline constexpr bool has_tags_v = ((std::is_base_of_v<base_tag, T_ARGS>) || ...);

    namespace details
    {
        template< typename... T_ARGS>
        consteval bool has_Tags( std::tuple<T_ARGS...>*) noexcept
        {
            return (( std::is_base_of_v<base_tag, T_ARGS>) || ...);
        }
    }

    template< typename T_TUPLE >
    inline constexpr bool tuple_has_tags_v = details::has_Tags( reinterpret_cast<T_TUPLE*>(nullptr) );

    //
    // Tuple Helpers
    //
    namespace details
    {
        //
        // type to index
        //
        namespace detail
        {
            template <typename T, typename T_TUPLE>
            struct tuple_t2i;

            template <typename T, typename... T_TYPES>
            struct tuple_t2i<T, std::tuple<T, T_TYPES...>>
            {
                constexpr static inline std::size_t value = 0;
            };

            template <typename T, typename T_NEXT, typename... T_TYPES>
            struct tuple_t2i<T, std::tuple<T_NEXT, T_TYPES...>>
            {
                constexpr static inline  std::size_t value = 1 + tuple_t2i<T, std::tuple<T_TYPES...>>::value;
            };
        }
        template< typename T, typename T_TUPLE>
        constexpr auto tuple_t2i_v = detail::tuple_t2i<T, T_TUPLE>::value;

        template< std::size_t T_INDEX_V, typename T_TUPLE>
        using tuple_i2t = std::tuple_element_t< T_INDEX_V, T_TUPLE >;

        //
        // tuple from variant
        //
        namespace details
        {
            template<typename T>
            struct tuple_from_variant;

            template<typename...T>
            struct tuple_from_variant< std::variant<T...> >
            {
                using type = std::tuple<T...>;
            };
        }
        template< typename T_VARIANT >
        using tuple_from_variant_t = typename details::tuple_from_variant<T_VARIANT>::type;

        //
        // Tuple tag to index
        //
        namespace detail
        {
            template < typename T_TAG, typename T_TUPLE >
            struct tuple_tag2i;

            template <typename T_TAG>
            struct tuple_tag2i<T_TAG, std::tuple<>>
            {
                constexpr static inline std::size_t value = 0;
            };

            template <typename T_TAG, typename T_NEXT, typename... T_TYPES>
            struct tuple_tag2i<T_TAG, std::tuple<T_NEXT, T_TYPES...>>
            {
                constexpr static inline  std::size_t value = 1 + (std::is_base_of_v<T_TAG, T_NEXT> ? 0 : tuple_tag2i< T_TAG, std::tuple<T_TYPES...>>::value);
            };
        }
        template< typename T_TAG, typename T_TUPLE >
        constexpr auto tuple_tag2i_v = detail::tuple_tag2i<tag<T_TAG>, T_TUPLE>::value - 1;

        template< typename T_TAG, typename T_TUPLE >
        using tuple_tag2t = tuple_i2t<tuple_tag2i_v<T_TAG, T_TUPLE>, T_TUPLE>;

        //
        // Tuple has tag?
        //
        namespace detail
        {
            template < typename T_TAG, typename T_TUPLE >
            struct tuple_has_tag;

            template <typename T_TAG>
            struct tuple_has_tag<T_TAG, std::tuple<>>
            {
                constexpr static inline bool value = false;
            };

            template <typename T_TAG, typename T_NEXT, typename... T_TYPES>
            struct tuple_has_tag<T_TAG, std::tuple<T_NEXT, T_TYPES...>>
            {
                constexpr static inline  std::size_t value = false | (std::is_base_of_v<T_TAG, T_NEXT> ? true : tuple_has_tag< T_TAG, std::tuple<T_TYPES...>>::value);
            };
        }
        template< typename T_TAG, typename T_TUPLE >
        constexpr auto tuple_has_tag_v = detail::tuple_has_tag<tag<T_TAG>, T_TUPLE>::value;

        //
        // Help concadenate tuples
        //
        template<typename ... T_TUPLES>
        using tuple_cat_t = decltype(std::tuple_cat(std::declval<T_TUPLES>()...));

        //
        // Filter Arguments by a tag and return a tuple with all the right types
        //
        template< typename T_TAG, typename...T_ARGS >
        using filter_by_tag_t = xproperty::details::tuple_cat_t< std::conditional_t<std::is_base_of_v<tag<T_TAG>, T_ARGS>, std::tuple<T_ARGS>, std::tuple<> > ... >;
    }

    //
    // Fixed string
    //
    namespace details
    {
        template<std::size_t T_SIZE_V>
        struct fixed_string
        {
            inline constexpr static uint32_t size_v = T_SIZE_V;
            consteval fixed_string(const char* const pStr)
            {
                for (auto i = 0u; i < T_SIZE_V; ++i) m_Value[i] = pStr[i];
            }

            consteval auto operator<=>(const fixed_string&) const = default;
            consteval operator const char* const () const { return m_Value; }
            consteval static uint32_t size ( void ) noexcept { return size_v; }
            char m_Value[T_SIZE_V];
        };

        template<std::size_t T_SIZE_V>
        fixed_string(const char(&)[T_SIZE_V])-> fixed_string<T_SIZE_V>;

        //--------------------------------------------------------------------------------------------
        // A simple string view
        //--------------------------------------------------------------------------------------------
        class str_view 
        {
        public:
            constexpr str_view( const char* a, uint32_t size ) noexcept : m_pStr( a ), m_Size( size - 1 ) {}
            template<size_t N>
            constexpr str_view( const char( &a )[ N ] ) noexcept : m_pStr( a ), m_Size( N - 1 ) {}
            template<size_t N>
            constexpr str_view(const std::array<char,N>& a) noexcept : m_pStr(a.data()), m_Size(N - 1) {}
            template<size_t N>
            constexpr str_view(const fixed_string<N>& a) noexcept : m_pStr(a.m_Value), m_Size(N - 1) {}
            constexpr str_view( const char* a, int& i ) noexcept 
                      : m_pStr(a)
                      , m_Size
                      {   static_cast<uint32_t>
                          ([]( const char* a, int& i ) constexpr
                          { 
                             for( i=0; a[i] && a[i] !='/' && a[i] != '[' ; i++ ); 
                             return i;
                          }(a, i)) 
                      }{}

            constexpr char          operator[]  ( std::size_t n )                           const noexcept { assert( n < m_Size ); return m_pStr[ n ]; }
            constexpr uint32_t      get_block   ( std::size_t BlockSize, const int idx )    const noexcept 
            {
                const auto     i  = ( BlockSize + idx ) * 4;
                const uint32_t b0 = static_cast<uint32_t>(m_pStr[ i ]);
                const uint32_t b1 = static_cast<uint32_t>(m_pStr[ i + 1 ]);
                const uint32_t b2 = static_cast<uint32_t>(m_pStr[ i + 2 ]);
                const uint32_t b3 = static_cast<uint32_t>(m_pStr[ i + 3 ]);
                return ( b3 << 24 ) | ( b2 << 16 ) | ( b1 << 8 ) | b0;
            }
            constexpr uint32_t      size        ( void )                const noexcept { return m_Size; }
            constexpr uint32_t      block_size  ( void )                const noexcept { return m_Size / 4; }
            constexpr char          tail        ( const int n )         const noexcept 
            {
                const int tail_size = m_Size % 4;
                return m_pStr[ m_Size - tail_size + n ];
            }

        protected:
            const char*     m_pStr;
            uint32_t        m_Size;
        };
    }

    //
    // Function Traits
    //
    namespace details
    {
        template<class T> struct function_traits;

        template<class T_RETURN, typename... T_ARGS>
        struct function_traits<T_RETURN(T_ARGS...)>
        {
            using                           return_type = T_RETURN;
            using                           args        = std::tuple<T_ARGS...>;
            constexpr static bool           is_member_v = false;
        };

        template <typename T_RETURN, typename... T_ARGS> struct function_traits<T_RETURN(T_ARGS...) noexcept> : public function_traits<T_RETURN(T_ARGS...)> {};

        template <typename T_RETURN, typename... T_ARGS> struct function_traits<T_RETURN(*)(T_ARGS...) noexcept> : public function_traits<T_RETURN(T_ARGS...)> {};
        template <typename T_RETURN, typename... T_ARGS> struct function_traits<T_RETURN(*)(T_ARGS...)         > : public function_traits<T_RETURN(T_ARGS...)> {};

        template<class T_RETURN, typename T_CLASS, typename... T_ARGS>
        struct function_traits<T_RETURN (T_CLASS::*)(T_ARGS...)> : function_traits<T_RETURN(T_ARGS...)>
        {
            using                           class_t     = T_CLASS;
            constexpr static bool           is_member_v = true;
        };

        template <typename T_RETURN, typename T_CLASS, typename... T_ARGS> struct function_traits<T_RETURN(T_CLASS::*)(T_ARGS...) noexcept>              : public function_traits<T_RETURN(T_CLASS::*)(T_ARGS...)> {};
        template <typename T_RETURN, typename T_CLASS, typename... T_ARGS> struct function_traits<T_RETURN const (T_CLASS::*)(T_ARGS...) noexcept>       : public function_traits<T_RETURN(T_CLASS::*)(T_ARGS...)> {};
        template <typename T_RETURN, typename T_CLASS, typename... T_ARGS> struct function_traits<T_RETURN const (T_CLASS::* const)(T_ARGS...) noexcept> : public function_traits<T_RETURN(T_CLASS::*)(T_ARGS...)> {};
        template <typename T_RETURN, typename T_CLASS, typename... T_ARGS> struct function_traits<T_RETURN const (T_CLASS::*)(T_ARGS...) >               : public function_traits<T_RETURN(T_CLASS::*)(T_ARGS...)> {};
        template <typename T_RETURN, typename T_CLASS, typename... T_ARGS> struct function_traits<T_RETURN const (T_CLASS::* const)(T_ARGS...) >         : public function_traits<T_RETURN(T_CLASS::*)(T_ARGS...)> {};
    }

    //
    // basic type
    //
    struct basic_type
    {
        void const* m_Value;
    };

    template<typename T>
    struct type_meta : basic_type
    {
        inline static constexpr char        size  = sizeof(T);
        inline static constexpr void const* value = &size;
        consteval type_meta() : basic_type{ value }{}
    };

    namespace type
    {
        struct object;
        struct any;
    }

    template< typename T >
    constexpr const type::object* getObjectByType(void) noexcept;

    template< typename T >
    constexpr const type::object* getObject(const T& ClassInstace) noexcept;

    //
    // PropertyObject base class to give your the user the ability to define hierarchical properties
    //
    struct base
    {
        virtual const type::object* getProperties   ( void ) const noexcept = 0;
        virtual                    ~base            ( void )       noexcept = default;
    };

    //
    // SETTINGS
    //
    namespace settings
    {
        struct context;

        //--------------------------------------------------------------------------------------------
        // computes murmur hash
        // http://szelei.me/constexpr-murmurhash/
        //--------------------------------------------------------------------------------------------
        constexpr uint32_t strguid(xproperty::details::str_view key, uint32_t seed = 0x9747b28c) noexcept
        {
            const uint32_t  c1 = 0xcc9e2d51;
            const uint32_t  c2 = 0x1b873593;

            uint32_t h1 = seed;
            {
                const int nblocks = static_cast<int>(key.block_size());
                for (int i = -nblocks; i; i++)
                {
                    uint32_t k1 = key.get_block(nblocks, i);

                    k1 *= c1;
                    k1 = (k1 << 15) | (k1 >> (32 - 15));
                    k1 *= c2;

                    h1 ^= k1;
                    h1 = (h1 << 13) | (h1 >> (32 - 13));
                    h1 = h1 * 5 + 0xe6546b64;
                }
            }
            {
                uint32_t k1 = 0;
                switch (key.size() & 3)
                {
                case 3: k1 ^= key.tail(2) << 16;
                case 2: k1 ^= key.tail(1) << 8;
                case 1: k1 ^= key.tail(0);
                    k1 *= c1;
                    k1 = (k1 << 15) | (k1 >> (32 - 15));
                    k1 *= c2;
                    h1 ^= k1;
                };
            }

            h1 ^= key.size();
            h1 ^= h1 >> 16;
            h1 *= 0x85ebca6b;
            h1 ^= h1 >> 13;
            h1 *= 0xc2b2ae35;
            h1 ^= h1 >> 16;

            // Can not be zero... 
            assert(h1);
            return h1;
        }

        //--------------------------------------------------------------------------------------------
        // Add support for enums_unregistered
        //--------------------------------------------------------------------------------------------
        struct enum_item
        {
            const char*                 m_pName;
            std::uint32_t               m_Value;

            enum_item() = default;

            template<typename T>
            constexpr enum_item( const char* pName, T Val )
                : m_pName { pName }
                , m_Value { static_cast<std::uint32_t>(Val) }
            {
                static_assert( std::is_enum_v<T> );
            }
        };

        //--------------------------------------------------------------------------------------------
        // We use the following template to define the default information for a xproperty type.
        // We can use the var_defaults template to help us define our type.
        //--------------------------------------------------------------------------------------------
        namespace details
        {
            template< bool T_ENABLE_V, typename T >
            struct add_unregistered_enum ;

            template<typename T >
            struct add_unregistered_enum<false, T>
            {
                using                        type   = T;
                inline constexpr static auto guid_v = 0u;
            };

            template<typename T >
            struct add_unregistered_enum<true, T>
            {
                static_assert(std::is_const_v<T> == false, "Please don't define types that are const");

                using                                              type        = T;
                inline constexpr static auto                       name_v      = xproperty::details::fixed_string("Unregistered Enum");
                inline constexpr static std::span<const enum_item> enum_list_v = {};
                inline constexpr static auto                       guid_v      = 1u;

                static_assert(sizeof(type) <= sizeof(data_memory));

                constexpr static void Write           ( type&       MemberVar,  const type& Data, context&  ) noexcept { MemberVar = Data; }
                constexpr static void Read            ( const type& MemberVar,        type& Data, context&  ) noexcept { Data      = MemberVar; }
                constexpr static void VoidConstruct   ( data_memory& Data                                   ) noexcept { new(&Data) type{}; }
                constexpr static void Destruct        ( data_memory& Data                                   ) noexcept { std::destroy_at(&reinterpret_cast<type&>(Data) ); }
                constexpr static void MoveConstruct   ( data_memory& Data1,           type&& Data2          ) noexcept { new(&Data1) type{ Data2 }; }
                constexpr static void CopyConstruct   ( data_memory& Data1,     const type&  Data2          ) noexcept { new(&Data1) type{ Data2 }; }
            };
        }

        //
        // Default information that you should have for a xproperty atomic variable.
        // Note that this particular specialization will only be used for unregistered
        // types such... unregistered enums
        //
        template< typename T >
        struct var_type : details::add_unregistered_enum<std::is_enum_v<T>, T>
        {
            static_assert(std::is_const_v<T> == false, "Please don't define types that are const");

            inline constexpr static bool is_list_v      = false;
            inline constexpr static bool is_pointer_v   = false;
            inline constexpr static auto is_const_v     = false;
            using                        atomic_type    = T;
            using                        specializing_t = T;
            using                        type           = T;

            inline constexpr static auto* getAtomic( type& MemberVar, context& ) noexcept { return &MemberVar; }
        };

        namespace details
        {
            template<typename T>
            using specializing_t = xproperty::details::remove_all_const_t<std::remove_reference_t<std::invoke_result_t<decltype([](T&& A)->auto&& { return *A; }), T >>>;

            template< typename T >
            consteval auto&& Resolve(T&& a)
            {
                #ifdef __clang__
                    #pragma clang diagnostic push
                    #pragma clang diagnostic ignored "-Wreturn-stack-address"
                #endif      
                using st = specializing_t<T>;
                static_assert(var_type<st>::is_list_v == false );
                if constexpr (var_type<st>::is_pointer_v ) return Resolve(st{});
                else                                       return st{};
                #ifdef __clang__
                    #pragma clang diagnostic pop
                #endif
            }

            template< typename T >
            using atomic_type_t = std::remove_reference_t<std::invoke_result_t<decltype(details::Resolve<T>), T>>;


            template<typename T>
            using specializing_t2 = std::remove_reference_t< std::invoke_result_t<decltype([](T&& A)->auto&& { return *A; }), T >>;

            template< typename T >
            consteval auto HasConstResolve(T&& a)
            {
                if constexpr (var_type<xproperty::details::remove_all_const_t <T>>::is_pointer_v)
                {
                    using st  = specializing_t2<T>;
                    using stc = xproperty::details::remove_all_const_t <st>;
                    static_assert(var_type<stc>::is_list_v == false);
                    if constexpr (xproperty::details::has_const_v<st>) return std::true_type {};
                    else
                    {
                        if constexpr (var_type<stc>::is_pointer_v) return HasConstResolve(st{});
                        else                                       return std::false_type{};
                    }
                }
                else
                {
                    if constexpr (xproperty::details::has_const_v<T>) return std::true_type {};
                    else                                             return std::false_type{};
                }
            }

            template< typename T >
            inline constexpr bool has_const_resolve_v = xproperty::details::has_const_v<T>
                        || std::is_same_v< std::true_type, std::remove_reference_t<std::invoke_result_t<decltype(details::HasConstResolve<T>), T>>>;

        }

        //--------------------------------------------------------------------------------------------
        // Default information that you should have for a xproperty variable.
        // This class servers as a convenient way to define the default information for a xproperty
        // Also documents the interface that a xproperty type should have.
        //--------------------------------------------------------------------------------------------
        template<xproperty::details::fixed_string T_NAME_V, typename T >
        struct var_defaults
        {
            static_assert(std::is_const_v<T> == false, "Please don't define types that are const");

            inline constexpr static bool is_list_v      = false;
            inline constexpr static bool is_pointer_v   = false;
            inline constexpr static auto name_v         = T_NAME_V;
            inline constexpr static auto guid_v         = xproperty::settings::strguid(name_v);
            inline constexpr static auto is_const_v     = false;
            using                        type           = T;
            using                        atomic_type    = T;
            using                        specializing_t = T;

            static_assert(sizeof(type) <= sizeof(data_memory));

            // Any of the following can be overriden
            constexpr static void              Write           ( type&       MemberVar, const type& Data, context& ){ MemberVar = Data;      }
            constexpr static void              Read            ( const type& MemberVar, type&       Data, context& ){ Data      = MemberVar; }
            constexpr static specializing_t*   getObject       ( type& MemberVar, context&) noexcept { return &MemberVar; }
            constexpr static atomic_type*      getAtomic       ( type& MemberVar, context&) noexcept { return &MemberVar; }

            constexpr static void              VoidConstruct   ( data_memory& Data )                            noexcept  { new(&Data) type{}; }
            constexpr static void              Destruct        ( data_memory& Data )                            noexcept  { std::destroy_at(&reinterpret_cast<type&>(Data) ); }
            constexpr static void              MoveConstruct   ( data_memory& Data1,       type&& Data2 )       noexcept  { new(&Data1) type{ Data2 }; }
            constexpr static void              CopyConstruct   ( data_memory& Data1, const type&  Data2 )       noexcept  { new(&Data1) type{ Data2 }; }

        };

        //--------------------------------------------------------------------------------------------
        // There are two kinds of references in C++ (Pointers and actual references)
        // POINTERS
        //    For Pointers we have many... (T*, unique_ptr<T>, share_ptr<T>, etc.. )
        //    So for pointer types we will let the user specify/customize them
        //
        // REFERENCES
        //    For references we have two kinds (l & r)
        //    So we will handle those two kinds internally so the user should never have to worry about them.
        //--------------------------------------------------------------------------------------------
        template<xproperty::details::fixed_string T_NAME_V, typename T_REF_TYPE >
        struct var_ref_defaults
        {
            static_assert(xproperty::details::has_const_v<T_REF_TYPE> == false, "Please don't define types that are const");

            inline constexpr static bool is_list_v      = false;
            inline constexpr static bool is_pointer_v   = true;
            inline constexpr static auto name_v         = T_NAME_V;
            inline constexpr static auto guid_v         = xproperty::settings::strguid(name_v);
            using                        type           = T_REF_TYPE;
            using                        atomic_type    = details::atomic_type_t<T_REF_TYPE>;
            using                        specializing_t = details::specializing_t<T_REF_TYPE>;
            inline constexpr static auto is_const_v     = false;//details::has_const_resolve_v<T_REF_TYPE>;

            // Any of the following can be overriden
            constexpr static void              Write    (       type& MemberVar, const atomic_type& Data,  context& C ) noexcept { if( MemberVar != nullptr ) var_type<specializing_t>::Write( *MemberVar, Data, C ); }
            constexpr static void              Read     ( const type& MemberVar,       atomic_type& Data,  context& C ) noexcept { if( MemberVar != nullptr ) var_type<specializing_t>::Read ( *MemberVar, Data, C ); }
            constexpr static specializing_t*   getObject(       type& MemberVar,                           context&   ) noexcept { return (MemberVar) ? &(*MemberVar) : nullptr; }
            constexpr static atomic_type*      getAtomic(       type& MemberVar,                           context& C ) noexcept { return (MemberVar) ? var_type<specializing_t>::getAtomic(const_cast<specializing_t&>(*MemberVar), C ) : nullptr; }
        };

        //--------------------------------------------------------------------------------------------
        // These are for any kind of list type; std::array, std::vector, etc...
        //--------------------------------------------------------------------------------------------
        template< xproperty::details::fixed_string T_NAME_V, typename T_LIST, typename T_SPECIALIZING_TYPE, typename T_ITERATOR = typename T_LIST::iterator, typename T_ATOMIC_KEY = std::size_t>
        struct var_list_defaults
        {
            static_assert(xproperty::details::has_const_v<T_LIST> == false, "Please don't define types that are const");

            inline constexpr static bool is_list_v      = true;
            inline constexpr static bool is_pointer_v   = false; 
            inline constexpr static auto name_v         = T_NAME_V;
            inline constexpr static auto guid_v         = xproperty::settings::strguid(name_v);
            using                        type           = T_LIST;
            using                        specializing_t = xproperty::details::remove_all_const_t<T_SPECIALIZING_TYPE>;
            using                        atomic_type    = typename var_type<specializing_t>::atomic_type;
            using                        begin_iterator = T_ITERATOR;
            using                        end_iterator   = T_ITERATOR;
            using                        atomic_key     = T_ATOMIC_KEY;
            using                        any_t          = typename xproperty::details::delay_linkage< xproperty::type::any, T_ATOMIC_KEY >::type;

            inline constexpr static auto is_const_v     = xproperty::details::has_const_v<T_SPECIALIZING_TYPE> || var_type<specializing_t>::is_const_v || std::is_const_v<T_SPECIALIZING_TYPE>;

            // we should make sure that our iterator is not larger than the memory reserved for it
            static_assert(sizeof(begin_iterator) <= sizeof(iterator_memory));
            static_assert(sizeof(end_iterator)   <= sizeof(iterator_memory));
            static_assert(sizeof(atomic_key)     <= sizeof(data_memory));

            // Any of the following can be overriden 
            constexpr static void             Write           (       specializing_t& SpecTypeVar, const atomic_type&       Data,                       context& C ) noexcept { var_type<specializing_t>::Write( SpecTypeVar, Data, C ); }
            constexpr static void             Read            ( const specializing_t& SpecTypeVar,       atomic_type&       Data,                       context& C ) noexcept { var_type<specializing_t>::Read ( SpecTypeVar, Data, C ); }
            constexpr static void             Start           (       type&           MemberVar,         begin_iterator&    I,                          context&   ) noexcept { new(&I)   begin_iterator{ MemberVar.begin() }; }
            constexpr static void             End             (       type&           MemberVar,         end_iterator&      End,                        context&   ) noexcept { new(&End) end_iterator  { MemberVar.end() };   }
            constexpr static bool             Next            ( const type&           MemberVar,         begin_iterator&    I, const end_iterator& End, context&   ) noexcept 
            { 
                ++I; 
                return I != End; 
            }
            constexpr static std::size_t      getSize         ( const type&           MemberVar,                                                        context&   ) noexcept 
            { 
                auto size = MemberVar.size();
                return size;
            }
            constexpr static void             setSize         (       type&           MemberVar,   const std::size_t        Size,                       context&   ) noexcept {}
            constexpr static void             IteratorToKey   ( const type&           MemberVar,         any_t&             Key, const begin_iterator& I, context& ) noexcept { Key.template set<atomic_key>(I - MemberVar.begin()); }
            constexpr static specializing_t*  IteratorToObject(       type&           MemberVar,         begin_iterator&    I,                          context&   ) noexcept { return const_cast<specializing_t*>(&(*I)); }
            constexpr static specializing_t*  getObject       (       type&           MemberVar,   const any_t&             Key,                        context&   ) noexcept { return const_cast<specializing_t*>(&MemberVar[Key.template get<atomic_key>()]); }
            constexpr static void             DestroyBeginIterator  ( begin_iterator& I, context& ) noexcept { std::destroy_at(&I); }
            constexpr static void             DestroyEndIterator    ( end_iterator& I, context&   ) noexcept { std::destroy_at(&I); }
        };

        //--------------------------------------------------------------------------------------------
        // Add support for native c-arrays
        //--------------------------------------------------------------------------------------------
        template< xproperty::details::fixed_string T_NAME_V, std::size_t N, typename T_LIST, typename T_SPECIALIZING_TYPE, typename T_ITERATOR >
        struct var_list_native_defaults : var_list_defaults< T_NAME_V, T_LIST, T_SPECIALIZING_TYPE, T_ITERATOR >
        {
            static_assert(xproperty::details::has_const_v<T_LIST> == false, "Please don't define types that are const");

            using def_t             = var_list_defaults< T_NAME_V, T_LIST, T_SPECIALIZING_TYPE, T_ITERATOR >;
            using atomic_key        = typename def_t::atomic_key;
            using type              = typename def_t::type;
            using begin_iterator    = typename def_t::begin_iterator;
            using end_iterator      = typename def_t::end_iterator;
            using specializing_t    = typename def_t::specializing_t;
            using any_t             = typename xproperty::details::delay_linkage< xproperty::type::any, atomic_key >::type;

            constexpr static std::size_t      getSize         ( const type& MemberVar,                            context&) noexcept { return N; }
            constexpr static void             IteratorToKey   ( const type& MemberVar,       any_t&          Key, const begin_iterator& I, context& ) noexcept { Key.template set<atomic_key>(I); }
            constexpr static void             Start           (       type& MemberVar,       begin_iterator& I,   context&) noexcept { I = 0; }
            constexpr static void             End             (       type& MemberVar,       end_iterator&   End, context&) noexcept { End = N; }
            constexpr static specializing_t*  IteratorToObject(       type& MemberVar,       begin_iterator& I,   context&) noexcept { return &MemberVar[I]; }
        };
    }

    //
    // TYPES
    //
    namespace type
    {
        struct list_table;

        template<typename T>
        using var_t = xproperty::settings::var_type<xproperty::details::remove_all_const_t<T>>;

        struct atomic
        {
            using enum_item         = xproperty::settings::enum_item;
            using void_construct    = void(settings::data_memory&);
            using destruct          = void(settings::data_memory&);
            using move_construct    = void(settings::data_memory&,       settings::data_memory&);
            using copy_construct    = void(settings::data_memory&, const settings::data_memory&);

            const char*                 m_pName;
            std::uint32_t               m_GUID;
            std::uint32_t               m_Size;
            bool                        m_IsEnum;
            mutable std::span<const enum_item>  m_RegisteredEnumSpan;
            void_construct*             m_pVoidConstruct;
            destruct*                   m_pDestruct;
            move_construct*             m_pMoveConstruct;
            copy_construct*             m_pCopyConstruct;
        };

        template<typename T>
        inline constexpr auto atomic_v = []() consteval -> atomic
        {
            static_assert( xproperty::details::has_const_v<T>   == false );
            static_assert( settings::var_type<T>::is_list_v    == false );
            static_assert( settings::var_type<T>::is_pointer_v == false );
            return
            { .m_pName          = var_t<T>::name_v
            , .m_GUID           = var_t<T>::guid_v
            , .m_Size           = sizeof(T)
            , .m_IsEnum         = std::is_enum_v<T>
            , .m_RegisteredEnumSpan = []() consteval -> std::span<const atomic::enum_item>
                                    {
                                        if constexpr ( std::is_enum_v<T> ) return var_t<T>::enum_list_v;
                                        else                               return {};
                                    }()
            , .m_pVoidConstruct = var_t<T>::VoidConstruct
            , .m_pDestruct      = var_t<T>::Destruct
            , .m_pMoveConstruct = +[](settings::data_memory& D,       settings::data_memory& S) constexpr { settings::var_type<T>::MoveConstruct( D, reinterpret_cast<T&&>(S) );      }
            , .m_pCopyConstruct = +[](settings::data_memory& D, const settings::data_memory& S) constexpr { settings::var_type<T>::CopyConstruct( D, reinterpret_cast<const T&>(S));  }
            };
        }();

        namespace details
        {
            template< typename T >
            struct iterator_data
            {
                using table = typename xproperty::details::delay_linkage<list_table, T >::type;

                iterator_data() = delete;
                iterator_data( void* pData, const table& Table, settings::context& C ) noexcept
                : m_Data    { reinterpret_cast<settings::iterator_memory&>(pData) }
                , m_Table   { Table }
                , m_pObject { pData }
                , m_Context { C }
                {}

                constexpr std::size_t       getSize( void ) const noexcept { return m_Table.m_pGetSize(m_pObject, m_Context); }

                settings::iterator_memory   m_Data;
                const table&                m_Table;
                void* const                 m_pObject;
                settings::context&          m_Context;
            };

            template< typename T >
            struct end_iterator : iterator_data<T>
            {
                using parent = iterator_data<T>;
                using table  = typename parent::table;

                end_iterator() = delete;
                end_iterator(void* pData, const table& Table, settings::context& C) noexcept
                    : parent(pData, Table, C)
                {
                    parent::m_Table.m_pEnd(parent::m_pObject, *this, parent::m_Context);
                }

                ~end_iterator()
                {
                    if(parent::m_pObject) parent::m_Table.m_pDestroyEndIterator(*this, parent::m_Context);
                }
            };

            template< typename T >
            struct begin_iterator : iterator_data<T>
            {
                using parent = iterator_data<T>;
                using table  = typename parent::table;

                begin_iterator() = delete;
                begin_iterator( void* pData, const table& Table, settings::context& C ) noexcept
                    : parent(pData, Table, C )
                {
                    parent::m_Table.m_pStart(parent::m_pObject, *this, parent::m_Context);
                }

                constexpr void* getObject( void ) noexcept
                {
                    return parent::m_Table.m_pIteratorToObject(parent::m_pObject, *this, parent::m_Context);
                }

                constexpr bool getKey( any& Key ) const noexcept
                {
                    return parent::m_Table.m_pIteratorToKey(parent::m_pObject, *this, Key, parent::m_Context);
                }

                constexpr bool Next( const end_iterator<T>& End ) noexcept
                {
                    m_Index++;
                    return parent::m_Table.m_pNext(parent::m_pObject, *this, End, parent::m_Context);
                }

                constexpr int getIndex() const noexcept
                {
                    return m_Index;
                }

                ~begin_iterator()
                {
                    if (parent::m_pObject) parent::m_Table.m_pDestroyBeginIterator(*this, parent::m_Context);
                }

                int m_Index = 0;
            };
        }

        using begin_iterator = details::begin_iterator<int>;
        using end_iterator   = details::end_iterator<int>;

        struct any
        {
            constexpr std::uint32_t getTypeGuid() const noexcept
            {
                return m_pType ? m_pType->m_GUID : 0;
            }

            template<typename T>
            constexpr T& Reset() noexcept
            {
                static_assert(xproperty::details::has_const_v<T> == false);
                static_assert( std::is_enum_v<T> || settings::var_type<T>::guid_v != 0 );
                if(m_pType) 
                {
                    if( m_pType == &atomic_v<T> ) 
                    {
                        if constexpr (settings::var_type<T>::guid_v == 0)
                            m_pType->m_pDestruct(m_Data);
                        else
                            settings::var_type<T>::Destruct(m_Data);
                    }
                    else
                    {
                        m_pType->m_pDestruct(m_Data);
                        m_pType = &atomic_v<T>;
                    }
                }
                else
                {
                    m_pType = &atomic_v<T>;
                }

                if constexpr (settings::var_type<T>::guid_v == 0)
                    m_pType->m_pVoidConstruct(m_Data);
                else
                    settings::var_type<T>::VoidConstruct(m_Data);
                return reinterpret_cast<T&>(m_Data);
            }

            template<typename T>
            constexpr T& set( T&& Data ) noexcept
            {
                static_assert(xproperty::details::has_const_v<T> == false);
                static_assert(std::is_enum_v<T> || settings::var_type<T>::guid_v != 0);
                if (m_pType)
                {
                    if (m_pType == &atomic_v<T>)
                    {
                        if constexpr (settings::var_type<T>::guid_v == 0)
                            m_pType->m_pDestruct(m_Data);
                        else
                            settings::var_type<T>::Destruct(m_Data);
                    }
                    else
                    {
                        m_pType->m_pDestruct(m_Data);
                        m_pType = &atomic_v<T>;
                    }
                }
                else
                {
                    m_pType = &atomic_v<T>;
                }

                if constexpr (settings::var_type<T>::guid_v == 0)
                    m_pType->m_pMoveConstruct(m_Data, std::move(Data) );
                else
                    settings::var_type<T>::MoveConstruct(m_Data, std::move(Data) );
                return reinterpret_cast<T&>(m_Data);
            }

            template<typename T>
            constexpr T& set(T& Data) noexcept
            {
                static_assert(xproperty::details::has_const_v<T> == false);
                static_assert(std::is_enum_v<T> || settings::var_type<T>::guid_v != 0);
                if (m_pType)
                {
                    if (m_pType == &atomic_v<T>)
                    {
                        if constexpr (settings::var_type<T>::guid_v == 0)
                            m_pType->m_pDestruct(m_Data);
                        else
                            settings::var_type<T>::Destruct(m_Data);
                    }
                    else
                    {
                        m_pType->m_pDestruct(m_Data);
                        m_pType = &atomic_v<T>;
                    }
                }
                else
                {
                    m_pType = &atomic_v<T>;
                }

                if constexpr (settings::var_type<T>::guid_v == 0)
                    m_pType->m_pCopyConstruct(m_Data, Data);
                else
                    settings::var_type<T>::CopyConstruct(m_Data, Data);
                return reinterpret_cast<T&>(m_Data);
            }

            template<typename T>
            constexpr T& set(const T& Data) noexcept
            {
                static_assert(xproperty::details::has_const_v<T> == false);
                static_assert(std::is_enum_v<T> || settings::var_type<T>::guid_v != 0);
                return set(const_cast<T&>(Data));
            }

            template<typename T>
            constexpr T& get( void ) noexcept
            {
                static_assert(xproperty::details::has_const_v<T> == false);
                static_assert(std::is_enum_v<T> || settings::var_type<T>::guid_v != 0);
                assert(m_pType);
                assert(m_pType == &atomic_v<T>);
                return reinterpret_cast<T&>(m_Data);
            }

            constexpr std::uint32_t getEnumValue(void) const noexcept
            {
                assert(isEnum());
                switch (m_pType->m_Size)
                {
                case 1: return static_cast<std::uint32_t>(reinterpret_cast<const std::uint8_t&>(m_Data));
                case 2: return static_cast<std::uint32_t>(reinterpret_cast<const std::uint16_t&>(m_Data));
                case 4: return static_cast<std::uint32_t>(reinterpret_cast<const std::uint32_t&>(m_Data));
                case 8: return static_cast<std::uint32_t>(reinterpret_cast<const std::uint64_t&>(m_Data));
                default: assert(false); return 0;
                }
            }
            
            constexpr const std::span<const atomic::enum_item>& getEnumSpan( void ) const noexcept
            {
                assert(isEnum());
                return m_pType->m_RegisteredEnumSpan;
            }

            constexpr const char* getEnumString( void ) const noexcept
            {
                assert(isEnum());
                const auto EnumValue = getEnumValue();
                for (const auto& E : getEnumSpan() )
                {
                    if (E.m_Value == EnumValue) return E.m_pName;
                }
                return nullptr;
            }

            template<typename T>
            constexpr const T& get(void) const noexcept
            {
                static_assert(xproperty::details::has_const_v<T> == false);
                static_assert(std::is_enum_v<T> || settings::var_type<T>::guid_v != 0);
                assert(m_pType);
                assert(m_pType == &atomic_v<T>);
                return reinterpret_cast<const T&>(m_Data);
            }

            constexpr bool isEnum(void) const noexcept
            {
                return m_pType && m_pType->m_IsEnum;
            }

            ~any()
            {
                if (m_pType) 
                {
                    m_pType->m_pDestruct(m_Data);
                    m_pType = nullptr;
                }
            }

            any() = default;
            any( any&& Any )
            {
                m_pType = Any.m_pType;
                if (Any.m_pType)
                {
                    m_pType->m_pMoveConstruct(m_Data, Any.m_Data);
                    Any.m_pType = nullptr;
                }
            }

            any( const any& Any)
            {
                m_pType = Any.m_pType;
                if (Any.m_pType) m_pType->m_pCopyConstruct(m_Data, Any.m_Data);
            }

            any& operator = (const any& Any)
            {
                if(m_pType) m_pType->m_pDestruct(m_Data);
                m_pType = Any.m_pType;
                if (Any.m_pType) m_pType->m_pCopyConstruct(m_Data, Any.m_Data);
                return *this;
            }

            any& operator = (any&& Any)
            {
                if (m_pType) m_pType->m_pDestruct(m_Data);
                m_pType = Any.m_pType;
                if (Any.m_pType)
                {
                    m_pType->m_pMoveConstruct(m_Data, Any.m_Data);
                    Any.m_pType = nullptr;
                }
                return *this;
            }

            settings::data_memory       m_Data;
            const atomic*               m_pType = nullptr;
        };

        struct member_constructor
        {
            using fn = void*(void* pArgs);

            fn*                                     m_pCallConstructor;
            std::span< const xproperty::basic_type > m_ArgumentList;
        };

        using reference_fn = std::tuple<void*, const object*>(void* pClassInstance);

        struct list_table
        {
            using get_size_fn               = std::size_t (void* pClass, settings::context&);
            using set_size_fn               = void ( void*           pClass,       std::size_t,                                         settings::context&);
            using start_fn                  = void ( void*           pClass,       begin_iterator&  Iterator,                           settings::context&);
            using end_fn                    = void ( void*           pClass,       end_iterator&    End,                                settings::context&);
            using next_fn                   = bool ( void*           pClass,       begin_iterator&  Iterator, const end_iterator& End,  settings::context&);
            using iterator_to_key_fn        = bool ( void*           pClass, const begin_iterator&  Iterator, any&                Key,  settings::context&);
            using iterator_to_object_fn     = void*( void*           pClass,       begin_iterator&  Iterator,                           settings::context&);
            using get_object_fn             = void*( void*           pClass, const any&             Key,                                settings::context&);
            using destroy_begin_iterator_fn = void ( begin_iterator& Begin,                                                             settings::context&);
            using destroy_end_iterator_fn   = void ( end_iterator&   End,                                                               settings::context&);

            get_size_fn*                const m_pGetSize;
            set_size_fn*                const m_pSetSize;
            start_fn*                   const m_pStart;
            end_fn*                     const m_pEnd;
            next_fn*                    const m_pNext;
            iterator_to_key_fn*         const m_pIteratorToKey;
            iterator_to_object_fn*      const m_pIteratorToObject;
            get_object_fn*              const m_pGetObject;
            destroy_begin_iterator_fn*  const m_pDestroyBeginIterator;
            destroy_end_iterator_fn*    const m_pDestroyEndIterator;
            const atomic&                     m_KeyAtomicType;
        };

        struct members
        {
            struct props
            {
                using cast_fn = std::tuple<void*, const object*>(void* pClassInstance, settings::context&);
                cast_fn* const                          m_pCast;
            };

            struct scope
            {
                const std::span<const members>          m_Members;

                const members* findMember( std::uint32_t GUID ) const noexcept
                {
                    if(GUID)
                    {
                        for (auto& E : m_Members)
                        {
                            if (E.m_GUID == GUID) return &E;
                        }
                    }

                    return nullptr;
                }
            };

            struct var
            {
                using read_fn           = void( const void* pClass,       any& DataOut, const std::span<const atomic::enum_item>& S, settings::context& );
                using write_fn          = void(       void* pClass, const any& DataIn,  const std::span<const atomic::enum_item>& S, settings::context& );

                read_fn*  const                          m_pRead;
                write_fn* const                          m_pWrite;
                const atomic&                            m_AtomicType;
                const std::span<const atomic::enum_item> m_UnregisteredEnumSpan;
            };

            struct list_var
            {
                using read_fn               = void( const void* pClass,       any& DataOut, const std::span<const atomic::enum_item>& S, settings::context&);
                using write_fn              = void(       void* pClass, const any& DataIn,  const std::span<const atomic::enum_item>& S, settings::context&);

                read_fn*  const                          m_pRead;
                write_fn* const                          m_pWrite;
                const std::span<const list_table>        m_Table;
                const atomic&                            m_AtomicType;
                const std::span<const atomic::enum_item> m_UnregisteredEnumSpan;
            };

            struct list_props : props
            {
                const std::span<const list_table>        m_Table;
            };

            struct function
            {
                using fn = void(void* pClass, void* pArgs) noexcept;

                fn* const                                      m_pCallFunction;
                const std::span< const xproperty::basic_type>  m_ArgumentList;

                template<typename T, typename...TARGS >
                constexpr void CallFunction(T& Class, TARGS&&...Args) const noexcept
                {
                    using col = std::tuple<TARGS...>;
                    (([&]
                        {
                            auto v1 = type_meta< std::decay_t<TARGS> >::value;
                            auto v2 = m_ArgumentList[xproperty::details::tuple_t2i_v<TARGS, col>].m_Value;
                            assert(v1 == v2);
                        }()), ...);

                    col Arguments{ std::forward<TARGS>(Args)... };
                    m_pCallFunction(&Class, &Arguments);
                }
            };

            using mix_variant = std::variant
            < var
            , props
            , list_var
            , list_props
            , scope
            , function
            >;

            using fn_get_user_data = const void* (std::uint32_t GUID);

            template<typename T_USER_DATA_TYPE>
            constexpr const T_USER_DATA_TYPE* getUserData() const noexcept
            {
                if(m_pGetUserData) return static_cast<const T_USER_DATA_TYPE*>(m_pGetUserData(T_USER_DATA_TYPE::type_guid_v));
                return nullptr;
            }

            const std::uint32_t     m_GUID; 
            const char* const       m_pName;
            const mix_variant       m_Variant;
            const bool              m_bConst;   // Should this be here stead in each type?
            fn_get_user_data* const m_pGetUserData;
        };

        struct base : members::props
        {
            const bool          m_bConst;
        };

        struct object : members::scope
        {
            using fn_destroy_instance   = void( void* );

            const char* const                           m_pName;
            std::uint32_t                               m_GUID;
            fn_destroy_instance* const                  m_pDestroyInstance;
            const std::span<const member_constructor>   m_Constructors;
            const std::span<const base>                 m_BaseList;

            template< typename...T_ARGS>
            void* CreateInstance(T_ARGS&&... Args) const
            {
                using col = std::tuple<T_ARGS...>;

                for (auto& E : m_Constructors)
                {
                    bool bCompatible = sizeof...(T_ARGS) == E.m_ArgumentList.size();
                    if constexpr (sizeof...(T_ARGS) > 0) bCompatible = bCompatible &&
                        (([&]
                            {
                                auto v1 = type_meta< std::decay_t<T_ARGS> >::value;
                                auto v2 = E.m_ArgumentList[xproperty::details::tuple_t2i_v<T_ARGS, col>].m_Value;
                                return (v1 == v2);
                            }()) || ...);

                    if (bCompatible)
                    {
                        col Arguments{ std::forward<T_ARGS>(Args)... };
                        return E.m_pCallConstructor(&Arguments);
                    }
                }

                return nullptr;
            }

            void DestroyInstance( void* pInstance ) const noexcept
            {
                assert(m_pDestroyInstance);
                m_pDestroyInstance(pInstance);
            }
        };

        template< typename T >
        inline constinit const object* get_obj_info = nullptr;
    }

    template< typename T >
    constexpr const type::object* getObjectByType( void ) noexcept
    {
        using t = std::decay_t<T>;
        assert( type::get_obj_info<t> != nullptr );
        return type::get_obj_info<t>;
    }

    template< typename T >
    constexpr const type::object* getObject( const T& A ) noexcept
    {
        using t = std::decay_t<T>;
        if constexpr (std::is_base_of_v< xproperty::base, t >) return A.getProperties();
        else
        {
            assert(type::get_obj_info<t> != nullptr);
            (void)A; return type::get_obj_info<t>;
        }
    }

    //
    // META (HELPERS TO HELP FILL THE TYPES)
    //
    namespace meta
    {
        //
        // TAGS
        //
        struct obj_member_tag;
        struct obj_base_tag;
        struct obj_constructor_tag;
        struct member_overwrites_tag;
        struct member_help_tag;
        struct read_only_tag;
        struct enum_value_tag;
        struct enum_span_tag;
        struct user_data_tag;
        struct member_overwrite_list_size_tag;

        //
        // MEMBERS
        //
        template< details::fixed_string T_NAME_V, typename T_DATA, auto T_DATA_V, typename... T_ARGS >
        struct member;

        namespace details
        {
            template< typename T_CLASS, typename T, auto T_LAMBDA_V, typename... T_ARGS >
            struct var_io
            {
                using t        = xproperty::details::remove_all_const_t<T>;
                using member_t = typename type::var_t<t>::type;
                using atomic_t = typename type::var_t<t>::atomic_type;

                static_assert(xproperty::details::has_const_v<atomic_t> == false );

                using tuple_args = std::tuple< T_ARGS...>;

                //
                // Deal with register atomic types
                //
                static void Read ( const void* pClass, type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context )
                {
                    if constexpr (std::is_enum_v<atomic_t> && type::var_t<atomic_t>::guid_v == 1)
                    {
                        if (type::atomic_v<atomic_t>.m_RegisteredEnumSpan.size() == 0)
                            type::atomic_v<atomic_t>.m_RegisteredEnumSpan = S;
                        else
                            assert(S.size() == type::atomic_v<atomic_t>.m_RegisteredEnumSpan.size());
                    }

                    type::var_t<t>::Read
                    ( const_cast<xproperty::details::remove_all_const_t<t&>>(T_LAMBDA_V(*static_cast<T_CLASS*>(const_cast<void*>(pClass))))
                    , Any.Reset<atomic_t>()
                    , Context
                    );
                }

                static void Write(void* pClass, const type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context)
                {
                    if constexpr (std::is_enum_v<atomic_t> && type::var_t<atomic_t>::guid_v == 1)
                    {
                        if (type::atomic_v<atomic_t>.m_RegisteredEnumSpan.size() == 0)
                            type::atomic_v<atomic_t>.m_RegisteredEnumSpan = S;
                        else
                            assert(S.size() == type::atomic_v<atomic_t>.m_RegisteredEnumSpan.size());
                    }

                    type::var_t<t>::Write
                    ( const_cast<xproperty::details::remove_all_const_t<t&>>(T_LAMBDA_V(*static_cast<T_CLASS*>(pClass)))
                    , Any.get<atomic_t>()
                    , Context
                    );
                }
            };

            template< bool T_VALID_V, typename T_LAST, typename T_TYPE, typename...T_ARGS >
            struct list_dimensions;

            template< typename T_LAST, typename T_TYPE, typename...T_ARGS >
            struct list_dimensions<true, T_LAST, T_TYPE, T_ARGS...>
            {
                using specializing_t = typename settings::var_type<T_TYPE>::specializing_t;
                using parent_info    = list_dimensions< settings::var_type<specializing_t>::is_list_v, T_TYPE, specializing_t, T_ARGS..., T_TYPE >;
                using type           = typename parent_info::type;
                using last           = typename parent_info::last;
            };

            template< typename T_LAST, typename T_TYPE, typename...T_ARGS >
            struct list_dimensions<false, T_LAST, T_TYPE, T_ARGS...>
            {
                using type = std::tuple<T_ARGS...>;
                using last = T_LAST;
            };

            namespace details
            {
                template< auto T_LAMBDA_V, typename T_CLASS >
                struct get_member
                {
                    using fn_t = xproperty::details::function_traits<decltype(T_LAMBDA_V)>;
                    static constexpr auto* get(void* pClass, settings::context& C) noexcept
                    {
                        if constexpr (std::is_pointer_v<typename fn_t::return_type>)
                        {
                                 if constexpr (std::tuple_size_v<typename fn_t::args> == 1) return T_LAMBDA_V(*static_cast<T_CLASS*>(pClass));
                            else if constexpr (std::tuple_size_v<typename fn_t::args> == 2) return T_LAMBDA_V(*static_cast<T_CLASS*>(pClass), C);
                            else static_assert(always_false<fn_t>::value, "The Size function for the given list type must have 1 or 2 paramaters only");
                        }
                        else
                        {
                                 if constexpr (std::tuple_size_v<typename fn_t::args> == 1) return &T_LAMBDA_V(*static_cast<T_CLASS*>(pClass));
                            else if constexpr (std::tuple_size_v<typename fn_t::args> == 2) return &T_LAMBDA_V(*static_cast<T_CLASS*>(pClass), C);
                            else static_assert(always_false<fn_t>::value, "The Size function for the given list type must have 1 or 2 paramaters only");
                        }
                    }
                };
            }

            template< typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V >
            struct cast_scope
            {
                // This is put here because there is a bug in visual studio... 
                inline constexpr static auto callback_v = T_LAMBDA_V;

                constexpr static std::tuple<void*, const xproperty::type::object*> Cast( void* pClass, settings::context& C )
                {
                    // make sure we have not refs...
                    using t  = xproperty::details::remove_all_const_t< std::remove_pointer_t<T_MEMBER_TYPE>>;

                    auto pInst = [&]()->typename type::var_t<T_MEMBER_TYPE>::atomic_type*
                    {
                        if constexpr (std::is_pointer_v<T_MEMBER_TYPE>)
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass, C);
                            if (pA && *pA) return type::var_t<t>::getAtomic(const_cast<t&>(**pA), C);
                        }
                        else
                        {
                            T_MEMBER_TYPE* pA = details::get_member<callback_v, T_CLASS>::get(pClass, C);
                            if (pA) return type::var_t<t>::getAtomic(const_cast<t&>(*pA), C);
                        }
                        return nullptr;
                    }();

                    if constexpr (std::is_base_of_v< xproperty::base, t >)
                    {
                        if constexpr (type::var_t<t>::is_pointer_v)
                        {
                            if (pInst == nullptr) return { nullptr, nullptr };
                        }
                        return { pInst, pInst->type::template var_t<t>::atomic_type::getProperties() };
                    }
                    else
                    {
                        assert(type::get_obj_info<typename type::var_t<t>::atomic_type> != nullptr);
                        return { pInst, type::get_obj_info<typename type::var_t<t>::atomic_type> };
                    }
                }
            };

            template< typename T_MEMBER_TYPE >
            struct cast_scope_list
            {
                constexpr static std::tuple<void*, const type::object*> Cast( void* pClass, xproperty::settings::context& C )
                {
                    assert(pClass);

                    // make sure we have not refs...
                    using atomic_t = typename type::var_t<T_MEMBER_TYPE>::atomic_type;
                    atomic_t*   pA = nullptr;

                    if constexpr (type::var_t<T_MEMBER_TYPE>::is_pointer_v)
                    {
                        using ft = xproperty::details::function_traits<std::remove_const_t<decltype(type::var_t<T_MEMBER_TYPE>::getAtomic)>>;
                             if constexpr (std::tuple_size_v<typename ft::args> == 1) pA = type::var_t<T_MEMBER_TYPE>::getAtomic( *reinterpret_cast<T_MEMBER_TYPE*>(pClass)   );
                        else if constexpr (std::tuple_size_v<typename ft::args> == 2) pA = type::var_t<T_MEMBER_TYPE>::getAtomic( *reinterpret_cast<T_MEMBER_TYPE*>(pClass), C);
                        else static_assert(always_false<ft>::value, "The getAtomic function for the given list type must have at least 1 parameters => atomic_type* getAtomic( type& MemberVar )");
                    }
                    else
                    {
                        pA = reinterpret_cast<atomic_t*>(pClass);
                    }

                    if constexpr (std::is_base_of_v< xproperty::base, atomic_t >)
                    {
                        return { pA, pA->atomic_t::getProperties() };
                    }
                    else
                    {
                        assert(type::get_obj_info<atomic_t> != nullptr);
                        return { pA, type::get_obj_info<atomic_t> };
                    }
                }
            };

            template< typename T_CLASS, typename T, auto T_LAMBDA_V, typename... T_ARGS >
            struct read_list
            {
                using member_t = typename type::var_t< T >::type;
                using atomic_t = typename type::var_t< T >::atomic_type;

                using tuple_args = std::tuple< T_ARGS...>;

                static void Read ( void* pClass, void* pV, type::any& Key, settings::context& Context )
                {
                    using setting_read_fn_t = xproperty::details::function_traits<std::remove_const_t<decltype(type::var_t<T>::Read)>>;

                    // We will support two versions one with context one with out one... 
                    if constexpr (std::tuple_size_v<typename setting_read_fn_t::args> == 4)
                    {
      //                  static_assert(std::is_same_v< xproperty::details::tuple_i2t<1, typename setting_read_fn_t::args>, T&       >, "The 3 argument of a xproperty reader should be a reference to the context => void Read( const type&, type&, context& )");
                        type::var_t<T>::Read( T_LAMBDA_V(*reinterpret_cast<T_CLASS*>(pClass)), *reinterpret_cast<atomic_t*>(pV), Key, Context);
                    }
                    else
                    {
                        static_assert(std::tuple_size_v<typename setting_read_fn_t::args> == 3, "The read function for the given type must have at least 3 parameters => void Read( const type&, type& )");
                        type::var_t<T>::Read(T_LAMBDA_V(*reinterpret_cast<T_CLASS*>(pClass)), *reinterpret_cast<T*>(pV), Key);
                    }
                }
            };

            namespace details
            {
                template< typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V >
                consteval static type::list_table getListTable()
                {
                    using t                 = type::var_t<T_MEMBER_TYPE>;
                    using type              = typename t::type;
//                    using specializing_t    = typename t::specializing_t;
//                    using atomic_type       = typename t::atomic_type;
                    using begin_iterator_t  = typename t::begin_iterator;
                    using end_iterator_t    = typename t::end_iterator;

                    return
                    { .m_pGetSize = [](void* pClass, settings::context& C) constexpr -> std::size_t
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if (pA == nullptr) return 0;

                            return t::getSize(*pA, C);
                        }
                    , .m_pSetSize = [](void* pClass, std::size_t Size, settings::context& C) constexpr
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if (pA == nullptr) return;

                            t::setSize(const_cast<type&>(*pA), Size, C);
                        }
                    , .m_pStart = []( void* pClass, xproperty::type::begin_iterator& Iterator, settings::context& C ) constexpr
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if(pA ==nullptr) return;

                            t::Start(const_cast<type&>(*pA), reinterpret_cast<begin_iterator_t&>(Iterator.m_Data), C);
                        }
                    , .m_pEnd = [](void* pClass, xproperty::type::end_iterator& Iterator, settings::context& C) constexpr
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if (pA == nullptr) return;

                            t::End(const_cast<type&>(*pA), reinterpret_cast<end_iterator_t&>(Iterator.m_Data), C);
                        }
                    , .m_pNext = [](void* pClass, xproperty::type::begin_iterator& Iterator, const xproperty::type::end_iterator& End, settings::context& C) constexpr ->bool
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if (pA == nullptr) return false;

                            return t::Next(*pA, reinterpret_cast<begin_iterator_t&>(Iterator.m_Data), reinterpret_cast<const end_iterator_t&>(End.m_Data), C);
                        }
                    , .m_pIteratorToKey = [](void* pClass, const xproperty::type::begin_iterator& Iterator, xproperty::type::any& Key, settings::context& C) constexpr -> bool
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if (pA == nullptr) return false;

                            t::IteratorToKey(*pA, Key, reinterpret_cast<const begin_iterator_t&>(Iterator.m_Data), C);

                            return true;
                        }
                    , .m_pIteratorToObject = [](void* pClass, xproperty::type::begin_iterator& I, settings::context& C) constexpr -> void*
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass, C);
                            if (pA == nullptr) return nullptr;

                            return t::IteratorToObject(const_cast<type&>(*pA), reinterpret_cast<begin_iterator_t&>(I.m_Data), C );
                        }
                    , .m_pGetObject = [](void* pClass, const xproperty::type::any& Key, settings::context& C ) constexpr -> void*
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if(pA==nullptr) return nullptr;

                            return t::getObject(const_cast<type&>(*pA), Key, C );
                        }
                    , .m_pDestroyBeginIterator = [](xproperty::type::begin_iterator& Iterator, settings::context& C) constexpr
                        {
                            t::DestroyBeginIterator(reinterpret_cast<begin_iterator_t&>(Iterator.m_Data), C);
                        }
                    , .m_pDestroyEndIterator = [](xproperty::type::end_iterator& Iterator, settings::context& C) constexpr
                        {
                            t::DestroyEndIterator(reinterpret_cast<end_iterator_t&>(Iterator.m_Data), C);
                        }
                    , .m_KeyAtomicType = xproperty::type::atomic_v<typename t::atomic_key>
                    };
                }

                template< typename T>
                consteval auto getListTable2()
                {
                    return getListTable< T, T, +[](T& Class) constexpr noexcept ->T& { return Class; }>();
                }
            }

            template< typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V >
            struct list_table
            {
                using dimensions = list_dimensions
                < type::var_t<typename type::var_t<T_MEMBER_TYPE>::specializing_t>::is_list_v
                , T_MEMBER_TYPE
                , typename type::var_t<T_MEMBER_TYPE>::specializing_t
                >;

                using tuple_dimensions = typename dimensions::type;
                using last_dimension   = typename dimensions::last;

                consteval static auto GetArray()
                {
                    if constexpr( std::is_same_v< tuple_dimensions, std::tuple<> > ) return std::array{ details::getListTable<T_CLASS,T_MEMBER_TYPE,T_LAMBDA_V>() };
                    else                                                             return []<typename...T>(std::tuple<T...>*) consteval
                    {
                        return std::array
                        { details::getListTable< T_CLASS, T_MEMBER_TYPE, T_LAMBDA_V >()
                        , details::getListTable2< T >()
                          ...
                        };
                    }((tuple_dimensions*)0);
                }
            };

            //
            // unregistered enum array
            //
            template< bool T_UNREGISTERED_ENUM, typename... T_ARGS >
            struct unregistered_enum_array;

            template< typename... T_ARGS >
            struct unregistered_enum_array<false, T_ARGS...>
            {
                constexpr static inline auto value = std::span<type::atomic::enum_item>{};
            };

            template< typename... T_ARGS >
            struct unregistered_enum_array<true, T_ARGS...>
            {
                static_assert(sizeof...(T_ARGS) != 0, "You must have a list of items for your unregistered enumeration, (either member_enum_span, or member_enum_value)");

                constexpr static inline auto value =  []() consteval
                {
                    using enum_span_tuple  = xproperty::details::filter_by_tag_t< enum_span_tag, T_ARGS... >;
                    using enum_value_tuple = xproperty::details::filter_by_tag_t< enum_value_tag, T_ARGS... >;

                    if constexpr( std::is_same_v< enum_span_tuple, std::tuple<> > )
                    {
                        static_assert(std::tuple_size_v<enum_value_tuple> != 0, "You must have a list of items for your unregistered enumeration, (either member_enum_span, or member_enum_value)");

                        return []<typename... TS>( std::tuple<TS...>* ) consteval
                        {
                            return std::array
                                { type::atomic::enum_item { TS::name_v.m_Value, TS::value_v }
                                  ...
                                };
                        }((enum_value_tuple*)0);
                    }
                    else
                    {
                        static_assert(std::tuple_size_v<enum_span_tuple> == 1);
                        static_assert(std::is_same_v< std::tuple<>, xproperty::details::filter_by_tag_t< enum_value_tag, T_ARGS... >>, "You can only use one method to enumerate the enum...");
                        return std::tuple_element_t<0, enum_span_tuple>::span_v;
                    }
                }();
            };

            template< typename T_USER_DATA >
            const void* GetUserData(std::uint32_t GUID)
            {
                if constexpr (std::tuple_size_v<T_USER_DATA>)
                {
                    return[]<typename...T>(std::uint32_t GUID, std::tuple<T...>*) constexpr -> const void*
                    {
                        const void* pRet = nullptr;
                        ((pRet = (GUID == T::type_guid_v) ? []
                            {
                                static constexpr T Type{};
                                return &Type;
                            }() : nullptr) || ...);
                        return pRet;
                    }(GUID, static_cast<T_USER_DATA*>(nullptr));
                }
                return nullptr;
            }

            template< typename T >
            inline constexpr bool is_unregistered_enum_v = []() consteval
            {
                if constexpr( type::var_t<T>::guid_v == 1 )
                {
                    static_assert( std::is_enum_v<T> == true );
                    return true;
                }
                else return false;
            }();

            namespace details
            {
                template< typename T>
                struct is_const : std::is_const<T> {};

                template< typename T>
                struct is_const<const T*> : std::true_type {};

                template< typename T>
                struct is_const<const T**> : std::true_type {};

                template< typename T>
                struct is_const<const T***> : std::true_type {};
            }

            template< typename T_CLASS, typename T_MEMBER, typename...T_ARGS>
            inline constexpr bool is_read_only_v = std::is_const_v<T_CLASS>
                                                || details::is_const<T_MEMBER>::value
                                                || xproperty::details::tuple_has_tag_v< read_only_tag, std::tuple<T_ARGS...>>
                                                || type::var_t<T_MEMBER>::is_const_v;
        }

        //
        // Helper to enable and disable our different types
        //
        template< bool T_IS_VAR_V, bool T_IS_LIST_V, typename T_MEMBER_TYPE >
        inline constexpr bool meets_requirements_v = []() consteval
        {
            using tc = type::var_t<T_MEMBER_TYPE>;
            using at = type::var_t<typename tc::atomic_type>;


            if constexpr (T_IS_VAR_V)
            {
                return (std::is_enum_v<typename at::type> == true || at::guid_v != 0) && tc::is_list_v == T_IS_LIST_V;
            }
            else
            {
                return (std::is_enum_v<typename at::type> == false && at::guid_v == 0) && tc::is_list_v == T_IS_LIST_V;
            }
        }();

        //
        // HANDLE MEMBER VARIABLES with Registered ( REFS )
        // These type need a special structure since C++ does not support getting the address of a reference member variable
        // So solve this issue we use a macro xproperty::member_ref which will generate this structure for us
        // using this structure we can solve everything else
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename... T_ARGS >
        requires meets_requirements_v< true, false, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE&(*)(T_CLASS&), T_LAMBDA_V, T_ARGS... > 
        {
            using user_data_t = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;

            static consteval xproperty::type::members getInfo()
            {
                using            memc_t          = xproperty::details::remove_all_const_t<T_MEMBER_TYPE>;
                using            atomic_t        = typename type::var_t<T_MEMBER_TYPE>::atomic_type;
                constexpr bool   is_ready_only_v = details::is_read_only_v<T_CLASS, T_MEMBER_TYPE, T_ARGS...>;
                return
                { .m_GUID           = xproperty::settings::strguid(T_NAME_V)
                , .m_pName          = T_NAME_V
                , .m_Variant        = xproperty::type::members::var
                    { .m_pRead                = details::var_io< T_CLASS,memc_t,T_LAMBDA_V, T_ARGS... >::Read
                    , .m_pWrite               = is_ready_only_v ? nullptr : details::var_io< T_CLASS,memc_t,T_LAMBDA_V, T_ARGS... >::Write
                    , .m_AtomicType           = xproperty::type::atomic_v<atomic_t>
                    , .m_UnregisteredEnumSpan = details::unregistered_enum_array<details::is_unregistered_enum_v<atomic_t>, T_ARGS...>::value
                    }
                , .m_bConst = is_ready_only_v
                , .m_pGetUserData = details::GetUserData<user_data_t>
                };
            }
        };

        //
        // HANDLE MEMBER VARIABLES that are Registered ( VARS, pVARS )
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_MEMBER_PTR_V, typename... T_ARGS >
        requires meets_requirements_v< true, false, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE T_CLASS::*, T_MEMBER_PTR_V, T_ARGS... >
        {
            static consteval xproperty::type::members getInfo() noexcept
            {
                return member < T_NAME_V, T_MEMBER_TYPE&(*)(T_CLASS&), +[](T_CLASS& C)->T_MEMBER_TYPE& { return C.*T_MEMBER_PTR_V; }, T_ARGS... >::getInfo();
            }
        };

        //
        // VIRTUAL VARS
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename...T_ADDITIONAL, typename... T_ARGS >
        requires meets_requirements_v< true, false, T_MEMBER_TYPE>
        struct member<T_NAME_V, void(*)(T_CLASS&, bool, T_MEMBER_TYPE&, T_ADDITIONAL...), T_LAMBDA_V, T_ARGS... >
        {
            static_assert( sizeof...(T_ADDITIONAL) <= 1, "The only additional parameter you can have is the (xproperty::setting::context&) you have too many parameters" );
            using user_data_t = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;

            static consteval xproperty::type::members getInfo()
            {
                constexpr bool   is_ready_only_v = std::is_const_v<T_CLASS>
                                || xproperty::details::tuple_has_tag_v<read_only_tag, std::tuple<T_ARGS...>>;

                static_assert(!(is_ready_only_v && std::is_const_v<T_MEMBER_TYPE>), "You made the virtual xproperty read-only and write-only... we don't allow this");

                using t        = xproperty::details::remove_all_const_t<T_MEMBER_TYPE>;
                using atomic_t = typename type::var_t<t>::atomic_type;

                return
                { .m_GUID           = xproperty::settings::strguid(T_NAME_V)
                , .m_pName          = T_NAME_V
                , .m_Variant        = xproperty::type::members::var
                    { .m_pRead      = (std::is_const_v<T_MEMBER_TYPE>) ? nullptr : +[]( const void* pClass, type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context) constexpr
                    {
                        if constexpr (std::is_const_v<T_MEMBER_TYPE> == false)
                        {
                            if constexpr (std::is_enum_v<t> && type::var_t<t>::guid_v == 1)
                            {
                                type::atomic_v<t>.m_RegisteredEnumSpan = S;
                            }

                            if constexpr ( sizeof...(T_ADDITIONAL) == 0 )
                                T_LAMBDA_V
                                ( *static_cast<T_CLASS*>( const_cast<void*>(pClass))
                                , true
                                , Any.Reset<t>()
                                );
                            else
                                T_LAMBDA_V
                                ( *static_cast<T_CLASS*>(const_cast<void*>(pClass))
                                , true
                                , Any.Reset<t>()
                                , Context
                                );
                        }
                    }
                    , .m_pWrite     = is_ready_only_v ? nullptr : +[](void* pClass, const type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context) constexpr
                    {
                        if constexpr ( is_ready_only_v == false )
                        {
                            if constexpr (std::is_enum_v<t> && type::var_t<t>::guid_v == 1)
                            {
                                type::atomic_v<t>.m_RegisteredEnumSpan = S;
                            }

                            if constexpr (sizeof...(T_ADDITIONAL) == 0)
                                T_LAMBDA_V
                                ( *static_cast<T_CLASS*>(pClass)
                                , false
                                , const_cast<type::any&>(Any).get<t>()
                                );
                            else
                                T_LAMBDA_V
                               ( *static_cast<T_CLASS*>(pClass)
                               , false
                               , const_cast<type::any&>(Any).get<t>()
                               , Context
                               );
                        }
                    }
                    , .m_AtomicType = xproperty::type::atomic_v<atomic_t>
                    , .m_UnregisteredEnumSpan = details::unregistered_enum_array<details::is_unregistered_enum_v<atomic_t>, T_ARGS...>::value
                    }
                , .m_bConst = is_ready_only_v
                , .m_pGetUserData = details::GetUserData<user_data_t>
                };
            }
        };

        //
        // HANDLE MEMBER VARIABLES with unregistered ( REFS )
        //
        // These type need a special structure since C++ does not support getting the address of a reference member variable
        // So solve this issue we use a macro xproperty::member_ref which will generate this structure for us
        // using this structure we can solve everything else
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename... T_ARGS >
        requires meets_requirements_v< false, false, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE&(*)(T_CLASS&), T_LAMBDA_V, T_ARGS... >
        {
            using user_data_t = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;

            static consteval xproperty::type::members getInfo()
            {
                //
                // Handle Vars and Refs that are properties... we just convert them to a scope
                //
                return
                { .m_GUID       = xproperty::settings::strguid(T_NAME_V)
                , .m_pName      = T_NAME_V
                , .m_Variant    = xproperty::type::members::props{ .m_pCast = details::cast_scope<T_CLASS, T_MEMBER_TYPE, T_LAMBDA_V >::Cast }
                , .m_bConst     = details::is_read_only_v<T_CLASS, T_MEMBER_TYPE, T_ARGS...>
                , .m_pGetUserData = details::GetUserData<user_data_t>
                };
            }
        };

        //
        // HANDLE MEMBER VARIABLES with unregistered ( VARS, pVARS )
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_DATA, typename... T_ARGS >
        requires meets_requirements_v< false, false, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE T_CLASS::*, T_DATA, T_ARGS... >
        {
            static consteval xproperty::type::members getInfo( void ) noexcept
            {
                return member < T_NAME_V, T_MEMBER_TYPE& (*)(T_CLASS&), +[](T_CLASS& C) constexpr ->T_MEMBER_TYPE& {return C.*T_DATA; }, T_ARGS... >::getInfo();
            }
        };

        //
        // VIRTUAL SCOPES for non-atomic types
        //
        // These type need a special structure since C++ does not support getting the address of a reference member variable
        // So solve this issue we use a macro xproperty::member_ref which will generate this structure for us
        // using this structure we can solve everything else
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename...T_EXTRAS, typename... T_ARGS >
        requires meets_requirements_v< false, false, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE*(*)(T_CLASS&, T_EXTRAS...), T_LAMBDA_V, T_ARGS... >
        {
            using user_data_t = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;

            static consteval xproperty::type::members getInfo( void ) noexcept
            {
                //
                // Handle Vars and Refs that are properties... we just convert them to a scope
                //
                return
                { .m_GUID       = xproperty::settings::strguid(T_NAME_V)
                , .m_pName      = T_NAME_V
                , .m_Variant    = xproperty::type::members::props{ details::cast_scope<T_CLASS, T_MEMBER_TYPE, T_LAMBDA_V >::Cast }
                , .m_bConst     = details::is_read_only_v<T_CLASS, T_MEMBER_TYPE, T_ARGS...>
                , .m_pGetUserData = details::GetUserData<user_data_t>
                };
            }
        };

        //
        // HANDLE MEMBER VARIABLES LISTS with unregistered ( REF )
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename... T_ARGS >
        requires meets_requirements_v<false, true, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE&(*)(T_CLASS&), T_LAMBDA_V, T_ARGS... >
        {
            using                           table_helper    = details::list_table< T_CLASS, T_MEMBER_TYPE, T_LAMBDA_V >;
            inline static constexpr auto    array_v         = table_helper::GetArray();
            using                           user_data_t     = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;

            static consteval xproperty::type::members getInfo( void ) noexcept
            {
                using t              = type::var_t<T_MEMBER_TYPE>;

                return
                { .m_GUID               = xproperty::settings::strguid(T_NAME_V)
                , .m_pName              = T_NAME_V
                , .m_Variant            = xproperty::type::members::list_props
                    { xproperty::type::members::props{ details::cast_scope_list< typename t::specializing_t >::Cast }
                    , array_v
                    }
                , .m_bConst = details::is_read_only_v<T_CLASS, T_MEMBER_TYPE, T_ARGS...>
                , .m_pGetUserData = details::GetUserData<user_data_t>
                };
            };
        };

        //
        // HANDLE MEMBER VARIABLES LISTS with unregistered ( VARS, pVARS )
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_DATA, typename... T_ARGS >
        requires meets_requirements_v<false, true, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE T_CLASS::*, T_DATA, T_ARGS... >
        {
            static consteval xproperty::type::members getInfo( void ) noexcept
            {
                return member < T_NAME_V, T_MEMBER_TYPE&(*)(T_CLASS&), +[](T_CLASS& C) constexpr ->T_MEMBER_TYPE& {return C.*T_DATA; }, T_ARGS... >::getInfo();
            }
        };

        //
        // VIRTUAL LIST UNREGISTERED
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename...T_EXTRA_ARGS, typename... T_ARGS >
        requires meets_requirements_v<false, true, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE*(*)(T_CLASS&, T_EXTRA_ARGS...), T_LAMBDA_V, T_ARGS... >
        {
            using                   table_helper = details::list_table< T_CLASS, T_MEMBER_TYPE, T_LAMBDA_V >;
            using                   user_data_t  = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;

            inline static constexpr auto array_v = table_helper::GetArray();

            static consteval xproperty::type::members getInfo( void ) noexcept
            {
                using t              = type::var_t<T_MEMBER_TYPE>;

                return
                { .m_GUID       = xproperty::settings::strguid(T_NAME_V)
                , .m_pName      = T_NAME_V
                , .m_Variant    = xproperty::type::members::list_props
                    { xproperty::type::members::props {details::cast_scope_list< typename t::specializing_t >::Cast}
                     , array_v
                    }
                , .m_bConst     = details::is_read_only_v<T_CLASS, T_MEMBER_TYPE, T_ARGS...>
                , .m_pGetUserData = details::GetUserData<user_data_t>
                };
            };
        };

        //
        // HANDLE MEMBER VARIABLES LISTS with registered ( REFS )
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename...T_EXTRA_ARGS, typename... T_ARGS >
        requires meets_requirements_v<true, true, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE&(*)(T_CLASS&, T_EXTRA_ARGS...), T_LAMBDA_V, T_ARGS... >
        {
            using                       table_helper = details::list_table< T_CLASS, std::remove_reference_t<T_MEMBER_TYPE>, T_LAMBDA_V >;
            using                        user_data_t = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;
            inline static constexpr auto array_v     = table_helper::GetArray();

            static consteval xproperty::type::members getInfo( void ) noexcept
            {
                using           atomic_t        = typename type::var_t< T_MEMBER_TYPE >::atomic_type;
                using           last_t          = typename table_helper::last_dimension;

                // TODO: I may need to check if all its last_t are read only as well???
                constexpr bool  is_ready_only_v = details::is_read_only_v<T_CLASS, T_MEMBER_TYPE, T_ARGS...>;

                //
                // Handle Vars and Refs that are properties... we just convert them to a scope
                //
                return
                { .m_GUID               = xproperty::settings::strguid(T_NAME_V)
                , .m_pName              = T_NAME_V
                , .m_Variant            = xproperty::type::members::list_var
                    { .m_pRead          =  +[](const void* pClass, type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context) constexpr
                                            {
                                                if constexpr (std::is_enum_v<atomic_t> && type::var_t<atomic_t>::guid_v == 1)
                                                {
                                                    if (type::atomic_v<atomic_t>.m_RegisteredEnumSpan.size() == 0)
                                                        type::atomic_v<atomic_t>.m_RegisteredEnumSpan = S;
                                                    else
                                                        assert(S.size() == type::atomic_v<atomic_t>.m_RegisteredEnumSpan.size());
                                                }

                                                type::var_t<last_t>::Read
                                                ( *static_cast<const typename type::var_t<last_t>::specializing_t*>(pClass)
                                                , Any.Reset<atomic_t>()
                                                , Context
                                                );
                                            }
                                            
                    , .m_pWrite         = is_ready_only_v ? nullptr : +[]( void* pClass, const type::any& Any,const std::span<const type::atomic::enum_item>& S, settings::context& Context) constexpr
                                            {
                                                if constexpr (std::is_enum_v<atomic_t> && type::var_t<atomic_t>::guid_v == 1)
                                                {
                                                    if (type::atomic_v<atomic_t>.m_RegisteredEnumSpan.size() == 0 )
                                                        type::atomic_v<atomic_t>.m_RegisteredEnumSpan = S;
                                                    else
                                                        assert( S.size() == type::atomic_v<atomic_t>.m_RegisteredEnumSpan.size() );
                                                }

                                                type::var_t<last_t>::Write
                                                ( *static_cast< typename type::var_t<last_t>::specializing_t*>(pClass)
                                                , Any.get<atomic_t>()
                                                , Context
                                                );
                                            }
                    , .m_Table          = array_v
                    , .m_AtomicType     = xproperty::type::atomic_v<atomic_t>
                    , .m_UnregisteredEnumSpan = details::unregistered_enum_array<details::is_unregistered_enum_v<atomic_t>, T_ARGS...>::value
                    }
                , .m_bConst = is_ready_only_v
                , .m_pGetUserData = details::GetUserData<user_data_t>
                };
            };
        };

        //
        // HANDLE MEMBER VARIABLES LISTS with registered ( VARS, pVARS )
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_MEMBER_PTR_V, typename... T_ARGS >
        requires meets_requirements_v<true, true, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE T_CLASS::*, T_MEMBER_PTR_V, T_ARGS... >
        {
            static consteval xproperty::type::members getInfo( void ) noexcept
            {
                return member< T_NAME_V, T_MEMBER_TYPE& (*)(T_CLASS&), +[](T_CLASS& C) constexpr ->T_MEMBER_TYPE& {return C.*T_MEMBER_PTR_V; }, T_ARGS... >::getInfo();
            };
        };

        //
        // Handle member functions
        //
        struct member_function_tag{};
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename...T_FUNC_ARGS, typename T_RETURN, auto T_DATA, typename... T_ARGS  >
        struct member<T_NAME_V, T_RETURN(T_CLASS::*)(T_FUNC_ARGS...), T_DATA, T_ARGS...> : tag<member_function_tag>
        {
            using                        class_t           = T_CLASS;
            using                        function_return_t = T_RETURN;
            using                        user_data_t       = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;
            using                        function_args_t   = std::tuple<T_FUNC_ARGS...>;
            inline constexpr static auto arg_types_list_v  = []()consteval
            {
                if constexpr(std::tuple_size_v<function_args_t>)
                {
                    std::array<xproperty::basic_type, std::tuple_size_v<function_args_t>> List;
                    ((List[xproperty::details::tuple_t2i_v<T_FUNC_ARGS, function_args_t>].m_Value = type_meta<T_FUNC_ARGS>::value), ...);
                    return List;
                }
                else
                {
                    return std::span<xproperty::basic_type>{};
                }
            }();

            static consteval xproperty::type::members getInfo(bool bConst=false) noexcept
            {
                return
                { .m_GUID    = xproperty::settings::strguid(T_NAME_V)
                , .m_pName   = T_NAME_V
                , .m_Variant = xproperty::type::members::function
                    { .m_pCallFunction = []( void* pClass, void* pArgs) constexpr noexcept
                        {
                            auto pTheArgs = static_cast<function_args_t*>(pArgs);
                            std::invoke
                            ( T_DATA
                            , static_cast<class_t*>(pClass)
                            , std::get<T_FUNC_ARGS>(*pTheArgs) ...
                            );
                        }
                    , .m_ArgumentList  = arg_types_list_v
                    }
                , .m_bConst = bConst
                , .m_pGetUserData = details::GetUserData<user_data_t>
                };
            }

        //    constexpr static inline auto CallFunction = []<typename...TARGS>( void* pClass, TARGS&&...Args ){ return std::invoke( T_DATA, reinterpret_cast<class_t>(pClass), std::forward<TARGS>(Args)...); };
        };

        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename...T_FUNC_ARGS, typename T_RETURN, auto T_DATA, typename... T_ARGS  >
        struct member<T_NAME_V, T_RETURN(T_CLASS::*)(T_FUNC_ARGS...) const, T_DATA, T_ARGS...> : member<T_NAME_V, T_RETURN(T_CLASS::*)(T_FUNC_ARGS...), T_DATA, T_ARGS...>
        {
            static consteval xproperty::type::members getInfo( void ) noexcept
            {
                return member<T_NAME_V, T_RETURN(T_CLASS::*)(T_FUNC_ARGS...), T_DATA, T_ARGS...>::getInfo(true);
            }
        };

        //
        // SCOPE
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T >
        struct scope;

        template< xproperty::details::fixed_string T_NAME_V, typename...T_ARGS >
        struct scope< T_NAME_V, std::tuple<T_ARGS...> >
        {
            inline constexpr static auto name_v    = T_NAME_V;
            inline constexpr static auto members_v = []() consteval
            {
                if constexpr (sizeof...(T_ARGS)) return std::array{ T_ARGS::meta_t::getInfo() ... };
                else                             return std::span<type::members>{};
            }();
            using user_data_t = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;

            consteval static type::members::scope getInfoScope( void ) noexcept
            {
                return{ .m_Members = members_v };
            }

            consteval static type::members getInfo( void ) noexcept
            {
                return
                { .m_GUID           = xproperty::settings::strguid(T_NAME_V)
                , .m_pName          = name_v.m_Value
                , .m_Variant        = getInfoScope()
                , .m_bConst         = xproperty::details::tuple_has_tag_v< read_only_tag, std::tuple<T_ARGS...>>
                , .m_pGetUserData   = details::GetUserData<user_data_t>
                };
            }
        };

        template< typename T_OBJECT_TYPE, typename T >
        struct bases;

        template< typename T_OBJECT_TYPE, typename...T_ARGS >
        struct bases< T_OBJECT_TYPE, std::tuple<T_ARGS...> >
        {
            template< typename T >
            static consteval type::base getInfo( T* ) noexcept
            {
                using t = typename T::type;
                return 
                { type::members::props{ details::cast_scope< T_OBJECT_TYPE, t, +[](T_OBJECT_TYPE& C) constexpr noexcept ->t& {return C; } >::Cast }
                , T::is_const_v
                };
            }

            consteval static auto getArray( void ) noexcept
            {
                if constexpr ( sizeof...(T_ARGS) == 0 ) return std::span<type::base>{};
                else
                {
                    return std::array
                    { getInfo(static_cast<T_ARGS*>(nullptr))
                      ...
                    };
                }
            }
        };


        template< typename...T_ARGS >
        struct constructor
        {
            using function_args_t   = xproperty::details::tuple_cat_t< std::conditional_t< has_tags_v<T_ARGS>, std::tuple<>,       std::tuple<T_ARGS>> ...>;
            using arg_tuple_t       = xproperty::details::tuple_cat_t< std::conditional_t< has_tags_v<T_ARGS>, std::tuple<T_ARGS>, std::tuple<>>       ...>;
            using user_data_t       = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;

            inline constexpr static auto arg_types_list_v  = []()consteval
            {
                if constexpr(std::tuple_size_v<function_args_t>)
                {
                    std::array<xproperty::basic_type, std::tuple_size_v<function_args_t>> List;
                    [&] <typename...ARGS>(std::tuple<ARGS...>*) constexpr
                    {
                        ((List[xproperty::details::tuple_t2i_v<ARGS, function_args_t>].m_Value = type_meta<ARGS>::value), ...);
                    }(static_cast<function_args_t*>(nullptr));
                    return List;
                }
                else
                {
                    return std::span<xproperty::basic_type>{};
                }
            }();

            template<typename T_CLASS>
            static consteval type::member_constructor getInfo(std::tuple<T_CLASS>*) noexcept
            {
                // Check to make sure we can construct our object this these arguments
                [] <typename...ARGS>(std::tuple<ARGS...>*) constexpr
                {
                    static_assert( std::is_constructible_v<T_CLASS, ARGS...>, "xproperty::obj_constructor - You can not construct this object with the arguments that you have given" );
                }(static_cast<function_args_t*>(nullptr) );

                return
                { .m_pCallConstructor = [](void* pArgs) constexpr ->void*
                    {
                        auto pTheArgs = static_cast<function_args_t*>(pArgs);
                        return [&] <typename...ARGS>(std::tuple<ARGS...>*) constexpr
                        {
                            return new T_CLASS{ std::get<ARGS>(*pTheArgs)... };
                        }(static_cast<function_args_t*>(nullptr));
                    }
                , .m_ArgumentList = arg_types_list_v
                };
            }
        };

        template< xproperty::details::fixed_string T_NAME_V, typename T_OBJECT_TYPE, typename...T_ARGS >
        struct object
        {
            using                        members                 = xproperty::details::filter_by_tag_t< meta::obj_member_tag, T_ARGS... >;
            using                        scope_t                 = meta::scope< T_NAME_V, members >;
            using                        obj_bases               = xproperty::details::filter_by_tag_t< meta::obj_base_tag, T_ARGS... >;
            inline constexpr static auto obj_bases_list_v        = bases<T_OBJECT_TYPE, obj_bases>::getArray();
            using                        constructors            = xproperty::details::filter_by_tag_t< meta::obj_constructor_tag, T_ARGS... >;
            inline constexpr static auto constructor_list_v      = []() consteval
            {
                if constexpr (std::tuple_size_v<constructors>) return [&]<typename...ARGS>(std::tuple<ARGS...>*) constexpr
                {
                    return std::array
                    { type::member_constructor{.m_pCallConstructor = [](void*) constexpr ->void* { return new T_OBJECT_TYPE; }, .m_ArgumentList = {} }
                    , ARGS::meta_t::getInfo(static_cast<std::tuple<T_OBJECT_TYPE>*>(nullptr))
                      ...
                    };

                }(static_cast<constructors*>(nullptr));
                else
                {
                    return std::array{ type::member_constructor{.m_pCallConstructor = [](void*) constexpr ->void* { return new T_OBJECT_TYPE; }, .m_ArgumentList = {} } };
                }
            }();

            consteval static type::object getInfo( void ) noexcept
            {
                return
                { { scope_t::getInfoScope() }
                , scope_t::name_v
                , xproperty::settings::strguid(scope_t::name_v)
                , [](void* p) constexpr noexcept { delete static_cast<T_OBJECT_TYPE*>(p); }
                , constructor_list_v
                , obj_bases_list_v
                };
            }
        };
    }

    //
    // Object member are used for the following thing:
    // - Member Variables which are atomic or are properties
    // - Member Variables which are references
    // - Member Variables which are lists
    // - Member Scopes
    // - Member Functions
    //
    // VARIABLES ATOMIC
    // There are a few type of supported variables:
    // - Atomic type: (int, short, float, etc.. )
    // - xproperty vars: Classes which have properties
    //
    // VARIABLE REFERENCES
    // The supported references can be TO anything from the (VARIABLE ATOMIC)
    // - native references: <type>*, <type>&
    // - class references: std::unique_ptr<>, std::share_ptr<>
    //
    // VARIABLES LISTS
    // - This is for type which are a class that represent lists
    // - any of the supported generic_lists<>
    //
    // MEMBER SCOPES
    // These are scopes which includes additional properties which the object is responsible for
    // NOTE! That some of the properties an object can have could be virtual as well...
    // - Local Variables: These are objects instances inside our object which they also have properties
    // - Pointer Variables: These are external object instances that contain objects which also have properties
    // - List Variables: List of objects instances that also contain properties
    // - Scopes: This are additional scopes created by the user that may contain properties
    //
    template< details::fixed_string T_NAME_V, auto T_DATA, typename...T_ARGS >
    struct obj_member : tag<meta::obj_member_tag>
    {
        using meta_t = meta::member<T_NAME_V, decltype(T_DATA), T_DATA, T_ARGS... >;
    };

    template< details::fixed_string T_NAME_V, auto T_DATA, typename...T_ARGS >
    struct obj_member_ro : obj_member< T_NAME_V, T_DATA, xproperty::tag< meta::read_only_tag >, T_ARGS...>{};

    //
    // Overrides the default read, or write, or both of the xproperty member variable with a custom version by the user.
    // There are two different context that this can be used... for single variables or for variables that are lists.
    //
    // SINGLE VARIABLES
    //
    //      VIRTUAL PROPERTIES WITH FUNCTION POINTERS
    //                                REQUIRED                 REQUIRED                    OPTIONAL
    //                      ---------------------------  ------------------------------  --------------------------
    //      VAR READ  - []( const <object_type>& Class,        <atomic_type>& Data,      settings::context& Context ) -> void
    //      VAR WRITE - [](       <object_type>& Class,  const <atomic_type>& Data,      settings::context& Context ) -> void
    //
    //      SCOPE     - []( const <object_type>& Class,                                  settings::context& Context ) -> T&
    //
    //      VIRTUAL PROPERTIES WITH MEMBER FUNCTIONS
    //                                                REQUIRED                    OPTIONAL
    //                                     ------------------------------  --------------------------
    //      READ  - &class::Func ==> void(       <member_var_type>& Data,  settings::context& Context ) const
    //      WRITE - &class::Func ==> void( const <member_var_type>& Data,  settings::context& Context )
    //      
    //      LAMBDA VERSION
    //                                REQUIRED                        REQUIRED                      OPTIONAL                  OPTIONAL
    //                  -----------------------------------  ------------------------------  ---------------------------  --------------------------
    //      READ  - []( const <member_var_type>& MemberVar,        <member_var_type>& Data,        <object_type>& Class,  settings::context& Context ) -> void
    //      WRITE - [](       <member_var_type>& MemberVar,  const <member_var_type>& Data,  const <object_type>& Class,  settings::context& Context ) -> void
    //
    //      MEMBER FUNCTION VERSION
    //                                                    REQUIRED                        REQUIRED                    OPTIONAL
    //                                     ----------------------------------  -----------------------------  --------------------------
    //      READ  - &class::Func ==> void(       <member_var_type>& MemberVar,       <member_var_type>& Data, settings::context& Context )
    //      WRITE - &class::Func ==> void( const <member_var_type>& MemberVar, const <member_var_type>& Data, settings::context& Context )
    //
    // LIST VARIABLES
    //      READ      - []( const <object_type>& Class,       <member_var_type>& MemberVar,       <member_var_type>& Data, std::size_t Key )
    //      WRITE     - [](       <object_type>& Class, const <member_var_type>& MemberVar, const <member_var_type>& Data, std::size_t Key )
    //      START_END - []( const <object_type>& Class, const <member_var_type>& MemberVar,       generic_type::iterator& StarIterator,       generic_type::iterator& EndIterator )
    //      NEXT      - []( const <object_type>& Class, const <member_var_type>& MemberVar,       generic_type::iterator&     Iterator, const generic_type::iterator& EndIterator )
    // ITERATOR_2_KEY - []( const <object_type>& Class, const <member_var_type>& MemberVar, const generic_type::iterator&     Iterator ) -> std::size_t
    //      SIZE      - []( const <object_type>& Class, const <member_var_type>& MemberVar ) -> std::size_t
    template< auto...T_OVERWRITES >
    struct member_overwrites : xproperty::tag< xproperty::meta::member_overwrites_tag >
    {
        using tuple_t = std::tuple< decltype(T_OVERWRITES) ... >;
        constexpr static tuple_t overwrite_v = { T_OVERWRITES... };
    };

    // void(*)( Type& , std::size_t Size, context& Context )
    template< auto...T_OVERWRITE_LIST_SIZE >
    struct member_overwrite_set_size : xproperty::tag< xproperty::meta::member_overwrite_list_size_tag >
    {
        using tuple_t = std::tuple< decltype(T_OVERWRITE_LIST_SIZE) ... >;
        constexpr static auto overwrite_set_size_v = { T_OVERWRITE_LIST_SIZE... };
    };

    //
    // USER DATA
    //
    template< details::fixed_string ID, std::uint32_t GUID = xproperty::settings::strguid(ID) >
    struct member_user_data : xproperty::tag< xproperty::meta::user_data_tag >
    {
        constexpr static auto type_string_v = ID.m_Value;
        constexpr static auto type_guid_v   = GUID;
    };

    //
    // SCOPE
    //
    template< details::fixed_string T_NAME_V, typename...T_ARGS >
    struct obj_scope : tag<meta::obj_member_tag>
    {
        using meta_t = meta::scope<T_NAME_V, std::tuple<T_ARGS...> >;
    };

    template< details::fixed_string T_NAME_V, typename...T_ARGS >
    struct obj_scope_ro : obj_scope< T_NAME_V, xproperty::tag< meta::read_only_tag >, T_ARGS...> {};

    //
    // BASE (THE OBJECT PARENT)
    //
    template< typename T_BASE >
    struct obj_base : tag<meta::obj_base_tag>
    {
        using                        type       = T_BASE;
        inline static constexpr bool is_const_v = false;
    };

    template< typename T_BASE >
    struct obj_base_ro : tag<meta::obj_base_tag>
    {
        using                        type       = T_BASE;
        inline static constexpr bool is_const_v = true;
    };

    //
    // CONSTRUCTOR
    //
    template< typename...T_ARGS >
    struct obj_constructor : tag<meta::obj_constructor_tag>
    {
        using function_args_t = details::tuple_cat_t< std::conditional_t< has_tags_v<T_ARGS>, std::tuple<>, std::tuple<T_ARGS>> ...>;
        static_assert(std::tuple_size_v<function_args_t> > 0, "xproperty::obj_constructor - You don't need to add a constructor with no arguments");

        using meta_t          = meta::constructor< T_ARGS... >;
    };

    //
    // PROPERTY DEFINITIONS
    //
    struct def_base
    {
        const type::object&                     m_ObjectInfo;
        inline constinit static const def_base* m_pHead = nullptr;
        const def_base*                         m_pNext;
    };

    template
    < details::fixed_string     T_OBJECT_NAME_V
    , typename                  T_OBJECT_TYPE
    , typename...               T_ARGS
    >
    struct def : def_base
    {
        using meta_t    = meta::object< T_OBJECT_NAME_V, T_OBJECT_TYPE, T_ARGS...>;
        inline static constexpr xproperty::type::object     register_v  = meta_t::getInfo();

        def() : def_base{ register_v }
        {
            m_pNext = m_pHead;
            m_pHead = this;
            xproperty::type::get_obj_info<T_OBJECT_TYPE> = &register_v;
        }

        consteval static const xproperty::type::object* get() noexcept { return &register_v; }
    };

    //
    // Used to register a enum string value pair
    //
    template< details::fixed_string T_OBJECT_NAME_V, auto T_VALUE_V >
    struct member_enum_value : tag< meta::enum_value_tag >
    {
        inline constexpr static auto name_v  = T_OBJECT_NAME_V;
        inline constexpr static auto value_v = T_VALUE_V;
    };

    //
    // Used to register a enum string value pair
    //
    template< auto& T_SPAN_V >
    struct member_enum_span : tag< meta::enum_span_tag >
    {
        inline constexpr static auto span_v = T_SPAN_V;
    };

    // Put it in the main scope so that it can be used by the user
    using any            = type::any;
    using begin_iterator = type::begin_iterator;
    using end_iterator   = type::end_iterator;
}

//
// This is used to register the properties of the object
//
#define XPROPERTY_DEF( ... )  public: inline static auto PropertiesDefinition() { assert(false); using namespace xproperty; using namespace xproperty::settings; return xproperty::def<__VA_ARGS__ >(); }
#define XPROPERTY_VDEF( ... ) public: inline static auto PropertiesDefinition() { assert(false); using namespace xproperty; using namespace xproperty::settings; return xproperty::def<__VA_ARGS__ >(); } inline const xproperty::type::object* getProperties() const noexcept override;
#define XPROPERTY_REG2( NAMESPACE, TYPE )  namespace NAMESPACE { inline const decltype(TYPE::PropertiesDefinition()) g_PropertyRegistration_v{}; }
#define XPROPERTY_REG( TYPE ) XPROPERTY_REG2(TYPE##_props, TYPE)
#define XPROPERTY_VREG2( NAMESPACE, TYPE ) namespace NAMESPACE { inline const decltype(TYPE::PropertiesDefinition()) g_PropertyRegistration_v{}; } inline const xproperty::type::object* TYPE::getProperties() const noexcept { return NAMESPACE::g_PropertyRegistration_v.get();}
#define XPROPERTY_VREG( TYPE ) XPROPERTY_VREG2(TYPE##_props, TYPE)

#endif