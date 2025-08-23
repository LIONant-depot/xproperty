cmake_minimum_required(VERSION 3.10)
project(xproperty_unit_test LANGUAGES CXX)
set(TARGET_PROJECT "xproperty_unit_test")
set(COMPONENT_REGISTRY "")

# Fetch Common.cmake from xcmake_tools
include(FetchContent)
FetchContent_Declare(
  xcmake_tools
  GIT_REPOSITORY https://github.com/LIONant-depot/xcmake_tools.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/xcmake_tools"
)
FetchContent_MakeAvailable(xcmake_tools)
include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

# Define the main executable
add_executable(${TARGET_PROJECT}
  "source/examples/create_documentation/main.cpp"
  "source/examples/create_documentation/example_printing.h"
  "source/examples/create_documentation/example_properties.h"
  "source/examples/create_documentation/example_sprop.h"
  "source/examples/create_documentation/generate_documentation.h"
  "source/examples/create_documentation/my_properties.h"
)

# Organize source files in IDE
source_group("example/generate_documentation" FILES
  "source/examples/create_documentation/example_printing.h"
  "source/examples/create_documentation/example_properties.h"
  "source/examples/create_documentation/example_sprop.h"
  "source/examples/create_documentation/generate_documentation.h"
  "source/examples/create_documentation/my_properties.h"
)
source_group("" FILES
  "source/examples/create_documentation/main.cpp"
)

# Add dependency subdirectory
add_subdirectory("build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xproperty")

# Process components
ProcessComponents()