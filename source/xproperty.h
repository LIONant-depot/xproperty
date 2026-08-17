#ifndef XPROPERTY_H
#define XPROPERTY_H
#pragma once

#include<array>
#include<tuple>
#include<ranges>
#include<assert.h>
#include<variant>
#include<format>
#include<functional>
#include<bit>
#include<limits>
#include<optional>

#if defined(XPROPERTY_DEPRECATE_LEGACY_NAMES)
    #define XPROPERTY_LEGACY_API(MSG) [[deprecated(MSG)]]
#else
    #define XPROPERTY_LEGACY_API(MSG)
#endif

namespace xproperty
{
    // Core operations report machine-readable failures. Upper layers own formatting and policy.
    enum class error_code : std::uint8_t
    {
        none,
        member_not_found,
        type_mismatch,
        read_only,
        null_object,
        invalid_index,
        invalid_key,
        invalid_enum_value,
        unsupported_operation,
        constructor_not_found,
        invalid_path
    };

    struct error
    {
        error_code      m_Code         { error_code::none };
        std::uint32_t   m_PathOffset   { 0 };
        std::uint32_t   m_ExpectedType { 0 };
        std::uint32_t   m_ActualType   { 0 };

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return m_Code != error_code::none;
        }
    };

    namespace details
    {
        [[nodiscard]] constexpr error makeTypeMismatch(std::uint32_t Expected = 0, std::uint32_t Actual = 0) noexcept
        { return { .m_Code = error_code::type_mismatch, .m_ExpectedType = Expected, .m_ActualType = Actual }; }
        [[nodiscard]] constexpr error makeReadOnly(std::uint32_t Expected = 0, std::uint32_t Actual = 0) noexcept
        { return { .m_Code = error_code::read_only, .m_ExpectedType = Expected, .m_ActualType = Actual }; }
        [[nodiscard]] constexpr error makeInvalidKey(std::uint32_t Expected = 0, std::uint32_t Actual = 0) noexcept
        { return { .m_Code = error_code::invalid_key, .m_ExpectedType = Expected, .m_ActualType = Actual }; }
        [[nodiscard]] constexpr error makeUnsupportedOperation(std::uint32_t Expected = 0) noexcept
        { return { .m_Code = error_code::unsupported_operation, .m_ExpectedType = Expected }; }
        [[nodiscard]] constexpr error makeNullObject() noexcept
        { return { .m_Code = error_code::null_object }; }
    }

    template<typename T>
    class [[nodiscard]] result
    {
    public:
        constexpr result(T Value) noexcept(std::is_nothrow_move_constructible_v<T>)
            : m_Value{ std::move(Value) }
        {}
        constexpr result(error Error) noexcept : m_Error{ Error } {}

        [[nodiscard]] constexpr explicit operator bool() const noexcept { return m_Value.has_value(); }
        [[nodiscard]] constexpr bool hasValue() const noexcept { return m_Value.has_value(); }
        [[nodiscard]] constexpr const error& getError() const noexcept { return m_Error; }
        [[nodiscard]] constexpr T& value() & noexcept { assert(m_Value); return *m_Value; }
        [[nodiscard]] constexpr const T& value() const& noexcept { assert(m_Value); return *m_Value; }
        [[nodiscard]] constexpr T&& value() && noexcept { assert(m_Value); return std::move(*m_Value); }

    private:
        std::optional<T> m_Value;
        error            m_Error{};
    };

    template<>
    class [[nodiscard]] result<void>
    {
    public:
        constexpr result() noexcept = default;
        constexpr result(error Error) noexcept : m_Error{ Error } {}

        [[nodiscard]] constexpr explicit operator bool() const noexcept { return !m_Error; }
        [[nodiscard]] constexpr bool hasValue() const noexcept { return !m_Error; }
        [[nodiscard]] constexpr const error& getError() const noexcept { return m_Error; }

    private:
        error m_Error{};
    };
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
            consteval operator const char* () const { return m_Value; }
            consteval static uint32_t size ( void ) noexcept { return size_v; }
            char m_Value[T_SIZE_V];
        };

        template<std::size_t T_SIZE_V>
        struct fixed_wstring
        {
            inline constexpr static uint32_t size_v = T_SIZE_V;
            consteval fixed_wstring(const wchar_t* const pStr)
            {
                for (auto i = 0u; i < T_SIZE_V; ++i) m_Value[i] = pStr[i];
            }

            consteval auto operator<=>(const fixed_wstring&) const = default;
            consteval operator const wchar_t* () const { return m_Value; }
            consteval static uint32_t size(void) noexcept { return size_v; }
            wchar_t m_Value[T_SIZE_V];
        };

        template<std::size_t T_SIZE_V>
        fixed_string(const char(&)[T_SIZE_V])-> fixed_string<T_SIZE_V>;

        template<std::size_t T_SIZE_V>
        fixed_wstring(const wchar_t(&)[T_SIZE_V]) -> fixed_wstring<T_SIZE_V>;

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

    namespace details
    {
        template<typename T_TUPLE, std::size_t... I>
        [[nodiscard]] consteval auto make_argument_type_list_impl(std::index_sequence<I...>)
        {
            return std::array<xproperty::basic_type, sizeof...(I)>{ xproperty::type_meta<std::tuple_element_t<I, T_TUPLE>>{}... };
        }

        template<typename T_TUPLE>
        [[nodiscard]] consteval auto make_argument_type_list()
        {
            return make_argument_type_list_impl<T_TUPLE>(std::make_index_sequence<std::tuple_size_v<T_TUPLE>>{});
        }

        template<typename T_TUPLE, std::size_t... I>
        [[nodiscard]] constexpr bool tuple_types_match_impl(std::span<const xproperty::basic_type> Types, std::index_sequence<I...>) noexcept
        {
            return Types.size() == sizeof...(I)
                && ((xproperty::type_meta<std::tuple_element_t<I, T_TUPLE>>::value == Types[I].m_Value) && ...);
        }

        template<typename T_TUPLE>
        [[nodiscard]] constexpr bool tuple_types_match(std::span<const xproperty::basic_type> Types) noexcept
        {
            return tuple_types_match_impl<T_TUPLE>(Types, std::make_index_sequence<std::tuple_size_v<T_TUPLE>>{});
        }

        template<typename T_CALLABLE, typename T_TUPLE>
        constexpr decltype(auto) invoke_from_tuple(T_CALLABLE&& Callable, T_TUPLE&& Arguments)
        {
            return std::apply(std::forward<T_CALLABLE>(Callable), std::forward<T_TUPLE>(Arguments));
        }

        template<typename T_CLASS, typename T_TUPLE>
        [[nodiscard]] T_CLASS* construct_from_tuple(T_TUPLE&& Arguments)
        {
            return invoke_from_tuple([]<typename... T_ARGS>(T_ARGS&&... Args) -> T_CLASS*
            {
                return new T_CLASS{ std::forward<T_ARGS>(Args)... };
            }, std::forward<T_TUPLE>(Arguments));
        }

        template<std::size_t T_ENTRY_COUNT, std::uint32_t T_EMPTY_INDEX>
        struct static_guid_map
        {
            inline constexpr static std::size_t table_size_v =
                T_ENTRY_COUNT == 0 ? 0 : std::bit_ceil(T_ENTRY_COUNT * 2);
            using table_type = std::array<std::uint32_t, table_size_v>;

            template<typename T_ENTRIES>
            [[nodiscard]] static consteval table_type build(const T_ENTRIES& Entries)
            {
                table_type Lookup{};
                if constexpr (T_ENTRY_COUNT > 0)
                {
                    Lookup.fill(T_EMPTY_INDEX);
                    constexpr std::size_t Mask = table_size_v - 1;
                    for (std::uint32_t Index = 0; Index < T_ENTRY_COUNT; ++Index)
                    {
                        std::size_t Bucket = Entries[Index].m_GUID & Mask;
                        while (Lookup[Bucket] != T_EMPTY_INDEX)
                            Bucket = (Bucket + 1) & Mask;
                        Lookup[Bucket] = Index;
                    }
                }
                return Lookup;
            }
        };
    }

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
        // Foreign types you can't add XPROPERTY_DEF to directly (ImGui's ImVec2, xresource's type_guid,
        // etc.) get their reflection through a sibling wrapper struct instead
        // ("struct v2 : ImVec2 { XPROPERTY_DEF(...) }"), and members are still declared with the plain
        // foreign type. This trait is the explicit, deterministic link between the two: specialize it
        // wherever such a wrapper is defined so the validator and cast_scope both know where to find
        // T's actual reflection. Default is the identity mapping - most types need no specialization at
        // all, since they either have their own PropertiesDefinition() or aren't reflected as objects.
        //--------------------------------------------------------------------------------------------
        template<typename T>
        struct reflected_type
        {
            using type = T;
        };

        template<typename T>
        using reflected_type_t = typename reflected_type<T>::type;

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
                    [[fallthrough]];
                case 2: k1 ^= key.tail(1) << 8;
                    [[fallthrough]];
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
            const char*                 m_pHelp;
            std::uint32_t               m_Value;

            enum_item() = default;

            template<typename T>
            constexpr enum_item( const char* pName, T Val ) noexcept
                : m_pName { pName }
                , m_pHelp { nullptr }
                , m_Value { static_cast<std::uint32_t>(Val) }
            {
                static_assert( std::is_enum_v<T> );
            }

            template<typename T>
            constexpr enum_item(const char* pName, T Val, const char* pHelp ) noexcept
                : m_pName{ pName }
                , m_pHelp{ pHelp }
                , m_Value{ static_cast<std::uint32_t>(Val) }
            {
                static_assert(std::is_enum_v<T>);
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
                inline constexpr static auto name_v = xproperty::details::fixed_string("Unkown Type");
            };

            template<typename T >
            struct add_unregistered_enum<true, T>
            {
                static_assert(std::is_const_v<T> == false, "Please don't define types that are const");

                using                                              type        = T;
                inline constexpr static auto                       name_v      = xproperty::details::fixed_string("Unregistered Enum");
                inline constexpr static std::span<const enum_item> enum_list_v = {};
                inline constexpr static auto                       guid_v      = 1u;

                static_assert(sizeof(type) <= sizeof(xproperty::settings::data_memory));

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
            template<typename T_ADAPTER>
            consteval void validate_pointer_adapter()
            {
                using pointer_t = typename T_ADAPTER::type;
                using pointee_t = typename T_ADAPTER::specializing_t;
                using atomic_t  = typename T_ADAPTER::atomic_type;
                static_assert(T_ADAPTER::is_pointer_v && !T_ADAPTER::is_list_v,
                    "XPROP027: pointer adapter must declare is_pointer_v=true and is_list_v=false");
                static_assert(requires(pointer_t& Pointer, settings::context& Context)
                { { T_ADAPTER::getObject(Pointer, Context) } -> std::convertible_to<pointee_t*>; },
                    "XPROP028: pointer adapter getObject(pointer, context) must return a compatible pointee pointer");
                static_assert(requires(pointer_t& Pointer, settings::context& Context)
                { { T_ADAPTER::getAtomic(Pointer, Context) } -> std::convertible_to<atomic_t*>; },
                    "XPROP029: pointer adapter getAtomic(pointer, context) must return a compatible atomic pointer");
                static_assert(!std::is_const_v<std::remove_reference_t<decltype(*std::declval<pointer_t&>())>> || std::is_const_v<pointee_t>,
                    "XPROP030: pointer adapter must preserve pointee constness");
            }

            template<typename T>
            using specializing_t = xproperty::details::remove_all_const_t<std::remove_reference_t<decltype(*std::declval<T>())>>;

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
            using specializing_t2 = std::remove_reference_t<decltype(*std::declval<T>())>;

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
            static_assert(sizeof(T) <= sizeof(data_memory),
                "XPROP009: registered atomic type does not fit in settings::data_memory");
            static_assert(alignof(T) <= alignof(data_memory),
                "XPROP010: registered atomic type alignment exceeds settings::data_memory alignment");
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

            struct builder
            {
                builder() = default;
                ~builder() = default;
                builder(type&& x) : m_X{ std::move(x) }
                {
                }
                builder(const type& x) : m_X{ x }
                {
                }
                type m_X;
            };

            constexpr static void              VoidConstruct   ( data_memory& Data )                            noexcept  { std::construct_at(reinterpret_cast<builder*>(&Data)); }  //std::construct_at(&reinterpret_cast<type&>(Data) ); }
            constexpr static void              Destruct        ( data_memory& Data )                            noexcept  { std::destroy_at(reinterpret_cast<builder*>(&Data)); }//std::destroy_at(&reinterpret_cast<type&>(Data) ); }
            constexpr static void              MoveConstruct   ( data_memory& Data1,       type&& Data2 )       noexcept  { std::construct_at(reinterpret_cast<builder*>(&Data1), std::forward<type>(Data2)); }//new(&Data1) type{ Data2 }; }
            // Deliberately not noexcept: copy-constructing an arbitrary registered type (std::string and
            // any other heap-allocating type included) can throw bad_alloc. Default/move-construct and
            // destruct are guaranteed nothrow (enforced by atomic_v<T>'s static_asserts); copy is the one
            // place xproperty accepts a throw as the unavoidable cost of supporting ordinary value types.
            constexpr static void              CopyConstruct   ( data_memory& Data1, const type&  Data2 )                 { std::construct_at(reinterpret_cast<builder*>(&Data1), Data2); } //new(&Data1) type{ Data2 }; }

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

            // Fixed-size adapters (std::span, std::array, native C-arrays) inherit this struct's own
            // do-nothing setSize below - so TrySetSize needs an explicit way to tell "genuinely
            // resizable" from "silently a no-op". Any var_type<T> that overrides setSize with a real
            // implementation (e.g. std::vector, via .resize()) must also override this flag to true.
            inline constexpr static bool has_real_setSize_v = false;

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
            using move_construct    = void(settings::data_memory&,       settings::data_memory&&);
            using copy_construct    = void(settings::data_memory&, const settings::data_memory&);

            const char*                 m_pName;
            std::uint32_t               m_GUID;
            std::uint32_t               m_Size;
            bool                        m_IsEnum;
            // Best-effort cache for any-only enum-name display (AnyToString, getEnumString()) when no
            // property context/span is available - actual property reads/writes never consult this,
            // they always resolve directly off their own property-local span (see resolveEnumString()),
            // so this cache being stale or first-writer-wins for a shared "unregistered enum" sentinel
            // never causes a read/write correctness bug, only a possibly-wrong any-only display for a
            // DIFFERENT unregistered enum than the one last touched.
            mutable std::span<const enum_item>  m_RegisteredEnumSpan;
            void_construct*             m_pVoidConstruct;
            destruct*                   m_pDestruct;
            move_construct*             m_pMoveConstruct;
            copy_construct*             m_pCopyConstruct;

            [[nodiscard]] constexpr std::span<const enum_item> enumItems(std::span<const enum_item> Override = {}) const noexcept
            {
                return Override.empty() ? m_RegisteredEnumSpan : Override;
            }

            [[nodiscard]] xproperty::result<const enum_item*> TryFindEnumByName(
                std::string_view Name, std::span<const enum_item> Override = {}) const noexcept
            {
                if (!m_IsEnum) return xproperty::details::makeUnsupportedOperation(m_GUID);
                for (const auto& Item : enumItems(Override))
                    if (Name == Item.m_pName) return &Item;
                return xproperty::error{ .m_Code = error_code::invalid_enum_value, .m_ExpectedType = m_GUID };
            }

            [[nodiscard]] xproperty::result<const enum_item*> TryFindEnumByValue(
                std::uint64_t Value, std::span<const enum_item> Override = {}) const noexcept
            {
                if (!m_IsEnum) return xproperty::details::makeUnsupportedOperation(m_GUID);
                for (const auto& Item : enumItems(Override))
                    if (static_cast<std::uint64_t>(Item.m_Value) == Value) return &Item;
                return xproperty::error{ .m_Code = error_code::invalid_enum_value, .m_ExpectedType = m_GUID };
            }
        };

        template<typename T>
        inline constexpr auto atomic_v = []() consteval -> atomic
        {
            static_assert( xproperty::details::has_const_v<T>   == false );
            static_assert( settings::var_type<T>::is_list_v    == false );
            static_assert( settings::var_type<T>::is_pointer_v == false );

            // xproperty is a no-throw API: any's construct/move/destruct are all declared noexcept and
            // rely on this being true for anything reasonable to register. Catching a violation here, at
            // registration time, is far cheaper than discovering it as a std::terminate later.
            //
            // Copy construction is deliberately NOT required to be nothrow here - std::string and every
            // other heap-allocating type have a copy constructor that can throw bad_alloc by the standard,
            // and refusing to register them isn't a viable trade for a general-purpose reflection library.
            // any's copy path (operator=(const any&), the copying set()/Reset<T>()) is instead given
            // transactional safety: construct the replacement before touching the existing value, so a
            // throw during copy leaves the original untouched instead of destroyed-with-nothing-to-replace-it.
            static_assert(std::is_nothrow_default_constructible_v<T>,
                "XPROP035: registered type must be nothrow default-constructible (xproperty guarantees no-throw semantics end to end)");
            static_assert(std::is_nothrow_move_constructible_v<T>,
                "XPROP037: registered type must be nothrow move-constructible (xproperty guarantees no-throw semantics end to end)");
            static_assert(std::is_nothrow_destructible_v<T>,
                "XPROP038: registered type must be nothrow destructible (xproperty guarantees no-throw semantics end to end)");

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
            , .m_pMoveConstruct = +[](settings::data_memory& D,       settings::data_memory&& S) constexpr { settings::var_type<T>::MoveConstruct( D, std::forward<T&&>(reinterpret_cast<T&&>(S)) );      }
            , .m_pCopyConstruct = +[](settings::data_memory& D, const settings::data_memory& S)  constexpr { settings::var_type<T>::CopyConstruct( D, reinterpret_cast<const T&>(S));  }
            };
        }();

        namespace details
        {
            struct uninitialized_iterator_t { explicit constexpr uninitialized_iterator_t() noexcept = default; };
            inline constexpr uninitialized_iterator_t uninitialized_iterator{};

            template< typename T >
            struct iterator_data
            {
                using table = typename xproperty::details::delay_linkage<list_table, T >::type;

                iterator_data() = delete;
                iterator_data( void* pData, const table& Table, settings::context& C ) noexcept
                : m_Data    {}
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
                end_iterator(void* pData, const table& Table, settings::context& C, uninitialized_iterator_t) noexcept
                    : parent(pData, Table, C)
                {}

                [[nodiscard]] constexpr bool isInitialized() const noexcept { return m_Initialized; }
                constexpr void markInitialized() noexcept { m_Initialized = true; }

                ~end_iterator()
                {
                    if (m_Initialized && parent::m_Table.m_pDestroyEndIterator)
                        parent::m_Table.m_pDestroyEndIterator(*this, parent::m_Context);
                }

                bool m_Initialized = false;
            };

            template< typename T >
            struct begin_iterator : iterator_data<T>
            {
                using parent = iterator_data<T>;
                using table  = typename parent::table;

                begin_iterator() = delete;
                begin_iterator(void* pData, const table& Table, settings::context& C, uninitialized_iterator_t) noexcept
                    : parent(pData, Table, C)
                {}

                [[nodiscard]] constexpr bool isInitialized() const noexcept { return m_Initialized; }
                constexpr void markInitialized() noexcept { m_Initialized = true; }

                [[nodiscard]] xproperty::result<void*> TryGetObject() noexcept
                {
                    if (!m_Initialized) return xproperty::details::makeUnsupportedOperation();
                    if (!parent::m_Table.m_pIteratorToObject) return xproperty::details::makeUnsupportedOperation();
                    if (void* Object = parent::m_Table.m_pIteratorToObject(parent::m_pObject, *this, parent::m_Context)) return Object;
                    return xproperty::error{ .m_Code = error_code::invalid_index };
                }

                [[nodiscard]] xproperty::result<void> TryGetKey(any& Key) const noexcept
                {
                    if (!m_Initialized || !parent::m_Table.m_pIteratorToKey)
                        return xproperty::details::makeUnsupportedOperation();
                    if (!parent::m_Table.m_pIteratorToKey(parent::m_pObject, *this, Key, parent::m_Context))
                        return xproperty::details::makeInvalidKey();
                    return {};
                }

                [[nodiscard]] xproperty::result<bool> TryNext(const end_iterator<T>& End) noexcept
                {
                    if (!m_Initialized || !End.isInitialized() || !parent::m_Table.m_pNext)
                        return xproperty::details::makeUnsupportedOperation();
                    ++m_Index;
                    return parent::m_Table.m_pNext(parent::m_pObject, *this, End, parent::m_Context);
                }

                XPROPERTY_LEGACY_API("Use TryGetObject instead")
                constexpr void* getObject( void ) noexcept
                {
                    auto Result = TryGetObject();
                    return Result ? Result.value() : nullptr;
                }

                XPROPERTY_LEGACY_API("Use TryGetKey instead")
                constexpr bool getKey( any& Key ) const noexcept
                {
                    return static_cast<bool>(TryGetKey(Key));
                }

                XPROPERTY_LEGACY_API("Use TryNext instead")
                constexpr bool Next( const end_iterator<T>& End ) noexcept
                {
                    auto Result = TryNext(End);
                    return Result ? Result.value() : false;
                }

                constexpr int getIndex() const noexcept
                {
                    return m_Index;
                }

                ~begin_iterator()
                {
                    if (m_Initialized && parent::m_Table.m_pDestroyBeginIterator)
                        parent::m_Table.m_pDestroyBeginIterator(*this, parent::m_Context);
                }

                int  m_Index = 0;
                bool m_Initialized = false;
            };

            template<typename T_VALUE, typename T_ITERATOR>
            [[nodiscard]] constexpr T_VALUE& iteratorStorageAs(T_ITERATOR& Iterator) noexcept
            {
                static_assert(sizeof(T_VALUE) <= sizeof(settings::iterator_memory),
                    "XPROP017: iterator type exceeds settings::iterator_memory size");
                static_assert(alignof(T_VALUE) <= alignof(settings::iterator_memory),
                    "XPROP018: iterator alignment exceeds settings::iterator_memory alignment");
                return *std::launder(reinterpret_cast<T_VALUE*>(&Iterator.m_Data));
            }

            template<typename T_VALUE, typename T_ITERATOR>
            [[nodiscard]] constexpr const T_VALUE& iteratorStorageAs(const T_ITERATOR& Iterator) noexcept
            {
                static_assert(sizeof(T_VALUE) <= sizeof(settings::iterator_memory),
                    "XPROP017: iterator type exceeds settings::iterator_memory size");
                static_assert(alignof(T_VALUE) <= alignof(settings::iterator_memory),
                    "XPROP018: iterator alignment exceeds settings::iterator_memory alignment");
                return *std::launder(reinterpret_cast<const T_VALUE*>(&Iterator.m_Data));
            }

            template<typename T_ITERATOR, typename T_CALLBACK>
            [[nodiscard]] xproperty::result<void> TryInitializeIterator(
                void* Object,
                settings::context& Context,
                const list_table& Table,
                std::optional<T_ITERATOR>& Out,
                T_CALLBACK Callback) noexcept
            {
                Out.reset();
                if (!Object) return xproperty::details::makeNullObject();
                if (!Callback) return xproperty::details::makeUnsupportedOperation();

                Out.emplace(Object, Table, Context, uninitialized_iterator);
                auto Construction = Callback(Object, *Out, Context);
                if (!Construction)
                {
                    Out.reset();
                    return Construction.getError();
                }

                Out->markInitialized();
                return {};
            }
        }

        using begin_iterator = details::begin_iterator<int>;
        using end_iterator   = details::end_iterator<int>;

        class begin_iterator_holder
        {
        public:
            begin_iterator_holder() = default;
            begin_iterator_holder(const begin_iterator_holder&) = delete;
            begin_iterator_holder& operator=(const begin_iterator_holder&) = delete;
            begin_iterator_holder(begin_iterator_holder&&) = delete;
            begin_iterator_holder& operator=(begin_iterator_holder&&) = delete;

            [[nodiscard]] bool hasValue() const noexcept { return m_Value.has_value(); }
            [[nodiscard]] begin_iterator& value() noexcept { assert(m_Value); return *m_Value; }
            [[nodiscard]] const begin_iterator& value() const noexcept { assert(m_Value); return *m_Value; }
            void reset() noexcept { m_Value.reset(); }

        private:
            friend struct list_table;
            std::optional<begin_iterator> m_Value;
        };

        class end_iterator_holder
        {
        public:
            end_iterator_holder() = default;
            end_iterator_holder(const end_iterator_holder&) = delete;
            end_iterator_holder& operator=(const end_iterator_holder&) = delete;
            end_iterator_holder(end_iterator_holder&&) = delete;
            end_iterator_holder& operator=(end_iterator_holder&&) = delete;

            [[nodiscard]] bool hasValue() const noexcept { return m_Value.has_value(); }
            [[nodiscard]] end_iterator& value() noexcept { assert(m_Value); return *m_Value; }
            [[nodiscard]] const end_iterator& value() const noexcept { assert(m_Value); return *m_Value; }
            void reset() noexcept { m_Value.reset(); }

        private:
            friend struct list_table;
            std::optional<end_iterator> m_Value;
        };

        struct any
        {
            template<typename T>
            [[nodiscard]] constexpr T& storageAs() noexcept
            {
                static_assert(sizeof(T) <= sizeof(settings::data_memory),
                    "XPROP031: atomic value exceeds settings::data_memory size");
                static_assert(alignof(T) <= alignof(settings::data_memory),
                    "XPROP032: atomic value alignment exceeds settings::data_memory alignment");
                return *std::launder(reinterpret_cast<T*>(&m_Data));
            }

            template<typename T>
            [[nodiscard]] constexpr const T& storageAs() const noexcept
            {
                static_assert(sizeof(T) <= sizeof(settings::data_memory),
                    "XPROP031: atomic value exceeds settings::data_memory size");
                static_assert(alignof(T) <= alignof(settings::data_memory),
                    "XPROP032: atomic value alignment exceeds settings::data_memory alignment");
                return *std::launder(reinterpret_cast<const T*>(&m_Data));
            }

            constexpr void destroyValue() noexcept
            {
                if (m_pType) m_pType->m_pDestruct(m_Data);
            }

            // Used only for construction into fresh/empty storage (the copy constructor) - there is no
            // existing value that a throw could leave half-destroyed, so a plain throwing copy-construct
            // is fine here: it simply means *this never comes into being, standard C++ behavior.
            constexpr void copyValueFrom(const any& Source)
            {
                m_pType = Source.m_pType;
                if (m_pType) m_pType->m_pCopyConstruct(m_Data, Source.m_Data);
            }

            // Used for REPLACING an existing value (operator=(const any&)). Constructs the incoming
            // value into temporary erased storage before touching *this's current value, so a throwing
            // copy-construct (std::string on allocation failure, etc.) leaves *this completely untouched
            // - strong exception guarantee - instead of destroyed with nothing to replace it. The
            // move-construct/destruct of the temporary are both guaranteed nothrow (enforced by
            // atomic_v<T>'s static_asserts), so nothing past the copy-construct line can throw.
            constexpr void copyAssignFrom(const any& Source)
            {
                if (!Source.m_pType)
                {
                    destroyValue();
                    m_pType = nullptr;
                    return;
                }

                settings::data_memory Temp;
                Source.m_pType->m_pCopyConstruct(Temp, Source.m_Data);   // may throw - *this still intact

                destroyValue();
                m_pType = Source.m_pType;
                m_pType->m_pMoveConstruct(m_Data, std::move(Temp));      // nothrow
                m_pType->m_pDestruct(Temp);                              // nothrow
            }

            constexpr void moveValueFrom(any& Source) noexcept
            {
                m_pType = Source.m_pType;
                if (m_pType)
                {
                    m_pType->m_pMoveConstruct(m_Data, std::move(Source.m_Data));
                    Source.clear();
                }
            }

            [[nodiscard]] constexpr bool hasValue() const noexcept { return m_pType != nullptr; }
            [[nodiscard]] constexpr const atomic* getType() const noexcept { return m_pType; }

            template<typename T>
            [[nodiscard]] constexpr bool is() const noexcept
            {
                // Compares by GUID value, not by &atomic_v<T> address: atomic_v<T> is a per-binary
                // `inline` singleton (header-only, implicitly instantiated), so a plugin DLL's copy and
                // the host EXE's copy of atomic_v<T> live at different addresses even though both are
                // valid pointers in the one shared process. m_GUID is a compile-time string hash with no
                // address dependency, so it's identical across binaries for the same type - see
                // xnode_os_reflection_boundary_research notes for the full cross-DLL crash this fixes.
                return m_pType && m_pType->m_GUID == atomic_v<T>.m_GUID;
            }

            template<typename T>
            [[nodiscard]] constexpr xproperty::result<T*> tryGet() noexcept
            {
                if (!m_pType) return xproperty::details::makeTypeMismatch(atomic_v<T>.m_GUID);
                if (m_pType->m_GUID != atomic_v<T>.m_GUID) return xproperty::details::makeTypeMismatch(atomic_v<T>.m_GUID, m_pType->m_GUID); // GUID compare, not pointer - see is<T>()'s comment
                return &storageAs<T>();
            }

            template<typename T>
            [[nodiscard]] constexpr xproperty::result<const T*> tryGet() const noexcept
            {
                if (!m_pType) return xproperty::details::makeTypeMismatch(atomic_v<T>.m_GUID);
                if (m_pType->m_GUID != atomic_v<T>.m_GUID) return xproperty::details::makeTypeMismatch(atomic_v<T>.m_GUID, m_pType->m_GUID); // GUID compare, not pointer - see is<T>()'s comment
                return &storageAs<T>();
            }

            constexpr std::uint32_t getTypeGuid() const noexcept
            {
                return m_pType ? m_pType->m_GUID : 0;
            }

            template<typename T>
            constexpr T& Reset() noexcept
            {
                static_assert(xproperty::details::has_const_v<T> == false);
                static_assert( std::is_enum_v<T> || settings::var_type<T>::guid_v != 0 );
                destroyValue();

                m_pType = &atomic_v<T>;
                m_pType->m_pVoidConstruct(m_Data);

                return storageAs<T>();
            }

            constexpr void clear()
            {
                destroyValue();
                m_pType = nullptr;
            }

            template<typename T>
            constexpr T& set( T&& Data ) noexcept
            {
                static_assert(xproperty::details::has_const_v<T> == false);
                static_assert(std::is_enum_v<T> || settings::var_type<T>::guid_v != 0);
                destroyValue();

                m_pType = &atomic_v<T>;
                m_pType->m_pMoveConstruct(m_Data, std::forward<settings::data_memory&&>(reinterpret_cast<settings::data_memory&&>(Data)) );

                return storageAs<T>();
            }

            // Not noexcept: settings::var_type<T>::CopyConstruct can throw for an arbitrary registered
            // T (std::string on allocation failure, etc.). Same transactional shape as copyAssignFrom -
            // construct into temporary storage before touching *this's current value, so a throw leaves
            // *this untouched instead of destroyed-with-nothing-to-replace-it.
            template<typename T>
            constexpr T& set(T& Data)
            {
                static_assert(xproperty::details::has_const_v<T> == false);
                static_assert(std::is_enum_v<T> || settings::var_type<T>::guid_v != 0);

                settings::data_memory Temp;
                settings::var_type<T>::CopyConstruct(Temp, Data);   // may throw - *this still intact

                destroyValue();
                m_pType = &atomic_v<T>;
                settings::var_type<T>::MoveConstruct(m_Data, std::move(reinterpret_cast<T&>(Temp)));  // nothrow
                settings::var_type<T>::Destruct(Temp);                                                // nothrow

                return storageAs<T>();
            }

            template<typename T>
            constexpr T& set(const T& Data)
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
                assert(m_pType->m_GUID == atomic_v<T>.m_GUID); // GUID compare, not pointer - see any::is<T>()'s comment
                return storageAs<T>();
            }

            template< typename T >
            constexpr T getCastValue(void) const noexcept
            {
                switch (m_pType->m_Size)
                {
                case 1: return static_cast<T>(storageAs<std::uint8_t>());
                case 2: return static_cast<T>(storageAs<std::uint16_t>());
                case 4: return static_cast<T>(storageAs<std::uint32_t>());
                case 8: return static_cast<T>(storageAs<std::uint64_t>());
                default: assert(false); return 0;
                }
            }

            constexpr std::uint32_t getEnumValue(void) const noexcept
            {
                assert(isEnum());
                return getCastValue<std::uint32_t>();
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
                assert(m_pType->m_GUID == atomic_v<T>.m_GUID); // GUID compare, not pointer - see any::is<T>()'s comment
                return storageAs<T>();
            }

            constexpr bool isEnum(void) const noexcept
            {
                return m_pType && m_pType->m_IsEnum;
            }

            constexpr ~any() noexcept
            {
                clear();
            }

            constexpr any() = default;
            constexpr any(any&& Any) noexcept
            {
                moveValueFrom(Any);
            }

            // Not noexcept: constructing into fresh storage, so a throwing copy-construct (std::string
            // on allocation failure, etc.) simply means *this never comes into being - standard C++
            // behavior, nothing here needs to be undone.
            constexpr any(const any& Any)
            {
                copyValueFrom(Any);
            }

            template< typename T>
            requires (xproperty::settings::var_type<T>::guid_v != 0)
            constexpr any(const T& Data)
            {
                m_pType = &atomic_v<T>;
                m_pType->m_pCopyConstruct(m_Data, reinterpret_cast<const settings::data_memory&>(Data) );
            }

            template< typename T >
            requires (xproperty::settings::var_type<T>::guid_v != 0)
            constexpr any( T&& Data ) noexcept
            {
                m_pType = &atomic_v<T>;
                m_pType->m_pMoveConstruct( m_Data, reinterpret_cast<settings::data_memory&&>(Data) );
            }

            // Not noexcept: see copyAssignFrom's comment. Strong exception guarantee - if the source's
            // copy-construct throws, *this is left exactly as it was before the call.
            constexpr any& operator=(const any& Any)
            {
                if (this == &Any) return *this;
                copyAssignFrom(Any);
                return *this;
            }

            constexpr any& operator=(any&& Any) noexcept
            {
                if (this == &Any) return *this;
                destroyValue();
                m_pType = nullptr;
                moveValueFrom(Any);
                return *this;
            }

            settings::data_memory       m_Data  = {};
            const atomic*               m_pType = nullptr;
        };

        struct member_constructor
        {
            using fn = void*(void* pArgs);

            fn*                                      m_pCallConstructor = {};
            std::span< const xproperty::basic_type > m_ArgumentList     = {};
        };

        using reference_fn = std::tuple<void*, const object*>(void* pClassInstance);

        struct list_table
        {
            using get_size_fn               = std::size_t (void* pClass, settings::context&);
            using set_size_fn               = void ( void*           pClass,       std::size_t,                                         settings::context&);
            using start_fn                  = xproperty::result<void>(void* pClass, begin_iterator& Iterator, settings::context&);
            using end_fn                    = xproperty::result<void>(void* pClass, end_iterator& End, settings::context&);
            using next_fn                   = bool ( void*           pClass,       begin_iterator&  Iterator, const end_iterator& End,  settings::context&);
            using iterator_to_key_fn        = bool ( void*           pClass, const begin_iterator&  Iterator, any&                Key,  settings::context&);
            using iterator_to_object_fn     = void*( void*           pClass,       begin_iterator&  Iterator,                           settings::context&);
            using get_object_fn             = void*( void*           pClass, const any&             Key,                                settings::context&);
            using destroy_begin_iterator_fn = void ( begin_iterator& Begin,                                                             settings::context&);
            using destroy_end_iterator_fn   = void ( end_iterator&   End,                                                               settings::context&);

            get_size_fn*                const m_pGetSize;
            set_size_fn*                const m_pSetSize;
            const bool                        m_bHasRealSetSize; // false for fixed-size adapters (span/array/native C-arrays) whose setSize is just var_list_defaults's inherited no-op - m_pSetSize itself is always populated, TrySetSize consults this instead
            start_fn*                   const m_pStart;
            end_fn*                     const m_pEnd;
            next_fn*                    const m_pNext;
            iterator_to_key_fn*         const m_pIteratorToKey;
            iterator_to_object_fn*      const m_pIteratorToObject;
            get_object_fn*              const m_pGetObject;
            destroy_begin_iterator_fn*  const m_pDestroyBeginIterator;
            destroy_end_iterator_fn*    const m_pDestroyEndIterator;
            const atomic&                     m_KeyAtomicType;

            [[nodiscard]] xproperty::result<void> TryBegin(
                void* Object, settings::context& Context, begin_iterator_holder& Out) const
            {
                Out.reset();
                if (!m_pDestroyBeginIterator)
                    return Object ? xproperty::details::makeUnsupportedOperation()
                                  : xproperty::details::makeNullObject();
                return details::TryInitializeIterator(Object, Context, *this, Out.m_Value, m_pStart);
            }

            [[nodiscard]] xproperty::result<void> TryEnd(
                void* Object, settings::context& Context, end_iterator_holder& Out) const
            {
                Out.reset();
                if (!m_pDestroyEndIterator)
                    return Object ? xproperty::details::makeUnsupportedOperation()
                                  : xproperty::details::makeNullObject();
                return details::TryInitializeIterator(Object, Context, *this, Out.m_Value, m_pEnd);
            }

            [[nodiscard]] xproperty::result<std::size_t> TryGetSize(void* Object, settings::context& Context) const
            {
                if (!Object) return xproperty::details::makeNullObject();
                if (!m_pGetSize) return xproperty::details::makeUnsupportedOperation();
                return m_pGetSize(Object, Context);
            }

            [[nodiscard]] xproperty::result<void> TrySetSize(void* Object, std::size_t Size, settings::context& Context) const
            {
                if (!Object) return xproperty::details::makeNullObject();
                if (!m_pSetSize || !m_bHasRealSetSize) return xproperty::details::makeUnsupportedOperation();
                m_pSetSize(Object, Size, Context);
                return {};
            }

            [[nodiscard]] xproperty::result<void*> TryGetObject(void* Object, const any& Key, settings::context& Context) const
            {
                if (!Object) return xproperty::details::makeNullObject();
                if (!m_pGetObject) return xproperty::details::makeUnsupportedOperation();
                if (!Key.hasValue()) return xproperty::details::makeInvalidKey(m_KeyAtomicType.m_GUID);
                if (Key.getTypeGuid() != m_KeyAtomicType.m_GUID)
                    return xproperty::details::makeTypeMismatch(m_KeyAtomicType.m_GUID, Key.getTypeGuid());
                if (void* Value = m_pGetObject(Object, Key, Context)) return Value;
                return xproperty::details::makeInvalidKey(m_KeyAtomicType.m_GUID, Key.getTypeGuid());
            }
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
                inline constexpr static std::uint32_t invalid_index_v = std::numeric_limits<std::uint32_t>::max();

                const std::span<const members>          m_Members;
                const std::span<const std::uint32_t>    m_Lookup;

                [[nodiscard]] inline const members* findMember( std::uint32_t GUID ) const noexcept
                {
                    if (GUID == 0 || m_Lookup.empty()) return nullptr;

                    const std::size_t Mask = m_Lookup.size() - 1;
                    std::size_t Bucket = GUID & Mask;
                    for (std::size_t Probe = 0; Probe < m_Lookup.size(); ++Probe)
                    {
                        const std::uint32_t Index = m_Lookup[Bucket];
                        if (Index == invalid_index_v) return nullptr;
                        assert(Index < m_Members.size());
                        if (m_Members[Index].m_GUID == GUID) return &m_Members[Index];
                        Bucket = (Bucket + 1) & Mask;
                    }
                    return nullptr;
                }

                [[nodiscard]] inline xproperty::result<const members*> requireMember(std::uint32_t GUID) const noexcept
                {
                    if (const auto* Member = findMember(GUID)) return Member;
                    return xproperty::error{ .m_Code = error_code::member_not_found };
                }
            };

            struct var
            {
                using read_fn           = xproperty::result<void>( const void* pClass,       any& DataOut, const std::span<const atomic::enum_item>& S, settings::context& );
                using write_fn          = xproperty::result<void>(       void* pClass, const any& DataIn,  const std::span<const atomic::enum_item>& S, settings::context& );

                read_fn*  const                          m_pReadUnchecked;
                write_fn* const                          m_pWriteUnchecked;
                const atomic&                            m_AtomicType;
                const std::span<const atomic::enum_item> m_UnregisteredEnumSpan;

                [[nodiscard]] xproperty::result<void> TryRead(const void* Object, any& Out, settings::context& Context) const
                {
                    if (!Object) return xproperty::details::makeNullObject();
                    if (!m_pReadUnchecked) return xproperty::details::makeUnsupportedOperation();
                    return m_pReadUnchecked(Object, Out, m_UnregisteredEnumSpan, Context);
                }

                template<typename T_STRING = std::string>
                [[nodiscard]] xproperty::result<void> TryWrite(void* Object, const any& In, settings::context& Context) const
                {
                    if (!Object) return xproperty::details::makeNullObject();
                    if (!m_pWriteUnchecked) return xproperty::details::makeReadOnly(m_AtomicType.m_GUID, In.getTypeGuid());
                    if (!In.hasValue()) return xproperty::details::makeTypeMismatch(m_AtomicType.m_GUID);

                    if (In.getTypeGuid() != m_AtomicType.m_GUID)
                    {
                        if (!(m_AtomicType.m_IsEnum && In.getTypeGuid() == xproperty::settings::var_type<T_STRING>::guid_v))
                            return xproperty::details::makeTypeMismatch(m_AtomicType.m_GUID, In.getTypeGuid());

                        const auto* String = In.tryGet<T_STRING>().value();
                        const auto Found = m_AtomicType.TryFindEnumByName(*String, m_UnregisteredEnumSpan);
                        if (!Found) return xproperty::error{ .m_Code = error_code::invalid_enum_value, .m_ExpectedType = m_AtomicType.m_GUID, .m_ActualType = In.getTypeGuid() };
                    }

                    return m_pWriteUnchecked(Object, In, m_UnregisteredEnumSpan, Context);
                }

                XPROPERTY_LEGACY_API("Use TryRead instead")
                void Read(const void* Object, any& Out, settings::context& Context) const
                {
                    const auto Result = TryRead(Object, Out, Context);
                    assert(Result);
                }

                template<typename T_STRING = std::string>
                XPROPERTY_LEGACY_API("Use TryWrite instead")
                void Write(void* Object, const any& In, settings::context& Context) const
                {
                    const auto Result = TryWrite<T_STRING>(Object, In, Context);
                    assert(Result);
                }
            };

            struct list_var
            {
                using read_fn               = xproperty::result<void>( const void* pClass,       any& DataOut, const std::span<const atomic::enum_item>& S, settings::context&);
                using write_fn              = xproperty::result<void>(       void* pClass, const any& DataIn,  const std::span<const atomic::enum_item>& S, settings::context&);

                read_fn*  const                          m_pReadUnchecked;
                write_fn* const                          m_pWriteUnchecked;
                const std::span<const list_table>        m_Table;
                const atomic&                            m_AtomicType;
                const std::span<const atomic::enum_item> m_UnregisteredEnumSpan;

                [[nodiscard]] xproperty::result<std::size_t> TryGetSize(void* Object, std::size_t Dimension, settings::context& Context) const
                {
                    if (Dimension >= m_Table.size()) return xproperty::error{ .m_Code = error_code::invalid_index };
                    return m_Table[Dimension].TryGetSize(Object, Context);
                }

                [[nodiscard]] xproperty::result<void> TrySetSize(void* Object, std::size_t Dimension, std::size_t Size, settings::context& Context) const
                {
                    if (Dimension >= m_Table.size()) return xproperty::error{ .m_Code = error_code::invalid_index };
                    return m_Table[Dimension].TrySetSize(Object, Size, Context);
                }

                [[nodiscard]] xproperty::result<void*> TryGetObject(void* Object, std::size_t Dimension, const any& Key, settings::context& Context) const
                {
                    if (Dimension >= m_Table.size()) return xproperty::error{ .m_Code = error_code::invalid_index };
                    return m_Table[Dimension].TryGetObject(Object, Key, Context);
                }
            };

            struct list_props : props
            {
                const std::span<const list_table>        m_Table;

                [[nodiscard]] xproperty::result<std::size_t> TryGetSize(void* Object, std::size_t Dimension, settings::context& Context) const
                {
                    if (Dimension >= m_Table.size()) return xproperty::error{ .m_Code = error_code::invalid_index };
                    return m_Table[Dimension].TryGetSize(Object, Context);
                }

                [[nodiscard]] xproperty::result<void> TrySetSize(void* Object, std::size_t Dimension, std::size_t Size, settings::context& Context) const
                {
                    if (Dimension >= m_Table.size()) return xproperty::error{ .m_Code = error_code::invalid_index };
                    return m_Table[Dimension].TrySetSize(Object, Size, Context);
                }

                [[nodiscard]] xproperty::result<void*> TryGetObject(void* Object, std::size_t Dimension, const any& Key, settings::context& Context) const
                {
                    if (Dimension >= m_Table.size()) return xproperty::error{ .m_Code = error_code::invalid_index };
                    return m_Table[Dimension].TryGetObject(Object, Key, Context);
                }
            };

            struct function
            {
                using fn = void(void* pClass, void* pArgs) noexcept;

                fn* const                                      m_pCallFunction;
                const std::span< const xproperty::basic_type>  m_ArgumentList;

                template<typename T, typename...TARGS >
                [[nodiscard]] constexpr xproperty::result<void> TryCallFunction(T& Class, TARGS&&...Args) const noexcept
                {
                    using col = std::tuple<std::decay_t<TARGS>...>;
                    if (m_ArgumentList.size() != sizeof...(TARGS))
                        return xproperty::details::makeTypeMismatch();
                    const bool Compatible = xproperty::details::tuple_types_match<col>(m_ArgumentList);
                    if (!Compatible) return xproperty::details::makeTypeMismatch();

                    col Arguments{ std::forward<TARGS>(Args)... };
                    m_pCallFunction(&Class, &Arguments);
                    return {};
                }

                template<typename T, typename...TARGS >
                XPROPERTY_LEGACY_API("Use TryCallFunction instead")
                constexpr void CallFunction(T& Class, TARGS&&...Args) const noexcept
                {
                    const auto Result = TryCallFunction(Class, std::forward<TARGS>(Args)...);
                    assert(Result);
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

            [[nodiscard]] xproperty::result<void> TryRead(const void* Object, any& Out, settings::context& Context) const
            {
                if (const auto* Value = std::get_if<var>(&m_Variant)) return Value->TryRead(Object, Out, Context);
                if (const auto* Value = std::get_if<list_var>(&m_Variant))
                {
                    if (!Object) return xproperty::details::makeNullObject();
                    if (!Value->m_pReadUnchecked) return xproperty::details::makeUnsupportedOperation();
                    return Value->m_pReadUnchecked(Object, Out, Value->m_UnregisteredEnumSpan, Context);
                }
                return xproperty::details::makeUnsupportedOperation();
            }

            template<typename T_STRING = std::string>
            [[nodiscard]] xproperty::result<void> TryWrite(void* Object, const any& In, settings::context& Context) const
            {
                if (const auto* Value = std::get_if<var>(&m_Variant)) return Value->TryWrite<T_STRING>(Object, In, Context);
                if (const auto* Value = std::get_if<list_var>(&m_Variant))
                {
                    if (!Object) return xproperty::details::makeNullObject();
                    if (!Value->m_pWriteUnchecked) return xproperty::details::makeReadOnly(Value->m_AtomicType.m_GUID, In.getTypeGuid());
                    if (!In.hasValue()) return xproperty::details::makeTypeMismatch(Value->m_AtomicType.m_GUID);
                    if (In.getTypeGuid() != Value->m_AtomicType.m_GUID)
                    {
                        if (!(Value->m_AtomicType.m_IsEnum && In.getTypeGuid() == xproperty::settings::var_type<T_STRING>::guid_v))
                            return xproperty::details::makeTypeMismatch(Value->m_AtomicType.m_GUID, In.getTypeGuid());

                        const auto* String = In.tryGet<T_STRING>().value();
                        const auto Found = Value->m_AtomicType.TryFindEnumByName(*String, Value->m_UnregisteredEnumSpan);
                        if (!Found) return xproperty::error{ .m_Code = error_code::invalid_enum_value, .m_ExpectedType = Value->m_AtomicType.m_GUID, .m_ActualType = In.getTypeGuid() };
                    }
                    return Value->m_pWriteUnchecked(Object, In, Value->m_UnregisteredEnumSpan, Context);
                }
                return xproperty::details::makeUnsupportedOperation();
            }
        };

        struct base : members::props
        {
            const bool          m_bConst;
        };

        struct object : members::scope
        {
            using fn_destroy_instance   = void( void* );

            struct deleter
            {
                constexpr deleter() noexcept = default;
                constexpr deleter(const deleter& X) noexcept : m_pDestroyInstance {X.m_pDestroyInstance}{ }
                constexpr deleter( fn_destroy_instance* p) noexcept : m_pDestroyInstance{ p }{}
                void operator()( void* p) const noexcept
                {
                    if(m_pDestroyInstance) m_pDestroyInstance(p);
                }

                fn_destroy_instance* m_pDestroyInstance{};
            };

            const char* const                           m_pName;
            std::uint32_t                               m_GUID;
            std::uint32_t                               m_GroupGUID;
            fn_destroy_instance* const                  m_pDestroyInstance;
            const std::span<const member_constructor>   m_Constructors;
            const std::span<const base>                 m_BaseList;

            template< typename...T_ARGS>
            [[nodiscard]] xproperty::result<std::unique_ptr<void, deleter>> TryCreateInstance(T_ARGS&&... Args) const
            {
                using col = std::tuple<std::decay_t<T_ARGS>...>;
                for (auto& E : m_Constructors)
                {
                    bool Compatible = sizeof...(T_ARGS) == E.m_ArgumentList.size();
                    if (Compatible)
                    {
                        Compatible = xproperty::details::tuple_types_match<col>(E.m_ArgumentList);
                    }
                    if (Compatible)
                    {
                        col Arguments{ std::forward<T_ARGS>(Args)... };
                        return std::unique_ptr<void, deleter>{ E.m_pCallConstructor(&Arguments), deleter{m_pDestroyInstance} };
                    }
                }
                return xproperty::error{ .m_Code = error_code::constructor_not_found };
            }

            template< typename...T_ARGS>
            XPROPERTY_LEGACY_API("Use TryCreateInstance instead")
            std::unique_ptr<void, deleter> CreateInstance(T_ARGS&&... Args) const
            {
                auto Result = TryCreateInstance(std::forward<T_ARGS>(Args)...);
                if (!Result) return { nullptr, {} };
                return std::move(Result).value();
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

    // Forward declaration only (defined further below) - needed here so validate_reflected_object_type()
    // can pattern-match against it to extract the type a wrapper struct actually registered itself
    // under, without waiting for the full definition.
    template< details::fixed_string T_OBJECT_NAME_V, typename T_OBJECT_TYPE, typename... T_ARGS >
    struct def;

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
        struct obj_group_tag;
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
        template<typename...>
        inline constexpr bool dependent_false_v = false;

        template< xproperty::details::fixed_string T_NAME_V, typename T_DATA, auto T_DATA_V, typename... T_ARGS >
        struct member
        {
            static_assert(dependent_false_v<T_DATA>,
                "XPROP002: unsupported property callback or member signature. "
                "Use a data-member pointer, member-function pointer, or a captureless callback "
                "matching one of the documented xproperty contracts");
        };

        namespace details
        {
            // Single, shared implementation of "the incoming value is a string naming one of this
            // enum's entries - resolve it to the real enum value" - used by every member category that
            // can receive an enum property write (plain data members, virtual/lambda properties, and
            // list elements). Previously this logic was copy-pasted independently into each of those
            // three call sites, which is exactly how the registered-enum TryWrite regression happened:
            // one copy got a fix, the other two didn't.
            //
            // `S` must always be the PROPERTY-LOCAL enum item span (the one passed down from the actual
            // member's own registration), never a cached/shared one - using anything else risks one
            // property resolving names that actually belong to a different property sharing the same
            // underlying enum type.
            template<typename T_ATOMIC>
            [[nodiscard]] inline xproperty::result<type::any> resolveEnumString(
                const type::any& Value, const std::span<const type::atomic::enum_item>& S) noexcept
            {
                static_assert(std::is_enum_v<T_ATOMIC>);

                for (auto& E : S)
                {
                    if (Value.get<std::string>() == E.m_pName)
                    {
                        type::any Resolved;
                        Resolved.set<T_ATOMIC>(static_cast<T_ATOMIC>(E.m_Value));
                        return Resolved;
                    }
                }
                return xproperty::error{ .m_Code = error_code::invalid_enum_value };
            }

            // True when Value holds a std::string rather than T_ATOMIC directly - i.e. when it needs to
            // go through resolveEnumString() before it can be used as T_ATOMIC.
            template<typename T_ATOMIC>
            [[nodiscard]] inline bool isUnresolvedEnumString(const type::any& Value) noexcept
            {
                return Value.m_pType->m_GUID == xproperty::details::delay_linkage< xproperty::settings::var_type<std::string>, T_ATOMIC>::type::guid_v;
            }

            // Best-effort echo of the property-local span into atomic_v<T_ATOMIC>.m_RegisteredEnumSpan,
            // purely so any-only callers with no property context (AnyToString, any::getEnumString(),
            // e.g. the ImGui inspector's undo-command display) have *something* to resolve an unregistered
            // enum's name against. First-writer-wins per T_ATOMIC (an unregistered enum has no real type
            // identity beyond the shared guid_v==1 sentinel, so there's no way to key this correctly for
            // more than one logical enum at a time) - this can occasionally show the wrong name for a
            // DIFFERENT unregistered enum than the one just read/written, but never affects the read/write
            // itself, which always resolves off the real property-local span passed in here, not this
            // cache.
            template<typename T_ATOMIC>
            inline void cacheEnumSpanBestEffort(const std::span<const type::atomic::enum_item>& S) noexcept
            {
                if constexpr (std::is_enum_v<T_ATOMIC> && type::var_t<T_ATOMIC>::guid_v == 1)
                {
                    auto& Cached = type::atomic_v<T_ATOMIC>.m_RegisteredEnumSpan;
                    if (Cached.size() == 0)
                        Cached = S;
                    else
                        assert(S.size() == Cached.size());
                }
            }

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
                static xproperty::result<void> Read ( const void* pClass, type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context )
                {
                    details::cacheEnumSpanBestEffort<atomic_t>(S);
                    auto& Member = const_cast<xproperty::details::remove_all_const_t<t&>>(T_LAMBDA_V(*static_cast<T_CLASS*>(const_cast<void*>(pClass))));
                    if constexpr (type::var_t<t>::is_pointer_v)
                    {
                        if (!type::var_t<t>::getAtomic(Member, Context)) return xproperty::details::makeNullObject();
                    }
                    type::var_t<t>::Read(Member, Any.Reset<atomic_t>(), Context);
                    return {};
                }

                static xproperty::result<void> Write(void* pClass, const type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context)
                {
                    details::cacheEnumSpanBestEffort<atomic_t>(S);
                    auto& Member = const_cast<xproperty::details::remove_all_const_t<t&>>(T_LAMBDA_V(*static_cast<T_CLASS*>(pClass)));
                    if constexpr (type::var_t<t>::is_pointer_v)
                    {
                        if (!type::var_t<t>::getAtomic(Member, Context)) return xproperty::details::makeNullObject();
                    }

                    if constexpr (std::is_enum_v<atomic_t>)
                    {
                        if (details::isUnresolvedEnumString<atomic_t>(Any))
                        {
                            auto Resolved = details::resolveEnumString<atomic_t>(Any, S);
                            if (!Resolved) return Resolved.getError();

                            type::var_t<t>::Write(Member, Resolved.value().template get<atomic_t>(), Context);
                            return {};
                        }
                    }

                    if (Any.m_pType->m_GUID != type::var_t<atomic_t>::guid_v)
                        return xproperty::details::makeTypeMismatch(type::var_t<atomic_t>::guid_v, Any.m_pType->m_GUID);

                    type::var_t<t>::Write(Member, Any.get<atomic_t>(), Context);
                    return {};
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
                        // var_t<t>::atomic_type - not bare t - is the fully-resolved key here: it unwraps
                        // however many pointer/smart-pointer layers t itself didn't (t only had ONE
                        // std::remove_pointer_t applied above, so a Base1** member still leaves t as
                        // Base1*, not Base1), and for non-pointer members (fvec3, type_guid, ...) it's the
                        // identity, matching how a wrapper struct's XPROPERTY_DEF always registers under
                        // the raw foreign type's name ("ImVec2", "vector3", ...), never the wrapper itself.
                        using key_t = typename type::var_t<t>::atomic_type;
                        assert(type::get_obj_info<key_t> != nullptr);
                        return { pInst, type::get_obj_info<key_t> };
                    }
                }
            };

            template<typename T>
            [[nodiscard]] constexpr T* objectAs(void* Object) noexcept
            {
                return static_cast<T*>(Object);
            }

            template<typename T>
            [[nodiscard]] constexpr const T* objectAs(const void* Object) noexcept
            {
                return static_cast<const T*>(Object);
            }

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
                             if constexpr (std::tuple_size_v<typename ft::args> == 1) pA = type::var_t<T_MEMBER_TYPE>::getAtomic( *objectAs<T_MEMBER_TYPE>(pClass)   );
                        else if constexpr (std::tuple_size_v<typename ft::args> == 2) pA = type::var_t<T_MEMBER_TYPE>::getAtomic( *objectAs<T_MEMBER_TYPE>(pClass), C);
                        else static_assert(always_false<ft>::value, "The getAtomic function for the given list type must have at least 1 parameters => atomic_type* getAtomic( type& MemberVar )");
                    }
                    else
                    {
                        pA = objectAs<atomic_t>(pClass);
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
                template<typename T_ADAPTER>
                [[nodiscard]] consteval bool hasRealSetSize() noexcept
                {
                    if constexpr (requires { T_ADAPTER::has_real_setSize_v; })
                        return static_cast<bool>(T_ADAPTER::has_real_setSize_v);
                    else
                        return false;
                }

                template<typename T_ADAPTER, typename T_CONTAINER>
                consteval void validate_list_adapter()
                {
                    using begin_iterator_t = typename T_ADAPTER::begin_iterator;
                    using end_iterator_t   = typename T_ADAPTER::end_iterator;
                    using key_t            = typename T_ADAPTER::atomic_key;
                    using container_t      = typename T_ADAPTER::type;

                    static_assert(requires(container_t& Container, settings::context& Context)
                    {
                        { T_ADAPTER::getSize(Container, Context) } -> std::convertible_to<std::size_t>;
                    }, "XPROP020: list adapter is missing getSize(container, context), or its result is not convertible to std::size_t");

                    static_assert(requires(container_t& Container, begin_iterator_t& Iterator, settings::context& Context)
                    {
                        { T_ADAPTER::Start(Container, Iterator, Context) } -> std::same_as<void>;
                    }, "XPROP021: list adapter Start(container, begin_iterator, context) contract is invalid");

                    static_assert(requires(container_t& Container, end_iterator_t& Iterator, settings::context& Context)
                    {
                        { T_ADAPTER::End(Container, Iterator, Context) } -> std::same_as<void>;
                    }, "XPROP022: list adapter End(container, end_iterator, context) contract is invalid");

                    static_assert(requires(begin_iterator_t& Begin, end_iterator_t& End, settings::context& Context)
                    {
                        { T_ADAPTER::DestroyBeginIterator(Begin, Context) } -> std::same_as<void>;
                        { T_ADAPTER::DestroyEndIterator(End, Context) } -> std::same_as<void>;
                    }, "XPROP023: list adapter iterator destruction contract is invalid");

                    static_assert(type::var_t<key_t>::guid_v != 1,
                        "XPROP024: list adapter key type is not registered as an atomic type");

                    static_assert(requires(container_t& Container, begin_iterator_t& Begin, const end_iterator_t& End, settings::context& Context)
                    {
                        { T_ADAPTER::Next(Container, Begin, End, Context) } -> std::convertible_to<bool>;
                    }, "XPROP025: list adapter Next(container, begin, end, context) contract is invalid");

                    static_assert(requires(container_t& Container, const typename T_ADAPTER::any_t& Key, settings::context& Context)
                    {
                        T_ADAPTER::getObject(Container, Key, Context);
                    }, "XPROP026: list adapter getObject(container, any-key, context) contract is invalid");

                    static_assert(std::same_as<std::remove_cv_t<container_t>, std::remove_cv_t<T_CONTAINER>>,
                        "XPROP034: list adapter container type does not match the reflected member type");
                }

                template< typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename... T_ARGS >
                consteval static type::list_table getListTable()
                {
                    using t                 = type::var_t<T_MEMBER_TYPE>;
                    using type              = typename t::type;
//                    using specializing_t    = typename t::specializing_t;
//                    using atomic_type       = typename t::atomic_type;
                    using begin_iterator_t  = typename t::begin_iterator;
                    using end_iterator_t    = typename t::end_iterator;

                    validate_list_adapter<t, T_MEMBER_TYPE>();

                    static_assert(sizeof(begin_iterator_t) <= sizeof(settings::iterator_memory),
                        "XPROP011: begin iterator does not fit in settings::iterator_memory");
                    static_assert(sizeof(end_iterator_t) <= sizeof(settings::iterator_memory),
                        "XPROP011: end iterator does not fit in settings::iterator_memory");
                    static_assert(alignof(begin_iterator_t) <= alignof(settings::iterator_memory),
                        "XPROP033: begin iterator alignment exceeds settings::iterator_memory alignment");
                    static_assert(alignof(end_iterator_t) <= alignof(settings::iterator_memory),
                        "XPROP033: end iterator alignment exceeds settings::iterator_memory alignment");

                    using  overwrite_list_size_tuple_t = xproperty::details::filter_by_tag_t< meta::member_overwrite_list_size_tag, T_ARGS... >;

                    // TrySetSize must honestly report "unsupported" for adapters that never provide real
                    // resize support - fixed-size containers (std::span, std::array, native C-arrays) just
                    // inherit var_list_defaults's own do-nothing setSize - instead of silently succeeding.
                    // A property-local overwrite_list_size callback always counts as real; otherwise defer
                    // to the adapter's own has_real_setSize_v flag. m_pSetSize itself stays unconditionally
                    // populated (unchanged) - only the separate m_bHasRealSetSize flag varies, so
                    // TrySetSize's null-object/no-op branches stay exactly as narrow as before.
                    constexpr bool has_setSize_override_v =
                           !std::is_same_v<std::tuple<>, overwrite_list_size_tuple_t >
                        || hasRealSetSize<t>();

                    return
                    { .m_pGetSize = [](void* pClass, settings::context& C) constexpr -> std::size_t
                        {
                            if constexpr( std::is_same_v<std::tuple<>, overwrite_list_size_tuple_t > )
                            {
                                T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                                if (pA == nullptr) return 0;

                                return t::getSize(*pA, C);
                            }
                            else
                            {
                                using callback = std::tuple_element_t< 0, overwrite_list_size_tuple_t >;
                                std::size_t Size;

                                using fn_t = xproperty::details::function_traits<callback>;
                                if constexpr (fn_t::arity_v == 3) callback::overwrite_size_v(*static_cast<T_CLASS*>(pClass), true, Size);
                                else                              callback::overwrite_size_v(*static_cast<T_CLASS*>(pClass), true, Size, C);

                                return Size;
                            }
                        }
                    , .m_pSetSize = [](void* pClass, std::size_t Size, settings::context& C) constexpr
                        {
                            if constexpr (std::is_same_v<std::tuple<>, overwrite_list_size_tuple_t >)
                            {
                                T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                                if (pA == nullptr) return;

                                t::setSize(const_cast<type&>(*pA), Size, C);
                            }
                            else
                            {
                                using callback = std::tuple_element_t< 0, overwrite_list_size_tuple_t >;

                                using fn_t = xproperty::details::function_traits<callback>;
                                if constexpr (fn_t::arity_v == 3) callback::overwrite_size_v(*static_cast<T_CLASS*>(pClass), false, Size);
                                else                              callback::overwrite_size_v(*static_cast<T_CLASS*>(pClass), false, Size, C);
                            }
                        }
                    , .m_bHasRealSetSize = has_setSize_override_v
                    , .m_pStart = [](void* pClass, xproperty::type::begin_iterator& Iterator, settings::context& C) constexpr -> xproperty::result<void>
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if (pA == nullptr) return xproperty::details::makeNullObject();
                            t::Start(const_cast<type&>(*pA), xproperty::type::details::iteratorStorageAs<begin_iterator_t>(Iterator), C);
                            return {};
                        }
                    , .m_pEnd = [](void* pClass, xproperty::type::end_iterator& Iterator, settings::context& C) constexpr -> xproperty::result<void>
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if (pA == nullptr) return xproperty::details::makeNullObject();
                            t::End(const_cast<type&>(*pA), xproperty::type::details::iteratorStorageAs<end_iterator_t>(Iterator), C);
                            return {};
                        }
                    , .m_pNext = [](void* pClass, xproperty::type::begin_iterator& Iterator, const xproperty::type::end_iterator& End, settings::context& C) constexpr ->bool
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if (pA == nullptr) return false;

                            return t::Next(*pA, xproperty::type::details::iteratorStorageAs<begin_iterator_t>(Iterator), xproperty::type::details::iteratorStorageAs<end_iterator_t>(End), C);
                        }
                    , .m_pIteratorToKey = [](void* pClass, const xproperty::type::begin_iterator& Iterator, xproperty::type::any& Key, settings::context& C) constexpr -> bool
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if (pA == nullptr) return false;

                            t::IteratorToKey(*pA, Key, xproperty::type::details::iteratorStorageAs<begin_iterator_t>(Iterator), C);

                            return true;
                        }
                    , .m_pIteratorToObject = [](void* pClass, xproperty::type::begin_iterator& I, settings::context& C) constexpr -> void*
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass, C);
                            if (pA == nullptr) return nullptr;

                            return t::IteratorToObject(const_cast<type&>(*pA), xproperty::type::details::iteratorStorageAs<begin_iterator_t>(I), C );
                        }
                    , .m_pGetObject = [](void* pClass, const xproperty::type::any& Key, settings::context& C ) constexpr -> void*
                        {
                            T_MEMBER_TYPE* pA = details::get_member<T_LAMBDA_V, T_CLASS>::get(pClass,C);
                            if(pA==nullptr) return nullptr;

                            return t::getObject(const_cast<type&>(*pA), Key, C );
                        }
                    , .m_pDestroyBeginIterator = [](xproperty::type::begin_iterator& Iterator, settings::context& C) constexpr
                        {
                            t::DestroyBeginIterator(xproperty::type::details::iteratorStorageAs<begin_iterator_t>(Iterator), C);
                        }
                    , .m_pDestroyEndIterator = [](xproperty::type::end_iterator& Iterator, settings::context& C) constexpr
                        {
                            t::DestroyEndIterator(xproperty::type::details::iteratorStorageAs<end_iterator_t>(Iterator), C);
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

            template< typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename... T_ARGS >
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
                    if constexpr( std::is_same_v< tuple_dimensions, std::tuple<> > ) return std::array{ details::getListTable<T_CLASS,T_MEMBER_TYPE,T_LAMBDA_V, T_ARGS...>() };
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

            template<typename...T>
            consteval bool unique_user_data_guids(std::tuple<T...>*)
            {
                constexpr std::array<std::uint32_t, sizeof...(T)> guids{ T::type_guid_v... };
                for (std::size_t i = 0; i < guids.size(); ++i)
                    for (std::size_t j = i + 1; j < guids.size(); ++j)
                        if (guids[i] == guids[j]) return false;
                return true;
            }

            template< typename T_USER_DATA >
            const void* GetUserData(std::uint32_t GUID)
            {
                static_assert(unique_user_data_guids(static_cast<T_USER_DATA*>(nullptr)),
                    "XPROP007: duplicate user-data GUIDs are attached to the same property. "
                    "Remove the duplicate metadata or assign distinct member_user_data GUIDs");

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
        namespace details
        {
            template<xproperty::details::fixed_string T_NAME_V, typename T_VARIANT, typename T_USER_DATA>
            [[nodiscard]] consteval xproperty::type::members make_member_descriptor(
                T_VARIANT Variant,
                bool IsConst)
            {
                return
                { .m_GUID = xproperty::settings::strguid(T_NAME_V)
                , .m_pName = T_NAME_V
                , .m_Variant = Variant
                , .m_bConst = IsConst
                , .m_pGetUserData = GetUserData<T_USER_DATA>
                };
            }
        }

        // Helper to enable and disable our different types
        //
        template<typename T>
        concept reflected_object_type = requires
        {
            { std::remove_cvref_t<settings::reflected_type_t<T>>::PropertiesDefinition() };
        };

        // Extracts the T_OBJECT_TYPE a wrapper's PropertiesDefinition() actually registered itself
        // under - i.e. XPROPERTY_DEF's own second argument - so it can be compared against the raw
        // type reflected_type<T> was specialized for. No primary definition: only matches an actual
        // xproperty::def<...>, which is exactly what a well-formed PropertiesDefinition() returns.
        template<typename T> struct def_registered_type;
        template<xproperty::details::fixed_string T_NAME_V, typename T_OBJECT_TYPE, typename... T_ARGS>
        struct def_registered_type<xproperty::def<T_NAME_V, T_OBJECT_TYPE, T_ARGS...>> { using type = T_OBJECT_TYPE; };

        template<typename T>
        consteval void validate_reflected_object_type()
        {
            static_assert(reflected_object_type<T>,
                "XPROP004: property type is not a registered atomic and has no xproperty definition. "
                "Register it with var_type<T> for an atomic value, add XPROPERTY_DEF/XPROPERTY_VDEF and "
                "XPROPERTY_REG for a reflected object, or - for a foreign type you can't add "
                "XPROPERTY_DEF to directly - specialize xproperty::settings::reflected_type<T> to point "
                "at a sibling wrapper struct that has one");

            // If T itself needed the reflected_type<T> redirection (T has no PropertiesDefinition() of
            // its own - it's a foreign type like xmath::fvec3 or xresource::type_guid), the wrapper that
            // carries its reflection MUST register itself via XPROPERTY_DEF("name", T, ...) using T
            // itself as the object type - NOT the wrapper struct's own type. get_obj_info<T> and every
            // runtime cast_scope::Cast() lookup are keyed on T, so a wrapper that registers under its
            // own type instead compiles fine and passes reflected_object_type above, then fails a
            // get_obj_info assert at runtime the first time something actually tries to inspect T -
            // silently, in whatever code path happens to touch it first. Catch it here instead.
            if constexpr (!std::is_same_v<T, settings::reflected_type_t<T>>)
            {
                using wrapper_t      = settings::reflected_type_t<T>;
                using registered_as  = typename def_registered_type<decltype(wrapper_t::PropertiesDefinition())>::type;
                static_assert(std::is_same_v<registered_as, T>,
                    "XPROP0xx: xproperty::settings::reflected_type<T> points at a wrapper struct that "
                    "registers itself under the WRONG type. The wrapper's XPROPERTY_DEF(\"name\", ..., ...) "
                    "second argument must be the same raw foreign type T that reflected_type<T> was "
                    "specialized for - not the wrapper struct's own type - because get_obj_info<T> and "
                    "cast_scope::Cast() both look objects up by T directly. Fix: change the wrapper's "
                    "XPROPERTY_DEF second argument to T (see xmath::vec3_friend or "
                    "xresource::type_guid_give_properties for the correct pattern).");
            }
        }

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
                return details::make_member_descriptor<T_NAME_V, xproperty::type::members::var, user_data_t>(
                    { .m_pReadUnchecked = details::var_io<T_CLASS, memc_t, T_LAMBDA_V, T_ARGS...>::Read
                    , .m_pWriteUnchecked = is_ready_only_v ? nullptr : details::var_io<T_CLASS, memc_t, T_LAMBDA_V, T_ARGS...>::Write
                    , .m_AtomicType = xproperty::type::atomic_v<atomic_t>
                    , .m_UnregisteredEnumSpan = details::unregistered_enum_array<details::is_unregistered_enum_v<atomic_t>, T_ARGS...>::value
                    }, is_ready_only_v);
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

                static_assert(!type::var_t<t>::is_pointer_v,
                    "XPROP031: pointer-backed virtual properties are unsupported; use a pointer-returning obj_member accessor instead");

                return
                { .m_GUID           = xproperty::settings::strguid(T_NAME_V)
                , .m_pName          = T_NAME_V
                , .m_Variant        = xproperty::type::members::var
                    { .m_pReadUnchecked      = (std::is_const_v<T_MEMBER_TYPE>) ? nullptr : +[]( const void* pClass, type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context) constexpr -> xproperty::result<void>
                    {
                        if constexpr (std::is_const_v<T_MEMBER_TYPE> == false)
                        {
                            details::cacheEnumSpanBestEffort<t>(S);

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
                            return {};
                        }
                        return xproperty::details::makeUnsupportedOperation();
                    }
                    , .m_pWriteUnchecked     = is_ready_only_v ? nullptr : +[](void* pClass, const type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context) constexpr -> xproperty::result<void>
                    {
                        if constexpr ( is_ready_only_v == false )
                        {
                            details::cacheEnumSpanBestEffort<t>(S);

                            if constexpr (std::is_enum_v<t>)
                            {
                                if (details::isUnresolvedEnumString<t>(Any))
                                {
                                    auto Resolved = details::resolveEnumString<t>(Any, S);
                                    if (!Resolved) return Resolved.getError();

                                    if constexpr (sizeof...(T_ADDITIONAL) == 0)
                                        T_LAMBDA_V(*static_cast<T_CLASS*>(pClass), false, Resolved.value().template get<t>());
                                    else
                                        T_LAMBDA_V(*static_cast<T_CLASS*>(pClass), false, Resolved.value().template get<t>(), Context);
                                    return {};
                                }
                            }

                            if constexpr (sizeof...(T_ADDITIONAL) == 0)
                            {
                                T_LAMBDA_V
                                ( *static_cast<T_CLASS*>(pClass)
                                , false
                                , const_cast<type::any&>(Any).get<t>()
                                );
                            }
                            else
                            {
                                T_LAMBDA_V
                                ( *static_cast<T_CLASS*>(pClass)
                                , false
                                , const_cast<type::any&>(Any).get<t>()
                                , Context
                                );
                            }
                            return {};
                        }
                        return xproperty::details::makeReadOnly();
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
                // Pointer members (of any depth) are unwrapped to their ultimate pointee via
                // var_t<T>::atomic_type, and foreign types that get their reflection through a sibling
                // wrapper struct (ImVec2 -> v2, xresource::type_guid -> type_guid_give_properties, ...)
                // are resolved through the explicit xproperty::settings::reflected_type<T> trait -
                // specialize that trait alongside the wrapper rather than expecting this check to find
                // it on its own. Pass the RAW type here (not pre-wrapped) - validate_reflected_object_type
                // applies reflected_type_t itself, and needs the raw type to also check the wrapper
                // registered itself under it.
                validate_reflected_object_type<typename type::var_t<T_MEMBER_TYPE>::atomic_type>();
                //
                // Handle Vars and Refs that are properties... we just convert them to a scope
                //
                return details::make_member_descriptor<T_NAME_V, xproperty::type::members::props, user_data_t>(
                    { .m_pCast = details::cast_scope<T_CLASS, T_MEMBER_TYPE, T_LAMBDA_V>::Cast },
                    details::is_read_only_v<T_CLASS, T_MEMBER_TYPE, T_ARGS...>);
            }
        };


        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, auto T_LAMBDA_V, typename... T_ARGS >
        struct member<T_NAME_V, std::pair<const xproperty::type::object*, void*>(*)(T_CLASS&), T_LAMBDA_V, T_ARGS... >
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
                , .m_Variant    = xproperty::type::members::props{ .m_pCast = [](void* pClass, settings::context& C) -> std::tuple<void*, const type::object*>
                {
                    using fn_t = xproperty::details::function_traits<decltype(T_LAMBDA_V)>;
                         if constexpr (std::tuple_size_v<typename fn_t::args> == 1) { auto a = T_LAMBDA_V(*static_cast<T_CLASS*>(pClass));    return { a.second, a.first }; }
                    else if constexpr (std::tuple_size_v<typename fn_t::args> == 2) { auto a = T_LAMBDA_V(*static_cast<T_CLASS*>(pClass), C); return { a.second, a.first }; }
                    else static_assert(always_false<fn_t>::value, "The Size function for the given list type must have 1 or 2 parameters only");
                }}
                , .m_bConst     = details::is_read_only_v<T_CLASS, int, T_ARGS...>
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
                return details::make_member_descriptor<T_NAME_V, xproperty::type::members::props, user_data_t>(
                    { .m_pCast = details::cast_scope<T_CLASS, T_MEMBER_TYPE, T_LAMBDA_V>::Cast },
                    details::is_read_only_v<T_CLASS, T_MEMBER_TYPE, T_ARGS...>);
            }
        };

        //
        // HANDLE MEMBER VARIABLES LISTS with unregistered ( REF )
        //
        template< xproperty::details::fixed_string T_NAME_V, typename T_CLASS, typename T_MEMBER_TYPE, auto T_LAMBDA_V, typename... T_ARGS >
        requires meets_requirements_v<false, true, T_MEMBER_TYPE>
        struct member<T_NAME_V, T_MEMBER_TYPE&(*)(T_CLASS&), T_LAMBDA_V, T_ARGS... >
        {
            using                           table_helper    = details::list_table< T_CLASS, T_MEMBER_TYPE, T_LAMBDA_V, T_ARGS... >;
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
                    { .m_pReadUnchecked          =  +[](const void* pClass, type::any& Any, const std::span<const type::atomic::enum_item>& S, settings::context& Context) constexpr -> xproperty::result<void>
                                            {
                                                details::cacheEnumSpanBestEffort<atomic_t>(S);

                                                type::var_t<last_t>::Read
                                                ( *static_cast<const typename type::var_t<last_t>::specializing_t*>(pClass)
                                                , Any.Reset<atomic_t>()
                                                , Context
                                                );
                                                return {};
                                            }
                                            
                    , .m_pWriteUnchecked         = is_ready_only_v ? nullptr : +[]( void* pClass, const type::any& Any,const std::span<const type::atomic::enum_item>& S, settings::context& Context) constexpr -> xproperty::result<void>
                                            {
                                                details::cacheEnumSpanBestEffort<atomic_t>(S);

                                                if constexpr (std::is_enum_v<atomic_t>)
                                                {
                                                    if (details::isUnresolvedEnumString<atomic_t>(Any))
                                                    {
                                                        auto Resolved = details::resolveEnumString<atomic_t>(Any, S);
                                                        if (!Resolved) return Resolved.getError();

                                                        type::var_t<last_t>::Write
                                                        ( *static_cast< typename type::var_t<last_t>::specializing_t*>(pClass)
                                                        , Resolved.value().template get<atomic_t>()
                                                        , Context
                                                        );
                                                        return {};
                                                    }
                                                }

                                                type::var_t<last_t>::Write
                                                ( *static_cast< typename type::var_t<last_t>::specializing_t*>(pClass)
                                                , Any.get<atomic_t>()
                                                , Context
                                                );
                                                return {};
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

        namespace details
        {
            template<typename T_ARGUMENTS>
            inline constexpr auto argument_type_list_v = xproperty::details::make_argument_type_list<T_ARGUMENTS>();

            template<typename T_CLASS, auto T_FUNCTION, typename T_ARGUMENTS>
            [[nodiscard]] consteval xproperty::type::members::function make_function_descriptor()
            {
                return
                { .m_pCallFunction = [](void* Class, void* Arguments) constexpr noexcept
                    {
                        auto& Tuple = *static_cast<T_ARGUMENTS*>(Arguments);
                        xproperty::details::invoke_from_tuple(
                            [Class](auto&&... Values) constexpr
                            {
                                std::invoke(
                                    T_FUNCTION,
                                    static_cast<T_CLASS*>(Class),
                                    std::forward<decltype(Values)>(Values)...);
                            },
                            Tuple);
                    }
                , .m_ArgumentList = argument_type_list_v<T_ARGUMENTS>
                };
            }

            template<typename T_CLASS, typename T_ARGUMENTS>
            [[nodiscard]] consteval xproperty::type::member_constructor make_constructor_descriptor()
            {
                return
                { .m_pCallConstructor = [](void* Arguments) constexpr -> void*
                    {
                        auto& Tuple = *static_cast<T_ARGUMENTS*>(Arguments);
                        return xproperty::details::construct_from_tuple<T_CLASS>(Tuple);
                    }
                , .m_ArgumentList = argument_type_list_v<T_ARGUMENTS>
                };
            }
        }

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
            static consteval xproperty::type::members getInfo(bool bConst=false) noexcept
            {
                return
                { .m_GUID    = xproperty::settings::strguid(T_NAME_V)
                , .m_pName   = T_NAME_V
                , .m_Variant = details::make_function_descriptor<class_t, T_DATA, function_args_t>()
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
            using                      members_t = xproperty::details::filter_by_tag_t< meta::obj_member_tag, T_ARGS... >;
            using                    user_data_t = xproperty::details::filter_by_tag_t< meta::user_data_tag, T_ARGS... >;

            inline constexpr static auto name_v    = T_NAME_V;
            inline constexpr static auto members_v = []() consteval
            {
                if constexpr ( std::tuple_size_v<members_t> > 0 ) return []< typename...T>(std::tuple<T...>*) consteval
                {
                    constexpr std::array GUIDs{ T::meta_t::getInfo().m_GUID ... };
                    constexpr bool Unique = [&]() consteval
                    {
                        for (std::size_t i = 0; i < GUIDs.size(); ++i)
                            for (std::size_t j = i + 1; j < GUIDs.size(); ++j)
                                if (GUIDs[i] == GUIDs[j]) return false;
                        return true;
                    }();
                    static_assert(Unique, "xproperty: duplicate member GUID/hash collision in scope");
                    return std::array{ T::meta_t::getInfo() ... };
                }((members_t*)0);
                else return std::span<type::members>{};
            }();

            using lookup_map_t = xproperty::details::static_guid_map<
                std::tuple_size_v<members_t>,
                type::members::scope::invalid_index_v>;

            inline constexpr static auto lookup_v = lookup_map_t::build(members_v);

            consteval static type::members::scope getInfoScope( void ) noexcept
            {
                return{ .m_Members = members_v, .m_Lookup = lookup_v };
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

            template<typename T_CLASS>
            static consteval type::member_constructor getInfo(std::tuple<T_CLASS>*) noexcept
            {
                // Check to make sure we can construct our object this these arguments
                [] <typename...ARGS>(std::tuple<ARGS...>*) constexpr
                {
                    static_assert( std::is_constructible_v<T_CLASS, ARGS...>, "xproperty::obj_constructor - You can not construct this object with the arguments that you have given" );
                }(static_cast<function_args_t*>(nullptr) );

                return details::make_constructor_descriptor<T_CLASS, function_args_t>();
            }
        };

        namespace details
        {
            template< typename T>
            consteval std::uint32_t getGroupGuid()
            {
                return std::tuple_element_t<0, T>::guid_v;
            }

            template<>
            consteval std::uint32_t getGroupGuid<std::tuple<>>()
            {
                return 0;
            }
        }


        template< xproperty::details::fixed_string T_NAME_V, typename T_OBJECT_TYPE, typename...T_ARGS >
        struct object
        {
            using                        members                 = ::xproperty::details::filter_by_tag_t< meta::obj_member_tag, T_ARGS... >;
            using                        group_t                 = ::xproperty::details::filter_by_tag_t< meta::obj_group_tag, T_ARGS... >;
            using                        scope_t                 = ::xproperty::meta::scope< T_NAME_V, members >;
            using                        obj_bases               = ::xproperty::details::filter_by_tag_t< meta::obj_base_tag, T_ARGS... >;
            inline constexpr static auto obj_bases_list_v        = bases<T_OBJECT_TYPE, obj_bases>::getArray();
            using                        constructors            = ::xproperty::details::filter_by_tag_t< meta::obj_constructor_tag, T_ARGS... >;
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
                , ::xproperty::settings::strguid(scope_t::name_v)
                , ::xproperty::meta::details::getGroupGuid< group_t>()
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


#if defined(XPROPERTY_DEPRECATE_LEGACY_NAMES)
    #define XPROPERTY_LEGACY_NAME(MSG) [[deprecated(MSG)]]
#else
    #define XPROPERTY_LEGACY_NAME(MSG)
#endif

    template< details::fixed_string T_NAME_V, auto T_DATA, typename...T_ARGS >
    struct XPROPERTY_LEGACY_NAME("Use xprop::member instead") obj_member : tag<meta::obj_member_tag>
    {
        using meta_t = meta::member<T_NAME_V, decltype(T_DATA), T_DATA, T_ARGS... >;
    };

    template< details::fixed_string T_NAME_V, auto T_DATA, typename...T_ARGS >
    struct obj_member_ro : obj_member< T_NAME_V, T_DATA, xproperty::tag< meta::read_only_tag >, T_ARGS...>{};

    // Add object to a group that we may want to handle in a special way...
    // This can be very handy for things like editors and other tools
    template< details::fixed_string T_NAME_V >
    struct obj_group : tag<meta::obj_group_tag>
    {
        constexpr static auto guid_v = xproperty::settings::strguid(T_NAME_V);
        constexpr static auto name_v = T_NAME_V.m_Value;
        std::uint32_t m_GUID  = guid_v;
        const char*   m_pName = name_v;
    };

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

    // void(*)( CLASS&, bool bRead, std::size_t& Size, context& Context )
    template< auto T_OVERWRITE_LIST_SIZE_CALLBACK >
    struct member_overwrite_list_size : tag< meta::member_overwrite_list_size_tag >
    {
        constexpr static auto overwrite_size_v = T_OVERWRITE_LIST_SIZE_CALLBACK;
    };

    //
    // USER DATA
    //
    template< details::fixed_string ID, std::uint32_t GUID = xproperty::settings::strguid(ID) >
    struct member_user_data : tag< meta::user_data_tag >
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
    struct obj_scope_ro : obj_scope< T_NAME_V, tag< meta::read_only_tag >, T_ARGS...> {};

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

        def() : def_base{ register_v, nullptr }
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

// Property-definition vocabulary. These types intentionally live outside the
// broad xproperty namespace to make declarations recognizable in large codebases.
namespace xprop
{
    namespace details
    {
        template<auto T_DATA>
        inline constexpr auto normalized_data_v = []() consteval
        {
            using data_t = decltype(T_DATA);
            if constexpr (std::is_class_v<data_t>)
            {
                static_assert(requires { +T_DATA; },
                    "XPROP012: property callback must be captureless and convertible to a function pointer");
                return +T_DATA;
            }
            else
            {
                return T_DATA;
            }
        }();
    }

    template<xproperty::details::fixed_string T_NAME_V, auto T_DATA, typename...T_ARGS>
    struct member : xproperty::tag<xproperty::meta::obj_member_tag>
    {
        inline constexpr static auto data_v = details::normalized_data_v<T_DATA>;
        using data_t = std::remove_cv_t<decltype(data_v)>;
        using meta_t = xproperty::meta::member<T_NAME_V, data_t, data_v, T_ARGS...>;
    };

    template<xproperty::details::fixed_string T_NAME_V, auto T_DATA, typename...T_ARGS>
    struct member_ro : member<T_NAME_V, T_DATA, xproperty::tag<xproperty::meta::read_only_tag>, T_ARGS...> {};

    template<xproperty::details::fixed_string T_NAME_V, typename...T_ARGS>
    struct scope : xproperty::tag<xproperty::meta::obj_member_tag>
    {
        using meta_t = xproperty::meta::scope<T_NAME_V, std::tuple<T_ARGS...>>;
    };

    template<xproperty::details::fixed_string T_NAME_V, typename...T_ARGS>
    struct scope_ro : scope<T_NAME_V, xproperty::tag<xproperty::meta::read_only_tag>, T_ARGS...> {};

    template<typename T_BASE>
    struct base : xproperty::tag<xproperty::meta::obj_base_tag>
    {
        using type = T_BASE;
        inline constexpr static bool is_const_v = false;
    };

    template<typename T_BASE>
    struct base_ro : xproperty::tag<xproperty::meta::obj_base_tag>
    {
        using type = T_BASE;
        inline constexpr static bool is_const_v = true;
    };

    template<typename...T_ARGS>
    struct constructor : xproperty::obj_constructor<T_ARGS...>
    {
        static_assert(sizeof...(T_ARGS) > 0,
            "XPROP008: xprop::constructor must contain at least one argument type");
    };

    template<xproperty::details::fixed_string T_NAME_V>
    using group = xproperty::obj_group<T_NAME_V>;
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

#undef XPROPERTY_LEGACY_NAME

#undef XPROPERTY_LEGACY_API

#endif