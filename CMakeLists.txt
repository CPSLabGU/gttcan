cmake_minimum_required(VERSION 3.21)

project(gttcan C)

# Require the C standard to be C17,
# but allow extensions.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Set the default build type to Debug.
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Debug)
endif()

include(project.cmake)

add_library(gttcan STATIC ${gttcan_SOURCES})
target_include_directories(gttcan PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Sources/gttcan/include>
	$<INSTALL_INTERFACE:include/gttcan>
	$<INSTALL_INTERFACE:include>
)
