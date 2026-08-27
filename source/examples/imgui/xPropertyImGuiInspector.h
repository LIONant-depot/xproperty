#ifndef XPROPERTY_IMGUI_INSPECTOR_H
#define XPROPERTY_IMGUI_INSPECTOR_H
#pragma once

#include<functional>
#include<string>

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

//-----------------------------------------------------------------------------------
// Inspector to display the properties
//-----------------------------------------------------------------------------------

class xproperty::inspector : public xproperty::base
{
public:

    struct v2 : ImVec2
    {
        using ImVec2::ImVec2;

        XPROPERTY_DEF
        ( "ImVec2", ImVec2, xproperty::settings::vector2_group
        , obj_member<"X", &ImVec2::x, member_ui<float>::scroll_bar<0.0f, 20.0f>, member_help<"X element of a vector"> >
        , obj_member<"Y", &ImVec2::y, member_ui<float>::scroll_bar<0.0f, 20.0f>, member_help<"Y element of a vector"> >
        )
    };

    struct settings
    {
        v2          m_WindowPadding             { 0, 3 };
        v2          m_FramePadding              { 1, 3.5 };
        v2          m_ItemSpacing               { 0.5f, 1.5f };
        float       m_IndentSpacing             { 3.5 };
        v2          m_TableFramePadding         { 2, 6 };

        bool        m_bRenderLeftBackground     { true };
        bool        m_bRenderRightBackground    { true };
        bool        m_bRenderBackgroundDepth    { true };
        float       m_ColorVScalar1             { 0.5f };
        float       m_ColorVScalar2             { 0.4f };
        float       m_ColorSScalar              { 0.4f };

        v2          m_HelpWindowPadding         { 10, 10 };
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

    on_custom_render_append     m_OnCustomRenderAppend;     // Fired once per property right after its normal value widget renders - lets a consumer draw additional content on the SAME row without replacing anything (level 1 of 4 planned custom-rendering levels, see the using declaration's own comment)

    void RenderBackground()
    {
        if (m_Settings.m_bRenderLeftBackground)
        {
            DrawBackground(m_SimpleDrawBk.m_iDepth, m_SimpleDrawBk.m_GlobalIndex);
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
    void        DrawBackground                      ( int Depth, int GlobalIndex )          const   noexcept;
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

    friend struct ui::details::group_render;

    XPROPERTY_VDEF
    ( "Inspector", inspector
    , obj_member<"Settings",   &inspector::m_Settings >
    )
};

XPROPERTY_VREG2(inspect_props,  xproperty::inspector)
XPROPERTY_REG2(v2_props,        xproperty::inspector::v2)
XPROPERTY_REG2(settings_props,  xproperty::inspector::settings)

// ImVec2 itself can't have XPROPERTY_DEF injected into it (it's ImGui's, not ours) - v2 above carries
// its reflection instead. This is the explicit link xproperty::settings::validate_reflected_object_type()
// and cast_scope look for when a member is declared as a plain ImVec2.
template<> struct xproperty::settings::reflected_type<ImVec2> { using type = xproperty::inspector::v2; };

#pragma warning( pop ) 
#endif