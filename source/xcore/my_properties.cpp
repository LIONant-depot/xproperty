#include "my_properties.h"
#include "dependencies/xresource_mgr/source/xresource_mgr.h"  // Resource mgr include isolated here

namespace xproperty::settings 
{
    xresource::instance_guid ResourcePointerToGUID(const xresource::full_guid& FullGuid)
    {
        return xresource::g_Mgr.getFullGuid(FullGuid).m_Instance;
    }
}