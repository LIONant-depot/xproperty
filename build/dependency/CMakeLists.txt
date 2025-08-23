# The library
cmake_minimum_required(VERSION 3.10)

# Set policy CMP0169 to OLD to allow FetchContent_Populate
# FetchContent_MakeAvailable will automatically execute root cmakes which will break the build
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

#
# Populate xtextfile
#
FetchContent_Declare(
  xtextfile
  GIT_REPOSITORY https://github.com/LIONant-depot/xtextfile.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xtextfile"  # Adjusted to go up one level
)

FetchContent_GetProperties(xerr)
if(NOT xtextfile_POPULATED)
  message(STATUS "Populating xtextfile...")
  FetchContent_Populate(xtextfile)
  
  # Include CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xtextfile/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xtextfile")
endif()


#
# Define as an interface library only if not already defined
#
if(NOT TARGET xproperty)

  # Define as temp library
  add_library(xproperty INTERFACE)

  # determine the root of our library
  if(DEFINED TARGET_PROJECT AND "${TARGET_PROJECT}" STREQUAL "xproperty_unit_test")
    set(ROOT ".")
  else()
    set(ROOT "dependencies/xproperty")

    # set the include path
    set(XPROPERTY_INCLUDE_LIST "${ROOT}")
  endif()

  # set the files required
  set(XFILE_FILES_LIST
    "${ROOT}/source/xproperty.h"
    "${ROOT}/source/sprop/property_sprop.h"
    "${ROOT}/source/sprop/property_sprop_collector.h"
    "${ROOT}/source/sprop/property_sprop_container.h"
    "${ROOT}/source/sprop/property_sprop_getset.h"
    "${ROOT}/source/examples/xcore_sprop_serializer.h"    
    "${ROOT}/README.md"
  )

  # Set global properties
  set_property(GLOBAL PROPERTY XPROPERTY_GROUP "dependencies/xcore/xproperty")
  set_property(GLOBAL PROPERTY XPROPERTY_FILES "${XPROPERTY_FILES_LIST}")
  set_property(GLOBAL PROPERTY XPROPERTY_INCLUDES "${XPROPERTY_INCLUDE_LIST}")
  set_property(GLOBAL APPEND PROPERTY COMPONENT_REGISTRY "XPROPERTY")

endif()