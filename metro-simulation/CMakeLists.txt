cmake_minimum_required(VERSION 3.20)
project(metro_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(METRO_SIM_ENABLE_GUI "Build the Qt Widgets GUI shell" ON)
option(METRO_SIM_ENABLE_THIRDPARTY "Build vendored Qt third-party widgets" ON)

if(METRO_SIM_ENABLE_GUI)
    find_package(Qt6 QUIET COMPONENTS Widgets PrintSupport Svg OpenGL OpenGLWidgets)
    if(NOT Qt6_FOUND)
        message(WARNING "Qt6 was not found; disabling GUI build for this configuration.")
        set(METRO_SIM_ENABLE_GUI OFF)
        set(METRO_SIM_ENABLE_THIRDPARTY OFF)
    endif()
endif()

# ========== 第三方库（vendored） ==========
# nlohmann/json: header-only，include/thirdparty/json.hpp
# QCustomPlot:   GPLv3，src/thirdparty/qcustomplot.cpp + include/thirdparty/qcustomplot.h
# QtWaitingSpinner: MIT，src/thirdparty/waitingspinnerwidget.cpp + include/thirdparty/waitingspinnerwidget.h

# ========== 项目源文件（按模块分组） ==========
set(PROJECT_HEADERS
    # 核心模块
    include/core/metro_graph.h
    include/core/passenger.h
    include/core/event.h
    include/core/simulation.h
    include/core/path_planner.h
    include/core/statistics.h
    include/core/result_export.h
    include/core/report_writer.h
    include/core/asset_catalog.h
    include/core/utils.h
    # 入口
    include/core/main.h
)

set(PROJECT_SOURCES
    # 核心模块
    src/core/metro_graph.cpp
    src/core/passenger.cpp
    src/core/event.cpp
    src/core/simulation.cpp
    src/core/path_planner.cpp
    src/core/statistics.cpp
    src/core/result_export.cpp
    src/core/report_writer.cpp
    src/core/asset_catalog.cpp
    src/core/utils.cpp
    # 入口
    src/core/main.cpp
)

set(GUI_HEADERS
    include/core/visualization.h
    include/core/mainwindow.h
    include/core/station_editor.h
    include/core/station_3d_view.h
)

set(GUI_SOURCES
    src/core/visualization.cpp
    src/core/mainwindow.cpp
    src/core/station_editor.cpp
    src/core/station_3d_view.cpp
)

# ========== 第三方源文件 ==========
set(THIRDPARTY_SOURCES
    src/thirdparty/qcustomplot.cpp
    src/thirdparty/waitingspinnerwidget.cpp
)

set(THIRDPARTY_HEADERS
    include/thirdparty/qcustomplot.h
    include/thirdparty/waitingspinnerwidget.h
)

set(ALL_PROJECT_SOURCES ${PROJECT_SOURCES})
set(ALL_PROJECT_HEADERS ${PROJECT_HEADERS})

if(METRO_SIM_ENABLE_GUI)
    list(APPEND ALL_PROJECT_SOURCES ${GUI_SOURCES})
    list(APPEND ALL_PROJECT_HEADERS ${GUI_HEADERS})
endif()

if(METRO_SIM_ENABLE_THIRDPARTY)
    list(APPEND ALL_PROJECT_SOURCES ${THIRDPARTY_SOURCES})
    list(APPEND ALL_PROJECT_HEADERS ${THIRDPARTY_HEADERS})
endif()

if(METRO_SIM_ENABLE_GUI)
    add_executable(metro_sim WIN32 ${ALL_PROJECT_SOURCES} ${ALL_PROJECT_HEADERS} "${CMAKE_SOURCE_DIR}/resources/metro_sim.rc")
else()
    add_executable(metro_sim ${ALL_PROJECT_SOURCES} ${ALL_PROJECT_HEADERS})
endif()

set_target_properties(metro_sim PROPERTIES
    WIN32_EXECUTABLE $<BOOL:${METRO_SIM_ENABLE_GUI}>)

if(METRO_SIM_ENABLE_GUI)
    target_compile_definitions(metro_sim PRIVATE METRO_SIM_ENABLE_GUI=1)
endif()

# ========== 头文件搜索路径 ==========
target_include_directories(metro_sim PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/core"
    "${CMAKE_SOURCE_DIR}/include/thirdparty"
)

if(METRO_SIM_ENABLE_GUI)
    target_link_libraries(metro_sim PRIVATE Qt6::Widgets Qt6::PrintSupport Qt6::Svg Qt6::OpenGL Qt6::OpenGLWidgets)
    if(METRO_SIM_ENABLE_THIRDPARTY)
        if(MINGW)
            set_source_files_properties(src/thirdparty/qcustomplot.cpp PROPERTIES
                COMPILE_FLAGS "-Wa,-mbig-obj")
        endif()
    endif()
endif()

# ========== Node Editor 第三方库 ==========
if(METRO_SIM_ENABLE_GUI)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/nodeeditor)
    target_link_libraries(metro_sim PRIVATE QtNodes::QtNodes)
endif()

# ========== 运行时数据与资源（每次构建后自动同步） ==========
add_custom_command(TARGET metro_sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        ${CMAKE_BINARY_DIR}/bin/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources
        ${CMAKE_BINARY_DIR}/bin/resources
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_BINARY_DIR}/bin/data/results
    COMMENT "Syncing data and resources to build output..."
)
