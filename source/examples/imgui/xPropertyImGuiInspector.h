#ifndef XPROPERTY_IMGUI_INSPECTOR_H
#define XPROPERTY_IMGUI_INSPECTOR_H
#pragma once

#include<functional>
#include<string>
#include<unordered_map>

#ifndef IMGUI_API
    #include "imgui.h"
#endif

#ifndef MY_PROPERTIES_H
    #include "my_properties.h"
#endif

#include "../../sprop/property_sprop.h"

#include "dependencies/xdelegate/source/xdelegate.h"

// Microsoft and its macros....
#undef max

// Disable this warnings
#pragma warning( push )
#pragma warning( disable : 4201)                    // warning C4201: nonstandard extension used: nameless struct/union

//-----------------------------------------------------------------------------------
// Different Editor Styles to display the properties, easy to access for the user.
//-----------------------------------------------------------------------------------
namespace xproperty
{
    class inspector;

    //-----------------------------------------------------------------------------------
    // Undo command information
    //-----------------------------------------------------------------------------------
    namespace ui::undo
    {
        //-----------------------------------------------------------------------------------
        // Base class of the cmd class
        struct cmd
        {
            std::string                     m_Name;                 // Full name of the property been edited
            const xproperty::type::object*  m_pPropObject;          // Pointer to the property table object
            void*                           m_pClassObject;         // Pointer to the class instance
            xproperty::any                  m_NewValue;             // Whenever a value changes we put it here
            xproperty::any                  m_Original;             // This is the value of the property before we made any changes
            union
            {
                std::uint8_t                m_Flags{ 0 };
                struct
                {
                    bool                    m_isEditing   : 1         // Is Item currently been edited
                                          , m_isChange    : 1         // Has the value is Change since the last time
                                          , m_bHasChanged : 1;        // Has the value has changed at some point
                };
            };

            XPROPERTY_DEF
            ( "UndoCmd", cmd
            , obj_member_ro<"Name",     &cmd::m_Name >
            , obj_member_ro<"NewValue", +[](cmd& O, bool bRead, std::string& OutValue)
                {
                    assert(bRead);
                    if (O.m_NewValue.m_pType)
                    {
                        std::array<char, 256> Buffer;
                        xproperty::settings::AnyToString(Buffer, O.m_NewValue);
                        OutValue = Buffer.data();
                    }
                    else
                    {
                        OutValue = "nullptr";
                    }
                }>
            , obj_member_ro<"Original", +[](cmd& O, bool bRead, std::string& OutValue)
                {
                    assert(bRead);
                    if (O.m_Original.m_pType)
                    {
                        std::array<char, 256> Buffer;
                        xproperty::settings::AnyToString(Buffer, O.m_Original);
                        OutValue = Buffer.data();
                    }
                    else
                    {
                        OutValue = "nullptr";
                    }
                }>
            )
        };
        XPROPERTY_REG(cmd)


        //-----------------------------------------------------------------------------------
        struct system
        {
            std::vector<cmd>  m_lCmds     {};
            int               m_Index     { 0 };
            int               m_MaxSteps  { 15 };

            void clear ( void ) noexcept
            {
                m_Index = 0;
                m_lCmds.clear();
            }

            std::string Undo(xproperty::settings::context& Context) noexcept;
            std::string Redo(xproperty::settings::context& Context) noexcept;
            void        Add (const cmd& Cmd) noexcept;

            XPROPERTY_DEF
            ( "Undo System", system
            , obj_member_ro<"Index",    &system::m_Index >
            , obj_member   <"MaxSteps", +[](system& O, bool bRead, int& InOutValue)
                {
                    if( bRead ) InOutValue = O.m_MaxSteps;
                    else
                    {
                        O.m_MaxSteps = std::min(100, std::max(1, InOutValue));

                        if(const int CurSize = static_cast<int>(O.m_lCmds.size()); CurSize >= O.m_MaxSteps )
                        {
                            // We delete the older commands first
                            const int ToDelete = CurSize - O.m_MaxSteps;
                            O.m_lCmds.erase(O.m_lCmds.begin(), O.m_lCmds.begin() + ToDelete);
                        }

                        // Make sure our index is safe
                        if (O.m_Index > O.m_MaxSteps) O.m_Index = O.m_MaxSteps;
                    }

                }
                , member_ui<int>::scroll_bar< 1, 100 > 
                >
            , obj_member_ro<"Commands", &system::m_lCmds >
            )
        };
        XPROPERTY_REG(system)
    }
    namespace ui::details
    {
        struct group_render;
    }
/*
    namespace ui::details
    {
        struct group_render;
    
        template< typename...T_ARGS >
        struct delegate final
        {
            struct info
            {
                using callback = void(void* pPtr, T_ARGS...);

                callback*   m_pCallback;
                void*       m_pClass;
            };

            delegate(const delegate& ) = delete;
            delegate(void) noexcept = default;

            template< auto T_FUNCTION_PTR_V, typename  T_CLASS >
            __inline void Register( T_CLASS& Class ) noexcept
            {
                m_Delegates.push_back
                (
                    info
                    {
                        .m_pCallback = [](void* pPtr, T_ARGS... Args) constexpr noexcept
                        {
                            std::invoke(T_FUNCTION_PTR_V, static_cast<T_CLASS*>(pPtr), std::forward<T_ARGS>(Args)...);
                        }
                    ,  .m_pClass = &Class
                    }
                );
            }

            template< auto T_FUNCTION_PTR_V >
            __inline void Register( void ) noexcept
            {
                m_Delegates.push_back
                (
                    info
                    {
                        .m_pCallback = [](void*, T_ARGS... Args) constexpr noexcept
                        {
                            std::invoke(T_FUNCTION_PTR_V, std::forward<T_ARGS>(Args)...);
                        }
                    ,  .m_pClass = nullptr
                    }
                );
            }

            constexpr
            __inline void NotifyAll( T_ARGS... Args ) const noexcept
            {
                for (const auto& D : m_Delegates)
                {
                    D.m_pCallback(D.m_pClass, std::forward<T_ARGS>(Args)...);
                }
            }

            template< typename T_CLASS >
            void RemoveDelegate(T_CLASS& Class) noexcept
            {
                m_Delegates.erase
                (
                    std::remove_if
                    (
                        m_Delegates.begin(), m_Delegates.end()
                        , [&](const info& I) noexcept
                        {
                            return I.m_pClass == &Class;
                        }
                    )
                );
            }

            std::vector<info>   m_Delegates{};
        };
    }
*/
}

// Free-standing (not nested inside xproperty::inspector, unlike its old spot) so the
// reflected_type<ImVec2> specialization right below it can be visible BEFORE
// xproperty::inspector::settings (further down, inside the class) declares a plain ImVec2 field -
// an explicit specialization must be visible before its own template's first (implicit)
// instantiation, and settings' fields are declared as plain ImVec2 specifically so this redirect
// resolves them to this struct's XPROPERTY_DEF. aliased back to xproperty::inspector::v2 inside the
// class below, purely for the familiar short name - nothing outside this file ever names it directly.
namespace xproperty
{
    struct inspector_v2 : ImVec2
    {
        using ImVec2::ImVec2;

        XPROPERTY_DEF
        ( "ImVec2", ImVec2, xproperty::settings::vector2_group
        , obj_member<"X", &ImVec2::x, member_ui<float>::scroll_bar<0.0f, 20.0f>, member_help<"X element of a vector"> >
        , obj_member<"Y", &ImVec2::y, member_ui<float>::scroll_bar<0.0f, 20.0f>, member_help<"Y element of a vector"> >
        )
    };
}

// ImVec2 itself can't have XPROPERTY_DEF injected into it (it's ImGui's, not ours) - inspector_v2
// above carries its reflection instead. This is the explicit link
// xproperty::settings::validate_reflected_object_type() and cast_scope look for when a member is
// declared as a plain ImVec2. MUST be declared before anything actually uses a plain ImVec2 field
// (xproperty::inspector::settings, further down) - confirmed live as C2908/C2766 "explicit
// specialization already instantiated/defined" the first time this was declared AFTER settings
// instead of before: the compiler locks in the unspecialized primary template at first use and
// then refuses to let a later specialization override that already-instantiated choice.
template<> struct xproperty::settings::reflected_type<ImVec2> { using type = xproperty::inspector_v2; };

//-----------------------------------------------------------------------------------
// Inspector to display the properties
//-----------------------------------------------------------------------------------

class xproperty::inspector : public xproperty::base
{
public:

    // Defined at namespace scope, above this class, as xproperty::inspector_v2 - see that
    // definition's own comment for why (the reflected_type<ImVec2> specialization it needs must be
    // visible before settings, right below, uses a plain ImVec2 field).
    using v2 = xproperty::inspector_v2;

    struct settings
    {
        // Declared as plain ImVec2, not v2 - v2's OWN registration (XPROPERTY_REG2(v2_props,
        // xproperty::inspector::v2)) actually populates get_obj_info<ImVec2>, not get_obj_info<v2>,
        // because v2's XPROPERTY_DEF("ImVec2", ImVec2, ...) uses ImVec2 (not v2) as its object-type
        // argument - needed to resolve &ImVec2::x/&ImVec2::y, since those are inherited, not v2's own.
        // A field declared AS v2 directly looks up the never-populated get_obj_info<v2> and asserts
        // null - confirmed live via 'Assertion failed: type::get_obj_info<key_t> != nullptr' the
        // first time anything ever called getObject() on this settings struct. Plain ImVec2 correctly
        // finds v2's reflection through the reflected_type<ImVec2> redirect just above v2's own
        // definition - exactly the usage that redirect's own comment describes.
        ImVec2      m_WindowPadding             { 0, 3 };
        ImVec2      m_FramePadding              { 1, 3.5 };
        ImVec2      m_ItemSpacing               { 0.5f, 2.0f };
        float       m_IndentSpacing             { 3.5 };
        ImVec2      m_TableFramePadding         { 2, 6 };

        bool        m_bRenderLeftBackground     { true };
        bool        m_bRenderRightBackground    { true };
        bool        m_bRenderBackgroundDepth    { true };
        float       m_ColorVScalar1             { 0.5f };
        float       m_ColorVScalar2             { 0.4f };
        float       m_ColorSScalar              { 0.4f };

        ImVec2      m_HelpWindowPadding         { 10, 10 };
        int         m_HelpWindowSizeInChars     { 50 };

        XPROPERTY_DEF
        ( "Settings", settings
        , obj_member<"WindowPadding", &settings::m_WindowPadding,  member_help<"Blank Border for the property window"> >
        , obj_member<"FramePadding",  &settings::m_FramePadding,   member_help<"Main/Top Property Border size"> >
        , obj_member<"ItemSpacing",   &settings::m_ItemSpacing,    member_help<"Main/Top Property Border size"> >
        , obj_member<"IndentSpacing", &settings::m_IndentSpacing,  member_help<"Main/Top Property Border size"> >
        , obj_scope 
            < "Background"
            , obj_member<"RenderLeft",      &settings::m_bRenderLeftBackground, member_help<"Disable the rendering of the background on the left"> >
            , obj_member<"RenderRight",     &settings::m_bRenderRightBackground, member_help<"Disable the rendering of the background on the right"> >
            , obj_member<"Depth",           &settings::m_bRenderBackgroundDepth
                                                , member_dynamic_flags<+[]( const settings& S )
                                                {
                                                    if (S.m_bRenderLeftBackground == false && S.m_bRenderRightBackground == false) return xproperty::flags::type{ .m_Value = xproperty::flags::DONT_SHOW };
                                                    return xproperty::flags::type{ .m_Value = 0 };
                                                }>
                                                , member_help<"Disable the rendering of multiple color background"> >
            , obj_member<"ColorVScalar1",   &settings::m_ColorVScalar1
                                                , member_ui<float>::scroll_bar<0.0f, 2.0f>
                                                , member_dynamic_flags<+[]( const settings& S )
                                                {
                                                    if (S.m_bRenderLeftBackground == false && S.m_bRenderRightBackground == false) return xproperty::flags::type{ .m_Value = xproperty::flags::DONT_SHOW };
                                                    return xproperty::flags::type{ .m_Value = 0 };
                                                }>
                                                , member_help<"Changes the Luminosity of one of the alternate colors for the background"> >
            , obj_member<"ColorVScalar2",   &settings::m_ColorVScalar2
                                                , member_ui<float>::scroll_bar<0.0f, 2.0f>
                                                , member_dynamic_flags<+[]( const settings& S )
                                                {
                                                    if (S.m_bRenderLeftBackground == false && S.m_bRenderRightBackground == false) return xproperty::flags::type{ .m_Value = xproperty::flags::DONT_SHOW };
                                                    return xproperty::flags::type{ .m_Value = 0 };
                                                }>
                                                , member_help<"Changes the Luminosity of one of the alternate colors for the background"> >
            , obj_member<"ColorSScalar",    &settings::m_ColorSScalar
                                                , member_ui<float>::scroll_bar<0.0f, 10.0f>
                                                , member_dynamic_flags<+[]( const settings& S )
                                                {
                                                    if (S.m_bRenderLeftBackground == false && S.m_bRenderRightBackground == false) return xproperty::flags::type{ .m_Value = xproperty::flags::DONT_SHOW };
                                                    return xproperty::flags::type{ .m_Value = 0 };
                                                }>
                                                , member_help<"Changes the Saturation for all the colors in the background"> >
            >
        , obj_scope
            < "Help Popup"
            , obj_member<"HelpWindowPadding",       &settings::m_HelpWindowPadding,     member_help<"Border size"> >
            , obj_member<"HelpWindowSizeInChars",   &settings::m_HelpWindowSizeInChars, member_ui<int>::scroll_bar<1, 200>, member_help<"Max Size of the help window popup when it opens"> >
            >
        )
    };


public:

    inline                  inspector               ( const char* pName="Inspector", bool isOpen = true)    noexcept : m_pName{pName}, m_bWindowOpen{isOpen}
    {
        // Set the default real time handler... user can always clear and set their own if they want...
        m_OnRealtimeChangeEvent.Register< [](xproperty::inspector& Inspector, const xproperty::ui::undo::cmd& Cmd, xproperty::settings::context& Context)
        {
            std::string Error;
            xproperty::sprop::setProperty(Error, Cmd.m_pClassObject, *Cmd.m_pPropObject, xproperty::sprop::container::prop{ Cmd.m_Name, Cmd.m_NewValue }, Context);
            if (!Error.empty()) printf("Error: %s\n", Error.c_str());
        }>();

        m_OnGetComponentPointer.Register < [](xproperty::inspector& Inspector, const int Index, void*& pBase, void* pUserData)
        {
            // We are not replacing anything....
        }>();

        m_OnResourceLeftSize.Register < [](xproperty::inspector& Inspector, const xproperty::type::object&, void*, std::string_view Path, const xproperty::any&, ImGuiTreeNodeFlags flags, const char* pName, bool& Open)
        {
            Inspector.RenderBackground();
            // Path is a stable, genuinely unique identity (unlike pName, which can repeat across
            // sibling rows) - hashed into a void*-shaped id the same way other per-row ids in this
            // file already derive stability from a path hash rather than trusting the label text.
            if (!Path.empty()) Open = ImGui::TreeNodeEx(reinterpret_cast<void*>(std::hash<std::string_view>{}(Path)), flags, "%s", pName);
            else                Open = ImGui::TreeNodeEx(pName, flags);
        } > ();
    }
    virtual                ~inspector               ( void )                                                noexcept = default;
                void        clear                   ( void )                                                noexcept;
                void        AppendEntity            ( void )                                                noexcept;
                void        AppendEntityComponent   ( const xproperty::type::object& PropObject, void* pBase, void* pUserData = nullptr) noexcept;
                void        Show                    ( xproperty::settings::context& Context, std::function<void(void)> Callback ) noexcept;
                bool        empty                   ( void )                                        const   noexcept { return m_lEntities.empty(); }
    inline      void        setupWindowSize         ( int Width, int Height )                               noexcept { m_Width = Width; m_Height = Height; }
    inline      void        setOpenWindow           ( bool b )                                              noexcept { m_bWindowOpen = b; }
    constexpr   bool        isWindowOpen            ( void )                                        const   noexcept { return m_bWindowOpen; }
    inline      auto        getComponent            ( int iEntity, int iComponent )                 const   noexcept { return m_lEntities[iEntity]->m_lComponents[iComponent]->m_Base; }
    inline      auto        getName                 ( void )                                        const   noexcept { return m_pName; }

    using on_change_event           = xdelegate::thread_unsafe<inspector&, const xproperty::ui::undo::cmd& >;
    using on_realtime_change_event  = xdelegate::thread_unsafe<inspector&, const xproperty::ui::undo::cmd&, xproperty::settings::context& >;
    using on_get_component_pointer  = xdelegate::thread_unsafe<inspector&, const int, void*&, void*>;

    // All 3 resource-picker callbacks below identify "which property is this" the same way
    // m_OnOverrideCheck/m_OnOverrideReset do - the real (type::object&, instance) pair plus the full
    // canonical property path, instead of an opaque per-widget id or (m_OnResourceLeftSize's old
    // shape) the property's own RENDERED DISPLAY TEXT, which is what forced E20_Material_Instance_
    // Editor.cpp to string-parse `if (pName[0] == '[')` to figure out which texture slot it was even
    // looking at. m_OnResourceBrowser/m_OnResourceWigzmos already receive the value directly as a
    // strongly-typed full_guid& (no need for a redundant xproperty::any on top); m_OnResourceLeftSize
    // previously had zero value access at all, so it gains a resolved xproperty::any the same way
    // m_OnOverrideCheck does.
#ifdef XCORE_PROPERTIES_H
    using on_resource_browser       = xdelegate::thread_unsafe<inspector&, const xproperty::type::object&, void*, std::string_view, bool&, xresource::full_guid&, std::span<const xresource::type_guid>>;
    using on_resource_wigzmos       = xdelegate::thread_unsafe<inspector&, const xproperty::type::object&, void*, std::string_view, bool&, const xresource::full_guid&>;
#endif
    using on_resource_leftside = xdelegate::thread_unsafe<inspector&, const xproperty::type::object&, void*, std::string_view, const xproperty::any&, ImGuiTreeNodeFlags, const char*, bool&>;

    // Whether a given property currently differs from some consumer-defined notion of "its base
    // value" (a prefab/template/material-instance source, etc.) - xproperty never tries to know what
    // "overridden" means itself, it just calls out with everything it already has on hand: the real
    // (type::object&, instance) pair (so the consumer can re-query via sprop::getProperty against a
    // second/base object if that's their strategy) and the FULL property path (already a complete,
    // canonical key - it embeds any array index itself, e.g. "m_lTextures[G:2]", so it works as an
    // opaque lookup key into a consumer-owned override-set just as well as it works as a getProperty
    // argument - no parsing needed either way) plus the already-resolved current value, so a simple
    // consumer doesn't even need to re-fetch it.
    using on_override_check = xdelegate::thread_unsafe<inspector&, const xproperty::type::object&, void*, std::string_view, const xproperty::any&, bool&>;
    using on_override_reset = xdelegate::thread_unsafe<inspector&, const xproperty::type::object&, void*, std::string_view>;

    // First (least invasive) of 4 planned levels of custom-rendering control, in increasing order of
    // how much of a row's normal rendering gets taken over: (1) append after the normal value widget
    // - this one, purely additive, nothing skipped; (2) replace the value widget entirely, left column
    // untouched; (3) replace the whole row except structural controls (expand arrow, override-revert
    // button); (4) replace multiple rows, taking over rendering until the consumer signals it should
    // resume. Fired unconditionally for every property, same idiom as on_override_check/
    // on_resource_browser above - the consumer checks Path itself to decide whether to draw anything,
    // no separate per-property registration needed. Called once per entry, right after its value
    // column finishes rendering (whichever of Render()'s several internal paths - read-only, mid-edit-
    // continuation, or the normal HandleElement call - actually ran that frame), so it fires exactly
    // once regardless of which one it was.
    using on_custom_render_append = xdelegate::thread_unsafe<inspector&, const xproperty::type::object&, void*, std::string_view, const xproperty::any&>;

    // Level 2 of the same 4-level plan: replace the value column's default widget entirely - the left
    // column (tree/label) still renders normally, this only decides whether the RIGHT column's default
    // widget gets skipped. Same "fires for every property, consumer checks Path" idiom; the trailing
    // bool& starts false (normal rendering) and the consumer opts in per-property by setting it true.
    using on_custom_render_replace_value = xdelegate::thread_unsafe<inspector&, const xproperty::type::object&, void*, std::string_view, const xproperty::any&, bool&>;

    // Level 3 of the same 4-level plan: replace the LEFT column (the tree-node/label draw) - the
    // override-revert ">" button, if any, still renders first, unconditionally, regardless of this.
    // Render()'s own column transition (ImGui::NextColumn()) happens automatically right after this
    // fires, before the right column's own code runs - so this delegate alone only ever replaces the
    // left half of the row. Setting the trailing bool true ALSO seeds level 2's own bHandled (skipping
    // the default value widget), so a consumer wanting the whole row custom registers BOTH this AND
    // on_custom_render_replace_value for the same property - one drawing the label side, the other the
    // value side - rather than one delegate somehow spanning both columns in a single call. Composable
    // with level 1 (append-after) regardless - that one still fires afterward, on the same row.
    using on_custom_render_replace_row = xdelegate::thread_unsafe<inspector&, const xproperty::type::object&, void*, std::string_view, const xproperty::any&, bool&>;

    // Genuinely separate from the 3 levels above, not a 5th one layered on top of them - those all
    // still render INSIDE the property grid's 2-column layout (just skipping which parts of a single
    // row's own cells draw). This one actually leaves the grid: fired once per property, BEFORE any
    // column/row machinery runs at all, with the trailing bool starting false (normal grid rendering).
    // Setting it true tells Render() to call ImGui::Columns(1) for the duration of this ONE property's
    // draw, hand the consumer the full window width with none of the row's usual structural extras
    // (no override-revert button, no help tooltip, no background stripe - real content like a curve
    // editor needs the whole canvas, not a column-constrained cell fighting those for space), then
    // restore ImGui::Columns(2) immediately after so whatever property comes next renders normally.
    // Indentation is NOT reset - ImGui's own indent stack (from real TreePush/TreePop on open ancestor
    // scopes) is independent of column state, so a block nested inside an expanded scope still lines
    // up with that scope's other children, matching how nested content reads in other inspectors.
    // A "block" spanning several properties (a curve editor's title, body, footer as 3 separate
    // reflected members) is purely consumer-side state, same idiom as level 4 already was - the
    // consumer's own path-check/running-flag decides which properties set the bool true; Render()
    // does not need to know a block's extent in advance. The ImColor is this row's own striping
    // color (identical to what DrawBackground would have used) - handed over rather than applied
    // automatically, in case the consumer wants to paint their own partial background with it;
    // ImColor rather than a plain ImVec4 so it converts to whichever of ImU32/ImVec4 the consumer's
    // own drawing call happens to need, same as DrawBackground's own internal color already does.
    //
    // Fires TWICE for a property that claims the block, once for each bool bDryRun value - NOT once.
    // A first version fired unconditionally-once per property already wrapped in Columns(1)/Columns(2)
    // before knowing the answer, since there was no other way to let the consumer draw on demand - but
    // doing that EVERY property (not just claimed ones) corrupted this codebase's Columns()-drawn
    // vertical grid border for the WHOLE panel, and a genuinely full-width block still ended up
    // reading as separate one-line rows instead of one continuous span - confirmed live, both
    // symptoms, in the same session. Splitting into an ask phase (bDryRun=true, decide only - most
    // properties answer false here and NOTHING about Columns() is ever touched for them) and a draw
    // phase (bDryRun=false, Columns(1) genuinely active - only reached at all when the ask phase said
    // yes) keeps every non-block property's grid rendering completely undisturbed. State the consumer
    // mutates to decide bIsBlockContent (e.g. "are we still inside a block that started earlier") must
    // update on BOTH calls, not just the live one - a later property's own ask-phase check needs to
    // see it.
    using on_custom_render_block = xdelegate::thread_unsafe<inspector&, const xproperty::type::object&, void*, std::string_view, const xproperty::any&, ImColor, bool /*bDryRun*/, bool&>;

    settings                    m_Settings {};
    on_change_event             m_OnChangeEvent;            // This is the official change of value, this is where the undo system should be called
    on_realtime_change_event    m_OnRealtimeChangeEvent;    // When sliders and such happens property can change in real time but they are not yet consider an official change
                                                            //      User should use this to update the property in real time.

    on_get_component_pointer    m_OnGetComponentPointer;    // To subscribe the arguments look as follows
                                                            // inspector& - In, The instance of this class
                                                            // const int  - In, The index of the component
                                                            // void*&     - (In+Out), In - The pointer registered with the component
                                                            //                        Out - if the user wants to change it he should replace the correct value
                                                            // void*      - In, The user data given when the component was registered
                                                            // The system uses this event when it needs to display the component

#ifdef XCORE_PROPERTIES_H
    on_resource_browser         m_OnResourceBrowser;        // When the user needs to adquire a resource the system will isssue a event here...
    on_resource_wigzmos         m_OnResourceWigzmos;        // This callback is used to collect the name of the resource

    // Which property is "currently being drawn," for the resource-type draw<T,Style>::Render
    // specializations (e.g. draw<xresource::full_guid, style::defaulted>::Render) that call
    // m_OnResourceWigzmos/m_OnResourceBrowser - those specializations are reached through the same
    // generic value-drawing dispatch every type goes through (onRender -> ResolveDrawFn ->
    // draw<T,Style>::Render), several calls removed from Render()'s own per-entry loop where the
    // real path/object/instance actually live, and changing that whole generic pipeline's signature
    // to carry them would touch every registered type's draw fn, not just resource types. Render()
    // sets this right before drawing each entry's value instead - same side-channel role g_pInspector
    // itself already plays for reaching the current inspector from outside its own call chain.
    struct current_property_t
    {
        const xproperty::type::object* m_pObject   = nullptr;
        void*                          m_pInstance = nullptr;
        std::string_view               m_Path      = {};
    } m_CurrentProperty;
#endif
    on_resource_leftside        m_OnResourceLeftSize;       // Gets the height of the

    on_override_check           m_OnOverrideCheck;          // Registered by a consumer that has some notion of "base value" for its own properties (a prefab/template/material-instance source) - called per row; if it reports true, the row renders with an override indicator and a revert button
    on_override_reset           m_OnOverrideReset;          // Fired when the revert button (above) is clicked - consumer's job to actually remove/reset the override however that's meaningful for their own data model

    on_custom_render_append         m_OnCustomRenderAppend;         // Fired once per property right after its normal value widget renders - lets a consumer draw additional content on the SAME row without replacing anything (level 1 of 4 planned custom-rendering levels, see the using declaration's own comment)
    on_custom_render_replace_value  m_OnCustomRenderReplaceValue;   // Fired once per property BEFORE its value column would normally render - consumer sets the trailing bool true to draw its own widget instead and skip the default one entirely (level 2 of 4)
    on_custom_render_replace_row    m_OnCustomRenderReplaceRow;     // Fired once per property BEFORE its left-column label would normally render - consumer sets the trailing bool true to take over BOTH columns (the override-revert button, if any, still renders regardless) (level 3 of 4)
    on_custom_render_block          m_OnCustomRenderBlock;          // Fired once per property before ANY grid machinery runs - consumer sets the trailing bool true to escape the 2-column grid entirely for this property's draw (full window width, no structural extras) - see the using declaration's own comment

    void RenderBackground()
    {
        if (m_Settings.m_bRenderLeftBackground)
        {
            const ImVec2 Pos = ImGui::GetCursorScreenPos();
            DrawBackground(m_SimpleDrawBk.m_iDepth, m_SimpleDrawBk.m_GlobalIndex, Pos, Pos.y + ImGui::GetFrameHeight());
        }
    }

protected:

    struct entry
    {
        int                                             m_LeftUIGUID;
        int                                             m_RightUIGUID;
        xproperty::sprop::container::prop               m_Property;
        const char*                                     m_pHelp;
        const char*                                     m_pName;
        std::uint32_t                                   m_GUID;
        std::uint32_t                                   m_GroupGUID;
        const xproperty::type::members*                 m_pUserData;
        int                                             m_Dimensions;
        int                                             m_MyDimension;
        xproperty::flags::type                          m_Flags;
        bool                                            m_bScope;
        bool                                            m_bAtomicArray;
        bool                                            m_bDefaultOpen;
        void*                                           m_pInstance = nullptr; // the live class instance this entry belongs to - only meaningful for a member-function entry, which needs it to invoke through
        const char*                                     m_pSectionName = nullptr; // member_section tag value, if any - drives the layout pass's section-separator draw
        float                                           m_ItemWidth = -1.0f; // member_item_width/member_dynamic_item_width, if any - passed to ImGui::PushItemWidth() for this property's value widget; -1 (fill the column) unless overridden
    };

    struct component
    {
        std::pair<const xproperty::type::object*, void*>    m_Base      = { nullptr,nullptr };
        void*                                               m_pUserData = { nullptr };;
        std::vector<std::unique_ptr<entry>>                 m_List      = {};
    };

    struct entity
    {
        std::vector<std::unique_ptr<component>>     m_lComponents {};
    };

    struct simple_draw_background
    {
        bool m_bRenderLeftBackground;
        int  m_iDepth;
        int  m_GlobalIndex;
    };

protected:

    void        RefreshAllProperties                ( component& C )                                noexcept;
    void        Render                              ( component& C, int& GlobalIndex )              noexcept;
    void        Show                                ( void )                                        noexcept;
    void        DrawBackground                      ( int Depth, int GlobalIndex, ImVec2 StartPos, float EndY ) const noexcept;
    void        DrawBackground                      ( int Depth, int GlobalIndex, ImVec2 StartPos, float EndY, float Width ) const noexcept;
    ImColor     ComputeRowColor                      ( int Depth, int GlobalIndex )          const   noexcept;
    void        HelpMarker                          ( const char* desc )                    const   noexcept;
    void        Help                                ( const entry& Entry )                  const   noexcept;

protected:

    using cmd_variant = std::variant< entry*, xproperty::ui::undo::cmd >;

    const char*                                 m_pName         {nullptr};
    std::vector<std::unique_ptr<entity>>        m_lEntities     {};
    int                                         m_Width         {430};
    int                                         m_Height        {450};
    bool                                        m_bWindowOpen   { true };
    xproperty::settings::context*               m_pContext      {nullptr};
    cmd_variant                                 m_CmdCurrentEdit{ nullptr };
    simple_draw_background                      m_SimpleDrawBk  {};

    // Every component (and every block-escape's own Columns(1)/Columns(2) resume) gets its OWN,
    // independently-scoped ImGui Columns() storage - that's deliberate, since forcing them to share
    // one ImGui id makes ImGui's own resize-handle hit-test buttons collide across simultaneously-
    // visible components ("2 visible items with conflicting ID!"). Instead, this mirrors the
    // divider's position across all of them every frame (see the per-component loop in Show()).
    //
    // Mirroring via SetColumnWidth()/GetColumnWidth() (pixels) was tried first and reverted: a
    // column's PIXEL width is computed from its stored ratio times the CURRENT OffMinX/OffMaxX
    // range, which ImGui recomputes on every single BeginColumns() call - including the block
    // feature's own Columns(1)->Columns(2) resume - from whatever window->WorkRect happens to be at
    // that instant. A block's own full-width content transiently changes that work rect, so the
    // SAME underlying ratio read back as a DIFFERENT pixel width right after a block resumed -
    // confirmed live via debug logging (294.5 -> 299.8 -> 301.5px across two nested block resumes in
    // one frame, despite never touching SetColumnWidth in between) and then even more dramatically
    // once the user dragged the divider near an extreme, where "After Block"'s value column visibly
    // jumped far right of every normal row's. Mirroring the RATIO instead (ImGuiOldColumns's own
    // Columns[1].OffsetNorm, 0.0=far left..1.0=far right - a pure fraction, never touched by
    // OffMinX/OffMaxX) sidesteps this entirely: it's the same quantity ImGui itself persists across a
    // block's resume, just also propagated across components. Requires imgui_internal.h (already
    // included in the .cpp for other things). -1 means "no drag yet, leave ImGui's own default
    // (evenly split) alone".
    // Every distinct "PropsGrid" Columns() call site - the outer per-component establishment in
    // Show(), AND each block-escape's own resume inside Render() (a fresh Columns() call made from
    // one id-stack level deeper, past Render()'s own top-level PushTree() - confirmed live to hash to
    // a genuinely different ImGuiOldColumns entry than the outer call, not the same one) - keeps its
    // OWN, independently-scoped ImGui storage on purpose. Forcing any of them to share one ImGui id
    // (tried twice: once across components, once between a block's resume and the outer call) makes
    // ImGui's own resize-handle hit-test button collide, since that button's id is derived directly
    // from the SAME columns id and this mechanism opens/closes several "PropsGrid" sessions per frame
    // by design (ImGui's classic Columns() only expects one open/close per id per frame) - "2 visible
    // items with conflicting ID!" both times. Instead, this single shared value is mirrored into
    // EVERY session (see the per-component loop in Show(), and both block-escape transitions in
    // Render()): stamped in right after each one establishes, read back right before each one closes
    // - so a drag on any one of them updates this, and every other session (any component, any block)
    // picks it up, without any of their underlying Columns() sessions - or resize handles - ever
    // being the same ImGui widget.
    //
    // Mirroring via SetColumnWidth()/GetColumnWidth() (pixels) was tried first and reverted: a
    // column's PIXEL width is computed from its stored ratio times the CURRENT OffMinX/OffMaxX
    // range, which ImGui recomputes on every single BeginColumns() call - including a block's own
    // resume - from whatever window->WorkRect happens to be at that instant. A block's own full-width
    // content transiently changes that work rect, so the SAME underlying ratio read back as a
    // DIFFERENT pixel width right after a block resumed - confirmed live via debug logging (294.5 ->
    // 299.8 -> 301.5px across two nested block resumes in one frame, despite never touching
    // SetColumnWidth in between) and then even more dramatically once the user dragged the divider
    // near an extreme, where "After Block"'s value column visibly jumped far right of every normal
    // row's. Mirroring the RATIO instead (ImGuiOldColumns's own Columns[1].OffsetNorm, 0.0=far
    // left..1.0=far right - a pure fraction, never touched by OffMinX/OffMaxX) sidesteps this
    // entirely: it's the same quantity ImGui itself already persists across a block's resume, just
    // also propagated across every other session. Requires imgui_internal.h (already included in the
    // .cpp for other things). -1 means "no drag yet, leave ImGui's own default (evenly split) alone".
    float                                        m_SharedColumnRatio { -1.0f };

    // A row's LEFT background is drawn before its RIGHT (value) column even renders, so it can't
    // know in advance whether this row is about to grow past one line (APPEND_NEW_LINE). Rather
    // than guess via a formula - which drifted out of sync the moment real frame-height content
    // (buttons, not just plain text) appeared on the appended line, confirmed live - this caches
    // each row's actual extra height, keyed by its PathHash, as measured by the RIGHT column a
    // moment later in the SAME frame. The LEFT draw then uses LAST frame's cached value, which is
    // correct except for exactly one frame right after a row's append content genuinely changes
    // height - self-correcting from the very next frame on, and never wrong for the (overwhelming)
    // common case of a row whose shape doesn't change frame to frame.
    std::unordered_map<int, float>              m_RowExtraHeightCache {};

    friend struct ui::details::group_render;

    XPROPERTY_VDEF
    ( "Inspector", inspector
    , obj_member<"Settings",   &inspector::m_Settings >
    )
};

XPROPERTY_VREG2(inspect_props,  xproperty::inspector)
XPROPERTY_REG2(v2_props,        xproperty::inspector_v2)
XPROPERTY_REG2(settings_props,  xproperty::inspector::settings)

#pragma warning( pop )
#endif