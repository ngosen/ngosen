cmake_minimum_required(VERSION 3.10)

project(fcitx5-lotus VERSION 0.5.1)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(BUILD_TESTING "Build Lotus tests" OFF)
option(NGOSEN_RUST_CORE "Build the typing core from bamboo-rs/ (Rust) instead of the Go sources in bamboo/" OFF)
option(NGOSEN_CORE_ONLY "Build only the typing logic and its tests, without fcitx5" OFF)
option(NGOSEN_IBUS "Build the experimental IBus engine instead of the fcitx5 addon" OFF)

if (NGOSEN_CORE_ONLY OR NGOSEN_IBUS)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
    find_package(Pthread REQUIRED)
    if (NGOSEN_RUST_CORE)
        add_subdirectory(bamboo-rs)
    else()
        add_subdirectory(bamboo)
    endif()
    add_subdirectory(src/core)
    enable_testing()
    add_subdirectory(test/core)
    if (NGOSEN_IBUS)
        add_subdirectory(ibus)
        add_subdirectory(test/ibus)
    endif()
    return()
endif()

# The fcitx5 shell around the typing logic (src/core, built as ngosen_core). The addon entry point
# is excluded so the headless tests can link the same sources the module does.
set(fcitx_lotus_core_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lotus-engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lotus-candidates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lotus-utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-fcitx-host.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-fcitx-log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lotus-monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lotus-icon-resolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lotus-plasma-theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lotus-gnome-theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/emoji.cpp
)

set(REQUIRED_FCITX_VERSION 5.0.14)
find_package(ECM 1.0.0 REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(FeatureSummary)
include(GNUInstallDirs)
include(ECMUninstallTarget)

find_package(Fcitx5Core ${REQUIRED_FCITX_VERSION} REQUIRED)
find_package(Fcitx5ModuleEmoji REQUIRED)
find_package(Pthread REQUIRED)
find_package(Gettext REQUIRED)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")
add_definitions(-DFCITX_GETTEXT_DOMAIN=\"fcitx5-lotus\") 

if (Fcitx5Utils_VERSION VERSION_GREATER_EQUAL "5.1.13")
    add_definitions(-DLOTUS_USE_MODERN_FCITX_API)
    message(STATUS "Fcitx5 version ${Fcitx5Utils_VERSION} detected: Using modern API")
else()
    message(STATUS "Fcitx5 version ${Fcitx5Utils_VERSION} detected: Using legacy API")
endif()
fcitx5_add_i18n_definition()

add_subdirectory(po)
if (NGOSEN_RUST_CORE)
    add_subdirectory(bamboo-rs)
else()
    add_subdirectory(bamboo)
endif()
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(settings-gui)

if (BUILD_TESTING)
    include(CTest)
    add_subdirectory(test)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml.in.in"
    "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml.in"
    @ONLY
)

fcitx5_translate_desktop_file(
    "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml.in"
    org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml
    XML
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.Addon.Lotus.metainfo.xml" DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

# GNOME 50.0-50.3 drop keys an input method forwards; the user enables this extension there.
install(DIRECTORY gnome-shell/forward-keys@ngosen.github.io DESTINATION ${CMAKE_INSTALL_DATADIR}/gnome-shell/extensions)

install(FILES 
        LICENSES/GPL-3.0-or-later.txt
        LICENSES/LGPL-2.1-or-later.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME}
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
