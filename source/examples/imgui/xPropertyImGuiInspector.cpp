#define NOMINMAX
#include "dependencies/xproperty/source/examples/imgui/xPropertyImGuiInspector.h"
#include "dependencies/xproperty/source/sprop/property_sprop_getset.h"
#include "dependencies/xproperty/source/sprop/property_sprop_collector.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <charconv>
#include <cstring>
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

        // A cmd built by inspector::BeginEdit/CommitEdit carries a whole-instance text snapshot
        // (std::string) in m_Original/m_NewValue instead of a single scalar - everything else
        // (per-row scalar widgets, the function-button/scope-toggle commits) keeps using the
        // original Path+any scalar shape, so both kinds of cmd share one system unmodified.
        if (Value.m_Original.is<std::string>())
            ApplySnapshotFromString(*Value.m_pPropObject, Value.m_pClassObject, Value.m_Original.get<std::string>(), Context);
        else
            xproperty::sprop::setProperty(Error, Value.m_pClassObject, *Value.m_pPropObject, xproperty::sprop::container::prop{ Value.m_Name, Value.m_Original }, Context);
        return Error;
    }

    std::string system::Redo(xproperty::settings::context& Context) noexcept
    {
        if (m_Index == static_cast<int>(m_lCmds.size()))
            return {};

        auto& Value = m_lCmds[m_Index++];
        std::string Error;

        if (Value.m_NewValue.is<std::string>())
            ApplySnapshotFromString(*Value.m_pPropObject, Value.m_pClassObject, Value.m_NewValue.get<std::string>(), Context);
        else
            xproperty::sprop::setProperty(Error, Value.m_pClassObject, *Value.m_pPropObject, xproperty::sprop::container::prop{ Value.m_Name, Value.m_NewValue }, Context);
        return Error;
    }

    //-----------------------------------------------------------------------------------
    // The exact set of atomic types xproperty::settings::AnyToString/StringToAny round-trip - kept
    // as its own check (rather than just calling AnyToString and trusting its result) because
    // AnyToString's own "unhandled type" path is assert(false) by design, meant to catch a genuinely
    // new atomic type nobody taught it to print yet. Snapshotting a whole component walks EVERY
    // leaf, including ones that were never reachable this way before (an enum-backed virtual
    // property, e.g.) - confirmed live: BeginEdit on a real object containing one crashed a Debug
    // build outright the first time an array control finally became reachable for it. Skipping those
    // here (not captured/restored by BeginEdit/CommitEdit yet) is an honest, narrower gap than a
    // crash - extend this list (and AnyToString/StringToAny) if a type needs to round-trip too.
    bool bIsSnapshotableType(std::uint32_t GUID) noexcept
    {
        return GUID == xproperty::settings::var_type<std::int32_t>::guid_v
            || GUID == xproperty::settings::var_type<std::uint32_t>::guid_v
            || GUID == xproperty::settings::var_type<std::int16_t>::guid_v
            || GUID == xproperty::settings::var_type<std::uint16_t>::guid_v
            || GUID == xproperty::settings::var_type<std::int8_t>::guid_v
            || GUID == xproperty::settings::var_type<std::uint8_t>::guid_v
            || GUID == xproperty::settings::var_type<float>::guid_v
            || GUID == xproperty::settings::var_type<double>::guid_v
            || GUID == xproperty::settings::var_type<std::string>::guid_v
            || GUID == xproperty::settings::var_type<std::wstring>::guid_v
            || GUID == xproperty::settings::var_type<std::uint64_t>::guid_v
            || GUID == xproperty::settings::var_type<std::int64_t>::guid_v
            || GUID == xproperty::settings::var_type<bool>::guid_v
#ifdef XCORE_PROPERTIES_H
            || GUID == xproperty::settings::var_type<xresource::full_guid>::guid_v
#endif
            ;
    }

    std::string SnapshotToString(const xproperty::type::object& Obj, const void* pInstance, xproperty::settings::context& Context) noexcept
    {
        std::string Snapshot;
        xproperty::sprop::collector(pInstance, Obj, Context, [&](const char* pPropertyName, xproperty::any&& Value, const xproperty::type::members& Member, bool isConst, const void*)
            {
                if (isConst || Value.m_pType == nullptr) return;

                // Pure grouping entries (scopes, and object-array/props "directory" markers) carry
                // no independently-settable value of their own - only real leaves and an array's
                // own size marker (path ending "[]", same convention SetSize() itself writes to)
                // round-trip through setProperty, so those are the only entries worth recording.
                if (std::holds_alternative<xproperty::type::members::scope>(Member.m_Variant)
                 || std::holds_alternative<xproperty::type::members::props>(Member.m_Variant))
                    return;

                if (std::holds_alternative<xproperty::type::members::list_var>(Member.m_Variant)
                 || std::holds_alternative<xproperty::type::members::list_props>(Member.m_Variant))
                {
                    const auto NameLen = std::strlen(pPropertyName);
                    if (!(NameLen >= 2 && pPropertyName[NameLen - 1] == ']' && pPropertyName[NameLen - 2] == '['))
                        return; // per-dimension index marker, not a settable value
                }

                // Not one of AnyToString/StringToAny's known atomic types (an enum-backed property,
                // a reflected member function's own "value", etc.) - see bIsSnapshotableType's own
                // comment for why this is skipped rather than passed through.
                if (!bIsSnapshotableType(Value.getTypeGuid()))
                    return;

                std::array<char, 256> Buffer;
                const int Len = xproperty::settings::AnyToString(Buffer, Value);

                // Format assumes no Path or value text embeds a tab/newline - true for every case
                // this is used against today (scalar/std::string leaves with short display text);
                // a value that could genuinely contain either would need escaping here first.
                Snapshot += pPropertyName;
                Snapshot += '\t';
                Snapshot += std::to_string(Value.m_pType->m_GUID);
                Snapshot += '\t';
                Snapshot.append(Buffer.data(), static_cast<std::size_t>(std::max(0, Len)));
                Snapshot += '\n';
            });
        return Snapshot;
    }

    void ApplySnapshotFromString(const xproperty::type::object& Obj, void* pInstance, const std::string& Snapshot, xproperty::settings::context& Context) noexcept
    {
        std::size_t Pos = 0;
        while (Pos < Snapshot.size())
        {
            const auto LineEndPos = Snapshot.find('\n', Pos);
            const auto LineEnd    = (LineEndPos == std::string::npos) ? Snapshot.size() : LineEndPos;
            const std::string_view Line{ Snapshot.data() + Pos, LineEnd - Pos };
            Pos = (LineEndPos == std::string::npos) ? Snapshot.size() : LineEndPos + 1;
            if (Line.empty()) continue;

            const auto Tab1 = Line.find('\t');
            const auto Tab2 = (Tab1 == std::string_view::npos) ? std::string_view::npos : Line.find('\t', Tab1 + 1);
            if (Tab1 == std::string_view::npos || Tab2 == std::string_view::npos) continue;

            const std::string_view Path     = Line.substr(0, Tab1);
            const std::string_view GuidText = Line.substr(Tab1 + 1, Tab2 - Tab1 - 1);
            const std::string_view ValueText= Line.substr(Tab2 + 1);

            std::uint32_t TypeGUID = 0;
            std::from_chars(GuidText.data(), GuidText.data() + GuidText.size(), TypeGUID);

            std::array<char, 256> Buffer;
            const std::size_t CopyLen = std::min(ValueText.size(), Buffer.size() - 1);
            std::memcpy(Buffer.data(), ValueText.data(), CopyLen);
            Buffer[CopyLen] = '\0';

            xproperty::any Value;
            if (!xproperty::settings::StringToAny(Value, TypeGUID, { Buffer.data(), static_cast<std::uint32_t>(CopyLen) }))
                continue;

            std::string Error;
            xproperty::sprop::setProperty(Error, pInstance, Obj, xproperty::sprop::container::prop{ std::string(Path), Value }, Context);
        }
    }
}

void xproperty::inspector::BeginEdit(const xproperty::type::object& Obj, void* pInstance, std::string_view Name) noexcept
{
    xproperty::ui::undo::cmd Cmd;
    Cmd.m_Name         = Name;
    Cmd.m_pPropObject  = &Obj;
    Cmd.m_pClassObject = pInstance;
    Cmd.m_Original.set<std::string>(xproperty::ui::undo::SnapshotToString(Obj, pInstance, *m_pContext));
    m_PendingEdit = std::move(Cmd);
}

void xproperty::inspector::CommitEdit(xproperty::settings::context& Context) noexcept
{
    if (!m_PendingEdit.has_value()) return;

    xproperty::ui::undo::cmd Cmd = std::move(*m_PendingEdit);
    m_PendingEdit.reset();

    const std::string After = xproperty::ui::undo::SnapshotToString(*Cmd.m_pPropObject, Cmd.m_pClassObject, Context);
    if (After == Cmd.m_Original.get<std::string>())
        return; // nothing actually changed across this bracket

    Cmd.m_NewValue.set<std::string>(After);
    Cmd.m_bHasChanged = true;
    m_OnChangeEvent.NotifyAll(*this, Cmd);
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

            // The Size field is editable only when the list is genuinely resizable - either a real
            // container (std::vector) or a fixed-capacity one wired up via member_overwrite_list_size
            // (both already fold into m_bHasRealSetSize at the list_table level, see xproperty.h's
            // getListTable()). member_ui_list_size_t itself is purely a STYLE opt-in (drag_bar vs
            // scroll_bar, custom min/max) - it must not gate editability on its own, or a genuinely
            // resizable list with no custom style would render disabled for no reason.
            if constexpr ( std::is_same_v<xproperty::settings::member_ui_list_size_t, T_UI_TAG> )
            {
                bool bHasRealSetSize = false;
                if (const auto* pListVar = std::get_if<xproperty::type::members::list_var>(&Entry.m_Variant))
                    bHasRealSetSize = !pListVar->m_Table.empty() && pListVar->m_Table[0].m_bHasRealSetSize;
                else if (const auto* pListProps = std::get_if<xproperty::type::members::list_props>(&Entry.m_Variant))
                    bHasRealSetSize = !pListProps->m_Table.empty() && pListProps->m_Table[0].m_bHasRealSetSize;

                if (!bHasRealSetSize) Flags.m_bShowReadOnly = true;

                // Declaration-site opt-out (see member_array_size_readonly_t's own comment) - a real,
                // resizable container whose size is still meant to be read-only in THIS inspector (its
                // size is driven by something else entirely, e.g. an imported asset's material list).
                if (const auto* pSizeReadOnly = Entry.getUserData<xproperty::settings::member_array_size_readonly_t>(); pSizeReadOnly && pSizeReadOnly->m_bReadOnly)
                    Flags.m_bShowReadOnly = true;
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

            xproperty::settings::member_custom_render_block_t::callback* pCustomRenderBlock =
                [&]() -> xproperty::settings::member_custom_render_block_t::callback*
                {
                    if (auto* pTag = Member.getUserData<xproperty::settings::member_custom_render_block_t>(); pTag)
                        return pTag->m_pCallback;
                    return nullptr;
                }();

            // Same "resolve a tag, if any" shape as pCustomRenderBlock above, for the other 3 custom-
            // render levels plus override check/reset - see each _t struct's own comment.
            xproperty::settings::member_custom_render_append_t::callback* pCustomRenderAppend =
                [&]() -> xproperty::settings::member_custom_render_append_t::callback*
                {
                    if (auto* pTag = Member.getUserData<xproperty::settings::member_custom_render_append_t>(); pTag)
                        return pTag->m_pCallback;
                    return nullptr;
                }();
            xproperty::settings::member_custom_render_replace_value_t::callback* pCustomRenderReplaceValue =
                [&]() -> xproperty::settings::member_custom_render_replace_value_t::callback*
                {
                    if (auto* pTag = Member.getUserData<xproperty::settings::member_custom_render_replace_value_t>(); pTag)
                        return pTag->m_pCallback;
                    return nullptr;
                }();
            xproperty::settings::member_custom_render_replace_row_t::callback* pCustomRenderReplaceRow =
                [&]() -> xproperty::settings::member_custom_render_replace_row_t::callback*
                {
                    if (auto* pTag = Member.getUserData<xproperty::settings::member_custom_render_replace_row_t>(); pTag)
                        return pTag->m_pCallback;
                    return nullptr;
                }();
            xproperty::settings::member_override_check_t::callback* pOverrideCheck =
                [&]() -> xproperty::settings::member_override_check_t::callback*
                {
                    if (auto* pTag = Member.getUserData<xproperty::settings::member_override_check_t>(); pTag)
                        return pTag->m_pCallback;
                    return nullptr;
                }();
            xproperty::settings::member_override_reset_t::callback* pOverrideReset =
                [&]() -> xproperty::settings::member_override_reset_t::callback*
                {
                    if (auto* pTag = Member.getUserData<xproperty::settings::member_override_reset_t>(); pTag)
                        return pTag->m_pCallback;
                    return nullptr;
                }();

            // Same static/dynamic resolution shape as Flags above - see member_item_width_t's own
            // comment for why this exists (a wide value widget leaves zero room for a same-line
            // m_OnCustomRenderAppend unless something narrows it).
            float ItemWidth = [&]() -> float
                {
                    if (auto* pDynamicWidth = Member.getUserData<xproperty::settings::member_dynamic_item_width_t>(); pDynamicWidth)
                        return pDynamicWidth->m_pCallback(pInstance, *m_pContext);
                    else if (auto* pStaticWidth = Member.getUserData<xproperty::settings::member_item_width_t>(); pStaticWidth)
                        return pStaticWidth->m_Width;
                    return -1.0f;
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
                    , ItemWidth
                    , pCustomRenderBlock
                    , pCustomRenderAppend
                    , pCustomRenderReplaceValue
                    , pCustomRenderReplaceRow
                    , pOverrideCheck
                    , pOverrideReset
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
        // Cached from the size-marker entry the moment THIS level was pushed (where E.m_pUserData is
        // genuinely the array's own list_props/list_var Member) - by the time object-array elements
        // render at this same depth, E has moved on to their own first reflected sub-property, so this
        // is the only reliable way to get back to the array's list_table for per-element controls.
        const xproperty::type::members* m_pArrayMember = nullptr;
        // Cached alongside m_pArrayMember, from the SAME size-marker entry's own E.m_pInstance - the
        // collector (xproperty::sprop::collector, driving RefreshAllProperties) already walks every
        // props::m_pCast in the chain from the root down to whatever level this array actually lives
        // at (see property_sprop_collector.h's DumpObject/ProcessList), so this is the CORRECT, already-
        // resolved instance for this array - NOT necessarily C.m_Base.second once the array is nested
        // inside a scope, a genuinely different object, or even a dynamic union/variant view (e.g.
        // union_variant_properties's "SomeVariant/A" in the xProperty Examples window, which resolves
        // through a runtime std::variant accessor, not a plain member pointer at all). list_table's own
        // TrySwap/TrySetSize are raw function pointers compiled against that exact resolved type - unlike
        // sprop::setProperty (used by the SCALAR array-controls branch), which re-walks the whole path
        // itself and so tolerates any nesting depth even when hardcoded to C.m_Base.second/first, a raw
        // list_table call has no such self-healing and needs the real pointer handed to it directly.
        void*                            m_pArrayInstance = nullptr;
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

    // Persists ACROSS loop iterations (unlike everything declared inside the loop body) - true while
    // a run of consecutive properties are all claiming on_custom_render_block. Without this, a
    // multi-property block (Start/Middle/End) did its own independent Columns(1)->draw->Columns(2)
    // round-trip for EACH property, and each round-trip added its own bit of vertical space even for
    // a property that drew nothing at all (Middle) - confirmed live as unexplained growing gaps
    // between block properties. Entering Columns(1) once for the whole run and only leaving it once
    // a property actually declines removes the repeated toggling entirely.
    bool bInPersistentBlock = false;

    // Non-empty while walking the elements/descendant-members of a LIST property whose own SIZE-
    // MARKER entry ("...[]") just claimed block content (via either mechanism - a member_custom_
    // render_block tag or the broadcast m_OnCustomRenderBlock delegate). Needed because an element's
    // own descendant members are a SEPARATE, unrelated reflected type's own declarations (e.g. a
    // std::vector<curve_keyframe>'s size marker carries the array's own tag/Path, but each element's
    // Time/Value/etc. are curve_keyframe's OWN Member entries, with no tag and no Path match of their
    // own) - confirmed live: without this, those inner members fell through and rendered as a normal,
    // duplicate array section underneath the custom-drawn canvas. A claim on an array's own size
    // marker implicitly covers its whole subtree; this is the prefix ("...", without the trailing
    // "[]") used to recognize "still inside that subtree" for every entry that follows.
    std::string ClaimedArrayPrefix;

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

        // Genuinely leaves the property grid for this one property, unlike the 4 "levels" further
        // down which all still render inside it - see on_custom_render_block's own comment for the
        // full design, including why this is split into a dry-run "ask" call and a real "draw" call
        // rather than one call unconditionally wrapped in Columns(1)/Columns(2). Checked before ANY
        // other grid-relative logic (including the section-separator block right below) so a block
        // property is never threaded through column-relative code that assumes a normal row is about
        // to render. Skipped entirely when nothing's registered and we're not already inside a
        // persistent block - even the ask call isn't free. bInPersistentBlock MUST stay part of this
        // guard even though it never claims anything on its own: the "resume Columns(2)" cleanup
        // (the "else if (bInPersistentBlock)" branch below) lives INSIDE this same if - now that every
        // consumer in this codebase has migrated off the broadcast delegate to per-property
        // member_custom_render_block tags, m_OnCustomRenderBlock.m_Delegates is empty for entire
        // inspectors that still use tag-based blocks, so without this the very next (tag-less, non-
        // array) property after a block ends would skip that cleanup entirely and render stuck inside
        // the block's own Columns(1) session - confirmed live as "After Block (normal)" losing its
        // left-column label and spanning the full width like block content, instead of rendering as
        // the plain two-column row it's supposed to.
        if ( !m_OnCustomRenderBlock.m_Delegates.empty() || E.m_pCustomRenderBlock || !ClaimedArrayPrefix.empty() || bInPersistentBlock )
        {
            bool bIsBlockContent = false;
            const ImColor RowColor = ComputeRowColor( iDepth, GlobalIndex + 1 ); // +1 matches the ++GlobalIndex a normal row applies before computing its own color

            // Riding along inside a previously-claimed array's own subtree (see ClaimedArrayPrefix's
            // own comment) - claimed automatically, no ask call needed (an element's own descendant
            // member has no tag/Path of its own to ask about anyway).
            const bool bInsideClaimedArray = !ClaimedArrayPrefix.empty()
                && E.m_Property.m_Path.size() > ClaimedArrayPrefix.size()
                && E.m_Property.m_Path.compare(0, ClaimedArrayPrefix.size(), ClaimedArrayPrefix) == 0
                && E.m_Property.m_Path[ClaimedArrayPrefix.size()] == '[';

            // Ask phase - Columns() is untouched here regardless of bInPersistentBlock, so this costs
            // nothing beyond the call itself for the overwhelming majority of properties that answer
            // false. The property's own member_custom_render_block tag (if any) gets first say, since
            // it's the more specific, declaration-site source of truth; only falls through to the
            // shared broadcast delegate (still the right tool for a one-off demo flourish tied to a
            // specific caller, see member_custom_render_block_t's own comment) if the tag didn't claim
            // it - a property can use either mechanism, never needs both.
            bool bClaimedByTag = false;
            if (bInsideClaimedArray)
            {
                bIsBlockContent = true;
            }
            else
            {
                ClaimedArrayPrefix.clear(); // left whatever subtree was previously claimed, if any

                if (E.m_pCustomRenderBlock)
                {
                    E.m_pCustomRenderBlock( *this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, static_cast<ImU32>(RowColor), true, bIsBlockContent );
                    bClaimedByTag = bIsBlockContent;
                }
                if (!bIsBlockContent)
                    m_OnCustomRenderBlock.NotifyAll( *this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, RowColor, true, bIsBlockContent );

                // A claim on a LIST's own size-marker ("...[]") implicitly covers every element and
                // descendant member under it too - see ClaimedArrayPrefix's own comment for why those
                // can't carry a matching tag/Path of their own.
                if (bIsBlockContent && E.m_Property.m_Path.ends_with("[]"))
                    ClaimedArrayPrefix.assign(E.m_Property.m_Path, 0, E.m_Property.m_Path.size() - 2);
            }

            if ( bIsBlockContent )
            {
                // Enter Columns(1) only on the FIRST property of a run - a multi-property block
                // (Start/Middle/End) stays in the SAME Columns(1) session across all of them instead
                // of each one independently round-tripping through Columns(2) and back, which used to
                // add its own bit of unwanted vertical space per property even when nothing was drawn
                // (Middle) - confirmed live as a growing gap between block properties.
                if ( !bInPersistentBlock )
                {
                    // Whatever "PropsGrid" session is active right now (the outer per-component one,
                    // or an earlier block's own resume - see the resume branch's own comment for why
                    // each of these is a genuinely distinct ImGui id, not the same stored entry) is
                    // about to be torn down by the Columns(1) below. Capture its current ratio into
                    // the shared value NOW, while it's still reachable, so a drag that happened on
                    // THIS session's own divider still propagates forward - the alternative (only
                    // capturing at the outer loop's own final Columns(1)) would miss any drag that
                    // happened on a row belonging to a session that closes before then.
                    if (ImGuiOldColumns* pClosing = ImGui::GetCurrentWindow()->DC.CurrentColumns)
                        if (pClosing->Columns.Size > 1)
                            m_SharedColumnRatio = pClosing->Columns[1].OffsetNorm;

                    // Columns(1)'s own automatic border-drawing bookkeeping (for the 2-column grid's
                    // vertical divider) appears to leave a stray dark band right at a block's own top
                    // edge - confirmed live as a genuine black gap, specifically only where a block
                    // starts, nowhere else. Explicit border=false on this transient single-column call
                    // (Columns(1) never needs a divider anyway - there's nothing to divide) avoids
                    // whatever that bookkeeping does here, without touching the grid's own border,
                    // which is a separate, persistent "PropsGrid"-id session resumed further down.
                    ImGui::Columns( 1, nullptr, false );
                    bInPersistentBlock = true;
                }

                // Only the entry that just established a NEW claim this iteration actually draws
                // anything - an entry merely riding along inside an already-claimed array subtree is
                // claimed-but-silent, same as Block End's own "claimed but nothing to add" rows.
                if (!bInsideClaimedArray)
                {
                    bool bIgnored = true;
                    if (bClaimedByTag) E.m_pCustomRenderBlock( *this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, static_cast<ImU32>(RowColor), false, bIgnored );
                    else               m_OnCustomRenderBlock.NotifyAll( *this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, RowColor, false, bIgnored );
                }

                ++GlobalIndex;
                continue;
            }
            else if ( bInPersistentBlock )
            {
                // This property declined - leave the block NOW, before it falls through to normal
                // rendering below. Columns(2) always resets the "current column" to 0 (left), but
                // every entry in this loop is entered expecting to already be resting in column 1
                // (right), left there by the PREVIOUS entry's own row (see the member_section comment
                // right below, which relies on the exact same invariant) - one NextColumn() restores
                // it, confirmed live as a fully swapped/shifted grid without this.
                //
                // This Columns() call sits INSIDE Render(), past this component's own top-level
                // PushTree() (the "Deal with the top most tree" section) - one extra id-stack level
                // than the OUTER "PropsGrid" establishment in Show()'s per-component loop, which runs
                // BEFORE Render() is even called. So "PropsGrid" here hashes to a GENUINELY DIFFERENT
                // stored ImGuiOldColumns entry than the outer one - confirmed live via debug logging
                // (different pointer, different id, independently drifting OffsetNorm - not a
                // drift-over-time bug, a different object from the first frame). Plain rows never hit
                // this, since NextColumn() just reuses whatever window->DC.CurrentColumns already
                // points to with no id re-lookup - only an actual Columns() call re-resolves by id,
                // which only a block's own resume does mid-render.
                //
                // Forcing this to resolve to the OUTER call's exact id via PushOverrideID was tried
                // and reverted: it fixed the mismatch, but that outer entry's resize-handle hit-test
                // button then fired TWICE in one frame under the same id - once here (EndColumns()
                // closes whatever session preceded this block, using THAT session's own stored
                // border flag, not this call's request) and once more at the per-component loop's own
                // final Columns(1) - "2 visible items with conflicting ID!" again, just relocated.
                // ImGui's classic Columns() assumes one open/close per id per frame; this block
                // mechanism opens/closes several times a frame by design, so id-sharing can't work.
                //
                // Mirroring just the ratio VALUE instead - matching the outer call's own approach,
                // see m_SharedColumnRatio's comment - keeps this resume's session, and its resize
                // handle, on its own genuinely distinct id (no collision), while still visually
                // matching whatever the shared divider's current position is.
                ImGui::Columns( 2, "PropsGrid" );
                if (ImGuiOldColumns* pResumed = ImGui::GetCurrentWindow()->DC.CurrentColumns)
                    if (m_SharedColumnRatio >= 0.0f && pResumed->Columns.Size > 1)
                        pResumed->Columns[1].OffsetNorm = m_SharedColumnRatio;
                ImGui::NextColumn();
                bInPersistentBlock = false;
            }
        }

        // A member_section tag starts a new named section - draw a separator once, the first time
        // its name is seen at this depth (LastSectionAtDepth is reset to nullptr whenever a new scope
        // is pushed, so a section label never leaks from one object instance into a sibling or a
        // nested one). Pure layout sugar - no effect on m_List/serialization/the CRC-boundary scope
        // machinery above.
        //
        // A list member's section tag is attached once, at the declaration - but every row derived
        // from that same declaration (the array's own size-marker row AND each of its per-element/
        // index rows) shares the identical m_pUserData/m_pSectionName, since they all point back to
        // the one reflected Member. The size-marker row correctly starts the section; its elements
        // then push into a FRESH (reset) tree depth, where the same section name looks "new" again
        // and re-triggers the separator a second time. Only the size-marker row itself (path ends in
        // the empty "[]" RefreshAllProperties emits for it) should ever be allowed to start a section -
        // a per-element row's path instead ends in "[key:value]", confirmed live as the fix (an
        // element row's own section check was otherwise re-drawing the same header right before [0]).
        const bool bIsArrayElementRow = [&]
            {
                if (!E.m_pUserData) return false;
                if (!std::holds_alternative<xproperty::type::members::list_var>(E.m_pUserData->m_Variant)
                    && !std::holds_alternative<xproperty::type::members::list_props>(E.m_pUserData->m_Variant))
                    return false;
                const auto& Path = E.m_Property.m_Path;
                const bool bIsSizeMarker = Path.size() >= 2 && Path.back() == ']' && Path[Path.size() - 2] == '[';
                return !bIsSizeMarker;
            }();

        if (!bIsArrayElementRow && E.m_pSectionName && E.m_pSectionName != LastSectionAtDepth[iDepth]
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
            //
            // ImGui::SeparatorText() paints no background of its own (unlike a Framed obj_scope
            // header, which gets one for free from ImGui's own TreeNodeEx styling) - it draws straight
            // onto the raw window background, standing out as a stark black bar against the striped
            // rows around it. Confirmed live as looking "really bad." First fix used this row's own
            // ComputeRowColor (the same per-row striping gray every normal row uses) - technically a
            // background, but visually a section header just blended into the plain data rows instead
            // of reading as its own organizational level. A section sits, hierarchically, between a
            // component's own header (ImGuiCol_Header, blue) and a plain data row - so its background
            // is derived from that SAME header color (a lighter tint: same hue, reduced saturation,
            // raised value) instead of the row-striping palette, reading as "related to the header,
            // one step down" rather than "just another row." Two earlier attempts both missed: a
            // lightened tint that ALSO halved saturation read as "too different" (a washed-out pale
            // blue, not a relative of the header); blending mostly toward the plain row-striping gray
            // read as barely related to the header at all. What was actually wanted: the EXACT same
            // hue and saturation as the header, just a higher value (brightness) - a genuinely lighter
            // shade of the SAME blue, not a desaturated or gray-blended one.
            const ImColor SectionRowColor = [&]
            {
                const ImVec4 Header = ImGui::GetStyle().Colors[ImGuiCol_Header];
                float h, s, v;
                ImGui::ColorConvertRGBtoHSV(Header.x, Header.y, Header.z, h, s, v);
                return ImColor::HSV(h, s, ImMin(v * 1.35f, 1.0f), Header.w);
            }();
            ImGui::NextColumn();
            {
                const ImVec2 P = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(P, ImVec2(P.x + ImGui::GetContentRegionAvail().x, P.y + ImGui::GetFrameHeight() + 1.0f), SectionRowColor);
            }
            ImGui::SeparatorText(E.m_pSectionName);
            ImGui::NextColumn();
            {
                const ImVec2 P = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(P, ImVec2(P.x + ImGui::GetContentRegionAvail().x, P.y + ImGui::GetFrameHeight() + 1.0f), SectionRowColor);
            }
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

        // Set by the array-element controls block below (drag handle/insert/delete buttons) - those
        // buttons are drawn right after the row's own [i] label, so by the time the shared "print
        // help" check further down runs ImGui::IsItemHovered(), it would otherwise see whichever of
        // MY buttons was hovered last (not the label) and pop the row's generic Name/Type/FullName/
        // GUID/Help tooltip on top of my own per-button HelpMarker tooltip. That block captures the
        // label's own hover state itself and calls Help(E) directly when appropriate, then sets this
        // so the shared check downstream skips instead of double-firing off a stale hover target.
        bool bSuppressRowHelp = false;

        // Level 3 of the 4 planned custom-rendering levels: replace the ENTIRE row except structural
        // controls (the override-revert ">" button stays - see m_OnCustomRenderReplaceRow's own
        // comment). Declared at this scope because it's resolved in the LEFT-column leaf-entry code
        // below but also needs to suppress the separate RIGHT-column block further down.
        bool bReplacedRow = false;

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
            // The label itself is always one line - but when this row's VALUE column grows past
            // one line (APPEND_NEW_LINE), the label's own background must grow to match, or the
            // extra strip beneath the label falls through to the raw window background same as the
            // value column's did. This row's real extra height isn't known until the RIGHT column
            // renders, further down - rather than guess via a formula (which drifted the moment
            // real frame-height append content, not just plain text, showed up - confirmed live),
            // use LAST FRAME's actual measured value for this same row (see m_RowExtraHeightCache's
            // own comment): correct except for one frame right after this row's shape changes,
            // self-correcting from the next frame on.
            if (m_Settings.m_bRenderLeftBackground)
            {
                const ImVec2 P = ImGui::GetCursorScreenPos();
                const auto   It = m_RowExtraHeightCache.find(PathHash);
                const float  ExtraH = (It != m_RowExtraHeightCache.end()) ? It->second : 0.0f;
                // +1px - see the matching #else branch's own comment for why (kept in sync even
                // though this branch doesn't currently compile in this build).
                DrawBackground(iDepth, GlobalIndex, P, P.y + ImGui::GetFrameHeight() + 1.0f + ExtraH);
            }
        }

        /*
        if (E.m_Property.m_Value.m_pType && E.m_Property.m_Value.m_pType->m_GUID == xproperty::settings::var_type<xresource::full_guid>::guid_v)
        {
            ImGuiStyle& style = ImGui::GetStyle();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, 18.0f));
            if (m_Settings.m_bRenderLeftBackground) DrawBackground(iDepth, GlobalIndex, ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight());
            // Get the bounding box of the last item (the tree node)
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0.2f));
            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID)), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "  %s", E.m_pName);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            goto SKIP_TO_NEXTHINGS;
        }
        else
        {
            if (m_Settings.m_bRenderLeftBackground) DrawBackground(iDepth, GlobalIndex, ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight());
        }
        */
#else
        const bool bCustomRender = false;
        if (m_Settings.m_bRenderLeftBackground)
        {
            const ImVec2 P = ImGui::GetCursorScreenPos();
            const auto   It = m_RowExtraHeightCache.find(PathHash);
            const float  ExtraH = (It != m_RowExtraHeightCache.end()) ? It->second : 0.0f;
            // +1px as a fixed safety margin against sub-pixel drift between this hand-computed EndY
            // and wherever ImGui's own internal layout actually places the NEXT row's start -
            // confirmed live as a genuine, real (not just low-contrast) 1px black gap between some
            // row pairs and not others in the SAME panel, via direct pixel sampling (a plain
            // GetFrameHeight()-only EndY undershot by ~1px depending on cumulative fractional cursor
            // position, which varies per-window since cursor coordinates are absolute screen space).
            // Deliberately a small FIXED margin, not the full ItemSpacing.y - a first attempt used
            // ItemSpacing.y itself, which "fixed" the gap by swallowing it entirely, and stayed
            // invisible only because the default spacing is tiny (~1.5px). Stress-testing with a
            // deliberately large ItemSpacing.y (set live through this same inspector) showed rows
            // NEVER visually separating no matter how large the setting got - the fix had quietly
            // made the setting meaningless. The real bug was ~1px of rounding error, not "the whole
            // gap needs covering" - the gap itself, sized to whatever ItemSpacing.y actually is, is
            // the correct, intended look (that's what makes the setting visible at all). Overshooting
            // by 1px is still safe - the next row's own background draws on top of it - just no
            // longer enough to eat a deliberately large gap.
            DrawBackground(iDepth, GlobalIndex, P, P.y + ImGui::GetFrameHeight() + 1.0f + ExtraH);
        }
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

                // Captured BEFORE PushTree below - that call pushes the ELEMENT's own new scope (for
                // its sub-properties), incrementing iDepth itself, so reading Tree[iDepth] afterward
                // would silently see the freshly-pushed (empty) level instead of the array's own level
                // one below it, where this was cached (confirmed live: the whole block below silently
                // no-opped, no buttons, no error, until this was moved before the push).
                const xproperty::type::members* pArrayMember   = Tree[iDepth].m_pArrayMember;
                void*                           pArrayInstance = Tree[iDepth].m_pArrayInstance;

                PushTree(Name.data(), bCustomRender, InstancePath, E.m_MyDimension, Tree[iDepth].m_isDefaultOpen, Tree[iDepth].m_isReadOnly, Tree[iDepth].m_isHidden);

                // Same Unity-style per-element controls as the atomic-array branch above, generalized
                // to object (list_props) elements. A scalar element's whole value fits in one
                // xproperty::any, so that branch could build everything from ordinary setProperty-by-
                // path calls; an object element can't, so this one goes straight to the real
                // list_table::TrySwap/TrySetSize this session added instead - Move/Insert/Delete are
                // all just chains of real Swap calls around a resize, exactly the Resize+Swap
                // composition this was designed around from the start, now that a real container
                // pointer is reachable. E itself is NOT the array's own entry at this point (it's the
                // element's own first reflected sub-property - see this branch's own comment above), so
                // the array's Member/list_table comes from the pArrayMember captured just above.
                if (pArrayMember)
                {
                    if (const auto* pListProps = std::get_if<xproperty::type::members::list_props>(&pArrayMember->m_Variant);
                        pListProps && pListProps->m_Table.size() == 1)
                    {
                        const auto& Table = pListProps->m_Table[0];

                        // Parse this element's own ordinal index and the array's own path prefix out
                        // of InstancePath ("ArrayPrefix[KeyType:Index]") - Tree[iDepth].m_iArray is
                        // never incremented for object arrays (only the atomic branch above does that),
                        // so the index has to come from the path's own embedded key instead.
                        const auto LastOpen = InstancePath.rfind('[');
                        const auto Colon    = InstancePath.find(':', LastOpen);
                        if (LastOpen != std::string_view::npos && Colon != std::string_view::npos && InstancePath.back() == ']'
                            // Only an ordinal (std::size_t-keyed) list supports index-based Move/
                            // Insert/Delete via a swap chain - a GUID- or other custom-keyed one (e.g.
                            // a texture slot list keyed by resource GUID) has no meaningful "adjacent
                            // index" to bubble through.
                            && Table.m_KeyAtomicType.m_GUID == xproperty::settings::var_type<std::size_t>::guid_v)
                        {
                            const std::string_view ArrayPrefixView = InstancePath.substr(0, LastOpen);
                            const std::string_view IndexStr        = InstancePath.substr(Colon + 1, InstancePath.size() - 1 - (Colon + 1));

                            // No nesting-depth restriction any more - pArrayInstance (cached above from
                            // the array's own size-marker entry's E.m_pInstance) is whatever instance
                            // xproperty::sprop::collector already resolved for THIS exact array, via the
                            // same props::m_pCast chain sprop::setProperty itself walks internally - so
                            // it's correct regardless of how the array was reached (declared directly on
                            // the component, nested inside an obj_scope, nested inside a genuinely
                            // different object, or even behind a dynamic union/variant view like
                            // union_variant_properties's "SomeVariant"). It's null only if collection
                            // genuinely never resolved an instance for this member (e.g. a variant
                            // accessor that returned {nullptr,nullptr} for the currently-inactive
                            // alternative) - guarded below.
                            int  CurrentIndex = 0;
                            bool bValidIndex  = (pArrayInstance != nullptr) && !IndexStr.empty();
                            for (char c : IndexStr) { if (c < '0' || c > '9') { bValidIndex = false; break; } CurrentIndex = CurrentIndex * 10 + (c - '0'); }

                            if (bValidIndex && Table.m_bHasRealSetSize && !Tree[iDepth].m_isReadOnly)
                            {
                                void*      pInstance   = pArrayInstance;
                                const auto SizeResult  = Table.TryGetSize(pInstance, *m_pContext);
                                const std::size_t N    = SizeResult ? SizeResult.value() : 0;

                                const auto KeyOf = [](std::size_t Index) { xproperty::any K; K.set<std::size_t>(Index); return K; };
                                // Was a hand-built cmd (wrong-shaped: no Original/NewValue, so Undo/Redo
                                // couldn't actually restore anything) firing m_OnChangeEvent directly -
                                // same bug already fixed for the ATOMIC array branch's own Commit()
                                // earlier this session, just missed here since this is a separate,
                                // parallel implementation. BeginEdit is called at each of the 4 trigger
                                // sites below, BEFORE any TrySwap/TrySetSize call, same as the atomic
                                // branch's own pattern.
                                const auto Commit = [&]
                                    {
                                        CommitEdit(*m_pContext);
                                    };
                                const auto SwapAt = [&](std::size_t A, std::size_t B)
                                    {
                                        (void)Table.TrySwap(pInstance, KeyOf(A), KeyOf(B), *m_pContext);
                                    };
                                const auto MoveElement = [&](std::size_t FromIndex, std::size_t ToIndex)
                                    {
                                        if (FromIndex == ToIndex) return;
                                        if (FromIndex < ToIndex) for (std::size_t k = FromIndex; k < ToIndex; ++k) SwapAt(k, k + 1);
                                        else                      for (std::size_t k = FromIndex; k > ToIndex; --k) SwapAt(k, k - 1);
                                        Commit();
                                    };

                                // m_pOwningInstance is the real deciding field - m_ArrayGUID alone is just
                                // a hash of the DECLARATION path, so two instances of the SAME component
                                // type (two entities, or later two rows of an outer dimension) would
                                // otherwise hash identically and falsely accept a cross-instance drop,
                                // silently pulling whatever happens to sit at that offset in THIS array
                                // instead of the real source (confirmed as a live bug, not hypothetical -
                                // MoveElement/ValueAt below close over THIS Render() call's own C/iE, so
                                // a same-GUID-different-instance drop was never actually reading the
                                // source array at all).
                                struct array_reorder_drag_payload { void* m_pOwningInstance; std::uint32_t m_ArrayGUID; int m_SourceIndex; };
                                const std::uint32_t ThisArrayGUID = xproperty::settings::strguid({ ArrayPrefixView.data(), static_cast<std::uint32_t>(ArrayPrefixView.size()) });

                                const float Sz = ImGui::GetFrameHeight();
                                ImGui::PushID(static_cast<int>(iE));
                                // Same transparent-idle-background treatment as the atomic-array
                                // branch's identical button cluster - see that PushStyleColor's own
                                // comment.
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                                ImGui::SameLine();
                                ImGui::Button("\xEE\x9D\xAF", ImVec2(Sz, Sz)); // GripperBarHorizontal
                                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                                {
                                    array_reorder_drag_payload Payload{ pInstance, ThisArrayGUID, CurrentIndex };
                                    ImGui::SetDragDropPayload("XPROP_ARRAY_ELEMENT", &Payload, sizeof(Payload));
                                    ImGui::Text("Move [%d]", CurrentIndex);
                                    ImGui::EndDragDropSource();
                                }
                                // Peeked BEFORE BeginDragDropTarget - that call draws the "valid drop
                                // zone" highlight automatically for ANY matching payload TYPE, before
                                // the instance/GUID check below ever runs, so every other array in the
                                // whole inspector would light up as droppable while dragging, only to
                                // silently reject the drop afterward. Skipping BeginDragDropTarget
                                // entirely for a genuinely non-matching payload means only the real
                                // source array's own rows ever highlight at all.
                                if (const ImGuiPayload* Peek = ImGui::GetDragDropPayload();
                                    Peek && Peek->IsDataType("XPROP_ARRAY_ELEMENT") && Peek->DataSize == sizeof(array_reorder_drag_payload)
                                    && static_cast<const array_reorder_drag_payload*>(Peek->Data)->m_pOwningInstance == pInstance
                                    && static_cast<const array_reorder_drag_payload*>(Peek->Data)->m_ArrayGUID == ThisArrayGUID)
                                {
                                    if (ImGui::BeginDragDropTarget())
                                    {
                                        if (const ImGuiPayload* Pay = ImGui::AcceptDragDropPayload("XPROP_ARRAY_ELEMENT"))
                                        {
                                            const auto& P = *static_cast<const array_reorder_drag_payload*>(Pay->Data);
                                            if (P.m_SourceIndex != CurrentIndex)
                                            {
                                                BeginEdit(*C.m_Base.first, C.m_Base.second, "Reorder Array Element");
                                                MoveElement(static_cast<std::size_t>(P.m_SourceIndex), static_cast<std::size_t>(CurrentIndex));
                                            }
                                        }
                                        ImGui::EndDragDropTarget();
                                    }
                                }
                                HelpMarker("Drag to reorder this element");

                                ImGui::SameLine();
                                if (ImGui::Button("\xEE\x9C\x8E", ImVec2(Sz, Sz))) // ChevronUp
                                {
                                    // Insert Above: grow, then bubble the fresh (blank) trailing element
                                    // from N down to CurrentIndex via adjacent swaps - unlike the atomic
                                    // branch, there's no generic "copy a whole object" primitive here,
                                    // so the new slot is a blank default rather than a duplicate.
                                    BeginEdit(*C.m_Base.first, C.m_Base.second, "Insert Array Element");
                                    (void)Table.TrySetSize(pInstance, N + 1, *m_pContext);
                                    for (std::size_t k = N; k > static_cast<std::size_t>(CurrentIndex); --k) SwapAt(k, k - 1);
                                    Commit();
                                }
                                HelpMarker("Insert a new (blank) element above this one");

                                ImGui::SameLine();
                                if (ImGui::Button("\xEE\x9C\x8D", ImVec2(Sz, Sz))) // ChevronDown
                                {
                                    BeginEdit(*C.m_Base.first, C.m_Base.second, "Insert Array Element");
                                    (void)Table.TrySetSize(pInstance, N + 1, *m_pContext);
                                    for (std::size_t k = N; k > static_cast<std::size_t>(CurrentIndex) + 1; --k) SwapAt(k, k - 1);
                                    Commit();
                                }
                                HelpMarker("Insert a new (blank) element below this one");

                                ImGui::SameLine();
                                if (ImGui::Button("\xEE\x9D\x8D", ImVec2(Sz, Sz))) // Delete (same glyph as elsewhere in this codebase)
                                {
                                    BeginEdit(*C.m_Base.first, C.m_Base.second, "Delete Array Element");
                                    for (std::size_t k = static_cast<std::size_t>(CurrentIndex); k + 1 < N; ++k) SwapAt(k, k + 1);
                                    (void)Table.TrySetSize(pInstance, N - 1, *m_pContext);
                                    Commit();
                                }
                                HelpMarker("Delete this element");
                                ImGui::PopStyleColor();
                                ImGui::PopID();
                            }
                        }
                    }
                }

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

                // E.m_pUserData is genuinely the array's own list_props/list_var Member right here -
                // cached for the object-array index-row branch below, which runs at this same depth
                // once E has moved on to the element's own first sub-property (see the cache field's
                // own comment on element::m_pArrayMember).
                Tree[iDepth].m_pArrayMember   = E.m_pUserData;
                Tree[iDepth].m_pArrayInstance = E.m_pInstance;

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
                    // Unity-style per-element array controls for atomic/scalar 1D arrays (object
                    // arrays and multi-dimensional ones need a real container pointer via
                    // list_table::TrySwap, not yet wired at this call site - a plain scalar element's
                    // whole value already round-trips cleanly through xproperty::any, so all of this
                    // is built purely from the same setProperty-by-path machinery every other commit
                    // in this file already uses). Layout follows Unity's own convention: a drag handle
                    // leftmost (real drag-and-drop reorder), the [i] label, then Insert Above/Insert
                    // Below/Delete at the row's end. Element VALUES are read directly out of C.m_List
                    // (already refreshed this frame by RefreshAllProperties) rather than through a
                    // fresh get-by-path call - there is no getProperty() free function in this
                    // reflection layer, only setProperty(); since none of this frame's own writes so
                    // far ever feed back into C.m_List until next frame's RefreshAllProperties, every
                    // ValueAt() below always returns the pre-edit snapshot, which is exactly what keeps
                    // a multi-step shift self-consistent (each source index is read exactly once).
                    //
                    // This is the ACTUAL leaf-row branch for a scalar array element (confirmed live -
                    // an earlier attempt placed this same block one level up, inside "Tree[iDepth].
                    // m_bArrayMustInsertIndex", which the code right above only ever sets true for
                    // OBJECT arrays - dead code for this case, hence no buttons appeared at all).
                    const bool bArrayControls = (E.m_Dimensions == 1 && !E.m_GroupGUID);
                    const int  CurrentIndex   = Tree[iDepth].m_iArray - 1;

                    std::string ArrayPrefix;
                    char        KeyTypeChar = 0;
                    std::size_t N           = 0;
                    bool        bHasRealSetSize = false;
                    if (bArrayControls)
                    {
                        const std::string_view FullPath = E.m_Property.m_Path;
                        const auto              LastOpen = FullPath.rfind('[');
                        assert(LastOpen != std::string_view::npos);
                        ArrayPrefix = std::string(FullPath.substr(0, LastOpen));
                        KeyTypeChar = FullPath[LastOpen + 1];

                        // How many elements (including this one) remain from here to the array's own
                        // end, purely from C.m_List's flat order (RefreshAllProperties always emits an
                        // array's elements consecutively, right after its own size-marker).
                        std::size_t RemainingCount = 0;
                        for (std::size_t k = static_cast<std::size_t>(iE); k < C.m_List.size(); ++k)
                        {
                            const auto& P = C.m_List[k]->m_Property.m_Path;
                            if (P.size() <= ArrayPrefix.size() || P.compare(0, ArrayPrefix.size(), ArrayPrefix) != 0 || P[ArrayPrefix.size()] != '[')
                                break;
                            ++RemainingCount;
                        }
                        N = static_cast<std::size_t>(CurrentIndex) + RemainingCount;

                        if (const auto* pListVar = std::get_if<xproperty::type::members::list_var>(&E.m_pUserData->m_Variant))
                            bHasRealSetSize = !pListVar->m_Table.empty() && pListVar->m_Table[0].m_bHasRealSetSize;
                    }

                    // An array whose own declaration opted out of user-driven resizing (see
                    // member_array_size_readonly_t's own comment - e.g. a material list rebuilt from an
                    // imported asset, where inserting/deleting a slot through this UI would desync it
                    // from whatever else tracks the same slots by index) - elements stay fully editable,
                    // only the structural controls are suppressed.
                    bool bSizeReadOnly = false;
                    if (bArrayControls)
                    {
                        if (const auto* pSizeReadOnly = E.m_pUserData->getUserData<xproperty::settings::member_array_size_readonly_t>(); pSizeReadOnly)
                            bSizeReadOnly = pSizeReadOnly->m_bReadOnly;
                    }

                    // A non-resizable array (fixed std::array/C-array with no member_overwrite_list_size
                    // override) or a read-only one has nothing these controls can actually do - hidden
                    // entirely rather than shown-but-disabled, decluttering the row instead of leaving a
                    // permanently-greyed-out button cluster nobody can ever use.
                    const bool bShowArrayControls = bArrayControls && bHasRealSetSize && !E.m_Flags.m_bShowReadOnly && !bSizeReadOnly;

                    const auto ElementPath = [&](std::size_t Index)
                        {
                            return std::format("{}[{}:{}]", ArrayPrefix, KeyTypeChar, Index);
                        };
                    const auto ValueAt = [&](std::size_t Index) -> xproperty::any
                        {
                            return C.m_List[iE + (Index - static_cast<std::size_t>(CurrentIndex))]->m_Property.m_Value;
                        };
                    const auto SetValueAt = [&](std::size_t Index, const xproperty::any& Value)
                        {
                            std::string Error;
                            xproperty::sprop::setProperty(Error, C.m_Base.second, *C.m_Base.first, xproperty::sprop::container::prop{ ElementPath(Index), Value }, *m_pContext);
                        };
                    const auto SetSize = [&](std::size_t NewSize)
                        {
                            std::string    Error;
                            xproperty::any Value; Value.set<std::size_t>(NewSize);
                            xproperty::sprop::setProperty(Error, C.m_Base.second, *C.m_Base.first, xproperty::sprop::container::prop{ std::format("{}[]", ArrayPrefix), Value }, *m_pContext);
                        };
                    // BeginEdit must be called BEFORE any Set*/erase call below (it snapshots the
                    // owning component's pre-edit state) - each trigger site below calls it as its
                    // first statement, then does its own Set*/SetSize sequence, then calls Commit()
                    // here, which now just closes the bracket via CommitEdit instead of hand-
                    // building a cmd - fixes the previous version's broken cmd (m_Name missing the
                    // "[]" suffix SetSize() actually writes to, m_Original/m_NewValue never
                    // populated), which made these operations fire a change notification without
                    // being correctly undoable.
                    const auto Commit = [&]
                        {
                            CommitEdit(*m_pContext);
                        };
                    // Moves the element at FromIndex to ToIndex, shifting everything strictly between
                    // them by one - read the whole affected span first (all still pre-edit values, per
                    // the ValueAt note above), then write the rotated order back out, so this is a
                    // single self-consistent operation regardless of how far apart the two indices are
                    // (needed for a real drag-and-drop reorder, not just adjacent swaps).
                    const auto MoveElement = [&](std::size_t FromIndex, std::size_t ToIndex)
                        {
                            if (FromIndex == ToIndex) return;
                            const std::size_t Lo = std::min(FromIndex, ToIndex);
                            const std::size_t Hi = std::max(FromIndex, ToIndex);
                            std::vector<xproperty::any> Buffer;
                            Buffer.reserve(Hi - Lo + 1);
                            for (std::size_t m = Lo; m <= Hi; ++m) Buffer.push_back(ValueAt(m));
                            xproperty::any Moved = Buffer[FromIndex - Lo];
                            Buffer.erase(Buffer.begin() + (FromIndex - Lo));
                            Buffer.insert(Buffer.begin() + (ToIndex - Lo), Moved);
                            for (std::size_t m = Lo; m <= Hi; ++m) SetValueAt(m, Buffer[m - Lo]);
                            Commit();
                        };

                    // m_pOwningInstance is the real deciding field, not just m_ArrayGUID - see the
                    // object-array branch's identical struct for why a path-hash alone falsely matches
                    // across different instances of the same component/array declaration.
                    struct array_reorder_drag_payload { void* m_pOwningInstance; std::uint32_t m_ArrayGUID; int m_SourceIndex; };
                    const std::uint32_t ThisArrayGUID = bArrayControls
                        ? xproperty::settings::strguid({ ArrayPrefix.data(), static_cast<std::uint32_t>(ArrayPrefix.size()) })
                        : 0;

                    // Transparent idle background, per explicit request ("just a test to see") - only
                    // ImGuiCol_Button changes, hover/active stay whatever the theme already uses, so
                    // these still read as clickable on interaction, just without a visible "chip" sitting
                    // behind the icon at rest.
                    if (bShowArrayControls) { ImGui::PushID(static_cast<int>(iE)); ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); }

                    bool Open;
                    const auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (bCustomRender) m_OnResourceLeftSize.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, flags, Name.data(), Open);
                    else               ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID + Tree[iDepth].m_iArray)), flags, "%s", Name.data());

                    // Captured right here, before any of the array-control buttons below get drawn -
                    // otherwise the shared "print help" check further down in Render() would see
                    // whichever of THOSE was hovered last instead of this label (see bSuppressRowHelp's
                    // own comment at its declaration).
                    if (bShowArrayControls)
                    {
                        if (ImGui::IsItemHovered()) Help(E);
                        bSuppressRowHelp = true;
                    }

                    // Drag handle - right after the [i] label, matching where it lands for object-array
                    // elements (that branch draws its own "[i]" via PushTree before it can even reach
                    // this button cluster, so leftmost was never actually achievable there - matched
                    // here instead, for one consistent layout across atomic and object elements).
                    if (bShowArrayControls)
                    {
                        const float Sz = ImGui::GetFrameHeight();
                        ImGui::SameLine();
                        ImGui::Button("\xEE\x9D\xAF", ImVec2(Sz, Sz)); // Segoe MDL2 Assets GripperBarHorizontal (U+E76F) - same icon font as the trashcan glyph below
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                        {
                            array_reorder_drag_payload Payload{ C.m_Base.second, ThisArrayGUID, CurrentIndex };
                            ImGui::SetDragDropPayload("XPROP_ARRAY_ELEMENT", &Payload, sizeof(Payload));
                            ImGui::Text("Move [%d]", CurrentIndex);
                            ImGui::EndDragDropSource();
                        }
                        // Peeked BEFORE BeginDragDropTarget - see the object-array branch's identical
                        // comment for why: that call draws the "valid drop zone" highlight for ANY
                        // matching payload TYPE regardless of instance/GUID, so skipping it entirely for
                        // a genuinely non-matching payload keeps every other array from lighting up.
                        if (const ImGuiPayload* Peek = ImGui::GetDragDropPayload();
                            Peek && Peek->IsDataType("XPROP_ARRAY_ELEMENT") && Peek->DataSize == sizeof(array_reorder_drag_payload)
                            && static_cast<const array_reorder_drag_payload*>(Peek->Data)->m_pOwningInstance == C.m_Base.second
                            && static_cast<const array_reorder_drag_payload*>(Peek->Data)->m_ArrayGUID == ThisArrayGUID)
                        {
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* Pay = ImGui::AcceptDragDropPayload("XPROP_ARRAY_ELEMENT"))
                                {
                                    const auto& P = *static_cast<const array_reorder_drag_payload*>(Pay->Data);
                                    if (P.m_SourceIndex != CurrentIndex)
                                    {
                                        BeginEdit(*C.m_Base.first, C.m_Base.second, "Reorder Array Element");
                                        MoveElement(static_cast<std::size_t>(P.m_SourceIndex), static_cast<std::size_t>(CurrentIndex));
                                    }
                                }
                                ImGui::EndDragDropTarget();
                            }
                        }
                        HelpMarker("Drag to reorder this element");
                    }

                    if (bShowArrayControls)
                    {
                        const float Sz = ImGui::GetFrameHeight();

                        ImGui::SameLine();
                        if (ImGui::Button("\xEE\x9C\x8E", ImVec2(Sz, Sz))) // Segoe MDL2 Assets ChevronUp (U+E70E)
                        {
                            // Insert Above: grow by one, then shift [CurrentIndex..N-1] up into
                            // [CurrentIndex+1..N] - index CurrentIndex itself is never written by this
                            // loop, so it keeps holding this element's original value while its shifted
                            // copy also lands one slot below it, net effect a duplicate now sits above.
                            BeginEdit(*C.m_Base.first, C.m_Base.second, "Insert Array Element");
                            SetSize(N + 1);
                            for (std::size_t k = N; k > static_cast<std::size_t>(CurrentIndex); --k)
                                SetValueAt(k, ValueAt(k - 1));
                            Commit();
                        }
                        HelpMarker("Insert a new element above this one");

                        ImGui::SameLine();
                        if (ImGui::Button("\xEE\x9C\x8D", ImVec2(Sz, Sz))) // Segoe MDL2 Assets ChevronDown (U+E70D)
                        {
                            // Insert Below: grow by one, then shift [CurrentIndex+1..N-1] up into
                            // [CurrentIndex+2..N], then explicitly copy this element's value into the
                            // one slot the loop above doesn't touch (CurrentIndex+1).
                            BeginEdit(*C.m_Base.first, C.m_Base.second, "Insert Array Element");
                            SetSize(N + 1);
                            for (std::size_t k = N; k > static_cast<std::size_t>(CurrentIndex) + 1; --k)
                                SetValueAt(k, ValueAt(k - 1));
                            SetValueAt(static_cast<std::size_t>(CurrentIndex) + 1, ValueAt(CurrentIndex));
                            Commit();
                        }
                        HelpMarker("Insert a new element below this one");

                        ImGui::SameLine();
                        if (ImGui::Button("\xEE\x9D\x8D", ImVec2(Sz, Sz))) // same trashcan glyph as "Empty Trashcan"/"Delete Node" elsewhere in this codebase
                        {
                            // Shift [CurrentIndex+1..N-1] down into [CurrentIndex..N-2], forward this
                            // time - opposite direction from insert, since each target here has already
                            // been read before being overwritten - then shrink by one.
                            BeginEdit(*C.m_Base.first, C.m_Base.second, "Delete Array Element");
                            for (std::size_t k = static_cast<std::size_t>(CurrentIndex); k + 1 < N; ++k)
                                SetValueAt(k, ValueAt(k + 1));
                            SetSize(N - 1);
                            Commit();
                        }
                        HelpMarker("Delete this element");

                        ImGui::PopStyleColor();
                        ImGui::PopID();
                    }
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

            // When this row's VALUE column grew past one line last frame (APPEND_NEW_LINE - see
            // m_RowExtraHeightCache's own comment), the LEFT column's label still draws as a single
            // line and, left alone, sits flush against the row's TOP edge - visually odd once the row
            // is genuinely taller than the label needs, confirmed live as looking "strange" compared
            // to sitting centered in the row's own height. Nudging the cursor down by half the cached
            // extra height centers it instead - same one-frame-lag/self-correcting cache the
            // background fill already relies on, reused here rather than adding a second mechanism.
            // Applied before the override-revert button below too, so the button and the label it sits
            // next to move together instead of the button staying pinned to the top alone. Independent
            // of m_bRenderLeftBackground - this is about where the label TEXT sits, not whether its
            // background paints, so it still centers correctly with that setting off.
            {
                const auto  ItCenter = m_RowExtraHeightCache.find(PathHash);
                const float ExtraHForCenter = (ItCenter != m_RowExtraHeightCache.end()) ? ItCenter->second : 0.0f;
                if (ExtraHForCenter > 0.0f) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ExtraHForCenter * 0.5f);
            }

            // Ask a registered consumer (if any) whether this property currently differs from
            // whatever THEY consider its base value - xproperty never tries to know what "overridden"
            // means itself. E.m_Property.m_Path is the full, canonical path (already embeds any array
            // index, e.g. "m_lTextures[G:2]") - a complete, opaque key either for a lookup into a
            // consumer-owned override-set, or to hand straight back into sprop::getProperty against a
            // second/base object, whichever strategy the consumer uses. The already-resolved current
            // value is passed too so a simple consumer doesn't need to re-fetch it.
            bool bIsOverridden = false;
            // Property's own tag gets first say (see member_override_check_t's own comment); falls
            // through to the broadcast delegate only if the property carries no tag of its own.
            if (E.m_pOverrideCheck) E.m_pOverrideCheck( *this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, bIsOverridden );
            else                    m_OnOverrideCheck.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, bIsOverridden);
            if (bIsOverridden)
            {
                // Tint stays pushed through the label draw below too (matches E20's own convention -
                // the whole row's left-column text recolors, not just the button), popped right after.
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(170, 170, 255, 255));
                if (ImGui::Button(">"))
                {
                    if (E.m_pOverrideReset) E.m_pOverrideReset( *this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path );
                    else                    m_OnOverrideReset.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path);
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

            // Level 3: replace the LEFT column - the override-revert ">" button above already rendered
            // unconditionally and stays exactly as is; this only decides whether the tree-node/label
            // draw below gets skipped. Also seeds bReplacedValue (level 2's own flag) further down, so
            // a consumer wanting the whole row custom registers both this AND
            // m_OnCustomRenderReplaceValue for the same property - the automatic NextColumn() between
            // here and the right-column code means one delegate can't draw both halves in a single call.
            // Same "fires for every property, consumer checks Path" idiom as levels 1/2.
            //
            // ImGui::TreeNodeEx (the normal path just below, when the consumer declines) reserves
            // GetTreeNodeToLabelSpacing() of space before its label text even for a leaf node with
            // NoTreePushOnOpen - confirmed live via pixel sampling: a plain ImGui::TextColored drawn
            // by a ReplaceRow consumer landed ~15px LEFT of where a sibling row's real label text
            // started, at any depth, because it never reserved that same space. Bracketing the notify
            // call in that exact spacing - not just for the "replaced" case, since we don't know
            // bReplacedRow's value until after the consumer has already drawn - means a consumer who
            // does nothing more than call TextColored still lands exactly where a normal label would,
            // matching the block feature's own "framework computes it, consumer doesn't have to know
            // ImGui's internals" philosophy. Unconditionally paired with Unindent right after, so the
            // normal TreeNodeEx path below (when the consumer declines) isn't double-offset.
            const float ReplaceRowSpacing = ImGui::GetTreeNodeToLabelSpacing();
            ImGui::Indent(ReplaceRowSpacing);
            const ImVec2 PreReplaceRowPos = ImGui::GetCursorScreenPos();
            // Property's own tag gets first say (see member_custom_render_replace_row_t's own
            // comment); falls through to the broadcast delegate only if the property carries no tag.
            if (E.m_pCustomRenderReplaceRow) E.m_pCustomRenderReplaceRow( *this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, bReplacedRow );
            else                             m_OnCustomRenderReplaceRow.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, bReplacedRow);
            // bReplaceRowDrewSomething (below) compares the cursor against PreReplaceRowPos to tell
            // "the consumer drew something real" apart from "nothing drew here at all" - that check
            // must happen BEFORE Unindent, or the Unindent's own X shift would make an untouched
            // cursor look like it moved, even when the consumer drew nothing.
            const ImVec2 PostReplaceRowPos = ImGui::GetCursorScreenPos();
            ImGui::Unindent(ReplaceRowSpacing);

            if (!bReplacedRow)
            {
                if (bCustomRender) m_OnResourceLeftSize.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, flags, pLeftLabel, Open);
                else               ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::size_t>(E.m_GUID)), flags, "%s", pLeftLabel);
            }
            else
            {
                // The TreeNodeEx above (this row's own hoverable item) was skipped, since the
                // consumer's callback just drew something else in its place - or, for a fully
                // level-4-suppressed row, drew NOTHING at all. Either way the shared "print help"
                // check further down must not run for this row: with no real item of its own,
                // IsItemHovered() there would fall back to whatever item WAS last drawn (a prior
                // row's), firing that row's tooltip again under this one's mouse position - confirmed
                // live as two properties' Name/Type/GUID/Help tooltips stacking simultaneously.
                //
                // A first attempt just called IsItemHovered() here unconditionally, matching
                // bShowArrayControls' own capture above - but that array-controls label ALWAYS draws
                // a real TreeNodeEx first, while a level-4-suppressed row draws nothing at all, so
                // its own IsItemHovered() falls back to the SAME stale prior-row item and fires
                // Help() a second time instead of zero times - confirmed live right after the first
                // attempt. Comparing the cursor to PreReplaceRowPos (captured right before the
                // NotifyAll call) distinguishes "the consumer drew something real to check hover
                // against" from "nothing drew here at all, don't check anything."
                const bool bReplaceRowDrewSomething = PostReplaceRowPos.x != PreReplaceRowPos.x || PostReplaceRowPos.y != PreReplaceRowPos.y;
                if (bReplaceRowDrewSomething && ImGui::IsItemHovered()) Help(E);
                bSuppressRowHelp = true;
            }

            if (bIsOverridden) ImGui::PopStyleColor();
        }


        // Print the help
        if ( ImGui::IsItemHovered() && bRenderBlankRight == false && bSuppressRowHelp == false )
        {
            Help( E );
        }

        //
        // Render the right column
        //
        ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
        // E.m_ItemWidth defaults to -1 (fill the column, unchanged from before) - member_item_width/
        // member_dynamic_item_width let a property narrow this to leave room for something appended
        // via m_OnCustomRenderAppend on the same line, instead of always needing APPEND_NEW_LINE.
        ImGui::PushItemWidth( E.m_ItemWidth );

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
            if ( m_Settings.m_bRenderRightBackground ) DrawBackground( iDepth-1, GlobalIndex, ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight() + 1.0f, CRA.x );

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
                    // Same transparent-idle-background treatment as the array-element controls
                    // (drag/insert/delete) - explicit user request, "to the point that the O|C
                    // buttons should do the same." Doubled ChevronUp/ChevronDown glyphs were tried as
                    // an icon replacement (reusing codepoints already proven to render in this font)
                    // but reverted - the user immediately flagged them as reading too much like the
                    // array-element insert-above/insert-below buttons, confusing "expand/collapse all"
                    // with "insert a new element." Plain " O "/" C " text has no such collision and
                    // stays back.
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::SameLine();
                    if ( ImGui::Button( " O " ) ) Tree[iDepth].m_OpenAll = 1;
                    HelpMarker( "Open/Expands all entries in this scope" );
                    ImGui::SameLine();
                    if ( ImGui::Button( " C " ) ) Tree[iDepth].m_OpenAll = -1;
                    HelpMarker( "Closes/Collapses all entries in this scope" );
                    ImGui::PopStyleColor();
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
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                if ( ImGui::Button( " O " ) ) Tree[iDepth-1].m_OpenAll = 1;
                HelpMarker( "Open/Expands all entries in the list" );
                ImGui::SameLine();
                if ( ImGui::Button( " C " ) ) Tree[iDepth-1].m_OpenAll = -1;
                HelpMarker( "Closes/Collapses all entries in the list" );
                ImGui::PopStyleColor();
            }
        }
        else if ( E.m_pUserData && std::holds_alternative<xproperty::type::members::function>(E.m_pUserData->m_Variant) )
        {
            // A reflected member function - draw it as a single button spanning the value
            // column, invoke through the same TryCallFunction the rest of xproperty already
            // uses for reflected functions, and notify m_OnChangeEvent the same way a
            // committed value edit would - so undo/log systems already listening don't need
            // a second, action-specific event to subscribe to.
            if ( m_Settings.m_bRenderRightBackground ) DrawBackground( iDepth, GlobalIndex, ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight() + 1.0f, CRA.x );

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
            // Unlike the other DrawBackground call sites, this row's content can grow past one
            // line (APPEND_NEW_LINE below), and the framework has no reliable way to know the
            // final height up front for arbitrary consumer-drawn append content. So instead of
            // guessing, defer the actual draw until after the row's content has rendered and its
            // real final height is known - the channel split keeps it landing BEHIND that content
            // rather than on top of it, same as if it had been drawn first.
            ImDrawList* pRowDrawList = ImGui::GetWindowDrawList();
            if ( m_Settings.m_bRenderRightBackground )
            {
                pRowDrawList->ChannelsSplit( 2 );
                pRowDrawList->ChannelsSetCurrent( 1 );
            }

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

            // Level 2 of the 4 planned custom-rendering levels: replace the value column entirely -
            // the left column (tree/label) above has already rendered normally by this point, this
            // only decides whether the RIGHT column's default widget gets skipped. Checked once per
            // entry (not per grouped sub-component - a consumer wanting to replace a vector2/vector3's
            // packed row can still do so via this same Path, it just replaces the whole group's row at
            // once rather than one axis at a time). Same "fires for every property, consumer checks
            // Path" idiom as level 1/m_OnOverrideCheck; bHandled starts false (normal rendering) and
            // the consumer opts in per-property by setting it true instead of a separate registration.
            // Seeded from bReplacedRow (level 3) rather than always false - a row already fully
            // replaced up in the left-column code has no default value widget left to skip separately.
            bool bReplacedValue = bReplacedRow;
            // Property's own tag gets first say (see member_custom_render_replace_value_t's own
            // comment); falls through to the broadcast delegate only if the property carries no tag.
            if (E.m_pCustomRenderReplaceValue) E.m_pCustomRenderReplaceValue( *this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, bReplacedValue );
            else                               m_OnCustomRenderReplaceValue.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value, bReplacedValue);

            if (!bReplacedValue) if ( E.m_Flags.m_bShowReadOnly || Tree[iDepth].m_isReadOnly )
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

            // Level 1 custom-rendering hook - fired once per entry regardless of which of the three
            // branches above actually rendered the value column this frame (read-only, mid-edit-
            // continuation, or the normal per-i HandleElement loop), so a consumer sees this exactly
            // once per property either way. See on_custom_render_append's own comment for the full
            // 4-level plan this is the first (purely additive) piece of.
            //
            // The framework owns the SameLine()-vs-new-line layout decision (via the APPEND_NEW_LINE
            // flag, same member_flags<>/member_dynamic_flags<> mechanism as SHOW_READONLY etc.) rather
            // than leaving every consumer to call ImGui::SameLine() itself - default is same-line
            // (matches a value widget with genuine leftover column space, e.g. a checkbox); a wide
            // value widget that fills the whole column via -1 width has zero room for a same-line
            // append, confirmed live, so a property whose consumer wants to append there should opt
            // into a new line instead.
            //
            // SameLine() is only meaningful when something was actually drawn in this column already -
            // a level-3-replaced-to-blank row (nothing drawn on either column, e.g. a level-4 "consumed
            // into an earlier block" row) has no real "last item" here for SameLine() to anchor to, so
            // it falls back to stale state from elsewhere (the LEFT column's own last item) and lands
            // the appended text at a bogus position - confirmed live as a single stray clipped
            // character where a fully-blank row's append should have been. Comparing the cursor to
            // rpos (this column's own start position, captured right after NextColumn() above) is a
            // reliable "has anything drawn here yet" check regardless of what the value branch did.
            // APPEND_NEW_LINE doesn't need a full ImGui::NewLine() - a value widget's own
            // ItemSize() already leaves the cursor at the start of a fresh line once it's drawn
            // (that's what makes SameLine() necessary at all for the same-line case below), and
            // NewLine() advanced a SECOND, entirely blank line on top of that - confirmed live as a
            // visibly oversized gap. But zero extra gap (relying only on the widget's own automatic
            // trailing spacing) reads as too cramped between the value and its annotation - confirmed
            // live right after removing NewLine() outright. One deliberate half-step via Dummy() is
            // the middle ground between those two confirmed-live extremes.
            const bool bValueColumnHasContent = ImGui::GetCursorScreenPos().x != rpos.x || ImGui::GetCursorScreenPos().y != rpos.y;
            // The Dummy() half-step (below, historically) turned out to cost a FULL ItemSpacing.y,
            // not half: Dummy() is itself a real item, so its OWN ItemSize() adds a SECOND trailing
            // ItemSpacing.y on top of the explicit height passed in - confirmed live via debug logging
            // (ItemSpacing.y=2, but the dummy call alone advanced the cursor by 4). Combined with the
            // value widget's own natural trailing spacing already baked into the cursor position
            // entering this branch, the appended row sat a full ~6px lower than it needed to - visibly
            // too tall, per direct user feedback comparing it against the edit box just above it.
            // Removed entirely: APPEND_NEW_LINE now relies solely on the value widget's own automatic
            // trailing spacing (the same amount every other row already gets for free), no manufactured
            // extra gap on top of it.
            if (!E.m_Flags.m_bAppendNewLine && bValueColumnHasContent) ImGui::SameLine();
            // Property's own tag gets first say (see member_custom_render_append_t's own comment);
            // falls through to the broadcast delegate only if the property carries no tag of its own.
            if (E.m_pCustomRenderAppend) E.m_pCustomRenderAppend( *this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value );
            else                         m_OnCustomRenderAppend.NotifyAll(*this, *C.m_Base.first, C.m_Base.second, E.m_Property.m_Path, E.m_Property.m_Value);

            // Now that this row's content has actually drawn, rpos (its start, captured right
            // after NextColumn() above) and the current cursor give its real final height. Cache
            // the extra-beyond-one-line part for the LEFT column to use NEXT frame (see
            // m_RowExtraHeightCache's own comment) regardless of whether the right background is
            // currently enabled - cheap to measure, and keeps the cache from going stale if that
            // setting gets toggled. Clamped to at least one frame height so a normal single-line row
            // (the overwhelming majority) renders byte-for-byte what DrawBackground always computed.
            {
                const float MinEndY = rpos.y + ImGui::GetFrameHeight();
                // GetCursorScreenPos() always has ONE trailing ItemSpacing.y already baked in
                // by whatever drew last (ItemSize() adds it immediately, every time) - whether
                // that's the value widget alone, or - via the SameLine() above - an append
                // widget drawn after it (e.g. Seed's inline refresh button, Narrow Bool's
                // "<- appended" text). That trailing spacing IS the row's own normal
                // end-of-row gap, not extra content height, so it must be stripped before
                // comparing to MinEndY (a bare, spacing-free single line height). Confirmed
                // live via pixel sampling: without this, any row with same-line append content
                // measured ~ItemSpacing.y taller than a plain row, and the LEFT column's
                // background then swallowed the very gap it should have left visible.
                const float CurY    = ImGui::GetCursorScreenPos().y - ImGui::GetStyle().ItemSpacing.y;
                const float FinalEndY = CurY > MinEndY ? CurY : MinEndY;
                m_RowExtraHeightCache[PathHash] = FinalEndY - MinEndY;
                if ( m_Settings.m_bRenderRightBackground )
                {
                    pRowDrawList->ChannelsSetCurrent( 0 );
                    // CRA (captured right after NextColumn()/PushItemWidth(), before ANY of this
                    // row's own content drew) is the correct width - CalcItemWidth()/
                    // GetContentRegionAvail() called HERE instead (after content already drew) was
                    // the actual bug: both measure from the CURRENT cursor, and most value widgets
                    // (sliders, drag floats, ...) leave the cursor mid-line after drawing, so a LATE
                    // measurement returns whatever sliver is left on that line - confirmed live via a
                    // debug print showing ColorVScalar1's late-measured width as ~1px while its row
                    // still visually filled correctly (the SLIDER's own frame, drawn earlier at the
                    // correct full width, was already sitting there - only the background rect
                    // computed from the wrong, late measurement was too narrow). A checkbox happened
                    // to leave the cursor back at the line start after drawing, which coincidentally
                    // measured correctly and made ONLY non-checkbox rows look broken - backwards from
                    // what the visual symptom first suggested.
                    //
                    // +1px here (draw only - NOT added to FinalEndY itself, which still feeds the
                    // cache above unchanged) as the same fixed sub-pixel-drift safety margin the
                    // other DrawBackground call sites use (see the left-column one's own comment for
                    // why this is a small FIXED margin, not the full ItemSpacing.y) - a row that
                    // didn't grow (the common case, FinalEndY == MinEndY) can undershoot the NEXT
                    // row's real start by a fractional pixel depending on this window's absolute
                    // screen position, confirmed live via direct pixel sampling. Overshooting by 1px
                    // is safe - the next row's own background draws on top of it.
                    DrawBackground( iDepth, GlobalIndex, rpos, FinalEndY + 1.0f, CRA.x );
                    pRowDrawList->ChannelsMerge();
                }
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

            // Tell ImGui we are going to use 2 columns for this component's rows.
            ImGui::Columns( 2, "PropsGrid" );

            // Each component's Columns() call is hashed against its OWN PushID(&C) above, so ImGui
            // naturally gives every component an independent stored divider - confirmed live as the
            // divider only moving for whichever component was under the mouse. Forcing them to
            // resolve to one shared ImGui id instead (via PushOverrideID) was tried first and
            // reverted: EndColumns()'s own resize-handle hit-test button derives its id directly from
            // that SAME columns id (see imgui_tables.cpp's EndColumns, `column_id = columns->ID + n`),
            // so sharing it made ImGui think N simultaneously-visible drag handles (one per component,
            // all at different screen rects) were the same widget - "2 visible items with conflicting
            // ID!", exactly the warning the user remembered hitting before.
            //
            // Mirroring the stored ratio (OffsetNorm) instead - not a pixel width, see
            // m_SharedColumnRatio's own comment for why pixels drift across a block's resume - keeps
            // every component's Columns() session, and its resize handle, genuinely independent (no
            // id collision), while making them all visually move together: stamp the shared ratio in
            // before rendering, read back whatever the user actually dragged afterward.
            ImGuiOldColumns* pCols = ImGui::GetCurrentWindow()->DC.CurrentColumns;
            if (pCols && m_SharedColumnRatio >= 0.0f && pCols->Columns.Size > 1)
                pCols->Columns[1].OffsetNorm = m_SharedColumnRatio;

            // Let the user change the base pointer if needed...
            void* pBackup = C->m_Base.second;
            m_OnGetComponentPointer.NotifyAll(*this, static_cast<int>(&C - E->m_lComponents.data()), C->m_Base.second, C->m_pUserData);

            // Render the actual component
            Render( *C, GlobalIndex );

            // Restore the original base pointer
            C->m_Base.second = pBackup;

            // Reset back to a single column - a live drag is only actually APPLIED to OffsetNorm
            // inside EndColumns() itself (see imgui_tables.cpp's EndColumns, "Apply dragging after
            // drawing the column lines"), which Columns(1) triggers here. Reading the ratio BEFORE
            // this call - as an earlier version did - captured last frame's value, not the drag that
            // just happened THIS frame: confirmed live as the divider visually following the mouse
            // while held, then snapping back the instant it was released. pCols itself stays valid
            // across this call (EndColumns() only clears window->DC.CurrentColumns, it doesn't remove
            // the entry from window->ColumnsStorage) as long as nothing else allocates a new columns
            // entry in between, which nothing here does.
            ImGui::Columns( 1 );

            // Capture whatever the user just dragged (in THIS component, including during a block's
            // own resume - same id within one component, so a mid-block drag is captured too) so
            // every other component picks it up starting next frame.
            if (pCols && pCols->Columns.Size > 1)
                m_SharedColumnRatio = pCols->Columns[1].OffsetNorm;
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

ImColor xproperty::inspector::ComputeRowColor( int Depth, int GlobalIndex ) const noexcept
{
    if( m_Settings.m_bRenderBackgroundDepth == false )
        Depth = 0;

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

    return Color;
}

void xproperty::inspector::DrawBackground( int Depth, int GlobalIndex, ImVec2 StartPos, float EndY ) const noexcept
{
    // StartPos/EndY are caller-supplied rather than measured here - a row whose content can grow
    // past one line (the plain-value branch, when APPEND_NEW_LINE draws a second line) defers this
    // call until after that content has actually drawn, so EndY reflects its real final height
    // instead of a hardcoded single-line guess. See that call site for the draw-list channel-split
    // this depends on to still land the rect BEHIND already-drawn content.
    //
    // GetContentRegionAvail() here is intentionally indent-aware - correct for the LEFT column,
    // where an indented label genuinely has less available space before the column boundary. The
    // RIGHT column's own call sites do NOT use this overload - see the explicit-width one below for
    // why.
    ImGui::GetWindowDrawList()->AddRectFilled(
        StartPos
        , ImVec2( StartPos.x + ImGui::GetContentRegionAvail().x, EndY )
        , ComputeRowColor( Depth, GlobalIndex ) );
}

void xproperty::inspector::DrawBackground( int Depth, int GlobalIndex, ImVec2 StartPos, float EndY, float Width ) const noexcept
{
    // Explicit-width overload for the RIGHT (value) column specifically. A nested scope's own
    // TreeNodeEx indents its LEFT-column label as intended, but ImGui's indent is a single
    // window-level DC state, not per-column - NextColumn() does not reset it, so that SAME indent
    // was leaking into the right column's GetContentRegionAvail() (used by the other overload),
    // making it under-report available width by roughly the current indent amount. A -1-width
    // WIDGET (slider, InputInt, ...) doesn't have this problem - it resolves through
    // ImGui::CalcItemWidth(), which isn't reduced by indent the same way - so a wide value widget's
    // own opaque frame happened to visually cover the gap, while a narrow one (a checkbox, with
    // nothing filling the rest of the row) exposed it as a real, visible black sliver at the row's
    // right edge - confirmed live specifically on nested-scope bool rows with no custom append
    // content to coincidentally mask it. Callers pass ImGui::CalcItemWidth() explicitly so the
    // background matches whatever width the row's own value widget is ACTUALLY using, regardless of
    // indent leakage.
    ImGui::GetWindowDrawList()->AddRectFilled(
        StartPos
        , ImVec2( StartPos.x + Width, EndY )
        , ComputeRowColor( Depth, GlobalIndex ) );
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
