# Reusable object library compiling the 7 core Lotus sources once per compiler build,
# shared across all headless integration test executables.
add_library(lotus_test_core OBJECT ${fcitx_lotus_core_sources})
set_target_properties(lotus_test_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(lotus_test_core PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_definitions(lotus_test_core PRIVATE
    FCITX5_LOTUS_SETTINGS_PATH="${CMAKE_INSTALL_FULL_BINDIR}/fcitx5-lotus-settings"
    FCITX_LOTUS_ICON_DIR="${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps"
)
target_link_libraries(lotus_test_core PRIVATE
    Fcitx5::Core
    Fcitx5::Config
    Fcitx5::Module::Emoji
    Bamboo::Core
    Pthread::Pthread
)

# Helper macro to compile standalone headless integration test linked with Lotus engine.
#
# Usage:
#   add_lotus_headless_test(<target_name> <source_file> "<labels>")
# Example:
#   add_lotus_headless_test(my_test my-test.cpp "integration;myfeature")
function(add_lotus_headless_test target source labels)
    add_executable(${target} ${source} $<TARGET_OBJECTS:lotus_test_core>)
    target_include_directories(${target} PRIVATE
        ${PROJECT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_definitions(${target} PRIVATE
        FCITX5_LOTUS_SETTINGS_PATH="${CMAKE_INSTALL_FULL_BINDIR}/fcitx5-lotus-settings"
        FCITX_LOTUS_ICON_DIR="${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps"
        LOTUS_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    )
    target_link_libraries(${target} PRIVATE
        Fcitx5::Core
        Fcitx5::Config
        Fcitx5::Module::Emoji
        Bamboo::Core
        Pthread::Pthread
    )
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}" TIMEOUT 20)
endfunction()

# Helper macro to compile lightweight unit test linked with Bamboo engine core.
function(add_bamboo_test target source labels)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Bamboo::Core)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}" TIMEOUT 20)
endfunction()

add_lotus_headless_test(preedit_lifecycle preedit-lifecycle.cpp "integration;preedit")
add_lotus_headless_test(app_rule_reset app-rule-reset.cpp "integration;apprule")
add_lotus_headless_test(uinput_mode_migration uinput-mode-migration.cpp "integration;mode")
add_lotus_headless_test(messenger_defaults messenger-defaults.cpp "integration;mode")
add_lotus_headless_test(preedit_smoke_client preedit-smoke-client.cpp "integration;preedit")
add_lotus_headless_test(preedit_smoke_server preedit-smoke-server.cpp "integration;preedit")
add_lotus_headless_test(smooth_buffered_key_replay
    smooth-buffered-key-replay.cpp "integration;smooth")
add_lotus_headless_test(held_key_repeat
    held-key-repeat.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_half_updated_snapshot
    super-smooth-half-updated-snapshot.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_half_updated_mid_word
    super-smooth-half-updated-mid-word.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_wait
    super-smooth-settle-wait.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_wait_immediate
    super-smooth-settle-wait-immediate.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_plain_field
    super-smooth-settle-plain-field.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_empty_composer
    super-smooth-settle-empty-composer.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_messenger_select_overtype
    super-smooth-messenger-select-overtype.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_first_word
    super-smooth-settle-first-word.cpp "integration;smooth")
add_lotus_headless_test(modifier_key_bypasses_surrounding_text
    modifier-key-bypasses-surrounding-text.cpp "integration;surrounding")
add_lotus_headless_test(emoji_history_page_navigation
    emoji-history-page-navigation.cpp "integration;emoji")
add_lotus_headless_test(property_invariants
    property-invariants.cpp "integration;property")
add_bamboo_test(numeric_macro_case_preservation
    numeric-macro-case-preservation.cpp "unit;macro")

add_executable(server_key_request server-key-request.cpp)
target_include_directories(server_key_request PRIVATE ${PROJECT_SOURCE_DIR}/server)
add_test(NAME server_key_request COMMAND server_key_request)
set_tests_properties(server_key_request PROPERTIES
    LABELS "unit;server"
    TIMEOUT 20
)

add_executable(emoji_backspace_utf8
    test-emoji-backspace.cpp
    ${PROJECT_SOURCE_DIR}/src/lotus-utils.cpp
)
target_include_directories(emoji_backspace_utf8 PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(emoji_backspace_utf8 PRIVATE
    Fcitx5::Core
)
add_test(NAME emoji_backspace_utf8 COMMAND emoji_backspace_utf8)
set_tests_properties(emoji_backspace_utf8 PROPERTIES
    LABELS "unit;emoji"
    TIMEOUT 20
)

add_executable(plasma_panel_theme
    plasma-panel-theme.cpp
    ${PROJECT_SOURCE_DIR}/src/lotus-plasma-theme.cpp
    ${PROJECT_SOURCE_DIR}/src/lotus-utils.cpp
)
target_include_directories(plasma_panel_theme PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(plasma_panel_theme PRIVATE
    Fcitx5::Core
    Fcitx5::Utils
)
add_test(NAME plasma_panel_theme COMMAND plasma_panel_theme)
set_tests_properties(plasma_panel_theme PROPERTIES
    LABELS "unit;icon"
    TIMEOUT 20
)

add_executable(gnome_panel_theme
    gnome-panel-theme.cpp
    ${PROJECT_SOURCE_DIR}/src/lotus-gnome-theme.cpp
    ${PROJECT_SOURCE_DIR}/src/lotus-utils.cpp
)
target_include_directories(gnome_panel_theme PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gnome_panel_theme PRIVATE
    Fcitx5::Core
    Fcitx5::Utils
)
add_test(NAME gnome_panel_theme COMMAND gnome_panel_theme)
set_tests_properties(gnome_panel_theme PROPERTIES
    LABELS "unit;icon"
    TIMEOUT 20
)
