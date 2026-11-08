# Tests of the typing logic alone, run against a fake app field. They build without fcitx5.
# An empty DISPLAY keeps the X server of the session running the tests out of them.
add_executable(core_typing core-typing.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_typing PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict")
target_link_libraries(core_typing PRIVATE ngosen_core)
add_test(NAME core_typing COMMAND core_typing)
set_tests_properties(core_typing PROPERTIES LABELS "unit;core" TIMEOUT 240 ENVIRONMENT "DISPLAY=")

add_executable(core_fuzz core-fuzz.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_fuzz PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict")
target_link_libraries(core_fuzz PRIVATE ngosen_core)
add_test(NAME core_fuzz COMMAND core_fuzz)
set_tests_properties(core_fuzz PROPERTIES LABELS "unit;core" TIMEOUT 120 ENVIRONMENT "DISPLAY=")

add_executable(core_replay core-replay.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_replay PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict")
target_link_libraries(core_replay PRIVATE ngosen_core)
add_test(NAME core_replay COMMAND core_replay ${CMAKE_CURRENT_SOURCE_DIR}/replay-sample.txt)
set_tests_properties(core_replay PROPERTIES LABELS "unit;core" TIMEOUT 60 ENVIRONMENT "DISPLAY=")
# A key rolled over in a slow app: our commit lands while the app still reports the old text.
add_test(NAME core_replay_slow_app COMMAND core_replay ${CMAKE_CURRENT_SOURCE_DIR}/replay-slow-app.txt)
set_tests_properties(core_replay_slow_app PROPERTIES LABELS "unit;core" TIMEOUT 60 ENVIRONMENT "NGOSEN_REPLAY_LAG_MS=30;DISPLAY=")

add_test(NAME core_symbol_names
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DLIB=$<TARGET_FILE:ngosen_core> -P ${CMAKE_CURRENT_SOURCE_DIR}/core-symbol-names.cmake)
set_tests_properties(core_symbol_names PROPERTIES LABELS "unit;core" TIMEOUT 60)

add_executable(core_retype core-retype.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_retype PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict")
target_link_libraries(core_retype PRIVATE ngosen_core)
add_test(NAME core_retype COMMAND core_retype)
set_tests_properties(core_retype PROPERTIES LABELS "unit;core" TIMEOUT 60 ENVIRONMENT "DISPLAY=")

add_executable(core_retone core-retone.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_retone PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict")
target_link_libraries(core_retone PRIVATE ngosen_core)
add_test(NAME core_retone COMMAND core_retone)
set_tests_properties(core_retone PROPERTIES LABELS "unit;core" TIMEOUT 120 ENVIRONMENT "DISPLAY=")

add_executable(core_surrounding_count core-surrounding-count.cpp)
target_link_libraries(core_surrounding_count PRIVATE ngosen_core)
add_test(NAME core_surrounding_count COMMAND core_surrounding_count)
set_tests_properties(core_surrounding_count PROPERTIES LABELS "unit;core" TIMEOUT 30)

add_executable(core_gnome_apps core-gnome-apps.cpp)
target_link_libraries(core_gnome_apps PRIVATE ngosen_core)
add_test(NAME core_gnome_apps COMMAND core_gnome_apps)
set_tests_properties(core_gnome_apps PROPERTIES LABELS "unit;core" TIMEOUT 30)

add_executable(core_flatpak_names core-flatpak-names.cpp)
target_link_libraries(core_flatpak_names PRIVATE ngosen_core)
add_test(NAME core_flatpak_names COMMAND core_flatpak_names)
set_tests_properties(core_flatpak_names PROPERTIES LABELS "unit;core" TIMEOUT 30)

# How each replacement in these tests deletes and ends, so code that moves them can show it changes none.
foreach (name core_typing core_fuzz core_retype core_retone)
    add_test(NAME ${name}_record
        COMMAND ${CMAKE_COMMAND} -DNAME=${name} -DBIN=$<TARGET_FILE:${name}> -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/replace-record/${name}.txt -P ${CMAKE_CURRENT_SOURCE_DIR}/replace-record.cmake)
    set_tests_properties(${name}_record PROPERTIES LABELS "unit;core" TIMEOUT 240)
endforeach ()
