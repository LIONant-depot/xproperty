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
}

//-----------------------------------------------------------------------------------
// Inspector to display the properties
//-----------------------------------------------------------------------------------

class xproperty::inspector : public xproperty::base
{
public:

    struct v2 : ImVec2
    {
        XPROPERTY_DEF
        ( "ImVec2", ImVec2, xproperty::settings::vector2_group
        , obj_member<"X", &ImVec2::x, member_ui<float>::scroll_bar<0.0f, 20.0f>, member_help<"X element of a vector"> >
        , obj_member<"Y", &ImVec2::y, member_ui<float>::scroll_bar<0.0f, 20.0f>, member_help<"Y element of a vector"> >
        )
    };

    struct settings
    {
        ImVec2      m_WindowPadding             { 0, 3 };
        ImVec2      m_FramePadding              { 1, 3.5 };
        ImVec2      m_ItemSpacing               { 0.5f, 1.5f };
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


    using on_change_event           = ui::details::delegate<inspector&, const xproperty::ui::undo::cmd& >;
    using on_realtime_change_event  = ui::details::delegate<inspector&, const xproperty::ui::undo::cmd&, xproperty::settings::context& >;
    using on_get_component_pointer  = ui::details::delegate<inspector&, const int, void*&, void*>;

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



protected:

    struct entry
    {
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

    friend struct ui::details::group_render;

    XPROPERTY_VDEF
    ( "Inspector", inspector
    , obj_member<"Settings",   &inspector::m_Settings >
    )
};

XPROPERTY_VREG2(inspect_props,  xproperty::inspector)
XPROPERTY_REG2(v2_props,        xproperty::inspector::v2)
XPROPERTY_REG2(settings_props,  xproperty::inspector::settings)

#pragma warning( pop ) 
#endif