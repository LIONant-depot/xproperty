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
            bool  m_bShowReadOnly   : 1     // Tells the UI to show this property/ies as read only
                , m_bDontSave       : 1     // Tells the serializer not to save this property/ies
                , m_bDontShow       : 1     // Tells the UI not to show this property/ies
                , m_bAppendNewLine  : 1     // Tells inspector::m_OnCustomRenderAppend's call site to start a new line before invoking the callback, instead of the default ImGui::SameLine() right after the value widget - same idiom as SHOW_READONLY etc., so member_flags<APPEND_NEW_LINE>/member_dynamic_flags<...> already work here for free
                ;
        };
    };
    static_assert(sizeof(type)==1);

    enum _flags : std::uint8_t
    { SHOW_READONLY   = std::uint32_t(1<<0)
    , DONT_SAVE       = std::uint32_t(1<<1)
    , DONT_SHOW       = std::uint32_t(1<<2)
    , APPEND_NEW_LINE = std::uint32_t(1<<3)
    };

}

namespace xproperty::ui::details
{
    struct member_ui_base;
}

// Forward declared, not included - my_property_ui.h is reached via my_properties.h, which many
// property-declaring consumers (plugin DLLs, headless tools) include WITHOUT ever pulling in the
// ImGui inspector itself. An incomplete type is enough for member_custom_render_block_t below (only
// ever used behind a reference in a function-pointer TYPE, never dereferenced in this header) - same
// reason xPropertyImGuiInspector.h itself forward-declares this before using it.
namespace xproperty { class inspector; }

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

    // Controls the width ImGui::PushItemWidth() uses for this property's default value widget -
    // previously hardcoded to -1 (fill the entire column) everywhere, which is exactly why a wide
    // widget leaves zero room for a same-line m_OnCustomRenderAppend (see APPEND_NEW_LINE's own
    // comment). Same ImGui PushItemWidth semantics apply: positive = absolute pixel width, negative =
    // distance from the column's right edge (e.g. -40 leaves 40px of room at the end).
    struct member_item_width_t : xproperty::member_user_data<"Item Width">
    {
        float m_Width;
    };

    struct member_dynamic_item_width_t : xproperty::member_user_data<"Dynamic Item Width">
    {
        using callback = float(const void*, settings::context&) noexcept;
        callback* m_pCallback;
    };

    struct list_flags_ui_t : xproperty::member_user_data<"List Flags">
    {
        std::uint32_t m_Flags;
    };

    // Pure layout sugar - a section separator drawn before the first member of a new named group, no
    // serialization impact. Named "section" rather than the roadmap's original "group" specifically to
    // avoid colliding with the existing, unrelated m_GroupGUID concept (vector2_group/vector3_group,
    // which packs sibling scalar members like x/y/z into one inline row - a completely different idea).
    struct member_section_t : xproperty::member_user_data<"Section">
    {
        const char* m_pSectionName;
    };

    // Lets a property declare its OWN full-width custom rendering (a curve editor, a graph, anything
    // that needs to escape the 2-column grid entirely - see inspector::on_custom_render_block's own
    // comment for the mechanism) directly at the obj_member<> site, instead of a caller having to
    // register a SHARED inspector-wide delegate and Path.ends_with()-match this property out of every
    // other property in the system. That broadcast-and-match shape still exists (m_OnCustomRenderBlock)
    // and is still the right tool for a one-off UI flourish tied to a specific demo/consumer - but a
    // genuinely reusable, type-level capability (a curve type usable from any struct) belongs on the
    // property declaration itself, the same way member_section/member_dynamic_flags already do, not
    // duplicated into every inspector-wiring call site that happens to show that property.
    // RowColorU32 is a raw ImU32 (not ImColor) specifically so this header - included by plugin
    // property-declaration code that may not link ImGui at all - never needs an ImGui include; the one
    // real caller (Render(), which does have ImGui) reads/writes it as an ImU32/ImColor pair via a
    // trivial implicit conversion.
    struct member_custom_render_block_t : xproperty::member_user_data<"Custom Render Block">
    {
        using callback = void
        ( xproperty::inspector&           // the inspector doing the drawing - lets the callback call BeginEdit/CommitEdit on it
        , const xproperty::type::object&  // the reflected type of the owning component
        , void*                           // the live component instance
        , std::string_view                // this property's full path
        , const xproperty::any&           // this property's current (resolved) value
        , std::uint32_t                   // row background color, as a packed ImU32
        , bool                            // bDryRun - true for the "ask" call, false for the real draw (see on_custom_render_block's own comment)
        , bool&                           // bIsBlockContent - set true to claim this property as block content
        ) noexcept;
        callback* m_pCallback;
    };

    // Same rationale as member_custom_render_block_t above, for the other 3 of the 4 planned custom-
    // rendering levels (see inspector::on_custom_render_append/on_custom_render_replace_value/
    // on_custom_render_replace_row's own comments for what each level replaces) - a property declares
    // its own append/replace behavior directly at its obj_member<> site instead of a shared,
    // inspector-wide delegate having to Path.ends_with()-match it out of every other property. Each
    // still coexists with its broadcast delegate counterpart, still the right tool for a one-off UI
    // flourish tied to a specific caller.
    struct member_custom_render_append_t : xproperty::member_user_data<"Custom Render Append">
    {
        using callback = void
        ( xproperty::inspector&
        , const xproperty::type::object&
        , void*
        , std::string_view
        , const xproperty::any&
        ) noexcept;
        callback* m_pCallback;
    };

    struct member_custom_render_replace_value_t : xproperty::member_user_data<"Custom Render Replace Value">
    {
        using callback = void
        ( xproperty::inspector&
        , const xproperty::type::object&
        , void*
        , std::string_view
        , const xproperty::any&
        , bool&                           // bHandled - set true to skip the default value widget
        ) noexcept;
        callback* m_pCallback;
    };

    struct member_custom_render_replace_row_t : xproperty::member_user_data<"Custom Render Replace Row">
    {
        using callback = void
        ( xproperty::inspector&
        , const xproperty::type::object&
        , void*
        , std::string_view
        , const xproperty::any&
        , bool&                           // bHandled - set true to take over the left (label) column too
        ) noexcept;
        callback* m_pCallback;
    };

    // Same rationale again, for on_override_check/on_override_reset - a property declares directly
    // (via a comparison against whatever it considers its own base value) that it can be "overridden",
    // instead of a shared delegate Path-matching it. Kept as two separate tags, matching the delegate
    // pair's own split (a property can be checkable without being resettable, in principle), though in
    // practice a consumer will normally attach both to the same property.
    struct member_override_check_t : xproperty::member_user_data<"Override Check">
    {
        using callback = void
        ( xproperty::inspector&
        , const xproperty::type::object&
        , void*
        , std::string_view
        , const xproperty::any&
        , bool&                           // bIsOverridden
        ) noexcept;
        callback* m_pCallback;
    };

    struct member_override_reset_t : xproperty::member_user_data<"Override Reset">
    {
        using callback = void
        ( xproperty::inspector&
        , const xproperty::type::object&
        , void*
        , std::string_view
        ) noexcept;
        callback* m_pCallback;
    };

    // Presence-only marker (no payload) - a real, normally-reflected bool member carries this when it
    // should also act as its OWN PARENT SCOPE's enable-toggle: rendered as a checkbox merged into the
    // scope's own header row instead of a separate row, with the scope's children only rendering while
    // it's true. Deliberately its own dedicated marker rather than a bit in xproperty::flags::type - a
    // scope-toggle isn't a "how should this member render/save" flag the way SHOW_READONLY/DONT_SAVE/
    // DONT_SHOW are, it's an unrelated structural relationship to a DIFFERENT entry (the parent scope),
    // so it doesn't belong sharing that bitset. The member must be the scope's FIRST child for the
    // renderer to find it - this is checked positionally, not by walking the whole scope.
    struct scope_toggle_t : xproperty::member_user_data<"Scope Toggle">
    {
    };

}

namespace xproperty::settings
{
    struct member_ui_open_t : xproperty::member_user_data<"UI Default Open">
    {
        bool m_bOpen;
    };

    // Marks an array's SIZE as non-resizable through the inspector - no insert/delete/reorder
    // controls on the array itself - while leaving each element's own value fully editable. For
    // arrays whose size is driven by something other than the user directly resizing it through this
    // UI (e.g. rebuilt from an imported asset's own material list), where bHasRealSetSize would
    // otherwise show full structural controls just because the backing container happens to be a
    // real std::vector. Deliberately separate from member_flags<SHOW_READONLY>, which disables the
    // array's ELEMENTS too - this only ever gates the size/structural controls (see
    // xPropertyImGuiInspector.cpp's own bShowArrayControls).
    struct member_array_size_readonly_t : xproperty::member_user_data<"Array Size ReadOnly">
    {
        bool m_bReadOnly;
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

    // See settings::member_custom_render_block_t's own comment for why this exists alongside
    // inspector::m_OnCustomRenderBlock rather than replacing it. T_CALLBACK_V's signature must match
    // member_custom_render_block_t::callback exactly (a plain function pointer, not function_traits-
    // erased like member_dynamic_flags above) - this tag is for a property that always renders the
    // same custom way, not one with several arg-count overloads to support.
    template< auto T_CALLBACK_V >
    struct member_custom_render_block : settings::member_custom_render_block_t
    {
        constexpr member_custom_render_block() noexcept
            : settings::member_custom_render_block_t{ .m_pCallback = T_CALLBACK_V } {}
    };

    // Same "plain function pointer, match the callback signature exactly" shape as
    // member_custom_render_block above, for the other 3 custom-render levels plus override check/reset.
    template< auto T_CALLBACK_V >
    struct member_custom_render_append : settings::member_custom_render_append_t
    {
        constexpr member_custom_render_append() noexcept
            : settings::member_custom_render_append_t{ .m_pCallback = T_CALLBACK_V } {}
    };

    template< auto T_CALLBACK_V >
    struct member_custom_render_replace_value : settings::member_custom_render_replace_value_t
    {
        constexpr member_custom_render_replace_value() noexcept
            : settings::member_custom_render_replace_value_t{ .m_pCallback = T_CALLBACK_V } {}
    };

    template< auto T_CALLBACK_V >
    struct member_custom_render_replace_row : settings::member_custom_render_replace_row_t
    {
        constexpr member_custom_render_replace_row() noexcept
            : settings::member_custom_render_replace_row_t{ .m_pCallback = T_CALLBACK_V } {}
    };

    template< auto T_CALLBACK_V >
    struct member_override_check : settings::member_override_check_t
    {
        constexpr member_override_check() noexcept
            : settings::member_override_check_t{ .m_pCallback = T_CALLBACK_V } {}
    };

    template< auto T_CALLBACK_V >
    struct member_override_reset : settings::member_override_reset_t
    {
        constexpr member_override_reset() noexcept
            : settings::member_override_reset_t{ .m_pCallback = T_CALLBACK_V } {}
    };

    // Static width - see settings::member_item_width_t's own comment for the PushItemWidth semantics.
    template< float T_WIDTH_V >
    struct member_item_width : settings::member_item_width_t
    {
        constexpr member_item_width() noexcept : settings::member_item_width_t{ .m_Width = T_WIDTH_V } {}
    };

    // Dynamic width - same shape as member_dynamic_flags, for when the reserved space depends on
    // instance state (e.g. only leave room for an append when some condition is actually true).
    template< auto T_CALLBACK_V >
    struct member_dynamic_item_width : settings::member_dynamic_item_width_t
    {
        using fn_t = xproperty::details::function_traits<decltype(T_CALLBACK_V)>;
        static_assert(std::tuple_size_v<typename fn_t::args> <= 2);
        static_assert(std::is_same_v<typename fn_t::return_type, float>);

        using arg1 = std::tuple_element_t<0, typename fn_t::args>;
        static_assert( std::is_reference_v<arg1>);
        using arg1_t = std::remove_reference_t<arg1>;

        constexpr member_dynamic_item_width() noexcept
            : settings::member_dynamic_item_width_t{ .m_pCallback = []( const void* pObj, settings::context& C) constexpr noexcept -> float
                { if constexpr (std::tuple_size_v<typename fn_t::args> == 1) return T_CALLBACK_V(*static_cast<const arg1_t*>(pObj));
                  else                                                       return T_CALLBACK_V(*static_cast<const arg1_t*>(pObj), C);
                } } {}
    };

    // Pure layout sugar - see settings::member_section_t above for why this isn't called member_group.
    template< xproperty::details::fixed_string T_NAME_V >
    struct member_section : settings::member_section_t
    {
        constexpr member_section() noexcept : settings::member_section_t{ .m_pSectionName = T_NAME_V.m_Value } {}
    };

    // Sugar over obj_member for a reflected action (a plain member-function pointer, e.g.
    // &Class::Method) - decltype(T_DATA) alone already selects xproperty's member_function_tag
    // dispatch regardless of the macro name used at the call site, so this is purely a declaration-
    // site name that reads as "this member is an action", not a new mechanism.
    template< xproperty::details::fixed_string T_NAME_V, auto T_DATA, typename...T_ARGS >
    using obj_action = xproperty::obj_member<T_NAME_V, T_DATA, T_ARGS...>;

    // Sugar over obj_member for a real bool data member that ALSO acts as its own parent obj_scope's
    // enable-toggle (see settings::scope_toggle_t above) - must be the scope's first child. Bakes in
    // the scope_toggle_t marker so the declaration site reads as "this bool controls its own scope"
    // without the caller needing to know the marker type exists:
    //   obj_scope<"Advanced Settings"
    //       , obj_scope_toggle<"Enabled", &Class::m_bAdvancedEnabled>
    //       , obj_member<"Speed", &Class::m_Speed>
    //       ...>
    template< xproperty::details::fixed_string T_NAME_V, auto T_DATA, typename...T_ARGS >
    using obj_scope_toggle = xproperty::obj_member<T_NAME_V, T_DATA, settings::scope_toggle_t, T_ARGS...>;

    template< bool T_OPEN_V >
    struct member_ui_open : settings::member_ui_open_t
    {
        constexpr member_ui_open() noexcept : settings::member_ui_open_t{ .m_bOpen = T_OPEN_V } {}
    };

    template< bool T_READONLY_V = true >
    struct member_array_size_readonly : settings::member_array_size_readonly_t
    {
        constexpr member_array_size_readonly() noexcept : settings::member_array_size_readonly_t{ .m_bReadOnly = T_READONLY_V } {}
    };

    namespace ui::undo
    {
        struct cmd;
    }

    namespace ui::details
    {
        // Identifies a (Type, Style) drawer pair by two compile-time GUIDs rather than a resolved
        // function pointer - a plugin that merely ATTACHES a style to a member (member_ui<T>::Style<...>)
        // only ever stores these two integers in its own constexpr data, so it never instantiates
        // DrawErased/draw<T,Style>::Render itself. Only whichever binary actually draws (calls
        // ResolveDrawFn, below - see xPropertyImGuiInspector.cpp) needs the real Render implementations
        // linked in. This is the same GUID-value-not-pointer-identity principle xproperty::any::is<T>()
        // already uses for cross-binary type comparisons, applied one layer up to UI dispatch.
        struct member_ui_base
        {
            std::uint32_t   m_TypeGUID;
            std::uint32_t   m_StyleGUID;
        };

        using draw_fn = void
        ( int GUID
        , ui::undo::cmd& Cmd
        , const xproperty::any& Value
        , const member_ui_base& Info
        , xproperty::flags::type Flags
        ) noexcept;

        // Resolves a real drawer via (TypeGUID, StyleGUID). Defined in xPropertyImGuiInspector.cpp,
        // the one place with real draw<T,Style>::Render bodies to register - never in a header, so it's
        // never pulled into a plugin's own translation unit.
        draw_fn* ResolveDrawFn(std::uint32_t TypeGUID, std::uint32_t StyleGUID) noexcept;

        // Registers one (TypeGUID, StyleGUID) -> real Render mapping - called only from
        // xPropertyImGuiInspector.cpp's own static registration block, once per combo it actually
        // implements.
        void RegisterDrawFn(std::uint32_t TypeGUID, std::uint32_t StyleGUID, draw_fn* pFn) noexcept;

        // Each style tag carries its own compile-time GUID (a hash of its name, exactly like an atomic
        // type's own var_type<T>::guid_v) - portable across binaries since it depends only on the name
        // string, never an address.
        struct style
        {
            struct edit_box    { static constexpr xproperty::details::fixed_string name_v = "edit_box";    static constexpr std::uint32_t guid_v = xproperty::settings::strguid(name_v); };
            struct scroll_bar  { static constexpr xproperty::details::fixed_string name_v = "scroll_bar";  static constexpr std::uint32_t guid_v = xproperty::settings::strguid(name_v); };
            struct drag_bar    { static constexpr xproperty::details::fixed_string name_v = "drag_bar";    static constexpr std::uint32_t guid_v = xproperty::settings::strguid(name_v); };
            struct enumeration { static constexpr xproperty::details::fixed_string name_v = "enumeration"; static constexpr std::uint32_t guid_v = xproperty::settings::strguid(name_v); };
            struct defaulted   { static constexpr xproperty::details::fixed_string name_v = "defaulted";   static constexpr std::uint32_t guid_v = xproperty::settings::strguid(name_v); };
            struct button      { static constexpr xproperty::details::fixed_string name_v = "button";      static constexpr std::uint32_t guid_v = xproperty::settings::strguid(name_v); };
            struct file_dialog { static constexpr xproperty::details::fixed_string name_v = "file_dialog"; static constexpr std::uint32_t guid_v = xproperty::settings::strguid(name_v); };
        };

        template< typename T_TYPE, typename T_STYLE>
        struct draw
        {
            static void Render(int GUID, ui::undo::cmd& Cmd, const T_TYPE& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept;
        };

        template< typename T_TYPE, typename T_STYLE>
        void DrawErased
        ( int GUID
        , ui::undo::cmd& Cmd
        , const xproperty::any& Value
        , const member_ui_base& Info
        , xproperty::flags::type Flags
        ) noexcept
        {
            assert(Value.m_pType);
            assert(Value.m_pType->m_GUID == xproperty::settings::var_type<T_TYPE>::guid_v);
            draw<T_TYPE, T_STYLE>::Render(GUID, Cmd, Value.get<T_TYPE>(), Info, Flags);
        }

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
                { {.m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::scroll_bar::guid_v }
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
                { {.m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::edit_box::guid_v }
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
                { { .m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::drag_bar::guid_v }
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

    template<> struct member_ui<std::int64_t>   : ui::details::member_ui_numbers<std::int64_t,  "%lld"  >   {};
    template<> struct member_ui<std::uint64_t>  : ui::details::member_ui_numbers<std::uint64_t, "%llu"  >   {};
    template<> struct member_ui<std::int32_t>   : ui::details::member_ui_numbers<std::int32_t,  "%d"    >   {};
    template<> struct member_ui<std::uint32_t>  : ui::details::member_ui_numbers<std::uint32_t, "%u"    >   {};
    template<> struct member_ui<std::int16_t>   : ui::details::member_ui_numbers<std::int16_t,  "%hd"   >   {};
    template<> struct member_ui<std::uint16_t>  : ui::details::member_ui_numbers<std::uint16_t, "%hu"   >   {};
    template<> struct member_ui<std::int8_t>    : ui::details::member_ui_numbers<std::int8_t,   "%hhd"  >   {};
    template<> struct member_ui<std::uint8_t>   : ui::details::member_ui_numbers<std::uint8_t,  "%hhu"  >   {};
    template<> struct member_ui<char>           : ui::details::member_ui_numbers<int8_t,        "%hhd"  >   {};
    template<> struct member_ui<float>          : ui::details::member_ui_numbers<float,         "%g">   {};   // %.4g
    template<> struct member_ui<double>         : ui::details::member_ui_numbers<double,        "%g">   {};   // %.4g

    template<> struct member_ui<std::string>
    {
        member_ui() = delete;

        using data = ui::details::member_ui_base;

        inline static constexpr auto type_guid_v = xproperty::settings::var_type<std::string>::guid_v;

        template< typename T = ui::details::style::defaulted >
        struct button : settings::member_ui_t
        {
            inline static constexpr data data_v
            { .m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::button::guid_v };

            constexpr button() : settings::member_ui_t{ .m_pUIBase = &data_v } {}
        };

        struct defaults : settings::member_ui_t
        {
            inline static constexpr data data_v
            { .m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::defaulted::guid_v };

            constexpr defaults() : settings::member_ui_t{.m_pUIBase  = &data_v} {}
        };
    };

    template<> struct member_ui<std::wstring>
    {
        member_ui() = delete;

        // This path can be set by the user to indicate what is the current relevant path
        // this is used for the file dialog and the path dialogs
        inline static std::wstring g_CurrentPath;

        struct data : ui::details::member_ui_base
        {
            const wchar_t*  m_pFilter;
            bool            m_bMakePathRelative;
            int             m_RelativeCurrentPathMinusCount;
            bool            m_bFolders;
        };

        inline static constexpr auto type_guid_v = xproperty::settings::var_type<std::wstring>::guid_v;

        template< typename T = ui::details::style::defaulted >
        struct button : settings::member_ui_t
        {
            inline static constexpr data data_v
            { {.m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::button::guid_v }, nullptr, false, 0, false };

            constexpr button() : settings::member_ui_t{ .m_pUIBase = &data_v } {}
        };

        template< xproperty::details::fixed_wstring  T_FILTER = L"All\0*.*\0Text\0*.TXT\0", bool T_MAKE_PATH_RELATIVE_V = false, int T_RELATIVE_CURRENT_PATH_MINUS_COUNT_V = 0 >
        struct file_dialog : settings::member_ui_t
        {
            inline static constexpr data data_v
            { {.m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::file_dialog::guid_v }
            , T_FILTER
            , T_MAKE_PATH_RELATIVE_V
            , T_RELATIVE_CURRENT_PATH_MINUS_COUNT_V
            , false
            };

            constexpr file_dialog() : settings::member_ui_t{ .m_pUIBase = &data_v } {}
        };

        template< xproperty::details::fixed_wstring  T_FILTER = L"All\0*\0", bool T_MAKE_PATH_RELATIVE_V = false, int T_RELATIVE_CURRENT_PATH_MINUS_COUNT_V = 0 >
        struct folder_dialog : settings::member_ui_t
        {
            inline static constexpr data data_v
            { {.m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::file_dialog::guid_v }
            , T_FILTER
            , T_MAKE_PATH_RELATIVE_V
            , T_RELATIVE_CURRENT_PATH_MINUS_COUNT_V
            , true
            };

            constexpr folder_dialog() : settings::member_ui_t{ .m_pUIBase = &data_v } {}
        };

        struct defaults : settings::member_ui_t
        {
            inline static constexpr data data_v
            { {.m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::defaulted::guid_v}, nullptr, false, 0, false };

            constexpr defaults() : settings::member_ui_t{ .m_pUIBase = &data_v } {}
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
            { .m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::defaulted::guid_v };

            constexpr defaults() : settings::member_ui_t{ .m_pUIBase = &data_v }
            {
                static_assert(type_guid_v == data_v.m_TypeGUID, "What the hells..." );

            }
        };
    };

#ifdef XCORE_PROPERTIES_H
    template<> struct member_ui<xresource::full_guid>
    {
        member_ui() = delete;

        struct data : ui::details::member_ui_base
        {
            std::span<const xresource::type_guid> m_FilerTypes = {};
        };

        inline static constexpr auto type_guid_v = xproperty::settings::var_type<xresource::full_guid>::guid_v;

        template< auto& T_TYPES_SPAN_V >
        struct type_filters : settings::member_ui_t
        {
            inline static constexpr data data_v
            { {.m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::defaulted::guid_v}, T_TYPES_SPAN_V };

            constexpr type_filters() : settings::member_ui_t{ .m_pUIBase = &data_v } {}
        };

        struct defaults : settings::member_ui_t
        {
            inline static constexpr data data_v
            { {.m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::defaulted::guid_v} };

            constexpr defaults() : settings::member_ui_t{ .m_pUIBase = &data_v }
            {
                static_assert(type_guid_v == data_v.m_TypeGUID, "What the hells...");
            }
        };
    };

    template< xresource::type_guid T_GUID_V >
    struct member_ui<xresource::def_guid<T_GUID_V>>
    {
        member_ui() = delete;
        inline static constexpr auto type_guid_v = xproperty::settings::var_type<xresource::def_guid<T_GUID_V>>::guid_v;

        // Static filter span with only the fixed T_GUID_V
        inline static constexpr std::array<xresource::type_guid, 1> filter_span_v = { T_GUID_V };

        using data = member_ui<xresource::full_guid>::data;

        // Reuse full_guid's type_filters with the auto-generated span
        template< auto& T_TYPES_SPAN_V = filter_span_v >
        struct type_filters : settings::member_ui_t
        {
            inline static constexpr data data_v
            { {.m_TypeGUID = type_guid_v, .m_StyleGUID = ui::details::style::defaulted::guid_v}, T_TYPES_SPAN_V };
            constexpr type_filters() : settings::member_ui_t{ .m_pUIBase = &data_v } {}
        };

        // Default to the filtered version
        struct defaults : type_filters<>
        {
            // Inherits the filtered draw behavior
        };
    };
#endif

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

// Optional UI user-data vocabulary. The xproperty core does not depend on it.
namespace xprop
{
    template<xproperty::flags::_flags...T_FLAGS>
    using flags = xproperty::member_flags<T_FLAGS...>;

    template<auto T_CALLBACK_V>
    using dynamic_flags = xproperty::member_dynamic_flags<T_CALLBACK_V>;
}

namespace xprop_ui
{
    template<typename T>
    using for_type = xproperty::member_ui<T>;

    template<bool T_OPEN_V>
    using default_open = xproperty::member_ui_open<T_OPEN_V>;
}

#endif