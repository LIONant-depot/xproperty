
#include "xPropertyImGuiInspector.h"
#include "..\..\sprop\property_sprop_getset.h"
#include "..\..\sprop\property_sprop_collector.h"
#include <windows.h>
#include <algorithm>
#include "calculator.cpp"


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
                Cmd.m_NewValue.set<T>(V);
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
                    Cmd.m_NewValue.set<T>(std::min<T>(I.m_Max, std::max<T>(I.m_Min, static_cast<T>(Result))));
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

    template<> void draw<std::int64_t, style::scroll_bar> ::Render( undo::cmd& Cmd, const std::int64_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_S64>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int64_t, style::drag_bar>   ::Render( undo::cmd& Cmd, const std::int64_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_S64>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int64_t, style::edit_box>   ::Render( undo::cmd& Cmd, const std::int64_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_S64>( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint64_t, style::scroll_bar>::Render( undo::cmd& Cmd, const std::uint64_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_U64>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint64_t, style::drag_bar>  ::Render( undo::cmd& Cmd, const std::uint64_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_U64>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint64_t, style::edit_box>  ::Render( undo::cmd& Cmd, const std::uint64_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_U64>( Cmd, Value, I, Flags ); }

    template<> void draw<std::int32_t, style::scroll_bar> ::Render( undo::cmd& Cmd, const std::int32_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_S32>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int32_t, style::drag_bar>   ::Render( undo::cmd& Cmd, const std::int32_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_S32>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int32_t, style::edit_box>   ::Render( undo::cmd& Cmd, const std::int32_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_S32>( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint32_t, style::scroll_bar>::Render( undo::cmd& Cmd, const std::uint32_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_U32>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint32_t, style::drag_bar>  ::Render( undo::cmd& Cmd, const std::uint32_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_U32>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint32_t, style::edit_box>  ::Render( undo::cmd& Cmd, const std::uint32_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_U32>( Cmd, Value, I, Flags ); }

    template<> void draw<std::int16_t, style::scroll_bar> ::Render( undo::cmd& Cmd, const std::int16_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_S16>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int16_t, style::drag_bar>   ::Render( undo::cmd& Cmd, const std::int16_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_S16>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int16_t, style::edit_box>   ::Render( undo::cmd& Cmd, const std::int16_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_S16>( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint16_t, style::scroll_bar>::Render( undo::cmd& Cmd, const std::uint16_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_U16>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint16_t, style::drag_bar>  ::Render( undo::cmd& Cmd, const std::uint16_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_U16>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint16_t, style::edit_box>  ::Render( undo::cmd& Cmd, const std::uint16_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_U16>( Cmd, Value, I, Flags ); }

    template<> void draw<std::int8_t, style::scroll_bar> ::Render( undo::cmd& Cmd, const std::int8_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_S8>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int8_t, style::drag_bar>   ::Render( undo::cmd& Cmd, const std::int8_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_S8>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::int8_t, style::edit_box>   ::Render( undo::cmd& Cmd, const std::int8_t&  Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_S8>( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint8_t, style::scroll_bar>::Render( undo::cmd& Cmd, const std::uint8_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { DragRenderNumbers<ImGuiDataType_U8>   ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint8_t, style::drag_bar>  ::Render( undo::cmd& Cmd, const std::uint8_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { SlideRenderNumbers<ImGuiDataType_U8>  ( Cmd, Value, I, Flags ); }
    template<> void draw<std::uint8_t, style::edit_box>  ::Render( undo::cmd& Cmd, const std::uint8_t& Value, const member_ui_base& I, xproperty::flags::type Flags ) noexcept { EditBoxRenderNumbers<ImGuiDataType_U8>( Cmd, Value, I, Flags ); }

    template<> void draw<float, style::scroll_bar>::Render(undo::cmd& Cmd, const float& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { DragRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }
    template<> void draw<float, style::drag_bar>  ::Render(undo::cmd& Cmd, const float& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { SlideRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }
    template<> void draw<float, style::edit_box>  ::Render(undo::cmd& Cmd, const float& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { EditBoxRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }

    template<> void draw<double, style::scroll_bar>::Render(undo::cmd& Cmd, const double& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { DragRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }
    template<> void draw<double, style::drag_bar>  ::Render(undo::cmd& Cmd, const double& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { SlideRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }
    template<> void draw<double, style::edit_box>  ::Render(undo::cmd& Cmd, const double& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept { EditBoxRenderNumbers<ImGuiDataType_Float>(Cmd, Value, I, Flags); }


    //-----------------------------------------------------------------------------------
    // OTHERS!!!
    //-----------------------------------------------------------------------------------

    template<>
    void draw<bool, style::defaulted>::Render(undo::cmd& Cmd, const bool& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
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
    std::array<char, 16 * 1024>   g_ScrachCharBuffer;

    template<>
    void draw<std::string, style::defaulted>::Render(undo::cmd& Cmd, const std::string& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
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

    //-----------------------------------------------------------------------------------
    
    template<>
    void draw<std::string, style::file_dialog>::Render(undo::cmd& Cmd, const std::string& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
    {
        auto& I = reinterpret_cast<const xproperty::member_ui<std::string>::data&>(IB);

        ImVec2 charSize = ImGui::CalcTextSize("A");
        float ButtonWidth = charSize.x * 3;
        float f = (ImGui::GetColumnWidth() - ButtonWidth) / charSize.x;
        float f2 = Value.length() - f;

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
            Value.copy(g_ScrachCharBuffer.data(), Value.length());
            g_ScrachCharBuffer[Value.length()] = 0;
            ImGui::BeginGroup();

            const auto CurPos = ImGui::GetCursorPosX();
            const bool WentOver = f2 > -1 && Cmd.m_isEditing == false;
            if (WentOver) ImGui::SetCursorPosX(CurPos - (f2 + 1) * charSize.x);

            if (Cmd.m_isEditing == false) ImGui::PushItemWidth(ItemWidth);
            Cmd.m_isChange = ImGui::InputText("##value", g_ScrachCharBuffer.data(), g_ScrachCharBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            if (Cmd.m_isEditing == false) ImGui::PopItemWidth();

            if (ImGui::IsItemActivated())
            {
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<std::string>(Value);
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
                // Set the current path as the dafaulted path
                std::string CurrentPath;
                {
                    std::array< wchar_t, MAX_PATH > WCurrentPath;
                    GetCurrentDirectory(static_cast<DWORD>(WCurrentPath.size()), WCurrentPath.data());
                    std::transform(WCurrentPath.begin(), WCurrentPath.end(), std::back_inserter(CurrentPath), [](wchar_t c) {return (char)c; });
                }

                OPENFILENAMEA ofn;
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize     = sizeof(ofn);
                ofn.hwndOwner       = GetActiveWindow();
                ofn.lpstrFile       = g_ScrachCharBuffer.data();
                ofn.lpstrFile[0]    = '\0';
                ofn.nMaxFile        = static_cast<std::uint32_t>(g_ScrachCharBuffer.size());
                ofn.lpstrFilter     = I.m_pFilter;
                ofn.nFilterIndex    = 1;
                ofn.lpstrFileTitle  = nullptr;
                ofn.nMaxFileTitle   = 0;
                ofn.lpstrInitialDir = CurrentPath.c_str();
                ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                if (GetOpenFileNameA(&ofn) == TRUE)
                {
                    Cmd.m_isChange = true;

                    if ( I.m_bMakePathRelative )
                    {
                        int nPops = 1;

                        // Count the paths for the current path
                        for (const char* p = CurrentPath.c_str(); *p; p++)
                        {
                            if (*p == '\\' || *p == '/') nPops++;
                        }

                        // Add whatever the user requested
                        nPops -= I.m_RelativeCurrentPathMinusCount;

                        // Find our relative path and set the new string
                        for (const char* p = ofn.lpstrFile; *p; p++)
                        {
                            if (*p == '\\' || *p == '/') nPops--;
                            if (nPops <= 0)
                            {
                                ++p;
                                for ( int i=0; g_ScrachCharBuffer[i] = *p; ++i, ++p ){}
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
                if (Cmd.m_isEditing == false) Cmd.m_Original.set<std::string>(Value);
                Cmd.m_isEditing = true;
                Cmd.m_NewValue.set<std::string>(g_ScrachCharBuffer.data());

                // Have we really changed anything?
                if (Cmd.m_Original.get<std::string>() == Cmd.m_NewValue.get<std::string>())
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

            ImGui::TextUnformatted(Value.c_str());

            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }
    }

    //-----------------------------------------------------------------------------------

    template<>
    void draw<std::string, style::button>::Render(undo::cmd& Cmd, const std::string& Value, const member_ui_base& IB, xproperty::flags::type Flags) noexcept
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
    static void onRender( xproperty::ui::undo::cmd& Cmd, const xproperty::any& Value, const xproperty::type::members& Entry, xproperty::flags::type Flags) noexcept
    {
        using generic = void(xproperty::ui::undo::cmd&, const std::uint64_t& Value, const member_ui_base& I, xproperty::flags::type Flags) noexcept;

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
        reinterpret_cast<generic*>(StyleBase.m_pDrawFn)(Cmd, *reinterpret_cast<const std::uint64_t*>(&Value), StyleBase, Flags);
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
            if (GroupEntry.m_GroupGUID == xproperty::settings::vector2_group::guid_v)
            {
                auto& I = member_ui<float>::defaults::data_v;
                ImGuiStyle* style = &ImGui::GetStyle();
                const auto   Width = (ImGui::GetContentRegionAvail().x - style->ItemInnerSpacing.x - 14*2)  / 2;
                const auto   Height = ImGui::GetFrameHeight();
                ImVec2       pos;
                ImU32        Color;

                if (iElement == 0)
                {
                    ImGui::PushItemWidth(Width);
                    Color = ImU32(0x440000ff); 
                    ImGui::Text("%c:", Entry.m_pName[0]);
                    ImGui::SameLine();
                    pos = ImGui::GetCursorScreenPos();
                    if (ImGui::IsItemHovered()) Inspector.Help(IEntry);
                }

                if (iElement == 1) 
                {
                    ImGui::SameLine(0, 2);

                    Color = ImU32(0x4400ff00);

                    ImGui::Text("%c:", Entry.m_pName[0]);
                    ImGui::SameLine();
                    pos = ImGui::GetCursorScreenPos();
                    if (ImGui::IsItemHovered()) Inspector.Help(IEntry);
                }

                ImGui::PushID(Entry.m_GUID);
                onRender<xproperty::settings::member_ui_t>(Cmd, Value, Entry, Flags);
                ImGui::PopID();

                if(iElement == 1 ) ImGui::PopItemWidth();
                ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + Width, pos.y + Height), Color);
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
    m_UndoSystem.clear();
}

//-------------------------------------------------------------------------------------------------
void xproperty::inspector::AppendEntity(void) noexcept
{
    m_lEntities.push_back( std::make_unique<entity>() );
}

//-------------------------------------------------------------------------------------------------
void xproperty::inspector::AppendEntityComponent(const xproperty::type::object& Object, void* pBase) noexcept
{
    auto Component = std::make_unique<component>();

    // Cache the information
    Component->m_Base = { &Object, pBase };

    m_lEntities.back()->m_lComponents.push_back(std::move(Component));

}

//-------------------------------------------------------------------------------------------------

void xproperty::inspector::Undo(void) noexcept
{
    if (m_UndoSystem.m_Index == 0 || m_UndoSystem.m_lCmds.size() == 0)
        return;

    if(m_UndoSystem.m_Index == (m_UndoSystem.m_lCmds.size()-1) )
    {
        if( m_UndoSystem.m_lCmds.back().m_bHasChanged == false )
        {
            m_UndoSystem.m_lCmds.pop_back();
            Undo();
            return;
        }
    }
    auto&               Value = m_UndoSystem.m_lCmds[--m_UndoSystem.m_Index];
    std::string         Error;
    xproperty::sprop::io_property<true>(Error, Value.m_pClassObject, *Value.m_pPropObject, xproperty::sprop::container::prop{ Value.m_Name, Value.m_Original }, m_Context);

    if (Error.empty() == false)
    {
        // Print error...
    }
}

//-------------------------------------------------------------------------------------------------

void xproperty::inspector::Redo(void) noexcept
{
    if (m_UndoSystem.m_Index == static_cast<int>(m_UndoSystem.m_lCmds.size()))
        return;

    auto&               Value = m_UndoSystem.m_lCmds[m_UndoSystem.m_Index++];
    std::string         Error;
    xproperty::sprop::io_property<true>(Error, Value.m_pClassObject, *Value.m_pPropObject, xproperty::sprop::container::prop{ Value.m_Name, Value.m_NewValue }, m_Context);

    if (Error.empty() == false)
    {
        // Print error...
    }
}

//-------------------------------------------------------------------------------------------------
    
void xproperty::inspector::RefreshAllProperties( void ) noexcept
{
    for ( auto& E : m_lEntities )
    {
        for ( auto& C : E->m_lComponents )
        {
            C->m_List.clear();
            int                    iDimensions = -1;
            int                    myDimension = -1;
            xproperty::sprop::collector( C->m_Base.second, *C->m_Base.first, m_Context, [&](const char* pPropertyName, xproperty::any&& Value, const xproperty::type::members& Member, bool isConst, const void* pInstance )
            {
                std::uint32_t          GUID        = Member.m_GUID;
                std::uint32_t          GroupGUID   = 0;
                
                // Handle the flags
                xproperty::flags::type Flags = [&]
                {
                    if(auto* pDynamicFlags = Member.getUserData<xproperty::settings::member_dynamic_flags_t>(); pDynamicFlags)
                    {
                        return pDynamicFlags->m_pCallback(pInstance, m_Context);
                    }
                    else if (auto* pStaticFlags = Member.getUserData<xproperty::settings::member_flags_t>(); pStaticFlags )
                    {
                        return pStaticFlags->m_Flags;
                    }
                    else
                    {
                        return xproperty::flags::type{.m_Value = 0};
                    }
                }();

                Flags.m_bShowReadOnly |= isConst;
                bool bScope            =    std::holds_alternative<xproperty::type::members::scope>(Member.m_Variant)
                                         || std::holds_alternative<xproperty::type::members::props>(Member.m_Variant);

                const bool bAtomicArray = std::holds_alternative<xproperty::type::members::list_var>(Member.m_Variant);

                const bool bDefaultOpen = [&]
                {
                    if (auto* pDefaultOpen = Member.getUserData<xproperty::settings::member_ui_open_t>(); pDefaultOpen) return pDefaultOpen->m_bOpen;
                    return !(bAtomicArray || std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant));
                }();

                if(bScope || std::holds_alternative<xproperty::type::members::var>(Member.m_Variant) )
                {
                    iDimensions = -1;
                    myDimension = -1;
                }

                if( std::holds_alternative<xproperty::type::members::props>(Member.m_Variant) 
                 || std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant) )
                {
                    // GUIDs for groups are marked as u32... vs sizes are mark as u64
                    if( Value.m_pType->m_GUID == xproperty::settings::var_type<std::uint32_t>::guid_v )
                    {
                        GroupGUID = Value.get<std::uint32_t>();
                    }
                }

                // Check if we are dealing with atomic types and the size field...
                if ( std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant)
                  || std::holds_alternative<xproperty::type::members::list_var>(Member.m_Variant) )
                {
                    auto i = std::strlen(pPropertyName);
                    if( (pPropertyName[i-1] == ']') && (pPropertyName[i - 2] == '[') )
                    {
                        bScope = true;

                        std::visit([&]( auto& List ) constexpr
                        {
                            if constexpr (std::is_same_v<decltype(List), const xproperty::type::members::list_props&> ||
                                          std::is_same_v<decltype(List), const xproperty::type::members::list_var&>  )
                            {
                                myDimension = 0;
                                iDimensions = static_cast<int>(List.m_Table.size());
                                for (i -= 3; pPropertyName[i] == ']'; --i)
                                {
                                    myDimension++;

                                    // Find the matching closing bracket...
                                    while (pPropertyName[--i] != '[');
                                }
                            }
                            else
                            {
                                assert(false);
                            }

                        }, Member.m_Variant );

                        // We don't deal with zero size arrays...
                        if (0 == Value.get<std::size_t>())
                            return;
                    }
                    else
                    {
                        
                    }
                }

                auto* pHelp = Member.getUserData<xproperty::settings::member_help_t>();

                C->m_List.push_back
                ( std::make_unique<entry>
                    ( xproperty::sprop::container::prop{ pPropertyName, std::move(Value) }
                    , pHelp ? pHelp->m_pHelp : "<<No help>>"
                    , Member.m_pName
                    , Member.m_GUID
                    , GroupGUID
                    , & Member //bScope ? nullptr : &Member
                    , iDimensions
                    , myDimension
                    , Flags
                    , bScope
                    , bAtomicArray
                    , bDefaultOpen
                    ) 
                );
            }, true );
        }
    }

    int a = 33;
}

//-------------------------------------------------------------------------------------------------

void xproperty::inspector::Show( std::function<void(void)> Callback ) noexcept
{
    if( m_bWindowOpen == false ) return;

    //
    // Key styles 
    //
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding,   m_Settings.m_WindowPadding );
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding,    m_Settings.m_FramePadding );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing,     m_Settings.m_ItemSpacing );
    ImGui::PushStyleVar( ImGuiStyleVar_IndentSpacing,   m_Settings.m_IndentSpacing );
    ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, 0 );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0 );

    //
    // Open the window
    //
    ImGui::SetNextWindowSize( ImVec2( static_cast<float>(m_Width), static_cast<float>(m_Height) ), ImGuiCond_FirstUseEver );
    if ( !ImGui::Begin( m_pName, &m_bWindowOpen ) )
    {
        ImGui::PopStyleVar( 5 );
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
    ImGui::Columns( 2 );
    ImGui::Separator();

    Show();

    ImGui::Columns( 1 );
    ImGui::Separator();
    ImGui::PopStyleVar( 6 );
    ImGui::End();
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

        return Open;
    };

    const auto PushTree = [&]( const char* pTreeName, std::string_view Path, int myDimension, bool bDefaultOpen, bool isReadOnly, bool isHidden, bool bArray = false, bool bAtomic = false )
    {
        bool Open = iDepth<0? true : Tree[ iDepth ].m_isOpen;
        if( Open )
        {
            if ( iDepth >0 && Tree[iDepth-1].m_OpenAll ) ImGui::SetNextItemOpen( Tree[iDepth-1].m_OpenAll > 0 );
            Open = ImGui::TreeNodeEx(pTreeName, (bDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0) | ((iDepth == -1) ? ImGuiTreeNodeFlags_Framed : 0));
        }

        PushTreeStruct(Open, Path, myDimension, bDefaultOpen, isReadOnly, isHidden, bArray, bAtomic);

        return Open;
    };

    auto PopTree = [ & ]()
    {
        // Handle muti-dimensional array increment of entries
        if (Tree[iDepth].m_MyDimension > 0 && iDepth > 1 ) Tree[iDepth - 1].m_iArray++;

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
        PushTree(C.m_Base.first->m_pName, C.m_Base.first->m_pName, -1, true, false, false);

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
    bool bArrayMustInsertIndex = false;
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
             && E.m_Dimensions > 1 
             && Tree[iDepth].m_iArray >= 0 
             && Tree[iDepth].m_MyDimension >= E.m_MyDimension
             ) return false;

            return ComputeCRC(E.m_Property.m_Path, T.m_iEnd) == Tree[iDepth].m_CRC;
        };

        while (CheckSameLevel() == false)
        {
            PopTree();
        }

        // A scope is hidden...
        if (E.m_Flags.m_bDontShow && E.m_bScope)
        {
            // Push a temp tree node
            PushTreeStruct( false, E.m_Property.m_Path, 0, false, false, true );

            // Start skipping the entries
            bArrayMustInsertIndex = false;
            continue;
        }

        if( Tree[iDepth].m_isOpen == false )
        {
            bArrayMustInsertIndex = false;
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
        if( Tree[iDepth].m_iArray >= 0 ) ImGui::PushID( E.m_GUID + Tree[iDepth].m_iArray + iDepth * 1000 + Tree[iDepth].m_MyDimension * 1000000 );
        else                             ImGui::PushID( E.m_GUID + iDepth * 1000 );
        if ( m_Settings.m_bRenderLeftBackground ) DrawBackground( iDepth, GlobalIndex );

        bool bRenderBlankRight = false;

        // Handle property groups
        if (E.m_GroupGUID != 0)
        {
            if( E.m_GroupGUID == xproperty::settings::vector2_group::guid_v )
            {
                // This guy is not longer a scope...
                E.m_bScope = false;
            }
        }

        // Create a new tree
        if( ((Tree[iDepth].m_isAtomicArray && Tree[iDepth].m_iArray >= 0) 
            || Tree[iDepth].m_iArray == -1 
            || (E.m_Dimensions > 1) ) // We can only start new scopes in cases where we are not 
            && E.m_bScope                       // We are handling some scope
            && bArrayMustInsertIndex == false   // If we are adding Sub Indices then we should not enter here
            )
        {
            // Is an array?
            if( E.m_Property.m_Path.back() == ']' )
            {
                if( *(E.m_Property.m_Path.end()-2) == '[' && E.m_Dimensions == 1 )//Tree[iDepth].m_iArray < 0 )
                {
                    std::array<char, 128> Name;
                    snprintf(Name.data(), Name.size(), "%s[%dd] ", E.m_pName, E.m_Dimensions );
                    PushTree(Name.data(), E.m_Property.m_Path, E.m_MyDimension, E.m_bDefaultOpen, E.m_Flags.m_bShowReadOnly, false, true, E.m_bAtomicArray );

                    if (Tree[iDepth].m_isAtomicArray == false)
                        bArrayMustInsertIndex = true;
                }
                else
                {
                    // multi dimensional arrays
                    std::array<char, 128> Name;
                    int stroffset = 0;
                    stroffset += snprintf(&Name[stroffset], Name.size() - stroffset, "%s", E.m_pName);
                    for( int i= E.m_MyDimension-1; i >= 0; --i )
                    {
                        auto Index = Tree[iDepth - i].m_iArray;
                        stroffset += snprintf( &Name[stroffset], Name.size()- stroffset, "[%d]", Index );
                    }

                    stroffset += snprintf(&Name.data()[stroffset], Name.size() - stroffset, "[%dd]", E.m_Dimensions - E.m_MyDimension);
                    PushTree(Name.data(), E.m_Property.m_Path, E.m_MyDimension, E.m_bDefaultOpen, E.m_Flags.m_bShowReadOnly, false, true, E.m_bAtomicArray);
                }
            }
            else
            {
                PushTree( E.m_pName, E.m_Property.m_Path, E.m_MyDimension, E.m_bDefaultOpen, E.m_Flags.m_bShowReadOnly, E.m_Flags.m_bDontShow );
            }
        }
        else
        {
            // if it is an array...
            if( Tree[iDepth].m_iArray >= 0 )
            {
                std::array<char, 128> Name;
                snprintf( Name.data(), Name.size(), "[%d]", Tree[iDepth].m_iArray++ );

                // Atomic array
                if ( Tree[iDepth].m_isAtomicArray || E.m_GroupGUID )
                {
                    ImGui::TreeNodeEx( reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID + Tree[iDepth].m_iArray)), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s", Name.data() );
                }
                else
                {
                    PushTree( Name.data(), E.m_Property.m_Path, E.m_MyDimension, Tree[iDepth].m_isDefaultOpen, E.m_Flags.m_bShowReadOnly, E.m_Flags.m_bDontShow);

                    bRenderBlankRight = true;

                    // We need to redo this entry
                    iE--;

                    bArrayMustInsertIndex = false;
                }
            }
            else
            {
                ImGui::TreeNodeEx( reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID)), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s", E.m_pName );
            }
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
                if(bElementTag) xproperty::ui::details::onRender<xproperty::settings::member_ui_t>          (Cmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags);
                else            xproperty::ui::details::onRender<xproperty::settings::member_ui_list_size_t>(Cmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags);
            }

            if (Cmd.m_isEditing || Cmd.m_isChange)
            {
                assert(m_UndoSystem.m_Index <= static_cast<int>(m_UndoSystem.m_lCmds.size()));

                // Set the property value
                if (Cmd.m_isChange)
                {
                    std::string Error;
                    xproperty::sprop::setProperty(Error, C.m_Base.second, *C.m_Base.first, { Entry.m_Property.m_Path, Cmd.m_NewValue }, m_Context);
                    assert(Error.empty());
                    Cmd.m_bHasChanged = true;
                }


                // Make sure we reset the undo buffer to current entry
                if (m_UndoSystem.m_Index < static_cast<int>(m_UndoSystem.m_lCmds.size()))
                    m_UndoSystem.m_lCmds.erase(m_UndoSystem.m_lCmds.begin() + m_UndoSystem.m_Index, m_UndoSystem.m_lCmds.end());

                // Make sure we don't have more entries than we should
                while (m_UndoSystem.m_Index >= m_UndoSystem.m_MaxSteps)
                {
                    m_UndoSystem.m_lCmds.erase(m_UndoSystem.m_lCmds.begin());
                    m_UndoSystem.m_Index--;
                }

                // Insert the cmd into the list
                Cmd.m_Name.assign(Entry.m_Property.m_Path);
                Cmd.m_pPropObject = C.m_Base.first;
                Cmd.m_pClassObject = C.m_Base.second;

                m_UndoSystem.m_lCmds.push_back(std::move(Cmd));
            }
        };

        if( E.m_bScope || bRenderBlankRight )
        {
            if ( m_Settings.m_bRenderRightBackground ) DrawBackground( iDepth-1, GlobalIndex );

            if (E.m_Property.m_Path.back() == ']' && bRenderBlankRight == false )
            {
                ImGui::Text("Size:");
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
                    xproperty::ui::details::onRender<xproperty::settings::member_ui_t>(Cmd, E.m_Property.m_Value, *E.m_pUserData, E.m_Flags);
                    assert(Cmd.m_isChange == false);
                    assert(Cmd.m_isEditing == false);
                }

                ImGui::PopStyleColor();
            }
            else
            {
                for(int i=0; i<n; ++i) [&]( entry& Entry ) // Determine if we are dealing with the same entry we are editing
                {
                    // Check if we are editing an entry already
                    if( m_UndoSystem.m_lCmds.empty() == false && m_UndoSystem.m_Index < static_cast<int>(m_UndoSystem.m_lCmds.size()))
                    {
                        auto& CmdVariant = m_UndoSystem.m_lCmds[m_UndoSystem.m_Index];

                        // Same data type?
                        if( (Entry.m_Property.m_Value.m_pType && CmdVariant.m_Original.m_pType ) && (Entry.m_Property.m_Value.m_pType->m_GUID == CmdVariant.m_Original.m_pType->m_GUID) )
                        {
                            auto& UndoCmd = CmdVariant;
                            if( ( UndoCmd.m_isEditing || UndoCmd.m_isChange ) && std::strcmp( UndoCmd.m_Name.c_str(), Entry.m_Property.m_Path.c_str() ) == 0 )
                            {
                                if ( E.m_GroupGUID ) 
                                {
                                    xproperty::ui::details::group_render::RenderElement(E, i, UndoCmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags, *this, Entry);
                                }
                                else
                                {
                                    xproperty::ui::details::onRender<xproperty::settings::member_ui_t>( UndoCmd, Entry.m_Property.m_Value, *Entry.m_pUserData, Entry.m_Flags );
                                }

                                if (UndoCmd.m_isChange)
                                {
                                    std::string Error;
                                    xproperty::sprop::setProperty( Error, C.m_Base.second, *C.m_Base.first, { Entry.m_Property.m_Path, UndoCmd.m_NewValue }, m_Context );
                                    assert(Error.empty());
                                    UndoCmd.m_bHasChanged = true;
                                }

                                if( UndoCmd.m_isEditing == false )
                                {
                                    if(UndoCmd.m_bHasChanged) m_UndoSystem.m_Index++;
                                    assert( m_UndoSystem.m_Index <= static_cast<int>(m_UndoSystem.m_lCmds.size()) );
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

    //
    // Refresh all the properties
    //
    RefreshAllProperties();

    //
    // If we have multiple Entities refactor components
    //

    //
    // Render each of the components
    //
    for ( auto& E : m_lEntities )
    {
        int GlobalIndex = 0;
        for ( auto& C : E->m_lComponents )
        {
            ImGui::Columns( 2 );
            Render( *C, GlobalIndex );
            ImGui::Columns( 1 );
        }
    }
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
