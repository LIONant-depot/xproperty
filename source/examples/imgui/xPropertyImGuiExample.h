
#include "xPropertyImGuiInspector.h"
#include "../create_documentation/example_properties.h"

//-----------------------------------------------------------------------------------

using example_entry = std::pair<const xproperty::type::object&, void*>;

//-----------------------------------------------------------------------------------

template< typename T >
std::pair<const char*, example_entry> CreateInstance()
{
    static T Instance;
    Instance.setValues();
    return { xproperty::getObject(Instance)->m_pName, { *xproperty::getObject(Instance), &Instance } };
}

//-----------------------------------------------------------------------------------

template< typename... T_ARGS >
struct examples
{
    examples(T_ARGS&&... args)
        : m_Tables{ args.second... }
        , m_Names{ args.first ... }
    {}

    std::array<example_entry, sizeof...(T_ARGS) >   m_Tables;
    std::array<const char*, sizeof...(T_ARGS) >     m_Names;
};

//-----------------------------------------------------------------------------------
// EXAMPLES: https://github.com/LIONant-depot/xproperty/blob/master/documentation/Documentation.md 
// This are the examples that come from the xproperty library
examples Examples
{
      CreateInstance<my_object1>()
    , CreateInstance<my_object2>()
    , CreateInstance<my_object3>()
    , CreateInstance<derived1>()
    , CreateInstance<base1>()
    , CreateInstance<derived2>()
    , CreateInstance<common_types>()
    , CreateInstance<enums_unregistered>()
    , CreateInstance<enums_registered>()
    , CreateInstance<pointer_and_reference_c_style_values>()
    , CreateInstance<pointer_and_reference_c_style_props>()
    , CreateInstance<pointers_and_references_cpp_style>()
    , CreateInstance<list_c_arrays>()                   
    , CreateInstance<lists_cpp>()                    
    , CreateInstance<lists_advance>()                   
    , CreateInstance<virtual_properties>()
    , CreateInstance<union_variant_properties>()
    , CreateInstance<user_data_object>()
};


std::array<xproperty::inspector, 2>   Inspector{ "Settings", "xProperty Examples" };

//-----------------------------------------------------------------------------------

void DrawPropertyWindow()
{
    // Settings
    if constexpr ( true )
    {
        auto&                               I       = Inspector[0];
        static bool                         Init    = false;
        static xproperty::ui::undo::system  UndoSystem;

        if (Init == false)
        {
            Init = true;
            I.clear();
            I.AppendEntity();
            I.AppendEntityComponent(*xproperty::getObject(I), &I);
            I.AppendEntityComponent(*xproperty::getObject(UndoSystem), &UndoSystem);

            // One way to register the callbacks...
            // Note that the life time for the call back has to be the same as the undo system itself...
            I.m_OnChangeEvent.Register< [&](xproperty::inspector& Inspector, const xproperty::ui::undo::cmd& Cmd )
            {
                // This is where the undo system should be called
                UndoSystem.Add(Cmd);

            }>(); 
        }

        xproperty::settings::context Context;
        I.Show(Context, [&]
        {
            if (ImGui::Button("  Undo  ")) UndoSystem.Undo(Context);
            ImGui::SameLine(80);
            if (ImGui::Button("  Redo  ")) UndoSystem.Redo(Context);
        });
    }

    // Show xproperty examples
    // This example does not have undo/redo system just to show variety of examples
    if constexpr (true)
    {
        auto&                               I           = Inspector[1];
        static int                          iSelection  = -1;
        xproperty::settings::context        Context;

        I.Show(Context, [&]
        {
            if (ImGui::Combo("Select example", &iSelection, Examples.m_Names.data(), static_cast<int>(Examples.m_Names.size())))
            {
                I.clear();
                I.AppendEntity();
                I.AppendEntityComponent(Examples.m_Tables[iSelection].first, Examples.m_Tables[iSelection].second);
            }
        });
    }
}

