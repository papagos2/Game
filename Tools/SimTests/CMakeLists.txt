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

# Scene exporter used by the visual preview (Tools/Preview).
add_executable(bhexport ${CMAKE_CURRENT_SOURCE_DIR}/export/ExportScene.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Bot.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Playtest.cpp)
target_link_libraries(bhexport PRIVATE bhsim)
target_include_directories(bhexport PRIVATE ${SIM_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(bhexport PRIVATE ${STRICT_FLAGS})

# Exports the engine's final render buffers (palette look review in Tools/Preview/render.html).
add_executable(bhrender ${CMAKE_CURRENT_SOURCE_DIR}/export/ExportRender.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Bot.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Playtest.cpp)
target_link_libraries(bhrender PRIVATE bhsim)
target_include_directories(bhrender PRIVATE ${SIM_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(bhrender PRIVATE ${STRICT_FLAGS})

# Icon contact sheet (review the procedural icon set).
add_executable(bhicons ${CMAKE_CURRENT_SOURCE_DIR}/icons/ExportIcons.cpp)
target_link_libraries(bhicons PRIVATE bhsim)
target_include_directories(bhicons PRIVATE ${SIM_DIR})
target_compile_options(bhicons PRIVATE ${STRICT_FLAGS})

# Playtest matrix: bots play every mission and difficulty with several strategies and the
# telemetry reports pacing, economy, balance and simulation health.
add_executable(bhplaytest ${CMAKE_CURRENT_SOURCE_DIR}/playtest/PlaytestMain.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Bot.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Playtest.cpp)
target_link_libraries(bhplaytest PRIVATE bhsim)
target_include_directories(bhplaytest PRIVATE ${SIM_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(bhplaytest PRIVATE ${STRICT_FLAGS})
