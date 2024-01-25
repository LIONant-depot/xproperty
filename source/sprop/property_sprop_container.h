
#pragma once
namespace xproperty::sprop
{
    struct container
    {
        struct prop
        {
            std::string     m_Path;         // Path to the xproperty and all keys encoded
            xproperty::any   m_Value;        // Value + type associated with this xproperty
        };

        std::vector<prop>   m_Properties;
    };
}

