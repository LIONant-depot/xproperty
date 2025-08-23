cmake_minimum_required(VERSION 3.10)
project(xproperty_unit_test LANGUAGES CXX)

# Set a variable with the target name (visible in subdirectories)
set(TARGET_PROJECT "xproperty_unit_test")

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Removes the ZERO_CHECK project in the solution
set(CMAKE_SUPPRESS_REGENERATION true)

# Disable installation to avoid cmake_install.cmake
set(CMAKE_SKIP_INSTALL_RULES true)

# Limit build configurations to Debug and Release
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Available configuration types" FORCE)

# Initialize a list to track components.
# Using a blank set is fine, CMake list commands will initialize it.
set(COMPONENT_REGISTRY "" )

# set unicode for visual studio as the default
if(MSVC)
    add_definitions(-DUNICODE -D_UNICODE)
    add_compile_options(/utf-8)
endif()

# Include FetchContent module
include(FetchContent)

# Define the main executable
# This is important to be first so we cann add the target_links to it...
add_executable(xproperty_unit_test "source/examples/create_documentation/main.cpp"
                                   "source/examples/create_documentation/example_printing.h"
                                   "source/examples/create_documentation/example_properties.h"
                                   "source/examples/create_documentation/example_sprop.h"
                                   "source/examples/create_documentation/generate_documentation.h"
                                   "source/examples/create_documentation/my_properties.h"
)

#
# Organize the project a little
#
source_group("example/generate_documentation" FILES
                                   "source/examples/create_documentation/example_printing.h"
                                   "source/examples/create_documentation/example_properties.h"
                                   "source/examples/create_documentation/example_sprop.h"
                                   "source/examples/create_documentation/generate_documentation.h"
                                   "source/examples/create_documentation/my_properties.h"
)

source_group("" FILES "source/examples/create_documentation/main.cpp"
)

#
# Populate requirements
#
add_subdirectory("build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xproperty")

#
# Set the source file into the project as well as organize them in the IDE
#
get_property(COMPONENT_REGISTRY GLOBAL PROPERTY COMPONENT_REGISTRY)
message(STATUS "Processing component registry: ${COMPONENT_REGISTRY}")
foreach(COMPONENT_ID ${COMPONENT_REGISTRY})

    # add source files if it has any
    get_property(FILES GLOBAL PROPERTY ${COMPONENT_ID}_FILES)
    if(FILES)
        target_sources(${TARGET_PROJECT} PRIVATE ${FILES})
    endif()
    
    # add groups to organize the files in the ide
    get_property(GROUP GLOBAL PROPERTY ${COMPONENT_ID}_GROUP)
    if(GROUP)
        source_group("${GROUP}" FILES ${FILES})
    endif()

    # add includes if we need them
    get_property(INCLUDES GLOBAL PROPERTY ${COMPONENT_ID}_INCLUDES)
    if(INCLUDES)
        target_include_directories(${TARGET_PROJECT} PRIVATE ${INCLUDES})
    endif()

    # add linker paths
    get_property(LINKER_PATHS GLOBAL PROPERTY ${COMPONENT_ID}_LINKER_PATHS)
    if(LINKER_PATHS)
        message("LINKER_PATHS: ${LINKER_PATHS}")
        target_link_directories(${TARGET_PROJECT} PRIVATE ${LINKER_PATHS})
    endif()
        
endforeach()

#
# Set as the startup project in Visual Studio
#
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TARGET_PROJECT})
