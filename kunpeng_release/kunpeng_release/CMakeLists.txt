cmake_minimum_required(VERSION 3.20)
project(SCUT_WALK_TALL_KUNPENG VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

set(BUILD_TYPE "Release" CACHE STRING "Build type: Debug or Release")
set(PROPERTY CACHE BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

# 优先使用/usr/local下的SFML 3.0.0
set(CMAKE_PREFIX_PATH "/usr/local" ${CMAKE_PREFIX_PATH})

# SFML 3.x 需要使用大写组件名称
find_package(SFML 3 COMPONENTS Graphics Window System Audio REQUIRED)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
set(IMGUI_SFML_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui-sfml")
set(PUGIXML_DIR "${CMAKE_SOURCE_DIR}/third_party/pugixml")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(IMGUI_SFML_SOURCES
    ${IMGUI_SFML_DIR}/imgui-SFML.cpp
)

set(GAME_SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Game.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Enemy.cpp
    ${CMAKE_SOURCE_DIR}/src/game/ItemPickup.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Skill.cpp
    ${CMAKE_SOURCE_DIR}/src/player/Player.cpp
    ${CMAKE_SOURCE_DIR}/src/player/CSPlayer.cpp
    ${CMAKE_SOURCE_DIR}/src/player/ChemPlayer.cpp
    ${CMAKE_SOURCE_DIR}/src/map/Tile.cpp
    ${CMAKE_SOURCE_DIR}/src/map/Map.cpp
    ${CMAKE_SOURCE_DIR}/src/map/MapManager.cpp
)

add_executable(${PROJECT_NAME} ${GAME_SOURCES} ${IMGUI_SOURCES} ${IMGUI_SFML_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_SFML_DIR}
    ${PUGIXML_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:SFML_DEBUG>
    IMGUI_DEFINE_MATH_OPERATORS
    PUGIXML_HEADER_ONLY
)

# SFML 3.x 使用命名空间目标
target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    GL
    pthread
    dl
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets to output directory..."
)
