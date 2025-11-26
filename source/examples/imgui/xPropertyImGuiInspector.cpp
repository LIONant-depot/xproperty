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
            if( Cmd.m_isChange )
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

        bool bOpen = false;
        if (Flags.m_bShowReadOnly) ImGui::BeginDisabled();
        {
            g_pInspector->m_OnResourceWigzmos.NotifyAll(*g_pInspector, bOpen, Value);
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
            g_pInspector->m_OnResourceBrowser.NotifyAll(*g_pInspector, (void*)static_cast<std::uint64_t>(GUID), bOpen, FullGuid, I.m_FilerTypes);

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
        using generic = void(int, xproperty::ui::undo::cmd&, const std::uint64_t& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept;

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

        auto a = xproperty::settings::var_type<float>::guid_v;

        // Check to make sure that the user did not made a mistake...
        // the actual type of the property must match the type of the UI style..
        // NOTE: I had to comment this assert because visual studio 17.11.1 seems to be failing... which is non-sense.... 
      //  assert(Value.m_pType->m_GUID == StyleBase.m_TypeGUID );

        // Sanity check make sure that we have a function as well... 
        assert(StyleBase.m_pDrawFn);

        //ImGui::PushID(&Entry);
        reinterpret_cast<generic*>(StyleBase.m_pDrawFn)(GUID, Cmd, *reinterpret_cast<const std::uint64_t*>(&Value), StyleBase, Flags);
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
                auto i = std::strlen(pPropertyName);
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
                                i -= 3;
                                if (i >= 0)
                                {
                                    for (; i >= 0 && pPropertyName[i] == ']'; --i)
                                    {
                                        myDimension++;
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
            if (Path[L.m_iEnd - 1] == ']')
            {
                assert(Path[L.m_iEnd - 2] == '[');
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
            if (bCustomDraw) m_OnResourceLeftSize.NotifyAll( *this, 0, flags, pTreeName, Open );
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

        //
        // Render the left column
        //
        ++GlobalIndex;
        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
        ImVec2 lpos = ImGui::GetCursorScreenPos();
        auto CRA = ImGui::GetContentRegionAvail();
        if( Tree[iDepth].m_iArray >= 0 ) E.m_LeftUIGUID = E.m_GUID + Tree[iDepth].m_iArray + iDepth * 1000 + Tree[iDepth].m_MyDimension * 1000000;
        else                             E.m_LeftUIGUID = E.m_GUID + iDepth * 1000;
        ImGui::PushID(E.m_LeftUIGUID);
        E.m_RightUIGUID = static_cast<int>(std::hash<size_t>{}(static_cast<size_t>(E.m_LeftUIGUID)) % std::numeric_limits<int>::max());

        bool bRenderBlankRight = false;

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
                if (bCustomRender) m_OnResourceLeftSize.NotifyAll(*this, reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID + Tree[iDepth].m_iArray)), flags, Name.data(), Open);
                else               ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID + Tree[iDepth].m_iArray)), flags, "%s", Name.data());
            }
            else
            {
                PushTree(Name.data(), bCustomRender, E.m_Property.m_Path, E.m_MyDimension, Tree[iDepth].m_isDefaultOpen, Tree[iDepth].m_isReadOnly, Tree[iDepth].m_isHidden);

                bRenderBlankRight = true;

                iE--;
            }
        }
        else if (E.m_Property.m_Path.back() == ']')
        {
            if (*(E.m_Property.m_Path.end() - 2) == '[' )
            {
                if (E.m_MyDimension == 1)
                {
                    std::array<char, 128> Name;
                    snprintf(Name.data(), Name.size(), "%s[%dd] ", E.m_pName, E.m_MyDimension);
                    PushTree(Name.data(), bCustomRender, E.m_Property.m_Path, E.m_MyDimension, E.m_bDefaultOpen, E.m_Flags.m_bShowReadOnly, false, true, E.m_bAtomicArray);

                    if (Tree[iDepth].m_isAtomicArray == false)
                    {
                        Tree[iDepth].m_bArrayMustInsertIndex = true;
                    }
                }
                else
                {
                    // multi dimensional arrays
                    std::array<char, 128> Name;
                    int stroffset = 0;
                    stroffset += snprintf(&Name[stroffset], Name.size() - stroffset, "%s", E.m_pName);
                    for (int i = E.m_MyDimension - 1; i >= 0; --i)
                    {
                        auto Index = Tree[iDepth - i].m_iArray;
                        stroffset += snprintf(&Name[stroffset], Name.size() - stroffset, "[%d]", Index);
                    }

                    stroffset += snprintf(&Name.data()[stroffset], Name.size() - stroffset, "[%dd]", E.m_Dimensions - E.m_MyDimension);
                    PushTree(Name.data(), bCustomRender, E.m_Property.m_Path, E.m_MyDimension, E.m_bDefaultOpen, E.m_Flags.m_bShowReadOnly, false, true, E.m_bAtomicArray);
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
                    if (bCustomRender) m_OnResourceLeftSize.NotifyAll(*this, reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID + Tree[iDepth].m_iArray)), flags, Name.data(), Open);
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
            PushTree(E.m_pName, bCustomRender, E.m_Property.m_Path, E.m_MyDimension, E.m_bDefaultOpen, E.m_Flags.m_bShowReadOnly, E.m_Flags.m_bDontShow);
        }
        else
        {
            bool Open;
            const auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (bCustomRender) m_OnResourceLeftSize.NotifyAll(*this, reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID)), flags, E.m_pName, Open);
            else               ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID)), flags, "%s", E.m_pName);

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

            if( iDepth>0 && ((E.m_Dimensions - E.m_MyDimension) > 1 || Tree[iDepth].m_isAtomicArray == false) && Tree[iDepth].m_isOpen )
            {
                if (E.m_Property.m_Path.back() == ']') ImGui::SameLine();
                if ( ImGui::Button( " O " ) ) Tree[iDepth-1].m_OpenAll = 1;
                HelpMarker( "Open/Expands all entries in the list" );
                ImGui::SameLine();
                if ( ImGui::Button( " C " ) ) Tree[iDepth-1].m_OpenAll = -1;
                HelpMarker( "Closes/Collapses all entries in the list" );
            }
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
