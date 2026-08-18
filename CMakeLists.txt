# wxl-awesome-wotlk: a WarcraftXL extension for the 3.3.5a (12340) client.
#
# Builds one DLL against a WarcraftXL checkout. The core is a build-time dependency only: the SDK it
# provides is header-only, and everything else arrives through the API table at load, so nothing of
# the core is linked in.
#
# The client is a 32-bit process, so this configures Win32 and nothing else.

cmake_minimum_required(VERSION 3.20)
project(wxl-awesome-wotlk LANGUAGES CXX)

# --- where WarcraftXL is ---------------------------------------------------------------------------
# Resolution order: an explicit -DWXL_CORE_DIR, then the WXL_CORE_DIR environment variable, then the
# sibling checkout the two repositories sit in side by side. Nothing here names a machine.
if(NOT WXL_CORE_DIR AND DEFINED ENV{WXL_CORE_DIR})
    set(WXL_CORE_DIR "$ENV{WXL_CORE_DIR}")
endif()
set(WXL_CORE_DIR "${WXL_CORE_DIR}" CACHE PATH "WarcraftXL checkout this extension is built against")

if(NOT WXL_CORE_DIR)
    get_filename_component(WXL_CORE_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../wxl-core" ABSOLUTE)
    set(WXL_CORE_DIR "${WXL_CORE_DEFAULT}" CACHE PATH
        "WarcraftXL checkout this extension is built against" FORCE)
endif()

if(NOT EXISTS "${WXL_CORE_DIR}/include/wxl/PluginApi.h")
    message(FATAL_ERROR
        "WarcraftXL was not found at '${WXL_CORE_DIR}'.\n"
        "Point the build at a checkout with -DWXL_CORE_DIR=<path>, or set the WXL_CORE_DIR "
        "environment variable. The path must contain include/wxl/PluginApi.h.")
endif()
message(STATUS "WarcraftXL: ${WXL_CORE_DIR}")

# --- toolchain -------------------------------------------------------------------------------------
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "The target client is a 32-bit process; configure with -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static CRT, matching the core: the DLL loads inside Wow.exe, which has no reason to carry the
# toolset's redistributable, and a missing one is a loader failure at process start with no log.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# --- the extension ---------------------------------------------------------------------------------
# CONFIGURE_DEPENDS re-globs at build time, so adding a feature needs no edit here.
file(GLOB_RECURSE WXL_AWOW_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(wxl-awesome-wotlk SHARED ${WXL_AWOW_SRC})
set_target_properties(wxl-awesome-wotlk PROPERTIES OUTPUT_NAME "wxl-awesome-wotlk" PREFIX "")

# The core's own extension recipe also compiles src/game/*.cpp, the out-of-line half of its SDK. That
# half is the graphics facade; this module renders nothing and references none of it, so it is left
# out along with the d3d9/d3dcompiler dependency it would bring.
target_include_directories(wxl-awesome-wotlk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${WXL_CORE_DIR}/include"   # the extension ABI, shared verbatim with the core
    "${WXL_CORE_DIR}/src")      # the header-only SDK (wxl::game, wxl::events)

# WXL_EXTENSION is what makes PluginApi.h declare the two entry points as exports.
target_compile_definitions(wxl-awesome-wotlk PRIVATE
    WXL_EXTENSION WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)

if(MSVC)
    target_compile_options(wxl-awesome-wotlk PRIVATE /W4 /permissive-)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${WXL_AWOW_SRC})

# --- deployment --------------------------------------------------------------------------------------
# The core discovers extensions as Extensions\<folder>\<folder>.dll under the client directory, so
# the folder name and the DLL name have to agree with the target name above.
set(CLIENT_PATH "" CACHE PATH "Client folder the built DLL is copied into after each build")
if(CLIENT_PATH)
    add_custom_command(TARGET wxl-awesome-wotlk POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CLIENT_PATH}/Extensions/wxl-awesome-wotlk"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:wxl-awesome-wotlk>"
                "${CLIENT_PATH}/Extensions/wxl-awesome-wotlk/wxl-awesome-wotlk.dll"
        COMMENT "Deploy wxl-awesome-wotlk.dll -> ${CLIENT_PATH}/Extensions/wxl-awesome-wotlk")
endif()
