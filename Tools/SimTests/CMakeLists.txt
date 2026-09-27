# Native test harness for the Beaconhold simulation core.
# Builds the pure C++ core (Source/Beaconhold/Sim) outside Unreal with flags that are
# at least as strict as Unreal's clang toolchains (-Wall -Werror, no exceptions, no RTTI).
cmake_minimum_required(VERSION 3.16)
project(BeaconholdSimTests CXX)

if(NOT CMAKE_CXX_STANDARD)
	set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SIM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/Beaconhold/Sim)
file(GLOB SIM_SOURCES CONFIGURE_DEPENDS ${SIM_DIR}/*.cpp)

add_library(bhsim STATIC ${SIM_SOURCES})
target_include_directories(bhsim PUBLIC ${SIM_DIR})

set(STRICT_FLAGS -Wall -Wextra -Wshadow -Werror -Wno-unused-parameter -fno-exceptions -fno-rtti)
target_compile_options(bhsim PRIVATE ${STRICT_FLAGS})

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(bhtests ${TEST_SOURCES})
target_link_libraries(bhtests PRIVATE bhsim)
target_include_directories(bhtests PRIVATE ${SIM_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(bhtests PRIVATE ${STRICT_FLAGS})
