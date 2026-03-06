cmake_minimum_required(VERSION 3.20)
project(VCraft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Vulkan
find_package(Vulkan REQUIRED)

# GLFW – try find_package first, fall back to FetchContent
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.9
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# GLM
find_package(glm QUIET)
if(NOT glm_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()

# FastNoiseLite (header-only)
include(FetchContent)
FetchContent_Declare(
    fastnoiselite
    GIT_REPOSITORY https://github.com/Auburn/FastNoiseLite.git
    GIT_TAG v1.1.1
)
FetchContent_MakeAvailable(fastnoiselite)

# stb (header-only)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# VMA (header-only)
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.1.0
)
FetchContent_MakeAvailable(vma)

# Source files
set(SOURCES
    src/main.cpp
    src/engine.cpp
    src/world.cpp
    src/renderer.cpp
    src/raytracing.cpp
    src/camera.cpp
    src/input.cpp
    src/texture.cpp
    src/console.cpp
    src/ui_overlay.cpp
    src/player.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${stb_SOURCE_DIR}
    ${fastnoiselite_SOURCE_DIR}/Cpp
    ${vma_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
    NOMINMAX
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /permissive-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# ── Shader compilation ──────────────────────────────────────────────
find_program(GLSLC glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin")
if(NOT GLSLC)
    find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin")
endif()

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

set(SHADER_SOURCES
    ${SHADER_DIR}/mesh.vert
    ${SHADER_DIR}/mesh.frag
    ${SHADER_DIR}/sky.vert
    ${SHADER_DIR}/sky.frag
    ${SHADER_DIR}/ui.vert
    ${SHADER_DIR}/ui.frag
    ${SHADER_DIR}/vertex_lighting.comp
    ${SHADER_DIR}/vertex_lighting_simple.comp
    ${SHADER_DIR}/raygen.rgen
    ${SHADER_DIR}/miss.rmiss
    ${SHADER_DIR}/closesthit.rchit
)

set(SHADER_SPIRV_FILES "")
foreach(SHADER_SRC ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
    set(SHADER_SPV "${SHADER_BIN_DIR}/${SHADER_NAME}.spv")
    list(APPEND SHADER_SPIRV_FILES ${SHADER_SPV})

    add_custom_command(
        OUTPUT ${SHADER_SPV}
        COMMAND ${GLSLC} -V --target-env vulkan1.2 -o ${SHADER_SPV} ${SHADER_SRC}
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )
endforeach()

add_custom_target(shaders DEPENDS ${SHADER_SPIRV_FILES})
add_dependencies(${PROJECT_NAME} shaders)

# Copy texture atlas to build dir
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/ogsmp-RGBA.png"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/ogsmp-RGBA.png"
)

# Copy shaders to build dir
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${SHADER_BIN_DIR}"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
)
