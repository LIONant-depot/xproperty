#ifndef MY_PROPERTIES_UI_H
#define MY_PROPERTIES_UI_H
#pragma once

namespace xproperty::flags
{
    union type
    {
        std::uint8_t  m_Value;

        struct
        {
            bool  m_bShowReadOnly : 1       // Tells the UI to show this property/ies as read only
                , m_bDontSave     : 1       // Tells the serializer not to save this property/ies
                , m_bDontShow     : 1       // Tells the UI not to show this property/ies
                ;
        };
    };
    static_assert(sizeof(type)==1);

    enum _flags : std::uint8_t
    { SHOW_READONLY = std::uint32_t(1<<0)
    , DONT_SAVE     = std::uint32_t(1<<1)
    , DONT_SHOW     = std::uint32_t(1<<2)
    };

}

namespace xproperty::ui::details
{
    struct member_ui_base;
}

namespace xproperty::settings
{
    struct member_ui_t : xproperty::member_user_data<"UI">
    {
        const xproperty::ui::details::member_ui_base* m_pUIBase;
    };

    struct member_ui_list_size_t : xproperty::member_user_data<"UI LIST SIZE">
    {
    };

    struct member_flags_t : xproperty::member_user_data<"Flags">
    {
        xproperty::flags::type m_Flags;
    };

    struct member_dynamic_flags_t : xproperty::member_user_data<"Dynamic Flags">
    {
        using callback = xproperty::flags::type(const void*, settings::context& ) noexcept;
        callback* m_pCallback;
    };

    struct list_flags_ui_t : xproperty::member_user_data<"List Flags">
    {
        std::uint32_t m_Flags;
    };

}

namespace xproperty::settings
{
    struct member_ui_open_t : xproperty::member_user_data<"UI Default Open">
    {
        bool m_bOpen;
    };
}

namespace xproperty
{
    template< typename T >
    struct member_ui;

    template< xproperty::flags::_flags...T_V>
    struct member_flags : settings::member_flags_t
    {
        constexpr member_flags() noexcept
            : settings::member_flags_t{ .m_Flags = xproperty::flags::type{ .m_Value = ( T_V | ...) }  } {}
    };

    template< auto T_CALLBACK_V >
    struct member_dynamic_flags : settings::member_dynamic_flags_t
    {
        using fn_t = xproperty::details::function_traits<decltype(T_CALLBACK_V)>;
        static_assert(std::tuple_size_v<typename fn_t::args> <= 2);
        static_assert(std::is_same_v<typename fn_t::return_type, xproperty::flags::type>);

        using arg1 = std::tuple_element_t<0, typename fn_t::args>;
        static_assert( std::is_reference_v<arg1>);
        using arg1_t = std::remove_reference_t<arg1>;

        constexpr member_dynamic_flags() noexcept
            : settings::member_dynamic_flags_t{ .m_pCallback = []( const void* pObj, settings::context& C) constexpr noexcept  -> xproperty::flags::type
                { if constexpr (std::tuple_size_v<typename fn_t::args> == 1) return T_CALLBACK_V(*static_cast<const arg1_t*>(pObj));
                  else                                                       return T_CALLBACK_V(*static_cast<const arg1_t*>(pObj), C);
                } } {}
    };

    template< bool T_OPEN_V >
    struct member_ui_open : settings::member_ui_open_t
    {
        constexpr member_ui_open() noexcept : settings::member_ui_open_t{ .m_bOpen = T_OPEN_V } {}
    };

    namespace ui::undo
    {
        struct cmd;
    }

    namespace ui::details
    {
        struct member_ui_base
        {
            void*           m_pDrawFn;
            std::uint32_t   m_TypeGUID;
        };

        struct style
        {
            struct edit_box;
            struct scroll_bar;
            struct drag_bar;
            struct enumeration;
            struct defaulted;
            struct button;
            struct file_dialog;
        };

        template< typename T_TYPE, typename T_STYLE>
        struct draw
        {
            static void Render(ui::undo::cmd& Cmd, const T_TYPE& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept;
        };

        template<typename T, xproperty::details::fixed_string T_FORMAT_MAIN>
        struct member_ui_numbers //: ui::details::member_ui_base
        {
            inline static constexpr auto type_guid_v = xproperty::settings::var_type<T>::guid_v;

            member_ui_numbers() = delete;

            struct data : member_ui_base
            {
                T               m_Min;
                T               m_Max;
                const char*     m_pFormat;
                float           m_Speed;
            };

            template< T                                 T_MIN       = std::numeric_limits<T>::lowest()
                    , T                                 T_MAX       = std::numeric_limits<T>::max()
                    , xproperty::details::fixed_string  T_FORMAT    = T_FORMAT_MAIN
                    >
            struct scroll_bar : settings::member_ui_t
            {
                inline static constexpr data data_v
                { {.m_pDrawFn = &ui::details::draw<T, ui::details::style::scroll_bar>::Render, .m_TypeGUID = type_guid_v }
                , T_MIN
                , T_MAX
                , T_FORMAT
                , 0
                };
                constexpr scroll_bar() : settings::member_ui_t{ .m_pUIBase = &data_v }{}
            };

            template< T                                 T_MIN       = std::numeric_limits<T>::lowest()
                    , T                                 T_MAX       = std::numeric_limits<T>::max()
                    , xproperty::details::fixed_string  T_FORMAT    = T_FORMAT_MAIN
                    >
            struct edit_box : settings::member_ui_t
            {
                inline static constexpr data data_v
                { {.m_pDrawFn = &ui::details::draw<T, ui::details::style::edit_box>::Render, .m_TypeGUID = type_guid_v }
                , T_MIN
                , T_MAX
                , T_FORMAT
                , 0
                };
                constexpr edit_box() : settings::member_ui_t{ .m_pUIBase = &data_v }{}
            };

            template< float                             T_SPEED     = 0.5f
                    , T                                 T_MIN       = std::numeric_limits<T>::lowest()
                    , T                                 T_MAX       = std::numeric_limits<T>::max()
                    , xproperty::details::fixed_string  T_FORMAT    = T_FORMAT_MAIN
                    >
            struct drag_bar : settings::member_ui_t
            {
                inline static constexpr data data_v
                { { .m_pDrawFn = &ui::details::draw< T, ui::details::style::drag_bar>::Render, .m_TypeGUID = type_guid_v }
                , T_MIN
                , T_MAX
                , T_FORMAT
                , T_SPEED
                };
                constexpr drag_bar() : settings::member_ui_t{ .m_pUIBase  = &data_v }{}
            };

            using defaults = drag_bar<>;
        };

    }

    template<> struct member_ui<std::int64_t>   : ui::details::member_ui_numbers<std::int64_t,  "%d">   {};
    template<> struct member_ui<std::uint64_t>  : ui::details::member_ui_numbers<std::uint64_t, "%d">   {};
    template<> struct member_ui<std::int32_t>   : ui::details::member_ui_numbers<std::int32_t,  "%d">   {};
    template<> struct member_ui<std::uint32_t>  : ui::details::member_ui_numbers<std::uint32_t, "%d">   {};
    template<> struct member_ui<std::int16_t>   : ui::details::member_ui_numbers<std::int16_t,  "%d">   {};
    template<> struct member_ui<std::uint16_t>  : ui::details::member_ui_numbers<std::uint16_t, "%d">   {};
    template<> struct member_ui<std::int8_t>    : ui::details::member_ui_numbers<std::int8_t,   "%d">   {};
    template<> struct member_ui<std::uint8_t>   : ui::details::member_ui_numbers<std::uint8_t,  "%d">   {};
    template<> struct member_ui<char>           : ui::details::member_ui_numbers<int8_t,        "%d">   {};
    template<> struct member_ui<float>          : ui::details::member_ui_numbers<float,         "%g">   {};   // %.4g
    template<> struct member_ui<double>         : ui::details::member_ui_numbers<double,        "%g">   {};   // %.4g

    template<> struct member_ui<std::string>
    {
        member_ui() = delete;

        struct data : ui::details::member_ui_base
        {
            const char* m_pFilter = {nullptr};
        };

        inline static constexpr auto type_guid_v = xproperty::settings::var_type<std::string>::guid_v;

        template< typename T = ui::details::style::defaulted >
        struct button : settings::member_ui_t
        {
            inline static constexpr data data_v
            { { .m_pDrawFn = &ui::details::draw<std::string, ui::details::style::button>::Render, .m_TypeGUID = type_guid_v }, {} };

            constexpr button() : settings::member_ui_t{ .m_pUIBase = &data_v } {}
        };

        template< xproperty::details::fixed_string  T_FILTER = "All\0*.*\0Text\0*.TXT\0" >
        struct file_dialog : settings::member_ui_t
        {
            inline static constexpr data data_v
            {{ .m_pDrawFn = &ui::details::draw<std::string, ui::details::style::file_dialog>::Render, .m_TypeGUID = type_guid_v }
            , T_FILTER
            };

            constexpr file_dialog() : settings::member_ui_t{ .m_pUIBase = &data_v } {}
        };


        struct defaults : settings::member_ui_t
        {
            inline static constexpr data data_v
            { {.m_pDrawFn = &ui::details::draw<std::string, ui::details::style::defaulted>::Render, .m_TypeGUID = type_guid_v} };

            constexpr defaults() : settings::member_ui_t{.m_pUIBase  = &data_v} {}
        };
    };

    template<> struct member_ui<bool>
    {
        member_ui() = delete;

        using data = ui::details::member_ui_base;
        inline static constexpr auto type_guid_v = xproperty::settings::var_type<bool>::guid_v;

        struct defaults : settings::member_ui_t
        {
            inline static constexpr data data_v
            { .m_pDrawFn = &ui::details::draw<bool, ui::details::style::defaulted>::Render, .m_TypeGUID = type_guid_v };

            constexpr defaults() : settings::member_ui_t{ .m_pUIBase = &data_v }
            {
                static_assert(type_guid_v == data_v.m_TypeGUID, "What the hells..." );

            }
        };
    };

    struct member_ui_list_size
    {
        template<  std::uint64_t               T_MIN   = std::numeric_limits<std::uint64_t>::lowest()
                 , std::uint64_t               T_MAX   = std::numeric_limits<std::uint64_t>::max()
                 , float                       T_SPEED = 0.5f >
        struct drag_bar : member_ui<std::uint64_t>::drag_bar< T_SPEED, T_MIN, T_MAX >
        {
            constexpr static auto type_string_v = xproperty::settings::member_ui_list_size_t::type_string_v;
            constexpr static auto type_guid_v   = xproperty::settings::member_ui_list_size_t::type_guid_v;
        };

        template< std::uint64_t               T_MIN = std::numeric_limits<std::uint64_t>::lowest()
                , std::uint64_t               T_MAX = std::numeric_limits<std::uint64_t>::max()>
        struct scroll_bar : member_ui<std::uint64_t>::scroll_bar< T_MIN, T_MAX >
        {
            constexpr static auto type_string_v = xproperty::settings::member_ui_list_size_t::type_string_v;
            constexpr static auto type_guid_v   = xproperty::settings::member_ui_list_size_t::type_guid_v;
        };

        using defaults = drag_bar< 1, 10000 >;
    };
}
#endif