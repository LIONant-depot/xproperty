include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

FetchAndPopulate("https://github.com/LIONant-depot/xtextfile.git" "main")

DefineInterfaceComponent(xproperty "dependencies/xcore"
  "source/xproperty.h"
  "**sprop"
  "source/sprop/property_sprop.h"
  "source/sprop/property_sprop_collector.h"
  "source/sprop/property_sprop_container.h"
  "source/sprop/property_sprop_getset.h"
  "source/sprop/property_sprop_xtextfile_serializer.h"
  "README.md"
)
