cmake_minimum_required(VERSION 3.10)

project(fcitx5-ngosen VERSION 1.11.0)
# The typing log names the version it came from.
add_definitions(-DNGOSEN_VERSION=\"${PROJECT_VERSION}\")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(BUILD_TESTING "Build the tests" OFF)
option(NGOSEN_CORE_ONLY "Build only the typing logic and its tests, without fcitx5" OFF)
option(NGOSEN_VENDORED_CRATES "Build the Rust core from bamboo-rs/vendor; off uses the crates cargo is configured with" ON)
if (NGOSEN_VENDORED_CRATES)
    set(NGOSEN_CARGO_LOCKED --locked)
    set(NGOSEN_CARGO_VENDOR_FILES Cargo.lock .cargo/config.toml)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bamboo-rs/vendor")
    message(WARNING "NGOSEN_VENDORED_CRATES is off but bamboo-rs/vendor exists: bamboo-rs/.cargo/config.toml still selects it. "
                    "Delete bamboo-rs/vendor and point that config at the system crates.")
endif()

if (NGOSEN_CORE_ONLY)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
    find_package(Pthread REQUIRED)
    add_subdirectory(bamboo-rs)
    add_subdirectory(src/core)
    enable_testing()
    add_subdirectory(test/core)
    return()
endif()

# The fcitx5 shell around the typing logic (src/core, built as ngosen_core). The addon entry point
# is excluded so the headless tests can link the same sources the module does.
set(ngosen_fcitx_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-candidates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-fcitx-host.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-fcitx-log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-program-name.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-icon-resolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-menu-icon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-plasma-theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-gnome-theme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-gnome-shell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-ui-text.cpp
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
# Missing when fcitx5 is built without D-Bus; ngosen-save-log then has nothing to call.
find_package(Fcitx5ModuleDBus)
if (Fcitx5ModuleDBus_FOUND)
    add_definitions(-DNGOSEN_HAS_DBUS)
    list(APPEND ngosen_fcitx_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/ngosen-dbus-object.cpp)
    set(NGOSEN_DBUS_MODULE Fcitx5::Module::DBus)
    # fcitx5 on sd-bus hands out sd_bus_message pointers, which tell a message's serial on fcitx5
    # versions that do not.
    get_target_property(NGOSEN_FCITX_UTILS_LIB Fcitx5::Utils LOCATION)
    if (NGOSEN_FCITX_UTILS_LIB AND EXISTS "${NGOSEN_FCITX_UTILS_LIB}")
        file(STRINGS "${NGOSEN_FCITX_UTILS_LIB}" NGOSEN_FCITX_SDBUS LIMIT_COUNT 1 REGEX "^libsystemd\\.so")
    endif()
    if (NGOSEN_FCITX_SDBUS)
        add_definitions(-DNGOSEN_FCITX_SDBUS)
    endif()
endif()
# Missing when fcitx5 is built without notifications; the "What's new" notice is then skipped.
find_package(Fcitx5ModuleNotifications)
if (Fcitx5ModuleNotifications_FOUND)
    add_definitions(-DNGOSEN_HAS_NOTIFICATIONS)
    set(NGOSEN_NOTIFICATIONS_MODULE Fcitx5::Module::Notifications)
endif()
# Missing when fcitx5 is built without its clipboard module; the convert hotkey then sees only
# selections the app reports.
find_package(Fcitx5ModuleClipboard)
if (Fcitx5ModuleClipboard_FOUND)
    add_definitions(-DNGOSEN_HAS_CLIPBOARD)
    set(NGOSEN_CLIPBOARD_MODULE Fcitx5::Module::Clipboard)
endif()
find_package(Pthread REQUIRED)
find_package(Gettext REQUIRED)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")
add_definitions(-DFCITX_GETTEXT_DOMAIN=\"fcitx5-ngosen\") 

if (Fcitx5Utils_VERSION VERSION_GREATER_EQUAL "5.1.13")
    add_definitions(-DNGOSEN_USE_MODERN_FCITX_API)
    message(STATUS "Fcitx5 version ${Fcitx5Utils_VERSION} detected: Using modern API")
else()
    message(STATUS "Fcitx5 version ${Fcitx5Utils_VERSION} detected: Using legacy API")
endif()
fcitx5_add_i18n_definition()

add_subdirectory(po)
add_subdirectory(bamboo-rs)
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(settings-gui)

if (BUILD_TESTING)
    include(CTest)
    add_subdirectory(test)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/io.github.ngosen.NgoSen.metainfo.xml.in.in"
    "${CMAKE_CURRENT_BINARY_DIR}/io.github.ngosen.NgoSen.metainfo.xml.in"
    @ONLY
)

fcitx5_translate_desktop_file(
    "${CMAKE_CURRENT_BINARY_DIR}/io.github.ngosen.NgoSen.metainfo.xml.in"
    io.github.ngosen.NgoSen.metainfo.xml
    XML
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/io.github.ngosen.NgoSen.metainfo.xml" DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

# Names the focused app for fcitx5 before 5.1.22 and delivers forwarded keys on GNOME 50.0-50.3.
install(DIRECTORY gnome-shell/forward-keys@ngosen.github.io DESTINATION ${CMAKE_INSTALL_DATADIR}/gnome-shell/extensions)

install(FILES 
        LICENSES/GPL-3.0-or-later.txt
        LICENSES/LGPL-2.1-or-later.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME}
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
