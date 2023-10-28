# CMAKE_COMPILE_WARNING_AS_ERROR requires 3.24
cmake_minimum_required(VERSION 3.24)

# project() should be called ASAP following cmake_minimum_required()
# Sets PROJECT_NAME. Defaults to LANGUAGES C CXX. This will be the Visual Studio Solution name.
project(invaders-emulator)

message(VERBOSE, "PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}")
message(VERBOSE, "CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")
message(VERBOSE, "CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
message(VERBOSE, "CMAKE_GENERATOR = ${CMAKE_GENERATOR}")
message(VERBOSE, "CMAKE_GENERATOR_PLATFORM = ${CMAKE_GENERATOR_PLATFORM}")
message(VERBOSE, "CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
message(VERBOSE, "CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")

# Require C++17 to support terse static_assert
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Define DEBUG in Debug build, and RELEASE in Release build.
# Don't define either DEBUG or RELEASE in RelWithDebInfo build (CMAKE_CXX_FLAGS_RELWITHDEBINFO) which is essentially a "Dev" build.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DRELEASE")

#---------------------------------------------------------------------------------------------------
# "disassemble" target
set(DISASSEMBLE_TARGET "disassemble")

add_executable(
	${DISASSEMBLE_TARGET} # name is both name of executable and CMake target created
	"README.md"
)  

set(DISASSEMBLE_SRC_LIST
	"src/8080.cpp"
	"src/8080.h"
	"src/disassemble.cpp"
	"src/Helpers.cpp"
	"src/Helpers.h"
	"src/hp_assert.cpp"
	"src/hp_assert.h"
)
	
target_sources(${DISASSEMBLE_TARGET} PRIVATE ${DISASSEMBLE_SRC_LIST})

set_property(TARGET ${DISASSEMBLE_TARGET} PROPERTY COMPILE_WARNING_AS_ERROR ON)

# Compiler-specific options
if(MSVC)
	target_compile_options(
		${DISASSEMBLE_TARGET} 
		PRIVATE
		/W4 
		/wd4127 # conditional expression is constant
		/we4062 # enumerator 'identifier' in switch of enum 'enumeration' is not handled
	)

	target_compile_definitions(
		${DISASSEMBLE_TARGET} 
		PRIVATE 

		# warning C4996: 'fopen': This function or variable may be unsafe. Consider using fopen_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.
		# warning C4996: 'sprintf': This function or variable may be unsafe. Consider using sprintf_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.
		_CRT_SECURE_NO_WARNINGS
	)
else()
	target_compile_options(
		${DISASSEMBLE_TARGET} 
		PRIVATE 
		-Wall 
		-Wextra 
		-pedantic
	)
endif()

target_compile_options(${DISASSEMBLE_TARGET} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>) # MSVC multi-processor Compilation. Can reduce build time dramatically.

target_include_directories(
	${DISASSEMBLE_TARGET} 
	PRIVATE
	"src"
)

# Define grouping for source files in IDE project generation. This is just for Visual Studio IDE convenience.
# Without this, all .cpp files are in the "Source Files" filter, and all .h files are in the "Header files" filter.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${DISASSEMBLE_SRC_LIST}) # Requires CMake 3.8

set_target_properties(
	${DISASSEMBLE_TARGET} PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/data"
)

#---------------------------------------------------------------------------------------------------
# "emulator" target
set(EMULATOR_TARGET "emulator")

add_executable(
	${EMULATOR_TARGET} # name is both name of executable and CMake target created
	"README.md"
)  

set(EMULATOR_SRC_LIST
	"libs/gl3w/GL/gl3w.c"
	"libs/gl3w/GL/gl3w.h"
	"libs/gl3w/GL/glcorearb.h"
    "src/debugger/BreakpointsWindow.cpp"
    "src/debugger/BreakpointsWindow.h"
    "src/debugger/CpuWindow.cpp"
    "src/debugger/CpuWindow.h"
    "src/debugger/debugger.cpp"
    "src/debugger/debugger.h"
    "src/debugger/DebugWindow.cpp"
    "src/debugger/DebugWindow.h"
    "src/debugger/DisassemblyWindow.cpp"
    "src/debugger/DisassemblyWindow.h"
    "src/debugger/imgui_memory_editor.h"
    "src/debugger/MachineWindow.cpp"
    "src/debugger/MachineWindow.h"
    "src/imgui/imconfig.h"
    "src/imgui/imgui.cpp"
    "src/imgui/imgui.h"
    "src/imgui/imgui_demo.cpp"
    "src/imgui/imgui_draw.cpp"
    "src/imgui/imgui_impl_opengl3.cpp"
    "src/imgui/imgui_impl_opengl3.h"
    "src/imgui/imgui_impl_sdl.cpp"
    "src/imgui/imgui_impl_sdl.h"
    "src/imgui/imgui_internal.h"
    "src/imgui/imgui_widgets.cpp"
    "src/imgui/imstb_rectpack.h"
    "src/imgui/imstb_textedit.h"
    "src/imgui/imstb_truetype.h"
	"src/8080.cpp"
	"src/8080.h"
	"src/Audio.cpp"
	"src/Audio.h"
	"src/Display.cpp"
	"src/Display.h"
	"src/Helpers.cpp"
	"src/Helpers.h"
	"src/hp_assert.cpp"
	"src/hp_assert.h"
	"src/Input.cpp"
	"src/Input.h"
	"src/Invaders.cpp"
	"src/machine.cpp"
	"src/machine.h"
	"src/Renderer.cpp"
	"src/Renderer.h"
)
	
target_sources(${EMULATOR_TARGET} PRIVATE ${EMULATOR_SRC_LIST})

set_property(TARGET ${EMULATOR_TARGET} PROPERTY COMPILE_WARNING_AS_ERROR ON)

# Compiler-specific options
if(MSVC)
	target_compile_options(
		${EMULATOR_TARGET} 
		PRIVATE
		/W4 
		/wd4127 # conditional expression is constant
		/we4062 # enumerator 'identifier' in switch of enum 'enumeration' is not handled
        $<$<CONFIG:RELEASE>:/wd4390> # ImGui Release warning C4390: ';': empty controlled statement found; is this the intent?
	)

    target_compile_definitions(
		${EMULATOR_TARGET} 
		PRIVATE 

		# warning C4996: 'fopen': This function or variable may be unsafe. Consider using fopen_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.
		# warning C4996: 'sprintf': This function or variable may be unsafe. Consider using sprintf_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.
		_CRT_SECURE_NO_WARNINGS
	)
else()
	target_compile_options(
		${EMULATOR_TARGET} 
		PRIVATE 
		-Wall 
		-Wextra 
		-pedantic
	)
endif()

target_compile_options(${EMULATOR_TARGET} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>) # MSVC multi-processor Compilation. Can reduce build time dramatically.

target_include_directories(
	${EMULATOR_TARGET} 
	PRIVATE
	"src"
	"libs/gl3w"
)

# Define grouping for source files in IDE project generation. This is just for Visual Studio IDE convenience.
# Without this, all .cpp files are in the "Source Files" filter, and all .h files are in the "Header files" filter.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EMULATOR_SRC_LIST}) # Requires CMake 3.8

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(${EMULATOR_TARGET} PRIVATE ${OPENGL_LIBRARY})

# SDL2
# https://github.com/libsdl-org/SDL/releases
# See instructions in SDL2/docs/README-cmake.md and SDL2/cmake/sdl2-config.cmake
list(APPEND CMAKE_PREFIX_PATH "libs/SDL2/cmake") # path to sdl2-config.cmake

# 1. Look for a SDL2 package, 2. look for the SDL2 component and 3. fail if none can be found
find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)

# 1. Look for a SDL2 package, 2. Look for the SDL2maincomponent and 3. DO NOT fail when SDL2main is not available
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)

# n.b. target include directories are added by sdl2-config.cmake 

# SDL2::SDL2main may or may not be available. It is e.g. required by Windows GUI applications
if(TARGET SDL2::SDL2main)
	# It has an implicit dependency on SDL2 functions, so it MUST be added before SDL2::SDL2 (or SDL2::SDL2-static)
	target_link_libraries(${EMULATOR_TARGET} PRIVATE SDL2::SDL2main)
endif()

# Link to the actual SDL2 library. SDL2::SDL2 is the shared SDL library, SDL2::SDL2-static is the static SDL libarary.
target_link_libraries(${EMULATOR_TARGET} PRIVATE SDL2::SDL2)

# set "EMULATOR_DLL_LIST" var to contain SDL.dll location
get_target_property(SDL2_DLL SDL2::SDL2 IMPORTED_LOCATION)
list(APPEND EMULATOR_DLL_LIST "${SDL2_DLL}") # creates list if empty

# SDL2_mixer
# https://github.com/libsdl-org/SDL_mixer/releases
# I can't find any instructions for this in SDL2_mixer-devel-2.6.3-VC.zip, so following SDL2 pattern.
list(APPEND CMAKE_PREFIX_PATH "libs/SDL2_mixer/cmake") # path to sdl2_mixer-config.cmake
find_package(SDL2_mixer REQUIRED CONFIG REQUIRED COMPONENTS SDL2_mixer)
target_link_libraries(${EMULATOR_TARGET} PRIVATE SDL2_mixer::SDL2_mixer)
get_target_property(SDL2_mixer_DLL SDL2_mixer::SDL2_mixer IMPORTED_LOCATION)
list(APPEND EMULATOR_DLL_LIST "${SDL2_mixer_DLL}")

message(VERBOSE, "EMULATOR_DLL_LIST = ${EMULATOR_DLL_LIST}")

# Copy DLLs to output folder on Windows
if(WIN32)
	add_custom_command(TARGET ${EMULATOR_TARGET} POST_BUILD COMMAND
		${CMAKE_COMMAND} -E copy_if_different ${EMULATOR_DLL_LIST} $<TARGET_FILE_DIR:${EMULATOR_TARGET}>)
endif()

set_target_properties(
	${EMULATOR_TARGET} PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/data"
)

#---------------------------------------------------------------------------------------------------

# Default Visual Studio start up project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${EMULATOR_TARGET})
