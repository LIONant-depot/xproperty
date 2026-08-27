#define NOMINMAX
#include "dependencies/xproperty/source/examples/imgui/xPropertyImGuiInspector.h"
#include "dependencies/xproperty/source/sprop/property_sprop_getset.h"
#include "dependencies/xproperty/source/sprop/property_sprop_collector.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <olectl.h>
#include <shobjidl.h>
#include <comdef.h>
//#include <shlwapi.h> // For PathMatchSpecW
#include "calculator.cpp"

#include "imgui_internal.h"

#pragma comment( lib, "shlwapi.lib") // For PathMatchSpecW
#pragma comment( lib, "comdlg32.lib") // For GetOpenFileNameW - a plain project template typically links
                                       // this by default, so a consumer without that default (e.g. a
                                       // bare cl.exe invocation) would otherwise get an unresolved
                                       // external for it.

#include <unordered_map>

namespace xproperty::ui::details
{
    namespace
    {
        // (TypeGUID, StyleGUID) packed into one key - a few dozen entries total, all registered once
        // at static init, so a plain hash map lookup at draw time is more than fast enough (this is a
        // once-per-visible-widget-per-frame cost, nowhere near a hot inner loop).
        constexpr std::uint64_t MakeDrawKey(std::uint32_t TypeGUID, std::uint32_t StyleGUID) noexcept
        {
            return (std::uint64_t(TypeGUID) << 32) | StyleGUID;
        }

        std::unordered_map<std::uint64_t, draw_fn*>& GetDrawRegistry() noexcept
        {
            static std::unordered_map<std::uint64_t, draw_fn*> s_Registry;
            return s_Registry;
        }
    }

    void RegisterDrawFn(std::uint32_t TypeGUID, std::uint32_t StyleGUID, draw_fn* pFn) noexcept
    {
        GetDrawRegistry()[MakeDrawKey(TypeGUID, StyleGUID)] = pFn;
    }

    draw_fn* ResolveDrawFn(std::uint32_t TypeGUID, std::uint32_t StyleGUID) noexcept
    {
        auto& Registry = GetDrawRegistry();
        auto  It       = Registry.find(MakeDrawKey(TypeGUID, StyleGUID));
        return It == Registry.end() ? nullptr : It->second;
    }

    namespace
    {
        template<typename T, typename Style>
        void RegisterOne() noexcept
        {
            // Keyed by xproperty::type::atomic_v<T>.m_GUID - the SAME descriptor any::Reset<T>() tags
            // a value with (Value.m_pType == &atomic_v<T>) - not settings::var_type<T>::guid_v, which
            // can disagree with it depending on header include order (a pre-existing, previously-inert
            // mismatch: the old function-pointer dispatch never compared GUIDs for anything but a
            // since-disabled sanity assert, so it never mattered before).
            RegisterDrawFn(xproperty::type::atomic_v<T>.m_GUID, Style::guid_v, &DrawErased<T, Style>);
        }

        // One-time registration of every (Type,Style) combo this file actually implements a real
        // draw<T,Style>::Render for - the ONLY place any binary ever instantiates DrawErased/Render.
        // A plugin that merely attaches a style to a member (member_ui<T>::Style<...>) never does,
        // since member_ui_base now stores just two GUIDs (see my_property_ui.h) - so styled/read-only
        // members no longer force ImGui/this inspector to be linked into a plugin DLL; only whichever
        // binary actually calls Show() (and therefore needs this registration) does.
        struct register_all_draw_fns
        {
            register_all_draw_fns() noexcept
            {
                RegisterOne<std::int64_t,  style::drag_bar>();   RegisterOne<std::int64_t,  style::edit_box>();   RegisterOne<std::int64_t,  style::scroll_bar>();
                RegisterOne<std::uint64_t, style::drag_bar>();   RegisterOne<std::uint64_t, style::edit_box>();   RegisterOne<std::uint64_t, style::scroll_bar>();
                RegisterOne<std::int32_t,  style::drag_bar>();   RegisterOne<std::int32_t,  style::edit_box>();   RegisterOne<std::int32_t,  style::scroll_bar>();
                RegisterOne<std::uint32_t, style::drag_bar>();   RegisterOne<std::uint32_t, style::edit_box>();   RegisterOne<std::uint32_t, style::scroll_bar>();
                RegisterOne<std::int16_t,  style::drag_bar>();   RegisterOne<std::int16_t,  style::edit_box>();   RegisterOne<std::int16_t,  style::scroll_bar>();
                RegisterOne<std::uint16_t, style::drag_bar>();   RegisterOne<std::uint16_t, style::edit_box>();   RegisterOne<std::uint16_t, style::scroll_bar>();
                RegisterOne<std::int8_t,   style::drag_bar>();   RegisterOne<std::int8_t,   style::edit_box>();   RegisterOne<std::int8_t,   style::scroll_bar>();
                RegisterOne<std::uint8_t,  style::drag_bar>();   RegisterOne<std::uint8_t,  style::edit_box>();   RegisterOne<std::uint8_t,  style::scroll_bar>();
                RegisterOne<float,         style::drag_bar>();   RegisterOne<float,         style::edit_box>();   RegisterOne<float,         style::scroll_bar>();
                RegisterOne<double,        style::drag_bar>();   RegisterOne<double,        style::edit_box>();   RegisterOne<double,        style::scroll_bar>();

                RegisterOne<std::string,   style::button>();
                RegisterOne<std::string,   style::defaulted>();
                RegisterOne<std::wstring,  style::button>();
                RegisterOne<std::wstring,  style::file_dialog>();
                RegisterOne<std::wstring,  style::defaulted>();
                RegisterOne<bool,          style::defaulted>();
#ifdef XCORE_PROPERTIES_H
                RegisterOne<xresource::full_guid, style::defaulted>();
#endif
            }
        };
        const register_all_draw_fns s_RegisterAllDrawFns;
    }
}

namespace xproperty::ui::undo
{
    void system::Add(const cmd& Cmd) noexcept
    {
        if (m_Index < static_cast<int>(m_lCmds.size()))
        {
            m_lCmds.erase(m_lCmds.begin() + m_Index, m_lCmds.end());
        }
        m_lCmds.push_back(Cmd);
        m_Index = static_cast<int>(m_lCmds.size());

        if (m_Index > m_MaxSteps )
        {
            auto ToDelete = m_Index - m_MaxSteps;
            m_lCmds.erase(m_lCmds.begin(), m_lCmds.begin() + ToDelete);
            m_Index = static_cast<int>(m_lCmds.size());
        }
    }

    std::string system::Undo(xproperty::settings::context& Context) noexcept
    {
        if (m_Index == 0 || m_lCmds.size() == 0)
            return {};

        if (m_Index == static_cast<int>(m_lCmds.size() - 1))
        {
            if (m_lCmds.back().m_bHasChanged == false)
            {
                m_lCmds.pop_back();
                Undo(Context);
                return {};
            }
        }

        auto& Value = m_lCmds[--m_Index];
        std::string Error;
        xproperty::sprop::setProperty(Error, Value.m_pClassObject, *Value.m_pPropObject, xproperty::sprop::container::prop{ Value.m_Name, Value.m_Original }, Context);
        return Error;
    }

    std::string system::Redo(xproperty::settings::context& Context) noexcept
    {
        if (m_Index == static_cast<int>(m_lCmds.size()))
            return {};

        auto& Value = m_lCmds[m_Index++];
        std::string Error;
        xproperty::sprop::setProperty(Error, Value.m_pClassObject, *Value.m_pPropObject, xproperty::sprop::container::prop{ Value.m_Name, Value.m_NewValue }, Context);
        return Error;
    }
}

//-----------------------------------------------------------------------------------
// All the render functions
//-----------------------------------------------------------------------------------
namespace xproperty::ui::details
{
    //-----------------------------------------------------------------------------------

    template< auto T_IMGUID_DATA_TYPE_V, typename T >
    static void DragRenderNumbers(undo::cmd& Cmd, const T& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        auto& I = reinterpret_cast<const xproperty::member_ui<T>::data&>(IB);

        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {
            T V = Value;
            Cmd.m_isChange = ImGui::SliderScalar("##value", T_IMGUID_DATA_TYPE_V, &V, &I.m_Min, &I.m_Max, I.m_pFormat);
            if (Cmd.m_isChange)
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<T>(Value);
                Cmd.m_isEditing = true;
                Cmd.m_NewValue.set<T>(V);
            }
            if (Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit()) Cmd.m_isEditing = false;
        }
        if (Flags.m_bShowReadOnly) ImGui::EndDisabled();
    }

    //-----------------------------------------------------------------------------------
    template< auto T_IMGUID_DATA_TYPE_V, typename T >
    static void SlideRenderNumbers( undo::cmd& Cmd, const T& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        auto& I = reinterpret_cast<const xproperty::member_ui<T>::data&>(IB);

        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {
            T V = Value;
            Cmd.m_isChange = ImGui::DragScalar("##value", T_IMGUID_DATA_TYPE_V, &V, I.m_Speed, &I.m_Min, &I.m_Max, I.m_pFormat );
            if (Cmd.m_isChange)
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<T>(Value);
                Cmd.m_isEditing = true;
                Cmd.m_NewValue.set<T>(std::clamp(V, I.m_Min, I.m_Max));
            }
            if( Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit() ) Cmd.m_isEditing = false;
        }
        if (Flags.m_bShowReadOnly) ImGui::EndDisabled();
    }

    //-----------------------------------------------------------------------------------

    template< auto T_IMGUID_DATA_TYPE_V, typename T >
    static void EditBoxRenderNumbers( undo::cmd& Cmd, const T& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        auto& I = reinterpret_cast<const xproperty::member_ui<T>::data&>(IB);

        {
            T V = Value;
            std::array< char, 256> Buffer;
            std::snprintf(Buffer.data(), Buffer.size(), I.m_pFormat, V);

            if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
            Cmd.m_isChange = ImGui::InputText("##value", Buffer.data(), Buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue, nullptr );
            if (Flags.m_bShowReadOnly) ImGui::EndDisabled();

            if (ImGui::IsItemActivated())
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<T>(Value);
                Cmd.m_isEditing = true;
            }
            else
            {
                if (Cmd.m_isEditing == true && ImGui::IsItemActive() == false)
                    Cmd.m_isEditing = false;
            }

            if (Cmd.m_isChange)
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<T>(Value);
                Cmd.m_isEditing = true;
                try
                {
                    //double Result = xproperty::ui::details::calculator::evaluateExpression(Buffer.data());
                    calculator Calc(Buffer.data());
                    double Result = Calc.Compute(); //calcuxproperty::ui::details::calculator::evaluateExpression(Buffer.data());
                    Cmd.m_NewValue.set<T>(std::clamp(static_cast<T>(Result), I.m_Min, I.m_Max)); //  std::min<T>(I.m_Max, std::max<T>(I.m_Min, static_cast<T>(Result))));
                }
                catch (const std::exception& e)
                {
                    (void)e;
                    Cmd.m_isChange = false;
                }

                // Have we really changed anything?
                if (Value == Cmd.m_NewValue.get<T>())
                    Cmd.m_isChange = false;
            }
            if (Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit())
                Cmd.m_isEditing = false;
        }
    }

    //-----------------------------------------------------------------------------------
    // 64 bits int
    //-----------------------------------------------------------------------------------

    template<> void draw<std::int64_t, style::scroll_bar> ::Render( int GUID, undo::cmd& Cmd, const std::int64_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_S64>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int64_t, style::drag_bar>   ::Render( int GUID, undo::cmd& Cmd, const std::int64_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_S64>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int64_t, style::edit_box>   ::Render( int GUID, undo::cmd& Cmd, const std::int64_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_S64>( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint64_t, style::scroll_bar>::Render( int GUID, undo::cmd& Cmd, const std::uint64_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_U64>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint64_t, style::drag_bar>  ::Render( int GUID, undo::cmd& Cmd, const std::uint64_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_U64>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint64_t, style::edit_box>  ::Render( int GUID, undo::cmd& Cmd, const std::uint64_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_U64>( Cmd, Value, I, Flags ); }

    template<> void draw<std::int32_t, style::scroll_bar> ::Render( int GUID, undo::cmd& Cmd, const std::int32_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_S32>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int32_t, style::drag_bar>   ::Render( int GUID, undo::cmd& Cmd, const std::int32_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_S32>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int32_t, style::edit_box>   ::Render( int GUID, undo::cmd& Cmd, const std::int32_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_S32>( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint32_t, style::scroll_bar>::Render( int GUID, undo::cmd& Cmd, const std::uint32_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_U32>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint32_t, style::drag_bar>  ::Render( int GUID, undo::cmd& Cmd, const std::uint32_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_U32>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint32_t, style::edit_box>  ::Render( int GUID, undo::cmd& Cmd, const std::uint32_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_U32>( Cmd, Value, I, Flags ); }

    template<> void draw<std::int16_t, style::scroll_bar> ::Render( int GUID, undo::cmd& Cmd, const std::int16_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_S16>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int16_t, style::drag_bar>   ::Render( int GUID, undo::cmd& Cmd, const std::int16_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_S16>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int16_t, style::edit_box>   ::Render( int GUID, undo::cmd& Cmd, const std::int16_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_S16>( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint16_t, style::scroll_bar>::Render( int GUID, undo::cmd& Cmd, const std::uint16_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_U16>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint16_t, style::drag_bar>  ::Render( int GUID, undo::cmd& Cmd, const std::uint16_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_U16>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint16_t, style::edit_box>  ::Render( int GUID, undo::cmd& Cmd, const std::uint16_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_U16>( Cmd, Value, I, Flags ); }

    template<> void draw<std::int8_t, style::scroll_bar> ::Render( int GUID, undo::cmd& Cmd, const std::int8_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_S8>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int8_t, style::drag_bar>   ::Render( int GUID, undo::cmd& Cmd, const std::int8_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_S8>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int8_t, style::edit_box>   ::Render( int GUID, undo::cmd& Cmd, const std::int8_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_S8>( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint8_t, style::scroll_bar>::Render( int GUID, undo::cmd& Cmd, const std::uint8_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_U8>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint8_t, style::drag_bar>  ::Render( int GUID, undo::cmd& Cmd, const std::uint8_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_U8>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint8_t, style::edit_box>  ::Render( int GUID, undo::cmd& Cmd, const std::uint8_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_U8>( Cmd, Value, I, Flags ); }

    template<> void draw<float, style::scroll_bar>::Render(int GUID, undo::cmd& Cmd, const float& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { DragRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }
    template<> void draw<float, style::drag_bar>  ::Render(int GUID, undo::cmd& Cmd, const float& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { SlideRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }
    template<> void draw<float, style::edit_box>  ::Render(int GUID, undo::cmd& Cmd, const float& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { EditBoxRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }

    template<> void draw<double, style::scroll_bar>::Render(int GUID, undo::cmd& Cmd, const double& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { DragRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }
    template<> void draw<double, style::drag_bar>  ::Render(int GUID, undo::cmd& Cmd, const double& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { SlideRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }
    template<> void draw<double, style::edit_box>  ::Render(int GUID, undo::cmd& Cmd, const double& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { EditBoxRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }


    //-----------------------------------------------------------------------------------
    // OTHERS!!!
    //-----------------------------------------------------------------------------------

    template<>
    void draw<bool, style::defaulted>::Render(int GUID, undo::cmd& Cmd, const bool& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        auto& I = reinterpret_cast<const xproperty::member_ui<bool>::data&>(IB);

        bool V = Value;

        if ( Flags.m_bShowReadOnly )
        {
            ImGui::Checkbox("##value", &V);
            V = Value;
        }
        else 
        {
            Cmd.m_isChange = ImGui::Checkbox("##value", &V);
            if ( Cmd.m_isChange )
            {
                if(Cmd.m_isEditing == false) Cmd.m_Original.set<bool>(Value);
                Cmd.m_isEditing = true;
                Cmd.m_NewValue.set<bool>(V);
            } 
            if( Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit() ) Cmd.m_isEditing = false;
        }

        ImGui::SameLine();
        if (V) ImGui::Text(" True");
        else   ImGui::Text(" False");
    }

    //-----------------------------------------------------------------------------------
#ifdef XCORE_PROPERTIES_H
    static xproperty::inspector* g_pInspector{nullptr};

    template<>
    void draw<xresource::full_guid, style::defaulted>::Render(int GUID, undo::cmd& Cmd, const xresource::full_guid& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        auto& I = reinterpret_cast<const xproperty::member_ui<xresource::full_guid>::data&>(IB);

        const auto& Ctx = g_pInspector->m_CurrentProperty;

        bool bOpen = false;
        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {
            g_pInspector->m_OnResourceWigzmos.NotifyAll(*g_pInspector, *Ctx.m_pObject, Ctx.m_pInstance, Ctx.m_Path, bOpen, Value);
            if (bOpen && Cmd.m_isEditing == false)
            {
                Cmd.m_isEditing = true;
            }
            if (Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit()) Cmd.m_isEditing = false;
        }
        if (Flags.m_bShowReadOnly) ImGui::EndDisabled();

        if (not Flags.m_bShowReadOnly)
        {
            xresource::full_guid FullGuid;
            FullGuid.m_Type = Value.m_Type;
            g_pInspector->m_OnResourceBrowser.NotifyAll(*g_pInspector, *Ctx.m_pObject, Ctx.m_pInstance, Ctx.m_Path, bOpen, FullGuid, I.m_FilerTypes);

            // If it is not open any more we are done editing....
            Cmd.m_isEditing = bOpen;
            if (not FullGuid.empty())
            {
                if (FullGuid != Value)
                {
                    Cmd.m_isChange = true;
                    Cmd.m_NewValue.set<xresource::full_guid>(FullGuid);
                }
            }
        }
    }
#endif

    //-----------------------------------------------------------------------------------
    std::array<char,    16 * 1024>   g_ScrachCharBuffer;
    std::array<wchar_t, 16 * 1024>   g_WScrachCharBuffer;

    template<>
    void draw<std::string, style::defaulted>::Render(int GUID, undo::cmd& Cmd, const std::string& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
       // auto& I = reinterpret_cast<const xproperty::member_ui<std::string>::data&>(IB);

        ImVec2 charSize = ImGui::CalcTextSize("A");
        float f = ImGui::GetColumnWidth() / charSize.x;
        float f2 = Value.length() - f;

        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {
            const auto              InputLength = Value.length();

            Value.copy(g_ScrachCharBuffer.data(), InputLength );
            g_ScrachCharBuffer[InputLength] = 0;
            ImGui::BeginGroup();

            const auto CurPos   = ImGui::GetCursorPosX();
            const bool WentOver = f2 > -1 && Cmd.m_isEditing == false;
            if( WentOver ) ImGui::SetCursorPosX(CurPos - (f2 + 1) * charSize.x);

            Cmd.m_isChange = ImGui::InputText( "##value", g_ScrachCharBuffer.data(), g_ScrachCharBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            if( ImGui::IsItemActivated() )
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<std::string>(Value);
                Cmd.m_isEditing = true;
            }
            else
            {
                if(Cmd.m_isEditing == true && ImGui::IsItemActive() == false )
                    Cmd.m_isEditing = false;
            }

            // Draw the symbol to indicated that there is more string on the left
            if(WentOver)
            {
                ImGui::SameLine();
                ImGui::SetCursorPosX(CurPos-5);
                ImGui::ArrowButton("", ImGuiDir_Left);
            }

            ImGui::EndGroup();
            if ( Cmd.m_isChange )
            {
                if( Cmd.m_isEditing == false ) Cmd.m_Original.set<std::string>(Value);
                Cmd.m_isEditing = true;
                Cmd.m_NewValue.set<std::string>(g_ScrachCharBuffer.data());

                // Have we really changed anything?
                if(Cmd.m_Original.get<std::string>() == Cmd.m_NewValue.get<std::string>() )
                    Cmd.m_isChange = false;
            }
            if( Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit() ) 
                Cmd.m_isEditing = false;
        }
        if (Flags.m_bShowReadOnly) ImGui::EndDisabled();

        // For strings that are too long... we will show a tooltip with the full string
        if (f2 > -1 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) )
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 10, 10 });
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(charSize.x * 100);

            ImGui::TextUnformatted( Value.c_str() );

            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }
    }

    template<>
    void draw<std::wstring, style::defaulted>::Render(int GUID, undo::cmd& Cmd, const std::wstring& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        // auto& I = reinterpret_cast<const xproperty::member_ui<std::string>::data&>(IB);

        ImVec2 charSize = ImGui::CalcTextSize("A");
        float f         = ImGui::GetColumnWidth() / charSize.x;
        float f2        = Value.length() - f;

        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {
            const auto              InputLength = Value.length();

            Value.copy(g_WScrachCharBuffer.data(), InputLength);
            g_ScrachCharBuffer[InputLength] = 0;
            ImGui::BeginGroup();

            const auto CurPos = ImGui::GetCursorPosX();
            const bool WentOver = f2 > -1 && Cmd.m_isEditing == false;
            if (WentOver) ImGui::SetCursorPosX(CurPos - (f2 + 1) * charSize.x);


            // convert wide string to narrow to display with imgui
            auto size_needed = WideCharToMultiByte(CP_UTF8, 0, g_WScrachCharBuffer.data(), -1, nullptr, 0, nullptr, nullptr);
            WideCharToMultiByte(CP_UTF8, 0, g_WScrachCharBuffer.data(), -1, g_ScrachCharBuffer.data(), size_needed, nullptr, nullptr);

            // Let IMGUI handle the actual string...
            Cmd.m_isChange = ImGui::InputText("##value", g_ScrachCharBuffer.data(), g_ScrachCharBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);

            // convert back to wide
            size_needed = MultiByteToWideChar(CP_ACP, 0, g_ScrachCharBuffer.data(), -1, nullptr, 0);
            MultiByteToWideChar(CP_ACP, 0, g_ScrachCharBuffer.data(), -1, g_WScrachCharBuffer.data(), size_needed);


            if (ImGui::IsItemActivated())
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<std::wstring>(Value);
                Cmd.m_isEditing = true;
            }
            else
            {
                if (Cmd.m_isEditing == true && ImGui::IsItemActive() == false)
                    Cmd.m_isEditing = false;
            }

            // Draw the symbol to indicated that there is more string on the left
            if (WentOver)
            {
                ImGui::SameLine();
                ImGui::SetCursorPosX(CurPos - 5);
                ImGui::ArrowButton("", ImGuiDir_Left);
            }

            ImGui::EndGroup();
            if (Cmd.m_isChange)
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<std::wstring>(Value);
                Cmd.m_isEditing = true;
                Cmd.m_NewValue.set<std::wstring>(g_WScrachCharBuffer.data());

                // Have we really changed anything?
                if (Cmd.m_Original.get<std::wstring>() == Cmd.m_NewValue.get<std::wstring>())
                    Cmd.m_isChange = false;
            }
            if (Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit())
                Cmd.m_isEditing = false;
        }
        if (Flags.m_bShowReadOnly) ImGui::EndDisabled();

        // For strings that are too long... we will show a tooltip with the full string
        if (f2 > -1 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 10, 10 });
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(charSize.x * 100);

            // convert wide string to narrow to display with imgui
            auto size_needed = WideCharToMultiByte(CP_UTF8, 0, Value.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string utf8_text(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, Value.c_str(), -1, utf8_text.data(), size_needed, nullptr, nullptr);

            ImGui::TextUnformatted(utf8_text.c_str());

            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }
    }

    //-----------------------------------------------------------------------------------
    // Windows suck... I had to write this stupid class to have a nice looking folder browser
    //-----------------------------------------------------------------------------------
    bool CheckFolderFilter(const wchar_t* path, const wchar_t* pFilter)
    {
        if (pFilter == nullptr) return true;

        wchar_t filters[1024] = { 0 }; // Assumes filter string fits in this buffer
        for (int i = 0; (filters[i] = pFilter[i]) || (filters[i + 1] = pFilter[i + 1]); ++i) {}

        wchar_t* spec = filters;
        bool isValid = false;
        bool bFristDescriptor = true;
        while (*spec != L'\0')
        {
            // Skip to the specifications part, ignoring the description
            if (bFristDescriptor)
            {
                spec = wcschr(spec, L'\0') + 1;
                bFristDescriptor = false;
            }

            if (*spec == L'\0') break; // End of filter string

            // spec now points to the start of a filter specification
            wchar_t* nextSpec = wcschr(spec, L';');
            if (nextSpec == nullptr)
            {
                // If no ';' found, continue until the end of the string
                nextSpec = wcschr(spec, L'\0');
            }

            if (nextSpec == nullptr) break; // Safety check

            wchar_t* endOfSpec = nextSpec;
            *endOfSpec = L'\0'; // Temporarily null-terminate the spec for comparison

            // Use PathMatchSpecW to check if the path matches the current spec
            if (PathMatchSpecW(path, spec))
            {
                isValid = true;
                break; // Found a match, no need to check further
            }

            spec = endOfSpec + 1; // Move to next specification or end if no more
            if (*spec == L'\0')
            {
                bFristDescriptor = true;
                spec++;
            }
        }

        return isValid;
    }

    //-----------------------------------------------------------------------------------

    bool SelectFolderWithFilters( const wchar_t* pFilers, const wchar_t* pInitialPath )
    {
        struct CFolderFilter : public IFileDialogEvents
        {
            // IUnknown methods
            IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv)
            {
                if (riid == IID_IUnknown || riid == IID_IFileDialogEvents)
                {
                    *ppv = static_cast<IFileDialogEvents*>(this);
                    AddRef();
                    return S_OK;
                }
                *ppv = nullptr;
                return E_NOINTERFACE;
            }
            IFACEMETHODIMP_(ULONG) AddRef()     { return InterlockedIncrement(&m_cRef); }
            IFACEMETHODIMP_(ULONG) Release()    { ULONG cRef = InterlockedDecrement(&m_cRef); if (cRef == 0) delete this; return cRef; }
            IFACEMETHODIMP OnFolderChange(IFileDialog* pfd)     { UpdateOkButtonState(pfd); return S_OK; }
            IFACEMETHODIMP OnSelectionChange(IFileDialog* pfd)  { UpdateOkButtonState(pfd); return S_OK;}
            IFACEMETHODIMP OnFolderChanging(IFileDialog* pfd, IShellItem* psiFolder) { return S_OK; }
            IFACEMETHODIMP OnTypeChange(IFileDialog* pfd) { return S_OK; }
            IFACEMETHODIMP OnFileOk(IFileDialog* pfd)    { return UpdateOkButtonState(pfd);}
            IFACEMETHODIMP OnOverwrite(IFileDialog* pfd, IShellItem* psi, FDE_OVERWRITE_RESPONSE* pResponse) { return S_OK; }
            IFACEMETHODIMP OnShareViolation(IFileDialog* pfd, IShellItem* psi, FDE_SHAREVIOLATION_RESPONSE* pResponse) { *pResponse = FDESVR_DEFAULT; return S_OK; }
            void SetFilter(const wchar_t* filter) { m_filter = filter; }
            HWND GetParentWindow(IFileDialog* pfd) { HWND hwnd = FindWindowEx(GetActiveWindow(), nullptr, L"#32770", nullptr); return hwnd; }

            ULONG           m_cRef = 1;
            const wchar_t*  m_filter = nullptr;


            IFACEMETHODIMP UpdateOkButtonState(IFileDialog* pfd)
            {
                auto FinalRsult = S_OK;
                HWND hwndParent = GetParentWindow(pfd);

                IShellItem* psiResult = nullptr;
                if (SUCCEEDED(pfd->GetCurrentSelection(&psiResult)))
                {
                    PWSTR pszFilePath = nullptr;
                    if (SUCCEEDED(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
                    {
                        bool isValid = CheckFolderFilter(pszFilePath, m_filter);

                        IFileDialogCustomize* pfdc = nullptr;
                        if (SUCCEEDED(pfd->QueryInterface(IID_PPV_ARGS(&pfdc))))
                        {
                            if (true)
                            {
                                auto X = GetDlgItem(hwndParent, 1);
                                if (isValid) ShowWindow(X, SW_SHOW); //EnableWindow( X, true); //ShowWindow(X, SW_SHOW);
                                else         ShowWindow(X, SW_HIDE); //EnableWindow(X, false); //ShowWindow(X, SW_HIDE);
                            }
                            //pfdc->SetControlState(1, isValid ? CDCS_ENABLED : CDCS_INACTIVE);
                            if (isValid == false) FinalRsult = S_FALSE;
                        }

                        CoTaskMemFree(pszFilePath);

                    }
                    psiResult->Release();
                }

                return FinalRsult;
            }
        };

        // Helper function to create the event handler
        auto CFolderFilter_CreateInstance = [](REFIID riid, void** ppv, const wchar_t* pFilers)->HRESULT
        {
            *ppv = nullptr;
            CFolderFilter* pInstance = new (std::nothrow) CFolderFilter();
            pInstance->SetFilter(pFilers);
            if (pInstance == nullptr) return E_OUTOFMEMORY;
            HRESULT hr = pInstance->QueryInterface(riid, ppv);
            pInstance->Release(); // Release initial ref count
            return hr;
        };


        HRESULT hr;
        IFileDialog* pfd = nullptr;
        DWORD dwCookie;
        bool FinalResult = false;

        // Create the FileOpenDialog object.
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

        if (SUCCEEDED(hr))
        {
            DWORD dwOptions;

            // Get the current options.
            hr = pfd->GetOptions(&dwOptions);
            if (SUCCEEDED(hr))
            {
                // Set the FOS_PICKFOLDERS option to allow folder selection.
                hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
            }
            else return FinalResult;


            // Create and advise the event handler
            IFileDialogEvents* pfde = nullptr;
            hr = CFolderFilter_CreateInstance(IID_PPV_ARGS(&pfde), pFilers);
            if (SUCCEEDED(hr))
            {
                hr = pfd->Advise(pfde, &dwCookie);
                if (SUCCEEDED(hr))
                {
                    // set the initial path...
                    IShellItem* psiInitialDir = nullptr;
                    HRESULT hr = SHCreateItemFromParsingName(pInitialPath, nullptr, IID_PPV_ARGS(&psiInitialDir));
                    if (SUCCEEDED(hr)) pfd->SetFolder(psiInitialDir);

                    // Show the dialog
                    hr = pfd->Show(nullptr);
                    if (SUCCEEDED(hr))
                    {
                        IShellItem* psi;
                        // Get the folder selected by the user
                        hr = pfd->GetResult(&psi);
                        if (SUCCEEDED(hr))
                        {
                            PWSTR pszFilePath = nullptr;
                            hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                            if (SUCCEEDED(hr))
                            {
                                for (int i = 0; g_ScrachCharBuffer[i] = static_cast<char>(pszFilePath[i]); ++i) {}
                                CoTaskMemFree(pszFilePath);
                                FinalResult = true;
                            }
                            psi->Release();
                        }
                    }
                    // Unadvise the event handler
                    pfd->Unadvise(dwCookie);
                }
                pfde->Release();
            }
            pfd->Release();
        }

        return FinalResult;
    }

    //-----------------------------------------------------------------------------------

    template<>
    void draw<std::wstring, style::file_dialog>::Render(int GUID, undo::cmd& Cmd, const std::wstring& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        auto& I = reinterpret_cast<const xproperty::member_ui<std::wstring>::data&>(IB);

        ImVec2 charSize     = ImGui::CalcTextSize("A");
        float ButtonWidth   = charSize.x * 3;
        float f             = (ImGui::GetColumnWidth() - ButtonWidth) / charSize.x;
        float f2            = Value.length() - f;

        float ItemWidth = [&]
        {
            if( f2 > -1 )
            {
                return Value.length() * charSize.x + 3;
            }
            else
            {
                return ImGui::GetColumnWidth() - ButtonWidth - 3;
            }            
        }();

        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {
            Value.copy(g_WScrachCharBuffer.data(), Value.length());
            g_WScrachCharBuffer[Value.length()] = 0;
            ImGui::BeginGroup();

            const auto CurPos = ImGui::GetCursorPosX();
            const bool WentOver = f2 > -1 && Cmd.m_isEditing == false;
            if (WentOver) ImGui::SetCursorPosX(CurPos - (f2 + 1) * charSize.x);

            if (Cmd.m_isEditing == false) ImGui::PushItemWidth(ItemWidth);

            // convert wide string to narrow to display with imgui
            auto size_needed = WideCharToMultiByte(CP_UTF8, 0, g_WScrachCharBuffer.data(), -1, nullptr, 0, nullptr, nullptr);
            WideCharToMultiByte(CP_UTF8, 0, g_WScrachCharBuffer.data(), -1, g_ScrachCharBuffer.data(), size_needed, nullptr, nullptr);

            // Let IMGUI handle the actual string...
            Cmd.m_isChange = ImGui::InputText("##value", g_ScrachCharBuffer.data(), g_ScrachCharBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);

            // convert back to wide
            size_needed = MultiByteToWideChar(CP_ACP, 0, g_ScrachCharBuffer.data(), -1, nullptr, 0);
            MultiByteToWideChar(CP_ACP, 0, g_ScrachCharBuffer.data(), -1, g_WScrachCharBuffer.data(), size_needed);


            if (Cmd.m_isEditing == false) ImGui::PopItemWidth();

            if (ImGui::IsItemActivated())
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<std::wstring>(Value);
                Cmd.m_isEditing = true;
            }
            else
            {
                if (Cmd.m_isEditing == true && ImGui::IsItemActive() == false)
                    Cmd.m_isEditing = false;
            }

            ImGui::SameLine(0, -3);
            if( ImGui::Button("...",ImVec2(0, ButtonWidth-3)) )
            {
                // The user can change the path in the dialog... changing the current path.
                // We want to allow the user to do that because it is more convenient for them...
                std::wstring CurrentPath;// = xproperty::member_ui<std::string>::g_WCurrentPath;
                std::array< wchar_t, MAX_PATH > WCurrentPath;
                {
                    GetCurrentDirectory(static_cast<DWORD>(WCurrentPath.size()), WCurrentPath.data());
                    std::transform(WCurrentPath.begin(), WCurrentPath.end(), std::back_inserter(CurrentPath), [](wchar_t c) {return (char)c; });
                }

                // Set the scratch file to have nothing on it unless we put something...
                g_ScrachCharBuffer[0]=0;
                if (I.m_bFolders)
                {
                    SelectFolderWithFilters(I.m_pFilter, WCurrentPath.data());
                }
                else
                {
                    OPENFILENAMEW ofn;
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize     = sizeof(ofn);
                    ofn.hwndOwner       = GetActiveWindow();
                    ofn.lpstrFile       = g_WScrachCharBuffer.data();
                    ofn.lpstrFile[0]    = L'\0';
                    ofn.nMaxFile        = static_cast<std::uint32_t>(g_ScrachCharBuffer.size());
                    ofn.lpstrFilter     = I.m_pFilter;
                    ofn.nFilterIndex    = 1;
                    ofn.lpstrFileTitle  = nullptr;
                    ofn.nMaxFileTitle   = 0;
                    ofn.lpstrInitialDir = CurrentPath.c_str();
                    ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    if (GetOpenFileNameW(&ofn) == TRUE)
                    {
                        assert(g_WScrachCharBuffer[0]);
                    }
                }

                if (g_WScrachCharBuffer[0])
                {
                    Cmd.m_isChange = true;

                    if (I.m_bMakePathRelative)
                    {
                        int nPops = 1;

                        // Set the expected current path
                        CurrentPath = xproperty::member_ui<std::wstring>::g_CurrentPath;

                        // Count the paths for the current path
                        for (const wchar_t* p = CurrentPath.c_str(); *p; p++)
                        {
                            if (*p == '\\' || *p == '/') nPops++;
                        }

                        // Add whatever the user requested
                        nPops -= I.m_RelativeCurrentPathMinusCount;

                        // Find our relative path and set the new string
                        for (const wchar_t* p = g_WScrachCharBuffer.data(); *p; p++)
                        {
                            if (*p == L'\\' || *p == L'/') nPops--;
                            if (nPops <= 0)
                            {
                                ++p;
                                for (int i = 0; g_WScrachCharBuffer[i] = *p; ++i, ++p) {}
                                break;
                            }
                        }
                    }
                }
            }

            // Draw the symbol to indicated that there is more string on the left
            if (WentOver)
            {
                ImGui::SameLine();
                ImGui::SetCursorPosX(CurPos - 5);
                ImGui::ArrowButton("", ImGuiDir_Left);
            }

            ImGui::EndGroup();
            if (Cmd.m_isChange)
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<std::wstring>(Value);
                Cmd.m_isEditing = true;
                Cmd.m_NewValue.set<std::wstring>(g_WScrachCharBuffer.data());

                // Have we really changed anything?
                if (Cmd.m_Original.get<std::wstring>() == Cmd.m_NewValue.get<std::wstring>())
                    Cmd.m_isChange = false;
            }
            if (Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit())
                Cmd.m_isEditing = false;
        }
        if (Flags.m_bShowReadOnly) ImGui::EndDisabled();

        // For strings that are too long... we will show a tooltip with the full string
        if (f2 > -1 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 10, 10 });
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(charSize.x * 100);

            // convert wide string to narrow to display with imgui
            auto size_needed = WideCharToMultiByte(CP_UTF8, 0, Value.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string utf8_text(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, Value.c_str(), -1, utf8_text.data(), size_needed, nullptr, nullptr);

            ImGui::TextUnformatted(utf8_text.c_str());

            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }
    }

    //-----------------------------------------------------------------------------------

    template<>
    void draw<std::string, style::button>::Render(int GUID, undo::cmd& Cmd, const std::string& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        auto& I = reinterpret_cast<const xproperty::member_ui<bool>::data&>(IB);

        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {
            Cmd.m_isChange = ImGui::Button( Value.c_str(), ImVec2(-1, 0));
            if ( Cmd.m_isChange )
            {
                if( Cmd.m_isEditing == false ) Cmd.m_Original.set<std::string>(Value);
                Cmd.m_isEditing = true;
                Cmd.m_NewValue.set<std::string>( Value );
            }
            if( Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit() ) Cmd.m_isEditing = false;
        }
        if (Flags.m_bShowReadOnly) ImGui::EndDisabled();

    }

    //-----------------------------------------------------------------------------------

    template<>
    void draw<std::wstring, style::button>::Render(int GUID, undo::cmd& Cmd, const std::wstring& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        auto& I = reinterpret_cast<const xproperty::member_ui<bool>::data&>(IB);

        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {
            // convert wide string to narrow to display with imgui
            auto size_needed = WideCharToMultiByte(CP_UTF8, 0, g_WScrachCharBuffer.data(), -1, nullptr, 0, nullptr, nullptr);
            WideCharToMultiByte(CP_UTF8, 0, g_WScrachCharBuffer.data(), -1, g_ScrachCharBuffer.data(), size_needed, nullptr, nullptr);

            Cmd.m_isChange = ImGui::Button(g_ScrachCharBuffer.data(), ImVec2(-1, 0));
            if (Cmd.m_isChange)
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<std::wstring>(Value);
                Cmd.m_isEditing = true;
                Cmd.m_NewValue.set<std::wstring>(Value);
            }
            if (Cmd.m_isEditing && ImGui::IsItemDeactivatedAfterEdit()) Cmd.m_isEditing = false;
        }
        if (Flags.m_bShowReadOnly) ImGui::EndDisabled();

    }

    //-----------------------------------------------------------------------------------

    void draw_enums( undo::cmd& Cmd, const xproperty::any& AnyValue, xproperty::flags::type Flags) noexcept
    {
     //   assert(AnyValue.isEnum);
        assert(AnyValue.m_pType);
        assert(AnyValue.m_pType->m_RegisteredEnumSpan.size() > 0 );

        //
        // get the current selected item index
        //
        std::size_t current_item = [&]
        {
            // first extract the value of the enum for any type...
            std::uint64_t Value = [&]()->std::uint64_t
            {
                switch (AnyValue.m_pType->m_Size)
                {
                    case 1: return *reinterpret_cast<const std::uint8_t*>(&AnyValue.m_Data);
                    case 2: return *reinterpret_cast<const std::uint16_t*>(&AnyValue.m_Data);
                    case 4: return *reinterpret_cast<const std::uint32_t*>(&AnyValue.m_Data);
                    case 8: return *reinterpret_cast<const std::uint64_t*>(&AnyValue.m_Data);
                }

                assert(false);
                return 0;
            }();

            // then search to find which is the index
            std::size_t current_item=0;
            for (; current_item < AnyValue.m_pType->m_RegisteredEnumSpan.size(); ++current_item)
            {
                if (Value == AnyValue.m_pType->m_RegisteredEnumSpan[current_item].m_Value)
                {
                    break;
                }
            }

            // if we don't find the index... then we have a problem...
            if (current_item == AnyValue.m_pType->m_RegisteredEnumSpan.size())
            {
                // We should have had a value in the list...
                assert(false);
            }

            return current_item;
        }();

        //
        // Handle the UI part...
        //
        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {

            Cmd.m_isChange = false;
            if (ImGui::BeginCombo("##combo", AnyValue.m_pType->m_RegisteredEnumSpan[current_item].m_pName)) // The second parameter is the label previewed before opening the combo.
            {
                for (std::size_t n = 0; n < AnyValue.m_pType->m_RegisteredEnumSpan.size(); n++)
                {
                    bool is_selected = (current_item == n); // You can store your selection however you want, outside or inside your objects

                    if (ImGui::Selectable(AnyValue.m_pType->m_RegisteredEnumSpan[n].m_pName, is_selected))
                    {
                        if (Cmd.m_isEditing == false) Cmd.m_Original = AnyValue;

                        //
                        // Set the new value
                        //

                        // First iniailize all the type information...
                        Cmd.m_NewValue = AnyValue;

                        // Now overrite the value in a generic way...
                        switch (AnyValue.m_pType->m_Size)
                        {
                        case 1: *reinterpret_cast<std::uint8_t*>(&Cmd.m_NewValue.m_Data)  = AnyValue.m_pType->m_RegisteredEnumSpan[n].m_Value; break;
                        case 2: *reinterpret_cast<std::uint16_t*>(&Cmd.m_NewValue.m_Data) = AnyValue.m_pType->m_RegisteredEnumSpan[n].m_Value; break;
                        case 4: *reinterpret_cast<std::uint32_t*>(&Cmd.m_NewValue.m_Data) = AnyValue.m_pType->m_RegisteredEnumSpan[n].m_Value; break;
                        case 8: *reinterpret_cast<std::uint64_t*>(&Cmd.m_NewValue.m_Data) = AnyValue.m_pType->m_RegisteredEnumSpan[n].m_Value; break;
                        }
                        Cmd.m_isChange = true;
                    }

                    if (AnyValue.m_pType->m_RegisteredEnumSpan[n].m_pHelp)
                    {
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10,10));
                            ImGui::BeginTooltip();
                            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
                            ImGui::TextUnformatted(AnyValue.m_pType->m_RegisteredEnumSpan[n].m_pHelp);
                            ImGui::PopTextWrapPos();
                            ImGui::EndTooltip();
                            ImGui::PopStyleVar();
                        }
                    }

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
                }

                ImGui::EndCombo();
            }
        }
        if (Flags.m_bShowReadOnly) ImGui::EndDisabled();
    }

    //-----------------------------------------------------------------------------------
    /*
    template<>
    void draw<oobb, style::defaulted>(undo::cmd<oobb>& Cmd, const oobb& Value, const xproperty::ui::style_info<oobb>&, xproperty::flags::type Flags) noexcept
    {
        ImGuiStyle * style   = &ImGui::GetStyle();
        const auto   Width   = (ImGui::GetContentRegionAvail().x - style->ItemInnerSpacing.x ) / 2;
        const auto   Height  = ImGui::GetFrameHeight();
        ImVec2       pos     = ImGui::GetCursorScreenPos();
        oobb         Temp    = Value;

        ImGui::PushItemWidth( Width );

        // Min
        bool bChange      = ImGui::DragFloat( "##value1", &Temp.m_Min, 0.01f, -1000.0f, 1000.0f );
        bool bDoneEditing = ImGui::IsItemDeactivatedAfterEdit();
        ImGui::GetWindowDrawList()->AddRectFilled( pos, ImVec2( pos.x + Width, pos.y + Height ), ImU32( 0x440000ff ) );

        // Max
        ImGui::SameLine( 0, 2 );
        pos = ImGui::GetCursorScreenPos();

        bChange      |= ImGui::DragFloat( "##value2", &Temp.m_Max, 0.01f, -1000.0f, 1000.0f );
        bDoneEditing |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::GetWindowDrawList()->AddRectFilled( pos, ImVec2( pos.x + Width, pos.y + Height ), ImU32( 0x4400ff00 ) );

        // Done
        ImGui::PopItemWidth();

        if( bChange )
        {
            if ( Flags.m_isShowReadOnly ) return;

            if (Cmd.m_isEditing == false) Cmd.m_Original = Value;

            Cmd.m_isEditing     = true;
            Cmd.m_isChange      = true;
            Cmd.m_NewValue      = Temp;
        }
        if( bDoneEditing )
        {
            Cmd.m_isEditing = false;
        }
    }
    */

    template< typename T_UI_TAG >
    static void onRender(int GUID, xproperty::ui::undo::cmd& Cmd, const xproperty::any& Value, const xproperty::type::members& Entry, xproperty::flags::type Flags) noexcept
    {

        //
        // Enums are handle special... 
        //
        if (Value.m_pType->m_IsEnum)
        {
            draw_enums( Cmd, Value, Flags);
            return;
        }

        //
        // Handle the rest of UI elements
        //
        const auto& StyleBase = [&]() -> const member_ui_base&
        {
            const xproperty::settings::member_ui_t* pMemberUI = reinterpret_cast<const xproperty::settings::member_ui_t*>(Entry.getUserData<T_UI_TAG>());

            // if the user does not specify a way to edit the size of a list we assume it is read-only
            if constexpr ( std::is_same_v<xproperty::settings::member_ui_list_size_t, T_UI_TAG> )
            {
                if(pMemberUI==nullptr) Flags.m_bShowReadOnly = true;
            }

            // This is super strange... in visual studio 17.11.1 these static assets are failing... Not sure why...
            // static_assert(xproperty::member_ui<std::int64_t> ::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<std::int64_t>::guid_v);
            // static_assert(xproperty::member_ui<std::uint64_t>::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<std::uint64_t>::guid_v);
            // static_assert(xproperty::member_ui<std::int32_t> ::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<std::int32_t>::guid_v);
            // static_assert(xproperty::member_ui<std::uint32_t>::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<std::uint32_t>::guid_v);
            // static_assert(xproperty::member_ui<std::int16_t> ::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<std::int16_t>::guid_v);
            // static_assert(xproperty::member_ui<std::uint16_t>::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<std::uint16_t>::guid_v);
            // static_assert(xproperty::member_ui<std::int8_t>  ::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<std::int8_t>::guid_v);
            // static_assert(xproperty::member_ui<std::uint8_t> ::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<std::uint8_t>::guid_v);
            // static_assert(xproperty::member_ui<float>        ::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<float>::guid_v);
            // static_assert(xproperty::member_ui<double>       ::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<double>::guid_v);
            // static_assert(xproperty::member_ui<std::string>  ::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<std::string>::guid_v);
            // static_assert(xproperty::member_ui<bool>         ::defaults::data_v.m_TypeGUID == xproperty::settings::var_type<bool>::guid_v);

            // Lets see if the user decided to set the style... 
            if (pMemberUI == nullptr)
            {
                switch (Value.m_pType->m_GUID)
                {
                case xproperty::settings::var_type<std::int64_t>::guid_v:    return  member_ui<std::int64_t> ::defaults::data_v;
                case xproperty::settings::var_type<std::uint64_t>::guid_v:   return  member_ui<std::uint64_t>::defaults::data_v;
                case xproperty::settings::var_type<std::int32_t>::guid_v:    return  member_ui<std::int32_t> ::defaults::data_v;
                case xproperty::settings::var_type<std::uint32_t>::guid_v:   return  member_ui<std::uint32_t>::defaults::data_v;
                case xproperty::settings::var_type<std::int16_t>::guid_v:    return  member_ui<std::int16_t> ::defaults::data_v;
                case xproperty::settings::var_type<std::uint16_t>::guid_v:   return  member_ui<std::uint16_t>::defaults::data_v;
                case xproperty::settings::var_type<std::int8_t>::guid_v:     return  member_ui<std::int8_t>  ::defaults::data_v;
                case xproperty::settings::var_type<std::uint8_t>::guid_v:    return  member_ui<std::uint8_t> ::defaults::data_v;
                case xproperty::settings::var_type<float>::guid_v:           return  member_ui<float>        ::defaults::data_v;
                case xproperty::settings::var_type<double>::guid_v:          return  member_ui<double>       ::defaults::data_v;
                case xproperty::settings::var_type<std::string>::guid_v:     return  member_ui<std::string>  ::defaults::data_v;
                case xproperty::settings::var_type<bool>::guid_v:            return  member_ui<bool>         ::defaults::data_v;
                case xproperty::settings::var_type<std::wstring>::guid_v:    return  member_ui<std::wstring> ::defaults::data_v;
#ifdef XCORE_PROPERTIES_H
                case xproperty::settings::var_type<xresource::full_guid>::guid_v: return  member_ui<xresource::full_guid>::defaults::data_v;
#endif
                default: assert(false); return member_ui<bool>::defaults::data_v;
                }
            }
            else
            {
                assert(pMemberUI);
                assert(pMemberUI->m_pUIBase);
                return *pMemberUI->m_pUIBase;
            }
        }();


        // Resolve the real drawer from the registry (see xproperty::ui::details::RegisterDrawFn,
        // above) - StyleBase only ever carries a style GUID now, never a function pointer. Keyed by
        // Value.m_pType->m_GUID (the value's own real type tag), not StyleBase.m_TypeGUID - the two
        // aren't always equal (see RegisterOne's comment), so Value's is the one guaranteed to agree
        // with what got registered.
        auto* pDrawFn = xproperty::ui::details::ResolveDrawFn(Value.m_pType->m_GUID, StyleBase.m_StyleGUID);
        assert(pDrawFn);

        //ImGui::PushID(&Entry);
        pDrawFn(GUID, Cmd, Value, StyleBase, Flags);
        //ImGui::PopID();
    }

    //=================================================================================================

    struct group_render
    {
        static void RenderElement( inspector::entry& GroupEntry, int iElement, xproperty::ui::undo::cmd& Cmd, const xproperty::any& Value, const xproperty::type::members& Entry, xproperty::flags::type Flags, inspector& Inspector, inspector::entry& IEntry ) noexcept
        {
            //
            // Handle the case of vector2
            // All vector 2 should have 2 elements in the following order...
            // [0] = X, [1] = Y
            //
            if( GroupEntry.m_GroupGUID == xproperty::settings::vector2_group::guid_v 
             || GroupEntry.m_GroupGUID == xproperty::settings::vector3_group::guid_v)
            {
                int         MaxElemens = GroupEntry.m_GroupGUID == xproperty::settings::vector2_group::guid_v ? 2 : 3;
                auto&       I = member_ui<float>::defaults::data_v;
                ImGuiStyle* style = &ImGui::GetStyle();
                const auto   Width = (ImGui::GetContentRegionAvail().x - style->ItemInnerSpacing.x - 14* MaxElemens)  / MaxElemens;
                const auto   Height = ImGui::GetFrameHeight();
                ImVec2       pos;
                static constexpr auto Colors = std::array<ImU32, 3>{ 0x440000ff, 0x4400ff00, 0x44ff0000 };

                if (iElement == 0) ImGui::PushItemWidth(Width);
                else               ImGui::SameLine(0, 2);

                if (Flags.m_bShowReadOnly) ImGui::BeginDisabled(true);
                ImGui::Text("%c:", Entry.m_pName[0]);
                if (Flags.m_bShowReadOnly) ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) Inspector.Help(IEntry);
                ImGui::SameLine();
                pos = ImGui::GetCursorScreenPos();


                ImGui::PushID(Entry.m_GUID);
                onRender<xproperty::settings::member_ui_t>(Entry.m_GUID, Cmd, Value, Entry, Flags);
                ImGui::PopID();

                if( iElement == (MaxElemens-1) ) ImGui::PopItemWidth();
                ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + Width, pos.y + Height), Colors[iElement]);
            }
        }
    };
}


//-------------------------------------------------------------------------------------------------
// Inspector
//-------------------------------------------------------------------------------------------------

static std::array<ImColor, 20> s_ColorCategories =
{
    ImColor{ 0xffe8c7ae },
    ImColor{ 0xffb4771f },
    ImColor{ 0xff0e7fff },
    ImColor{ 0xff2ca02c },
    ImColor{ 0xff78bbff },
    ImColor{ 0xff8adf98 },
    ImColor{ 0xff2827d6 },
    ImColor{ 0xff9698ff },
    ImColor{ 0xffbd6794 },
    ImColor{ 0xffd5b0c5 },
    ImColor{ 0xff4b568c },
    ImColor{ 0xff949cc4 },
    ImColor{ 0xffc277e3 },
    ImColor{ 0xffd2b6f7 },
    ImColor{ 0xff7f7f7f },
    ImColor{ 0xffc7c7c7 },
    ImColor{ 0xff22bdbc },
    ImColor{ 0xff8ddbdb },
    ImColor{ 0xffcfbe17 },
    ImColor{ 0xffe5da9e }
};

//-------------------------------------------------------------------------------------------------

void xproperty::inspector::clear(void) noexcept
{
    m_lEntities.clear();
//    m_UndoSystem.clear();
}

//-------------------------------------------------------------------------------------------------
void xproperty::inspector::AppendEntity(void) noexcept
{
    m_lEntities.push_back( std::make_unique<entity>() );
}

//-------------------------------------------------------------------------------------------------
void xproperty::inspector::AppendEntityComponent(const xproperty::type::object& Object, void* pBase, void* pUserData ) noexcept
{
    auto Component = std::make_unique<component>();

    // Cache the information
    Component->m_Base = { &Object, pBase };
    Component->m_pUserData = pUserData;

    m_lEntities.back()->m_lComponents.push_back(std::move(Component));

}

//-------------------------------------------------------------------------------------------------


void xproperty::inspector::RefreshAllProperties(component& C) noexcept
{
    C.m_List.clear();
    int         iDimensions = -1;
    int         myDimension = -1;

    //
    // Start processing the properties...
    //
    xproperty::sprop::collector(C.m_Base.second, *C.m_Base.first, *m_pContext, [&](const char* pPropertyName, xproperty::any&& Value, const xproperty::type::members& Member, bool isConst, const void* pInstance)
        {
            std::uint32_t          GUID = Member.m_GUID;
            std::uint32_t          GroupGUID = 0;

            // Handle the flags
            xproperty::flags::type Flags = [&]
                {
                    if (auto* pDynamicFlags = Member.getUserData<xproperty::settings::member_dynamic_flags_t>(); pDynamicFlags)
                    {
                        return pDynamicFlags->m_pCallback(pInstance, *m_pContext);
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

            Flags.m_bShowReadOnly |= isConst;

            const char* pSectionName = [&]() -> const char*
                {
                    if (auto* pSection = Member.getUserData<xproperty::settings::member_section_t>(); pSection)
                        return pSection->m_pSectionName;
                    return nullptr;
                }();

            bool bScope = std::holds_alternative<xproperty::type::members::scope>(Member.m_Variant)
                || std::holds_alternative<xproperty::type::members::props>(Member.m_Variant);

            const bool bAtomicArray = std::holds_alternative<xproperty::type::members::list_var>(Member.m_Variant);

            const bool bDefaultOpen = [&]
                {
                    if (auto* pDefaultOpen = Member.getUserData<xproperty::settings::member_ui_open_t>(); pDefaultOpen) return pDefaultOpen->m_bOpen;
                    return !(bAtomicArray || std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant));
                }();

            if (bScope || std::holds_alternative<xproperty::type::members::var>(Member.m_Variant))
            {
                iDimensions = -1;
                myDimension = -1;
            }

            if (std::holds_alternative<xproperty::type::members::props>(Member.m_Variant)
                || std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant))
            {
                // GUIDs for groups are marked as u32... vs sizes are mark as u64
                if (Value.m_pType->m_GUID == xproperty::settings::var_type<std::uint32_t>::guid_v)
                {
                    GroupGUID = Value.get<std::uint32_t>();
                }
            }

            // Check if we are dealing with atomic types and the size field...
            if (std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant)
                || std::holds_alternative<xproperty::type::members::list_var>(Member.m_Variant))
            {
                const auto NameLen = std::strlen(pPropertyName);
                auto i = NameLen;
                if ((pPropertyName[i - 1] == ']') && (pPropertyName[i - 2] == '['))
                {
                    bScope = true;

                    std::visit([&](auto& List) constexpr
                        {
                            if constexpr (std::is_same_v<decltype(List), const xproperty::type::members::list_props&> ||
                                std::is_same_v<decltype(List), const xproperty::type::members::list_var&>)
                            {
                                myDimension = 1;
                                iDimensions = static_cast<int>(List.m_Table.size());
                                // Count how many complete "[key:value]" index groups precede the
                                // trailing "[]" - each group is multiple characters (e.g. "[G:0]"), so
                                // counting only consecutive ']' characters (the previous approach)
                                // silently undercounts past 2 dimensions: after the ONE ']' that
                                // closes the immediately-preceding group, the very next character
                                // back is that group's own value digit, not another ']', so the old
                                // loop stopped there regardless of how many more groups actually
                                // preceded it - confirmed by direct tracing, a 3D array's innermost
                                // marker came back myDimension=2, identical to its own middle marker,
                                // producing two visually-identical sibling rows in the inspector.
                                // Walk backward one whole bracket group at a time instead: from the
                                // ']' that closes whatever precedes the trailing "[]", jump back to
                                // that group's own '[', then step one more character to land on
                                // whatever closes the group before THAT, repeating until there's no
                                // preceding "]" left to find.
                                if (NameLen >= 3)
                                {
                                    i -= 3;
                                    while (i < NameLen && pPropertyName[i] == ']') // i < NameLen also
                                                                                    // catches the
                                                                                    // unsigned wrap
                                                                                    // from i=0 below
                                    {
                                        myDimension++;
                                        auto Open = i;
                                        while (Open > 0 && pPropertyName[Open] != '[') --Open;
                                        if (Open == 0) break;
                                        i = Open - 1;
                                    }
                                }
                            }
                            else
                            {
                                assert(false);
                            }

                        }, Member.m_Variant);

                    // We don't deal with zero size arrays...
                    if (0 == Value.get<std::size_t>())
                        return;
                }
                else
                {

                }
            }

            auto* pHelp = Member.getUserData<xproperty::settings::member_help_t>();

            C.m_List.push_back
            (std::make_unique<entry>
                (0, 0
                    , xproperty::sprop::container::prop{ pPropertyName, std::move(Value) }
                    , pHelp ? pHelp->m_pHelp : "<<No help>>"
                    , Member.m_pName
                    , Member.m_GUID
                    , GroupGUID
                    , &Member //bScope ? nullptr : &Member
                    , iDimensions
                    , myDimension
                    , Flags
                    , bScope
                    , bAtomicArray
                    , bDefaultOpen
                    , const_cast<void*>(pInstance)
                    , pSectionName
                )
            );
        }, true);
}

//-------------------------------------------------------------------------------------------------

void xproperty::inspector::Render( component& C, int& GlobalIndex ) noexcept
{
    struct element
    {
        std::string_view     m_Path;
        std::uint32_t        m_CRC;
        int                  m_iArray;
        std::size_t          m_iStart;
        std::size_t          m_iEnd;
        int                  m_OpenAll;
        int                  m_MyDimension;
        bool                 m_bArrayMustInsertIndex = false;
        bool                 m_isOpen          : 1
                             , m_isAtomicArray : 1
                             , m_isReadOnly    : 1
                             , m_isHidden      : 1
                             , m_isDefaultOpen : 1
                             ;
    };

    int                         iDepth   = -1;
    std::array<element,32>      Tree;
    // Name of the last member_section drawn at each tree depth this render pass - lets the section-
    // separator check below fire only once per distinct section name, without ever leaking a section
    // label from one object scope into a sibling or nested one (reset to nullptr the moment a new
    // scope is pushed, in PushTreeStruct below).
    std::array<const char*,32> LastSectionAtDepth{};

    constexpr auto ComputeCRC = []( std::string_view Str, std::size_t iEnd ) constexpr
    {
        return xproperty::settings::strguid({ Str.data(), static_cast<std::uint32_t>(iEnd+1)});
    };

    const auto PushTreeStruct = [&]( bool Open, std::string_view Path, int myDimension, bool bDefaultOpen, bool isReadOnly, bool isHidden, bool bArray = false, bool bAtomic = false )
    {
        //
        // Compute start / end
        //
        auto& L = Tree[++iDepth];
        LastSectionAtDepth[iDepth] = nullptr;

        // Compute start / end
        if (iDepth == 0) L.m_iStart = 0;
        else
        {
            L.m_iStart = Tree[iDepth - 1].m_iEnd + 1;
            while (Path[L.m_iStart] == '/') 
              L.m_iStart++;
        }

        if (L.m_iStart && Path[L.m_iStart-1] == '[')
        {
            L.m_iEnd = L.m_iStart+1;
            while (Path[L.m_iEnd] != ']') L.m_iEnd++;
        }
        else 
        {
            L.m_iEnd = Path.size();
            // Only an empty "[]" marker (a not-yet-indexed array-dimension placeholder) trims -
            // a path legitimately ending in a real index like "[G:0]" (e.g. an object-array
            // instance's own scope prefix) must keep its full length as the boundary, or
            // CheckSameLevel mis-bounds this scope and pops out on the instance's own siblings.
            if (Path[L.m_iEnd - 1] == ']' && Path[L.m_iEnd - 2] == '[')
            {
                L.m_iEnd -= 2;
            }
        }

        // set remaining fields
        L.m_Path            = Path;
        L.m_CRC             = ComputeCRC(Path, L.m_iEnd);
        L.m_iArray          = bArray ? 0 : -1;
        L.m_OpenAll         = 0;
        L.m_isOpen          = Open;
        L.m_isDefaultOpen   = bDefaultOpen;
        L.m_isAtomicArray   = bAtomic;
        L.m_isReadOnly      = isReadOnly || ((iDepth > 0) ? Tree[iDepth - 1].m_isReadOnly : false);
        L.m_MyDimension     = myDimension;
        L.m_isHidden        = isHidden;
        L.m_bArrayMustInsertIndex = false;

        return Open;
    };

    const auto PushTree = [&]( const char* pTreeName, bool bCustomDraw, std::string_view Path, int myDimension, bool bDefaultOpen, bool isReadOnly, bool isHidden, bool bArray = false, bool bAtomic = false )
    {
        bool Open = iDepth<0? true : Tree[ iDepth ].m_isOpen;
        if( Open )
        {
            if ( iDepth >0 && Tree[iDepth-1].m_OpenAll ) ImGui::SetNextItemOpen( Tree[iDepth-1].m_OpenAll > 0 );

            const ImGuiTreeNodeFlags flags = (bDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0) | ((iDepth == -1) ? ImGuiTreeNodeFlags_Framed : 0);
            if (bCustomDraw) m_OnResourceLeftSize.NotifyAll( *this, *C.m_Base.first, C.m_Base.second, Path, xproperty::any{}, flags, pTreeName, Open ); // no single leaf value at a scope/tree header - empty any
            else             Open = ImGui::TreeNodeEx(pTreeName, flags);
        }

        PushTreeStruct(Open, Path, myDimension, bDefaultOpen, isReadOnly, isHidden, bArray, bAtomic);

        return Open;
    };

    auto PopTree = [ & ]()
    {
        // Handle muti-dimensional array increment of entries
        if ( iDepth >= 1 )
        {
            if (Tree[iDepth-1].m_isAtomicArray == false && Tree[iDepth - 1].m_iArray >= 0)
            {
                Tree[iDepth - 1].m_iArray++;
            }
        }
        
        const auto& E = Tree[ iDepth-- ];

        if( E.m_isOpen )
        {
            ImGui::TreePop();
        }
    };

    //
    // Deal with the top most tree
    //
    {
        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, m_Settings.m_TableFramePadding );
        ImGui::AlignTextToFramePadding();

        // If the main tree is Close then forget about it
        PushTree(C.m_Base.first->m_pName, false, C.m_Base.first->m_pName, -1, true, false, false);

        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled( pos, ImVec2( pos.x + ImGui::GetContentRegionAvail().x, pos.y + ImGui::GetFrameHeight() ), ImGui::GetColorU32( ImGuiCol_Header ) );
        ImGui::PopStyleVar();
    }
        
    if( Tree[iDepth].m_isOpen == false )
    {
        PopTree();
        return;
    }

    //
    // Do all properties
    //
    for ( std::size_t iE = 0; iE<C.m_List.size(); ++iE )
    {
        auto& E = *C.m_List[iE];

        //
        // If the user ask us to hide this property we will do so
        //
        if (Tree[iDepth].m_isHidden)
        {
            const auto& T = Tree[iDepth];
            if( E.m_Property.m_Path.length() >= T.m_iEnd && ComputeCRC(E.m_Property.m_Path, T.m_iEnd ) == Tree[iDepth].m_CRC)
               continue;

            --iDepth;
        }

        //
        // If we have a close tree skip same level entries
        //
        auto CheckSameLevel = [&]
        {
            const auto& T = Tree[iDepth];
            if (auto l = E.m_Property.m_Path.length(); l < T.m_iStart || l < T.m_iEnd) return false;

            // Handle multidimensional arrays...
            if (E.m_bScope 
             && E.m_MyDimension > 1 
             && Tree[iDepth].m_iArray >= 0 
             && Tree[iDepth].m_MyDimension >= E.m_MyDimension
             ) return false;

            return ComputeCRC(E.m_Property.m_Path, T.m_iEnd) == Tree[iDepth].m_CRC;
        };

        bool bPoped = false;
        while (CheckSameLevel() == false)
        {
            PopTree();
            bPoped = true;
        }

        // A scope is hidden...
        if (E.m_Flags.m_bDontShow && E.m_bScope)
        {
            // Push a temp tree node
            PushTreeStruct( false, E.m_Property.m_Path, 0, false, false, true );

            // Start skipping the entries
            continue;
        }

        if( Tree[iDepth].m_isOpen == false )
        {
            continue;
        }
            

        //
        // Do we need to pop scopes?
        //

        // Make sure at this point everything is open
        assert( Tree[iDepth].m_isOpen );

        // A property is hidden...
        if (E.m_Flags.m_bDontShow) continue;

#ifdef XCORE_PROPERTIES_H
        // Valid for this entry's whole row (both columns) - see current_property_t's own comment for
        // why the resource-type draw<T,Style>::Render specializations need this side-channel at all.
        m_CurrentProperty = { C.m_Base.first, C.m_Base.second, E.m_Property.m_Path };
#endif

        // A member_section tag starts a new named section - draw a separator once, the first time
        // its name is seen at this depth (LastSectionAtDepth is reset to nullptr whenever a new scope
        // is pushed, so a section label never leaks from one object instance into a sibling or a
        // nested one). Pure layout sugar - no effect on m_List/serialization/the CRC-boundary scope
        // machinery above.
        if (E.m_pSectionName && E.m_pSectionName != LastSectionAtDepth[iDepth]
            && (LastSectionAtDepth[iDepth] == nullptr || std::strcmp(E.m_pSectionName, LastSectionAtDepth[iDepth]) != 0))
        {
            LastSectionAtDepth[iDepth] = E.m_pSectionName;
            // The loop always rests in the RIGHT column at this point (the tail end of the previous
            // entry's own row - "Render the left column" below unconditionally calls NextColumn() to
            // move from there into the left column for whatever entry comes next, every single time).
            // So this block must reproduce that same one-entry shape - NextColumn() into the left
            // column first, draw there, NextColumn() into the right column, draw there too - or every
            // row rendered after it ends up shifted by one column (confirmed live: an earlier version
            // that skipped this ordering swapped labels and values for the rest of the panel).
            // Independent per-column draws, not a merged/full-width one - same trick the top-level
            // scope header above already uses (AddRectFilled in each column separately) to look like
            // one continuous bar without ever touching the column count.
            ImGui::NextColumn();
            ImGui::SeparatorText(E.m_pSectionName);
            ImGui::NextColumn();
            // Call the exact same real widget again, with an empty label, instead of hand-drawing a
            // line to approximate it - two attempts at reverse-engineering SeparatorText's internal
            // line-Y math from memory (GetTextLineHeight()*0.5f, then a hand-rolled
            // ImTrunc((min+max)*0.5f) copied from a possibly-stale recollection of imgui_widgets.cpp)
            // both left it visibly 1px off against this actual 1.92.5 build. Calling the real ImGui
            // function a second time is exact by construction, whatever its internals actually are -
            // an empty label draws a plain full-width line with no text gap, which is exactly what the
            // right column needs since "Group A"'s text already appears on the left.
            ImGui::SeparatorText("");
        }

        //
        // Render the left column
        //
        ++GlobalIndex;
        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
    //    ImVec2 lpos = ImGui::GetCursorScreenPos();
        auto CRA = ImGui::GetContentRegionAvail();
        // E.m_GUID (a per-MEMBER constant, same for every element of an array) plus a per-tree-level
        // counter that resets fresh for every sibling container is not actually unique across
        // siblings - confirmed by a real ImGui "conflicting ID" warning once a 3D array's per-branch
        // leaves (all reset to the same starting counter value under their own freshly-pushed parent)
        // collided. E.m_Property.m_Path is genuinely unique per logical entry (confirmed by the
        // collector's own trace) and, unlike a visible-row ordinal, stays STABLE across frames
        // regardless of what else is expanded/collapsed elsewhere - folding its hash in fixes the
        // collision without introducing the frame-to-frame ID instability a visible-row-index would.
        const auto PathHash = static_cast<int>(std::hash<std::string>{}(E.m_Property.m_Path));
        if( Tree[iDepth].m_iArray >= 0 ) E.m_LeftUIGUID = E.m_GUID + Tree[iDepth].m_iArray + iDepth * 1000 + Tree[iDepth].m_MyDimension * 1000000 + PathHash;
        else                             E.m_LeftUIGUID = E.m_GUID + iDepth * 1000 + PathHash;
        ImGui::PushID(E.m_LeftUIGUID);
        E.m_RightUIGUID = static_cast<int>(std::hash<size_t>{}(static_cast<size_t>(E.m_LeftUIGUID)) % std::numeric_limits<int>::max());

        bool bRenderBlankRight = false;

        // Set by the E.m_bScope branch below (obj_scope_toggle support) when this scope's first
        // child carries scope_toggle_t - read back by the right-column section further down, since
        // the checkbox itself belongs in the value column like any other bool, not merged into the
        // header row on the left (name stays reserved for the label).
        entry* pScopeToggle              = nullptr;
        bool   bScopeToggleValue         = false;
        bool   bScopeToggleHasExpandable = false; // does this scope have any child worth bulk open/close-ing? see the O/C scan below

#ifdef XCORE_PROPERTIES_H
        const bool bCustomRender = E.m_Property.m_Value.m_pType && E.m_Property.m_Value.m_pType->m_GUID == xproperty::settings::var_type<xresource::full_guid>::guid_v;

        if (bCustomRender)
        {
            m_SimpleDrawBk.m_iDepth         = iDepth;
            m_SimpleDrawBk.m_GlobalIndex    = GlobalIndex;
        }
        else
        {
            if (m_Settings.m_bRenderLeftBackground) DrawBackground(iDepth, GlobalIndex);
        }

        /*
        if (E.m_Property.m_Value.m_pType && E.m_Property.m_Value.m_pType->m_GUID == xproperty::settings::var_type<xresource::full_guid>::guid_v)
        {
            ImGuiStyle& style = ImGui::GetStyle();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 18.0f));
            if (m_Settings.m_bRenderLeftBackground) DrawBackground(iDepth, GlobalIndex);
            // Get the bounding box of the last item (the tree node)
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0.2f));
            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID)), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "  %s", E.m_pName);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            goto SKIP_TO_NEXTHINGS;
        }
        else
        {
            if (m_Settings.m_bRenderLeftBackground) DrawBackground(iDepth, GlobalIndex);
        }
        */
#else
        const bool bCustomRender = false;
        if (m_Settings.m_bRenderLeftBackground) DrawBackground(iDepth, GlobalIndex);
#endif

        // Handle property groups
        if (E.m_GroupGUID != 0)
        {
            if( E.m_GroupGUID == xproperty::settings::vector2_group::guid_v 
             || E.m_GroupGUID == xproperty::settings::vector3_group::guid_v )
            {
                // This guy is not longer a scope...
                E.m_bScope = false;
            }
        }

        // Create a new tree
        // array with in array?
        if (Tree[iDepth].m_bArrayMustInsertIndex)
        {
            std::array<char, 128> Name;
            snprintf(Name.data(), Name.size(), "[%d]", Tree[iDepth].m_iArray);

            if (Tree[iDepth].m_isAtomicArray || E.m_GroupGUID)
            {
                Tree[iDepth].m_iArray++;
                bool Open;
                const auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (bCustomRender) m_OnResourceLeftSize.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, flags, Name.data(), Open);
                else               ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID + Tree[iDepth].m_iArray)), flags, "%s", Name.data());
            }
            else
            {
                // The [i] index node's own scope boundary must match the OBJECT INSTANCE's own path
                // prefix (e.g. "...[G:0][G:0]"), not E.m_Property.m_Path as-is - E here is whichever
                // entry happened to trigger this insertion, which is always the instance's FIRST
                // reflected sub-property (e.g. "...[G:0][G:0]/var"). Registering the scope boundary
                // with that longer, member-specific path made CheckSameLevel reject this SAME
                // instance's own later siblings ("setValues", "CheckValues") as not matching it,
                // popping back out prematurely and re-triggering another spurious index insertion for
                // each one - confirmed by direct observation: 2 real base1 instances rendered as 6
                // separate "[i]" rows, one per reflected sub-property instead of one per instance.
                std::string_view InstancePath = E.m_Property.m_Path;
                if (const auto Slash = InstancePath.rfind('/'); Slash != std::string_view::npos)
                    InstancePath = InstancePath.substr(0, Slash);

                PushTree(Name.data(), bCustomRender, InstancePath, E.m_MyDimension, Tree[iDepth].m_isDefaultOpen, Tree[iDepth].m_isReadOnly, Tree[iDepth].m_isHidden);

                bRenderBlankRight = true;

                iE--;
            }
        }
        else if (E.m_Property.m_Path.back() == ']')
        {
            if (*(E.m_Property.m_Path.end() - 2) == '[' )
            {
                // E.m_MyDimension is the CURRENT walk depth of this size-marker (1 at the outermost
                // level of ANY array, 1D or not) - it is not the array's total dimensionality
                // (E.m_Dimensions, constant for the whole array). The old `== 1` check conflated the
                // two, so a genuinely 2D/3D array's outer level(s) were mislabeled "[1d]" and treated
                // as if no further nesting existed, while the deepest level (where m_MyDimension
                // actually reaches m_Dimensions) fell into the "else" branch instead - which built its
                // composite name from Tree[iDepth-i].m_iArray entries that hadn't been pushed yet at
                // that point, since it was written assuming it only ever ran below at least one
                // already-pushed array level. Both branches collapse into one correct behavior: every
                // level (intermediate or final) pushes a node and lets the existing
                // m_bArrayMustInsertIndex mechanism above insert this level's own [index] and, if more
                // dimensions remain, recurse into the next one exactly the same way regardless of
                // depth. Label: "Name[Nd]" at the outermost level (N = total dimensions, no indices
                // chosen yet). What comes after that at deeper levels depends on whether a SEPARATE
                // [index] row exists for this array or not (see the bAtomicArray branch above this
                // one): for an atomic/scalar array, every level's own [index] row is skipped (folded
                // into a single leaf draw with no recursion), so the size-marker label is the ONLY
                // place any index ever appears - it must carry the full accumulated chain itself
                // ("Name[Md][i][j]..."). For a non-atomic/object array, each level genuinely DOES get
                // its own standalone [index] row (each element needs its own expandable scope for its
                // sub-properties) - repeating those same indices in the size-marker label directly
                // below that row would show the identical index twice for no reason, confirmed by
                // direct observation of a real 3D object array's rendered tree ("[0]" row immediately
                // followed by a label that says "[0]" again). Corrected design: size-marker labels
                // ALWAYS carry their own full accumulated-index chain, for both atomic and object
                // arrays alike - a separate standalone [index] row only ever gets inserted where one
                // is structurally necessary (see the bArrayMustInsertIndex-setting condition below,
                // fixed to fire only at the DEEPEST size-marker of an object array, right before the
                // real per-element content - each object element needs its own pushed scope for its
                // own sub-properties to unfold under, unlike a scalar leaf or an intermediate
                // container level, neither of which needs one). Indices are parsed directly out of
                // this entry's own path string ("...[g:0][g:0][]" for the 3rd level down) rather than
                // trusted from Tree-stack state, which had already proven unreliable at this exact
                // spot once.
                std::array<char, 128> Name;
                int NameOffset = snprintf(Name.data(), Name.size(), "%s[%dd]", E.m_pName, E.m_Dimensions - E.m_MyDimension + 1);
                if (E.m_MyDimension > 1)
                {
                    const std::string_view Path  = E.m_Property.m_Path;
                    std::size_t            Start = Path.find('[');
                    for (int Dim = 0; Dim < E.m_MyDimension - 1 && Start != std::string_view::npos; ++Dim)
                    {
                        const std::size_t Colon = Path.find(':', Start);
                        const std::size_t Close = Path.find(']', Start);
                        if (Colon != std::string_view::npos && Close != std::string_view::npos && Colon < Close)
                        {
                            const std::string_view Value = Path.substr(Colon + 1, Close - Colon - 1);
                            NameOffset += snprintf(&Name[NameOffset], Name.size() - NameOffset, "[%.*s]", static_cast<int>(Value.size()), Value.data());
                        }
                        Start = Path.find('[', Close + 1);
                    }
                }
                snprintf(&Name[NameOffset], Name.size() - NameOffset, " ");
                PushTree(Name.data(), bCustomRender, E.m_Property.m_Path, E.m_MyDimension, E.m_bDefaultOpen, E.m_Flags.m_bShowReadOnly, false, true, E.m_bAtomicArray);

                // Only insert a real, separate [index] row at the DEEPEST size-marker of an object
                // (non-atomic) array - the transition from "which dimension" bookkeeping into actual
                // per-element content, where each base1/etc. instance genuinely needs its own pushed
                // scope for its own sub-properties (var/setValues/CheckValues) to unfold under.
                // Intermediate levels (myDim < Dimensions) recurse straight into the NEXT size-marker
                // instead, which already carries the just-chosen index in its own label (above) - no
                // extra row needed, and none was structurally required there in the first place.
                if (Tree[iDepth].m_isAtomicArray == false && E.m_MyDimension == E.m_Dimensions)
                {
                    Tree[iDepth].m_bArrayMustInsertIndex = true;
                }
            }
            else
            {
                std::array<char, 128> Name;
                snprintf(Name.data(), Name.size(), "[%d]", Tree[iDepth].m_iArray++);

                // Atomic array
                if (Tree[iDepth].m_isAtomicArray || E.m_GroupGUID)
                {
                    bool Open;
                    const auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (bCustomRender) m_OnResourceLeftSize.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, flags, Name.data(), Open);
                    else               ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID + Tree[iDepth].m_iArray)), flags, "%s", Name.data());
                }
                else
                {
                    // First entry of the array?
                    PushTree(Name.data(), bCustomRender, E.m_Property.m_Path, E.m_MyDimension, Tree[iDepth].m_isDefaultOpen, Tree[iDepth].m_isReadOnly, Tree[iDepth].m_isHidden);

                    bRenderBlankRight = true;

                    // We need to redo this entry
                    iE--;
                }
            }
        }
        else if (E.m_bScope)
        {
            // A scope whose FIRST child carries scope_toggle_t (obj_scope_toggle) gets that bool
            // rendered as a checkbox in the RIGHT (value) column, in this same header row - the left
            // column stays reserved for the name, matching every other row's convention. Only the
            // peek + open-state sync happen here; the actual checkbox is drawn in the right-column
            // section below via pScopeToggle/bScopeToggleValue. Positional check only (must be the
            // immediate next entry in the flat list AND a genuine child of this scope, verified via
            // path-prefix) - no scope-wide search.
            if (static_cast<std::size_t>(iE) + 1 < C.m_List.size())
            {
                auto& Next = *C.m_List[iE + 1];
                if (Next.m_pUserData && Next.m_pUserData->getUserData<xproperty::settings::scope_toggle_t>()
                    && Next.m_Property.m_Path.size() > E.m_Property.m_Path.size()
                    && Next.m_Property.m_Path.compare(0, E.m_Property.m_Path.size(), E.m_Property.m_Path) == 0)
                {
                    pScopeToggle = &Next;
                }
            }

            bScopeToggleValue = pScopeToggle ? pScopeToggle->m_Property.m_Value.get<bool>() : false;
            if (pScopeToggle) ImGui::SetNextItemOpen(bScopeToggleValue); // keep the arrow's own visual state in sync - it isn't independently toggleable once a toggle child exists

            // O/C only mean something if at least one child is itself something that can be opened
            // or closed (a nested scope, or an array) - a scope made entirely of plain scalar leaves
            // (e.g. this one's Speed Multiplier/Max Retries/Debug Tag) has nothing to bulk-toggle.
            // Scan only as far as this scope's own children (path-prefix bounded, same check as the
            // toggle peek above) - cheap, bounded by this one scope's child count, not the whole tree.
            if (pScopeToggle)
            {
                for (std::size_t j = static_cast<std::size_t>(iE) + 2; j < C.m_List.size(); ++j)
                {
                    auto& Child = *C.m_List[j];
                    if (Child.m_Property.m_Path.size() <= E.m_Property.m_Path.size()
                        || Child.m_Property.m_Path.compare(0, E.m_Property.m_Path.size(), E.m_Property.m_Path) != 0)
                        break; // no longer a descendant of this scope

                    if (Child.m_bScope || (Child.m_Property.m_Path.size() >= 2 && Child.m_Property.m_Path.back() == ']'))
                    {
                        bScopeToggleHasExpandable = true;
                        break;
                    }
                }
            }

            PushTree(E.m_pName, bCustomRender, E.m_Property.m_Path, E.m_MyDimension, E.m_bDefaultOpen, E.m_Flags.m_bShowReadOnly, E.m_Flags.m_bDontShow);

            if (pScopeToggle) ++iE; // this entry is fully handled via pScopeToggle - skip its own normal row below
        }
        else
        {
            bool Open;
            const auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            // Ask a registered consumer (if any) whether this property currently differs from
            // whatever THEY consider its base value - xproperty never tries to know what "overridden"
            // means itself. E.m_Property.m_Path is the full, canonical path (already embeds any array
            // index, e.g. "m_lTextures[G:2]") - a complete, opaque key either for a lookup into a
            // consumer-owned override-set, or to hand straight back into sprop::getProperty against a
            // second/base object, whichever strategy the consumer uses. The already-resolved current
            // value is passed too so a simple consumer doesn't need to re-fetch it.
            bool bIsOverridden = false;
            m_OnOverrideCheck.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, bIsOverridden);
            if (bIsOverridden)
            {
                // Tint stays pushed through the label draw below too (matches E20's own convention -
                // the whole row's left-column text recolors, not just the button), popped right after.
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(170, 170, 255, 255));
                if (ImGui::Button(">"))
                {
                    m_OnOverrideReset.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path);
                }
                HelpMarker( "This property has been overridden from its base value - click to revert" );
                ImGui::SameLine();
            }

            // A real element (not a size-marker) belonging to a multi-dimensional array still
            // carries the array's own Member descriptor (list_var/list_props, same as its size
            // markers), so E.m_pName here would otherwise repeat the raw array name with none of
            // the indices that got it here - same disambiguation problem as the size-marker labels
            // above, fixed the same way: "Element[i][j]...[k]" with every index parsed straight out
            // of this entry's own path ("...[g:0][g:0][g:0]", no trailing "[]" for a real leaf).
            // Left as the plain E.m_pName for a genuinely 1D array - that convention already worked
            // and was never reported broken.
            std::array<char, 128> ElementName;
            const char* pLeftLabel = E.m_pName;
            if (E.m_Dimensions > 1)
            {
                int                     Offset = snprintf(ElementName.data(), ElementName.size(), "Element");
                const std::string_view  Path   = E.m_Property.m_Path;
                std::size_t             Start  = Path.find('[');
                for (int Dim = 0; Dim < E.m_Dimensions && Start != std::string_view::npos; ++Dim)
                {
                    const std::size_t Colon = Path.find(':', Start);
                    const std::size_t Close = Path.find(']', Start);
                    if (Colon != std::string_view::npos && Close != std::string_view::npos && Colon < Close)
                    {
                        const std::string_view Value = Path.substr(Colon + 1, Close - Colon - 1);
                        Offset += snprintf(&ElementName[Offset], ElementName.size() - Offset, "[%.*s]", static_cast<int>(Value.size()), Value.data());
                    }
                    Start = Path.find('[', Close + 1);
                }
                pLeftLabel = ElementName.data();
            }

            if (bCustomRender) m_OnResourceLeftSize.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, flags, pLeftLabel, Open);
            else               ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID)), flags, "%s", pLeftLabel);

            if (bIsOverridden) ImGui::PopStyleColor();
        }


        // Print the help
        if ( ImGui::IsItemHovered() && bRenderBlankRight == false )
        {
            Help( E );
        }

        //
        // Render the right column
        //
        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::PushItemWidth( -1 );

        ImVec2 rpos = ImGui::GetCursorScreenPos();
        CRA = ImGui::GetContentRegionAvail();

        auto HandleElement = [this, &C](entry& Entry, entry& ParentEntry, int i, bool bElementTag )
        {
            // Any other entry except the editing entry gets handle here
            xproperty::ui::undo::cmd Cmd;

            if (ParentEntry.m_GroupGUID)
            {
                xproperty::ui::details::group_render::RenderElement(ParentEntry, i, Cmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags, *this, Entry);
            }
            else
            {
                if(bElementTag) xproperty::ui::details::onRender<xproperty::settings::member_ui_t>          (Entry.m_RightUIGUID, Cmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags);
                else            xproperty::ui::details::onRender<xproperty::settings::member_ui_list_size_t>(Entry.m_RightUIGUID, Cmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags);
            }

            if (Cmd.m_isEditing || Cmd.m_isChange)
            {
                // Set the property value
                if (Cmd.m_isChange)
                {
                    Cmd.m_Name          = Entry.m_Property.m_Path;
                    Cmd.m_pClassObject  = C.m_Base.second;
                    Cmd.m_pPropObject   = C.m_Base.first;

                    m_OnRealtimeChangeEvent.NotifyAll(*this, Cmd, *m_pContext );

                    Cmd.m_bHasChanged = true;
                }

                if (Cmd.m_bHasChanged && Cmd.m_isEditing == false )
                {
                    // Insert the cmd into the list
                    Cmd.m_Name.assign(Entry.m_Property.m_Path);
                    Cmd.m_pPropObject = C.m_Base.first;
                    Cmd.m_pClassObject = C.m_Base.second;

                    m_OnChangeEvent.NotifyAll(*this, Cmd);
                    m_CmdCurrentEdit = nullptr;
                }
                else
                {
                    m_CmdCurrentEdit = Cmd;
                }
            }
        };

        if( E.m_bScope || bRenderBlankRight )
        {
            if ( m_Settings.m_bRenderRightBackground ) DrawBackground( iDepth-1, GlobalIndex );

            if (pScopeToggle)
            {
                // |Checkbox|O|C| - O/C (open/close all this scope's own children) only make sense,
                // and only appear, once the checkbox is actually true - no point offering to
                // open/close-all contents that aren't even shown.
                bool NewValue = bScopeToggleValue;
                if (ImGui::Checkbox("##ScopeToggle", &NewValue) && NewValue != bScopeToggleValue)
                {
                    // Same commit shape the reflected-function button branch below already uses
                    // (build a cmd, fire m_OnChangeEvent) - not the generic onRender<member_ui_t>
                    // dispatcher, which also tracks "the entry currently mid-edit" (m_CmdCurrentEdit)
                    // across frames; this checkbox is drawn outside pScopeToggle's own normal loop
                    // slot (that entry's row was skipped above), so it must commit on its own rather
                    // than risk desyncing that unrelated per-frame edit-tracking state.
                    std::string        Error;
                    xproperty::any     NewAny; NewAny.set<bool>(NewValue);
                    xproperty::sprop::setProperty(Error, C.m_Base.second, *C.m_Base.first, xproperty::sprop::container::prop{ pScopeToggle->m_Property.m_Path, NewAny }, *m_pContext);

                    xproperty::ui::undo::cmd Cmd;
                    Cmd.m_Name          = pScopeToggle->m_Property.m_Path;
                    Cmd.m_pClassObject  = C.m_Base.second;
                    Cmd.m_pPropObject   = C.m_Base.first;
                    Cmd.m_bHasChanged   = true;
                    m_OnChangeEvent.NotifyAll(*this, Cmd);
                }

                if (NewValue && bScopeToggleHasExpandable)
                {
                    ImGui::SameLine();
                    if ( ImGui::Button( " O " ) ) Tree[iDepth].m_OpenAll = 1;
                    HelpMarker( "Open/Expands all entries in this scope" );
                    ImGui::SameLine();
                    if ( ImGui::Button( " C " ) ) Tree[iDepth].m_OpenAll = -1;
                    HelpMarker( "Closes/Collapses all entries in this scope" );
                }
            }

            if (E.m_Property.m_Path.back() == ']' && bRenderBlankRight == false )
            {
                if (E.m_Flags.m_bShowReadOnly) ImGui::BeginDisabled(true);
                ImGui::Text("Size:");
                if (E.m_Flags.m_bShowReadOnly) ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::PushItemWidth(40*2);
                HandleElement( E, E, 0, false );
                ImGui::PopItemWidth();
            }

            // Requiring bRenderBlankRight here (a prior attempt at this same fix) turned out wrong -
            // it's only true for ONE specific array sub-case (an object array's own "[i]" index-
            // insertion rows), not array size-markers in general ("m_avListB[2d]" etc. never set it),
            // and excluding it broke O/C for every ordinary array (confirmed live). E.m_bScope isn't
            // the right signal either - it's explicitly set true for array size-marker entries too
            // (RefreshAllProperties' multi-dimension counting block), which is the ORIGINAL bug: it
            // doesn't distinguish "array size-marker" from "plain object scope" at all. The one signal
            // that actually does is the underlying Member's own variant kind - list_var/list_props for
            // a genuine array (where "open/close all repeated elements" is meaningful) vs. scope/props
            // for a plain named scope (fixed, non-repeated children - nothing to bulk-toggle).
            const bool bIsArrayEntry = E.m_pUserData
                && ( std::holds_alternative<xproperty::type::members::list_var>(E.m_pUserData->m_Variant)
                  || std::holds_alternative<xproperty::type::members::list_props>(E.m_pUserData->m_Variant) );

            // For an ATOMIC (scalar) array, only show O/C while there's still at least one more
            // dimension level below this one (E.m_MyDimension < E.m_Dimensions) - at the deepest
            // level, an atomic array's children are bare leaf values (a plain number), nothing to
            // open/close. For a non-atomic (object) array, always show it, even at the deepest level -
            // each element there still has its own expandable scope for its sub-properties (var/
            // setValues/CheckValues), confirmed live as the one case that stayed correct throughout.
            // The original condition used "> 1" (strictly MORE than one level remaining), which
            // excluded exactly the "one level remaining" case - confirmed live as a 3D atomic array's
            // [2d] level (myDim=2, Dimensions=3, exactly 1 remaining) missing O/C entirely.
            if( bIsArrayEntry && iDepth>0 && Tree[iDepth].m_isOpen
             && (E.m_MyDimension < E.m_Dimensions || Tree[iDepth].m_isAtomicArray == false) )
            {
                if (E.m_Property.m_Path.back() == ']') ImGui::SameLine();
                if ( ImGui::Button( " O " ) ) Tree[iDepth-1].m_OpenAll = 1;
                HelpMarker( "Open/Expands all entries in the list" );
                ImGui::SameLine();
                if ( ImGui::Button( " C " ) ) Tree[iDepth-1].m_OpenAll = -1;
                HelpMarker( "Closes/Collapses all entries in the list" );
            }
        }
        else if ( E.m_pUserData && std::holds_alternative<xproperty::type::members::function>(E.m_pUserData->m_Variant) )
        {
            // A reflected member function - draw it as a single button spanning the value
            // column, invoke through the same TryCallFunction the rest of xproperty already
            // uses for reflected functions, and notify m_OnChangeEvent the same way a
            // committed value edit would - so undo/log systems already listening don't need
            // a second, action-specific event to subscribe to.
            if ( m_Settings.m_bRenderRightBackground ) DrawBackground( iDepth, GlobalIndex );

            // E.m_Flags.m_bShowReadOnly is already resolved generically for every member, function
            // entries included (RefreshAllProperties' Flags block runs unconditionally, no branch on
            // variant kind) - so member_flags<SHOW_READONLY>/member_dynamic_flags<...> already work
            // here for free, same as any other member. An earlier version of this branch reinvented
            // this via a separate action_state/action_dynamic_state mechanism, duplicating something
            // that already worked; removed in favor of just reading the same flag everything else does.
            if ( E.m_Flags.m_bShowReadOnly ) ImGui::BeginDisabled();

            // ImGui::Button auto-sizes to its label and ignores PushItemWidth (unlike the input
            // widgets every other value type here uses) - explicit -1 width fills the column the
            // same way everything else in this right-hand column already does.
            if ( ImGui::Button(E.m_pName, ImVec2(-1, 0)) )
            {
                const auto Result = std::get<xproperty::type::members::function>(E.m_pUserData->m_Variant).TryCallFunction(*static_cast<char*>(E.m_pInstance));
                assert(Result);

                xproperty::ui::undo::cmd Cmd;
                Cmd.m_Name          = E.m_Property.m_Path;
                Cmd.m_pClassObject  = C.m_Base.second;
                Cmd.m_pPropObject   = C.m_Base.first;
                Cmd.m_bHasChanged   = true;
                m_OnChangeEvent.NotifyAll(*this, Cmd);
            }

            if ( E.m_Flags.m_bShowReadOnly ) ImGui::EndDisabled();
        }
        else
        {
            if ( m_Settings.m_bRenderRightBackground ) DrawBackground( iDepth, GlobalIndex );

            int n = 1;
            if (E.m_GroupGUID != 0)
            {
                ++iE;
                if (E.m_GroupGUID == xproperty::settings::vector2_group::guid_v)
                {
                    n = 2;
                }
                else if (E.m_GroupGUID == xproperty::settings::vector3_group::guid_v)
                {
                    n = 3;
                }
            }

            if ( E.m_Flags.m_bShowReadOnly || Tree[iDepth].m_isReadOnly )
            {
                E.m_Flags.m_bShowReadOnly = true;

                ImGuiStyle* style = &ImGui::GetStyle();
                ImColor     CC    = ImVec4( 0.7f, 0.7f, 1.0f, 0.35f );
                ImVec4      CC2f  = style->Colors[ ImGuiCol_Text ];

                CC2f.x *= 1.1f;
                CC2f.y *= 0.8f;
                CC2f.z *= 0.8f;

                ImGui::PushStyleColor( ImGuiCol_Text, CC2f );

                xproperty::ui::undo::cmd Cmd;
                if (E.m_GroupGUID != 0)
                {
                    for (int i = 0; i < n; ++i)
                    {
                        auto& Entry = *C.m_List[iE + i];
                        xproperty::ui::details::group_render::RenderElement(E, i, Cmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags, *this, Entry);
                    }
                }
                else
                {
                    xproperty::ui::details::onRender<xproperty::settings::member_ui_t>(E.m_RightUIGUID, Cmd, E.m_Property.m_Value, *E.m_pUserData, E.m_Flags);
                    assert(Cmd.m_isChange == false);
                    assert(Cmd.m_isEditing == false);
                }

                ImGui::PopStyleColor();
            }
            else
            {
                for(int i=0; i<n; ++i) [&]( entry& Entry ) // Determine if we are dealing with the same entry we are editing
                {
                    if (std::holds_alternative<ui::undo::cmd>(m_CmdCurrentEdit))
                    {
                        auto& CmdVariant = std::get<ui::undo::cmd>(m_CmdCurrentEdit);

                        // Same data type?
                        if ((Entry.m_Property.m_Value.m_pType && CmdVariant.m_Original.m_pType) && (Entry.m_Property.m_Value.m_pType->m_GUID == CmdVariant.m_Original.m_pType->m_GUID))
                        {
                            auto& UndoCmd = CmdVariant;
                            if ((UndoCmd.m_isEditing || UndoCmd.m_isChange) && std::strcmp(UndoCmd.m_Name.c_str(), Entry.m_Property.m_Path.c_str()) == 0)
                            {
                                if (E.m_GroupGUID)
                                {
                                    xproperty::ui::details::group_render::RenderElement(E, i, UndoCmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags, *this, Entry);
                                }
                                else
                                {
                                    xproperty::ui::details::onRender<xproperty::settings::member_ui_t>(Entry.m_RightUIGUID, UndoCmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags);
                                }

                                if (UndoCmd.m_isChange)
                                {
                                    UndoCmd.m_Name         = Entry.m_Property.m_Path;
                                    UndoCmd.m_pClassObject = C.m_Base.second;
                                    UndoCmd.m_pPropObject  = C.m_Base.first;
                                    m_OnRealtimeChangeEvent.NotifyAll(*this, UndoCmd, *m_pContext);

                                    UndoCmd.m_bHasChanged = true;
                                }

                                if (UndoCmd.m_isEditing == false)
                                {
                                    if (UndoCmd.m_bHasChanged)
                                    {
                                        m_OnChangeEvent.NotifyAll(*this, UndoCmd);
                                        m_CmdCurrentEdit = nullptr;
                                    }
                                }
                                return;
                            }
                        }

                    }

                    HandleElement(Entry, E, i, true);

                }( *C.m_List[iE + i] );
            }

            // Handle group entry increments
            iE += n - 1;
        }

        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    //
    // Pop any scope
    //
    while( iDepth >= 0 ) 
        PopTree();
}

//-------------------------------------------------------------------------------------------------

void xproperty::inspector::Show( void ) noexcept
{
    // Anything to render?
    if( m_lEntities.size() == 0 ) 
        return;

#ifdef XCORE_PROPERTIES_H
    xproperty::ui::details::g_pInspector = this;
#endif

    //
    // get the actual values
    //
    for (auto& E : m_lEntities)
    {
        for (auto& C : E->m_lComponents)
        {
            // Let the user change the base pointer if needed...
            void* pBackup = C->m_Base.second;
            m_OnGetComponentPointer.NotifyAll(*this, static_cast<int>(&C - E->m_lComponents.data()), C->m_Base.second, C->m_pUserData);

            // Refresh the actual properties for the given component
            RefreshAllProperties(*C);

            // Restore the original base pointer
            C->m_Base.second = pBackup;
        }
    }



    //
    // Render the components
    //
    for ( auto& E : m_lEntities )
    {
        int GlobalIndex = 0;
        for ( auto& C : E->m_lComponents )
        {
            ImGui::PushID(&C);

            // Tell ImGui we are going to use 2 columns
            ImGui::Columns( 2 );

            // Let the user change the base pointer if needed...
            void* pBackup = C->m_Base.second;
            m_OnGetComponentPointer.NotifyAll(*this, static_cast<int>(&C - E->m_lComponents.data()), C->m_Base.second, C->m_pUserData);

            // Render the actual component
            Render( *C, GlobalIndex );

            // Restore the original base pointer
            C->m_Base.second = pBackup;

            // Reset back to a single column
            ImGui::Columns( 1 );
            ImGui::PopID();
        }
    }
}

//-------------------------------------------------------------------------------------------------

void xproperty::inspector::Show(xproperty::settings::context& Context, std::function<void(void)> Callback) noexcept
{
    if (m_bWindowOpen == false) return;

    m_pContext = &Context;

    //
    // Key styles 
    //
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_Settings.m_WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, m_Settings.m_FramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, m_Settings.m_ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, m_Settings.m_IndentSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    //
    // Open the window
    //
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(m_Width), static_cast<float>(m_Height)), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(m_pName, &m_bWindowOpen))
    {
        ImGui::PopStyleVar(6);
        ImGui::End();
        return;
    }

    //
    // Let the user inject something at the top of the window
    //
    Callback();

    //
    // Display the properties
    //
    ImGui::Columns(2);
    ImGui::Separator();

    Show();

    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::PopStyleVar(6);
    ImGui::End();
}

//-------------------------------------------------------------------------------------------------

void xproperty::inspector::DrawBackground( int Depth, int GlobalIndex ) const noexcept
{
    if( m_Settings.m_bRenderBackgroundDepth == false ) 
        Depth = 0;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    auto Color = s_ColorCategories[Depth];

    float h, s, v;
    ImVec4 C = Color;
    ImGui::ColorConvertRGBtoHSV( C.x, C.y, C.z, h, s, v );

    if(GlobalIndex&1)
    {
        Color.SetHSV( h, s*m_Settings.m_ColorSScalar, v*m_Settings.m_ColorVScalar1 );
    }
    else
    {
        Color.SetHSV( h, s*m_Settings.m_ColorSScalar, v*m_Settings.m_ColorVScalar2 );
    }


    ImGui::GetWindowDrawList()->AddRectFilled(
        pos
        , ImVec2( pos.x + ImGui::GetContentRegionAvail().x
                , pos.y + ImGui::GetFrameHeight() )
        , Color );
}

//-----------------------------------------------------------------------------------

void xproperty::inspector::HelpMarker( const char* desc ) const noexcept
{
    if ( ImGui::IsItemHovered() )
    {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, m_Settings.m_HelpWindowPadding );
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos( ImGui::GetFontSize() * m_Settings.m_HelpWindowSizeInChars );
        ImGui::TextUnformatted( desc );
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
        ImGui::PopStyleVar();
    }
}

//-----------------------------------------------------------------------------------

void xproperty::inspector::Help( const entry& Entry ) const noexcept
{
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, m_Settings.m_HelpWindowPadding );
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos( ImGui::GetFontSize() * m_Settings.m_HelpWindowSizeInChars );

    ImGui::TextDisabled("Name:     ");
    ImGui::SameLine();
    ImGui::Text("%s", Entry.m_pName);

    ImGui::TextDisabled("Type:     ");
    ImGui::SameLine();
    ImGui::Text("%s", Entry.m_Property.m_Value.m_pType ? Entry.m_Property.m_Value.m_pType->m_pName : "<<Unkown>>");

    ImGui::TextDisabled( "FullName: ");
    ImGui::SameLine();
    ImGui::Text( "%s", Entry.m_Property.m_Path.c_str() );

    ImGui::TextDisabled( "GUID:     " );
    ImGui::SameLine();
    ImGui::Text( "0x%x", Entry.m_GUID );

    ImGui::TextDisabled("Help");
    ImGui::Separator();

    if( Entry.m_pHelp )
    {
        ImGui::TextUnformatted( Entry.m_pHelp );
    }
    else
    {
        ImGui::SameLine();
        ImGui::Text( "none provided" );
    }

    ImGui::EndTooltip();
    ImGui::PopStyleVar();
}
