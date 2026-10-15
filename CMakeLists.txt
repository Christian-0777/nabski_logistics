cmake_minimum_required(VERSION 3.16)

project(nlsi_exclusive_logbook VERSION 1.7.3 LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build validation tests." ON)
option(BUILD_DASHBOARD "Build the Qt Widgets dashboard application." ON)

include(GNUInstallDirs)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

set(NLSI_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/scs_sdk_1_15")

if(BUILD_DASHBOARD OR BUILD_TESTING)
    find_package(Qt6 QUIET COMPONENTS Core Widgets Network Concurrent)
    if(NOT Qt6_FOUND)
        find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network Concurrent)
        set(QT_VERSION_MAJOR 5)
    else()
        set(QT_VERSION_MAJOR 6)
    endif()

    if(QT_VERSION_MAJOR EQUAL 6)
        set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::Network Qt6::Concurrent)
    else()
        set(QT_LIBS Qt5::Core Qt5::Widgets Qt5::Network Qt5::Concurrent)
    endif()
endif()

add_library(nlsi_ipc STATIC
    src/shared/TelemetryIPC.cpp
)

target_include_directories(nlsi_ipc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
        ${CMAKE_CURRENT_SOURCE_DIR}/scs_sdk_1_15/include
)

target_compile_features(nlsi_ipc PUBLIC cxx_std_17)

if(BUILD_DASHBOARD OR BUILD_TESTING)
    add_library(nlsi_display_formatting STATIC
        src/shared/DisplayFormatting.cpp
    )
    target_include_directories(nlsi_display_formatting
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    )
    target_link_libraries(nlsi_display_formatting PUBLIC ${QT_LIBS})
    target_compile_features(nlsi_display_formatting PUBLIC cxx_std_17)
endif()

add_library(nlsi_plugin_ipc STATIC
    src/shared/TelemetryIPC.cpp
)
target_include_directories(nlsi_plugin_ipc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
        ${CMAKE_CURRENT_SOURCE_DIR}/scs_sdk_1_15/include
)
target_compile_features(nlsi_plugin_ipc PUBLIC cxx_std_17)

if(BUILD_DASHBOARD)
    add_executable(nlsi_dashboard WIN32
        src/DashboardMainWindow.cpp
        src/DashboardMainWindow.h
        src/DashboardTypography.h
        src/shared/JobHistory.cpp
        src/shared/JobHistory.h
        src/main.cpp
        src/version.rc
    )

    target_include_directories(nlsi_dashboard
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
    )

    target_link_libraries(nlsi_dashboard PRIVATE ${QT_LIBS} nlsi_ipc nlsi_display_formatting)
endif()

add_library(nlsi_scs_plugin SHARED
    src/plugin/ScsTelemetryPlugin.cpp
    src/plugin/nlsi_scs_plugin.def
)

target_include_directories(nlsi_scs_plugin
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
        ${CMAKE_CURRENT_SOURCE_DIR}/scs_sdk_1_15/include
)

target_compile_definitions(nlsi_scs_plugin PRIVATE SCSSDK_STATIC)

target_compile_features(nlsi_scs_plugin PRIVATE cxx_std_17)
target_link_libraries(nlsi_scs_plugin PRIVATE nlsi_plugin_ipc)
set_property(TARGET nlsi_plugin_ipc nlsi_scs_plugin PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

install(TARGETS nlsi_ipc nlsi_scs_plugin
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
    ARCHIVE DESTINATION .
)
if(BUILD_DASHBOARD)
    install(TARGETS nlsi_dashboard RUNTIME DESTINATION .)
endif()

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/nlsi.ico
    DESTINATION .
)

if(BUILD_TESTING)
    enable_testing()
    add_executable(nlsi_telemetry_tests
        tests/telemetry_ipc_tests.cpp
        src/shared/JobHistory.cpp
        src/shared/JobHistory.h
    )
    target_include_directories(nlsi_telemetry_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    )
    target_compile_features(nlsi_telemetry_tests PRIVATE cxx_std_17)
    if(BUILD_DASHBOARD)
        target_link_libraries(nlsi_telemetry_tests PRIVATE ${QT_LIBS} nlsi_ipc nlsi_display_formatting)
    else()
        target_link_libraries(nlsi_telemetry_tests PRIVATE nlsi_ipc nlsi_display_formatting)
    endif()
    add_test(NAME nlsi_telemetry_tests COMMAND nlsi_telemetry_tests)
    add_executable(nlsi_job_history_tests
        tests/job_history_tests.cpp
        src/shared/JobHistory.cpp
        src/shared/JobHistory.h
    )
    target_include_directories(nlsi_job_history_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    )
    target_compile_features(nlsi_job_history_tests PRIVATE cxx_std_17)
    target_link_libraries(nlsi_job_history_tests PRIVATE ${QT_LIBS} nlsi_ipc)
    add_test(NAME nlsi_job_history_tests COMMAND nlsi_job_history_tests)
    if(Qt6_FOUND)
        get_filename_component(_qt_bin_dir "${Qt6_DIR}/../../../bin" ABSOLUTE)
    elseif(Qt5_FOUND)
        get_filename_component(_qt_bin_dir "${Qt5_DIR}/../../../bin" ABSOLUTE)
    endif()
    if(DEFINED _qt_bin_dir)
        set_tests_properties(nlsi_telemetry_tests PROPERTIES
            ENVIRONMENT "PATH=${_qt_bin_dir}"
        )
        set_tests_properties(nlsi_job_history_tests PROPERTIES
            ENVIRONMENT "PATH=${_qt_bin_dir}"
        )
    endif()

    if(BUILD_DASHBOARD)
        add_executable(nlsi_dashboard_ui_tests
            src/DashboardMainWindow.cpp
            src/shared/JobHistory.cpp
            src/shared/JobHistory.h
            tests/dashboard_ui_tests.cpp
        )
        target_include_directories(nlsi_dashboard_ui_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
        )
        target_compile_features(nlsi_dashboard_ui_tests PRIVATE cxx_std_17)
        target_link_libraries(nlsi_dashboard_ui_tests PRIVATE ${QT_LIBS} nlsi_ipc nlsi_display_formatting)
        add_test(NAME nlsi_dashboard_ui_tests COMMAND nlsi_dashboard_ui_tests)
        set_tests_properties(nlsi_dashboard_ui_tests PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${_qt_bin_dir}"
        )
    endif()
endif()

if(BUILD_DASHBOARD)
    message(STATUS "NLSI dashboard target: nlsi_dashboard")
else()
    message(STATUS "NLSI dashboard target disabled for plugin-only architecture build")
endif()
message(STATUS "NLSI plugin target: nlsi_scs_plugin")
message(STATUS "SDK root: ${NLSI_SDK_ROOT}")
