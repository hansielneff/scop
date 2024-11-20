cmake_minimum_required(VERSION 3.5)

project(scop)

if(MSVC)
    add_compile_options(/WX /W3)
else()
    add_compile_options(-std=c99 -Werror)
endif()

if (APPLE)
    add_compile_options(-DAPPLE)
endif()

find_package(Vulkan REQUIRED)

include_directories(${Vulkan_INCLUDE_DIRS})

set(GLFW_BUILD_DOCS OFF CACHE BOOL  "GLFW lib only")
set(GLFW_INSTALL OFF CACHE BOOL  "GLFW lib only")

add_subdirectory(./src/glfw)

if(MSVC)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup")
endif()

set(scop-SRC
    ./src/main.c
    ./src/obj_parser.c
)

add_executable(scop WIN32 ${scop-SRC})
target_link_libraries(scop m ${Vulkan_LIBRARIES} glfw)
if(MSVC)
    if(${CMAKE_VERSION} VERSION_LESS "3.6.0")
        message("\n\t[ WARNING ]\n\n\tCMake version lower than 3.6.\n\n\t - Please update CMake and rerun; OR\n\t - Manually set 'scop' as StartUp Project in Visual Studio.\n")
    else()
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT scop)
    endif()
endif()
