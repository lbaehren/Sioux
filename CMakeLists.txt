## ==============================================================================
##
##  CMake project settings
##
## ==============================================================================

## Name of the project handled by CMake
project (Sioux)

## Minimum required version of CMake to configure the project
cmake_minimum_required (VERSION 2.8)

## Enforced CMake policy
cmake_policy (VERSION 2.8)

## ==============================================================================
##
##  System inspection
##
## ==============================================================================

find_package (Boost)

if (Boost_FOUND)
  include_directories (${Boost_INCLUDE_DIR})
endif (Boost_FOUND)

## ==============================================================================
##
##  Project files
##
## ==============================================================================

include_directories (
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/source
  )

add_subdirectory (source)
