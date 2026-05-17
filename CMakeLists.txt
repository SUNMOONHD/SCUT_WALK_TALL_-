cmake_minimum_required(VERSION 3.20)
project(SCUT_WALK_TALL VERSION 0.1.0 LANGUAGES CXX)

# ========== C++ Standard ==========
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ========== Third-party Library Paths ==========
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
set(SFML_DIR       "${THIRD_PARTY_DIR}/SFML/lib/cmake/SFML")
set(IMGUI_DIR      "${THIRD_PARTY_DIR}/imgui")
set(IMGUI_SFML_DIR "${THIRD_PARTY_DIR}/imgui-sfml")
set(PUGIXML_DIR    "${THIRD_PARTY_DIR}/pugixml")

# ========== Find SFML ==========
find_package(SFML 3 COMPONENTS Graphics Window System Audio REQUIRED)

# ========== ImGui Core Sources ==========
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.h
)

# ========== imgui-sfml Binding (provides SFML backend for ImGui) ==========
set(IMGUI_SFML_SOURCES
    ${IMGUI_SFML_DIR}/imgui-SFML.cpp
)

# ========== Game Sources ==========
set(GAME_SOURCES
    src/main.cpp
    src/game/Game.cpp
    src/game/Enemy.cpp
    src/game/ItemPickup.cpp
    src/game/Skill.cpp
    src/player/Player.cpp
    src/player/CSPlayer.cpp
    src/player/ChemPlayer.cpp
    src/map/Tile.cpp
    src/map/Map.cpp
    src/map/MapManager.cpp
)

# ========== Executable ==========
add_executable(${PROJECT_NAME} ${GAME_SOURCES} ${IMGUI_SOURCES} ${IMGUI_SFML_SOURCES})

# ========== Include Directories ==========
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SFML_DIR}/../../include
    ${IMGUI_DIR}
    ${IMGUI_SFML_DIR}
    ${PUGIXML_DIR}
    src
    include
)

# ========== Preprocessor Definitions ==========
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:SFML_DEBUG>
    IMGUI_DEFINE_MATH_OPERATORS
    PUGIXML_HEADER_ONLY
)

# ========== Force UTF-8 source encoding (MSVC) ==========
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# ========== Link Libraries ==========
target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    opengl32
)

# ========== Hide Console Window on Windows ==========
if(WIN32)
    if(MSVC)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        )
    endif()
endif()

# ========== Copy SFML DLLs to Output (Post-Build) ==========
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${THIRD_PARTY_DIR}/SFML/bin"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying SFML DLLs to output directory..."
)

# ========== Copy Assets to Output (Post-Build) ==========
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets to output directory..."
)
