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
    # An empty DISPLAY keeps XTEST off, so no test presses keys in the developer's X session.
    set_tests_properties(${target} PROPERTIES LABELS "${labels}" TIMEOUT 20 ENVIRONMENT "DISPLAY=")
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
add_lotus_headless_test(ctx_rule_cleanup ctx-rule-cleanup.cpp "integration;apprule")
add_lotus_headless_test(gnome_shared_ibus_context gnome-shared-ibus-context.cpp "integration;apprule;gnome")
add_lotus_headless_test(mode_name_migration mode-name-migration.cpp "integration;mode")
add_lotus_headless_test(messenger_defaults messenger-defaults.cpp "integration;mode")
add_lotus_headless_test(options_sync options-sync.cpp "integration;config")
add_lotus_headless_test(host_field_mapping host-field-mapping.cpp "unit;host")
add_lotus_headless_test(custom_keymap custom-keymap.cpp "integration;keymap")
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
add_lotus_headless_test(surrounding_wait_event
    surrounding-wait-event.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_wait
    super-smooth-settle-wait.cpp "integration;smooth")
add_lotus_headless_test(wayland_forward_backspaces
    wayland-forward-backspaces.cpp "integration;smooth")
add_lotus_headless_test(xim_key_handback
    xim-key-handback.cpp "integration;smooth")
add_lotus_headless_test(xim_forward_handback
    xim-forward-handback.cpp "integration;smooth")
add_lotus_headless_test(ibus_dbus_forward_backspaces
    ibus-dbus-forward-backspaces.cpp "integration;smooth")
add_lotus_headless_test(cursor_jump_reset
    cursor-jump-reset.cpp "integration;surrounding")
add_lotus_headless_test(x11_pointer_click
    x11-pointer-click.cpp "unit;x11")
add_lotus_headless_test(x11_xtest_replacement
    x11-xtest-replacement.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_wait_immediate
    super-smooth-settle-wait-immediate.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_plain_field
    super-smooth-settle-plain-field.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_empty_composer
    super-smooth-settle-empty-composer.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_messenger_select_overtype
    super-smooth-messenger-select-overtype.cpp "integration;smooth")
add_lotus_headless_test(replay_single_commit
    replay-single-commit.cpp "integration;smooth")
add_lotus_headless_test(super_smooth_settle_first_word
    super-smooth-settle-first-word.cpp "integration;smooth")
add_lotus_headless_test(emoji_candidate_keys emoji-candidate-keys.cpp "integration;emoji")
add_lotus_headless_test(emoji_history_page_navigation
    emoji-history-page-navigation.cpp "integration;emoji")
add_lotus_headless_test(property_invariants
    property-invariants.cpp "integration;property")
add_bamboo_test(numeric_macro_case_preservation
    numeric-macro-case-preservation.cpp "unit;macro")

add_executable(emoji_backspace_utf8
    test-emoji-backspace.cpp
    ${PROJECT_SOURCE_DIR}/src/ngosen-utf8.cpp
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

# Compares the typing logic's own UTF-8 helpers with the fcitx5 ones they replace.
add_executable(utf8_matches_fcitx utf8-matches-fcitx.cpp ${PROJECT_SOURCE_DIR}/src/ngosen-utf8.cpp)
target_include_directories(utf8_matches_fcitx PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(utf8_matches_fcitx PRIVATE Fcitx5::Utils)
add_test(NAME utf8_matches_fcitx COMMAND utf8_matches_fcitx)
set_tests_properties(utf8_matches_fcitx PROPERTIES LABELS "unit" TIMEOUT 60)
