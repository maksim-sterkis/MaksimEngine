set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

cmake_minimum_required(VERSION 3.20)
project(VK_game_engine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

# Custom command to compile shaders using glslc
set(GLSLC "${Vulkan_GLSLC_EXECUTABLE}")
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Please ensure Vulkan SDK is correctly installed.")
endif()

file(GLOB_RECURSE SHADERS "shaders/*.vert" "shaders/*.frag")
set(SPIRV_SHADERS "")
foreach(SHADER ${SHADERS})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
    )
    list(APPEND SPIRV_SHADERS ${SPIRV})
endforeach()

include(FetchContent)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.9
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.90.8
)
FetchContent_Populate(imgui)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG        release
)
FetchContent_MakeAvailable(tinyobjloader)

FetchContent_Declare(
  ktx
  GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
  GIT_TAG        v4.3.1
)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ktx)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
)
FetchContent_Populate(stb)

FetchContent_Declare(
  fastgltf
  GIT_REPOSITORY https://github.com/spnda/fastgltf.git
  GIT_TAG        main
)
set(FASTGLTF_ENABLE_DEPRECATED_CODE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fastgltf)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PRIVATE Vulkan::Vulkan glfw)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h" "*.hpp")
list(FILTER SOURCES EXCLUDE REGEX "${CMAKE_BINARY_DIR}/")
list(FILTER SOURCES EXCLUDE REGEX "tools/")
add_executable(${PROJECT_NAME} ${SOURCES} ${SPIRV_SHADERS})
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw imgui glm tinyobjloader ktx fastgltf::fastgltf)
target_include_directories(${PROJECT_NAME} PRIVATE src game ${stb_SOURCE_DIR})

add_executable(texture_compiler tools/texture_compiler.cpp)
target_include_directories(texture_compiler PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(texture_compiler PRIVATE ktx)

add_custom_target(run_texture_compiler
    COMMAND texture_compiler
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling textures..."
    DEPENDS texture_compiler
)
add_dependencies(${PROJECT_NAME} run_texture_compiler)

add_executable(model_compiler tools/model_compiler.cpp)
target_include_directories(model_compiler PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(model_compiler PRIVATE tinyobjloader fastgltf::fastgltf ktx)

add_custom_target(run_model_compiler
    COMMAND model_compiler
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling models..."
    DEPENDS model_compiler
)
add_dependencies(${PROJECT_NAME} run_model_compiler)
