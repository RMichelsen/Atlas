cmake_minimum_required(VERSION 3.16)

project(Atlas)
find_package(Vulkan REQUIRED)

add_library(FreeType STATIC)
target_include_directories(FreeType PRIVATE 
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/include
)
target_compile_definitions(FreeType PRIVATE FT2_BUILD_LIBRARY)
target_sources(FreeType PRIVATE
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/autofit/autofit.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/base/ftbase.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/base/ftbbox.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/base/ftbitmap.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/base/ftdebug.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/base/ftglyph.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/base/ftinit.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/base/ftsystem.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/gzip/ftgzip.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/psnames/psnames.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/sfnt/sfnt.c
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/src/truetype/truetype.c
)

add_executable(Atlas WIN32)
target_include_directories(Atlas PRIVATE
    ${CMAKE_SOURCE_DIR}/dependencies/freetype/include
)
target_link_libraries(Atlas PRIVATE
    FreeType
    Vulkan::Vulkan
)
target_sources(Atlas PRIVATE
    ${CMAKE_SOURCE_DIR}/src/main.c
)
target_precompile_headers(Atlas PRIVATE
    <assert.h>
    <malloc.h>
    <math.h>
    <stdbool.h>
    <stdint.h>
    <stdio.h>
    <stdlib.h>

    <ft2build.h>
    <freetype/freetype.h>
    <freetype/ftoutln.h>
    <vulkan/vulkan.h>
)

if(WIN32)
    target_compile_definitions(Atlas PRIVATE 
        WIN32_LEAN_AND_MEAN
        VK_USE_PLATFORM_WIN32_KHR
    )
    target_precompile_headers(Atlas PRIVATE
        <windows.h>
    )
else()
    find_package(X11 REQUIRED)
    target_compile_definitions(Atlas PRIVATE 
        VK_USE_PLATFORM_XCB_KHR
    )
    target_link_libraries(Atlas PRIVATE
        ${X11_LIBRARIES}
        xcb
        m
    )
    target_precompile_headers(Atlas PRIVATE
        <xcb/xcb.h>
        <xcb/xproto.h>
    )
endif()

set(Shaders
    fragment.frag
    vertex.vert
    write_texture_atlas.comp
)

foreach(S ${Shaders})
    add_custom_command(
        TARGET Atlas POST_BUILD
        COMMAND Vulkan::glslangValidator ${CMAKE_SOURCE_DIR}/src/shaders/${S} 
        -g -V --target-env vulkan1.1 -o $<TARGET_FILE_DIR:Atlas>/${S}.spv
    )
endforeach()
