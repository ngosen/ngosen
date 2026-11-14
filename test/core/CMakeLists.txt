# Tests of the typing logic alone, run against a fake app field. They build without fcitx5.
# An empty DISPLAY keeps the X server of the session running the tests out of them.
add_executable(core_typing core-typing.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_typing PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    NGOSEN_TEST_ENGLISH_WORDS="${PROJECT_SOURCE_DIR}/data/english-common.txt"
    NGOSEN_TEST_KEEP_WORDS="${PROJECT_SOURCE_DIR}/data/keep-words.txt")
target_link_libraries(core_typing PRIVATE ngosen_core)
add_test(NAME core_typing COMMAND core_typing)
set_tests_properties(core_typing PROPERTIES LABELS "unit;core" TIMEOUT 240 ENVIRONMENT "DISPLAY=")

add_executable(core_replay_fold core-replay-fold.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_replay_fold PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    NGOSEN_TEST_ENGLISH_WORDS="${PROJECT_SOURCE_DIR}/data/english-common.txt"
    NGOSEN_TEST_KEEP_WORDS="${PROJECT_SOURCE_DIR}/data/keep-words.txt")
target_link_libraries(core_replay_fold PRIVATE ngosen_core)
add_test(NAME core_replay_fold COMMAND core_replay_fold)
set_tests_properties(core_replay_fold PROPERTIES LABELS "unit;core" TIMEOUT 120 ENVIRONMENT "DISPLAY=")

add_executable(core_fuzz core-fuzz.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_fuzz PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    NGOSEN_TEST_ENGLISH_WORDS="${PROJECT_SOURCE_DIR}/data/english-common.txt"
    NGOSEN_TEST_KEEP_WORDS="${PROJECT_SOURCE_DIR}/data/keep-words.txt")
target_link_libraries(core_fuzz PRIVATE ngosen_core)
add_test(NAME core_fuzz COMMAND core_fuzz)
set_tests_properties(core_fuzz PROPERTIES LABELS "unit;core" TIMEOUT 120 ENVIRONMENT "DISPLAY=")

add_executable(core_replay core-replay.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_replay PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    NGOSEN_TEST_ENGLISH_WORDS="${PROJECT_SOURCE_DIR}/data/english-common.txt"
    NGOSEN_TEST_KEEP_WORDS="${PROJECT_SOURCE_DIR}/data/keep-words.txt")
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
target_compile_definitions(core_retype PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    NGOSEN_TEST_ENGLISH_WORDS="${PROJECT_SOURCE_DIR}/data/english-common.txt"
    NGOSEN_TEST_KEEP_WORDS="${PROJECT_SOURCE_DIR}/data/keep-words.txt")
target_link_libraries(core_retype PRIVATE ngosen_core)
add_test(NAME core_retype COMMAND core_retype)
set_tests_properties(core_retype PROPERTIES LABELS "unit;core" TIMEOUT 60 ENVIRONMENT "DISPLAY=")

add_executable(core_retone core-retone.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_retone PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    NGOSEN_TEST_ENGLISH_WORDS="${PROJECT_SOURCE_DIR}/data/english-common.txt"
    NGOSEN_TEST_KEEP_WORDS="${PROJECT_SOURCE_DIR}/data/keep-words.txt")
target_link_libraries(core_retone PRIVATE ngosen_core)
add_test(NAME core_retone COMMAND core_retone)
set_tests_properties(core_retone PROPERTIES LABELS "unit;core" TIMEOUT 120 ENVIRONMENT "DISPLAY=")

add_executable(core_raw_word core-raw-word.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_raw_word PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    NGOSEN_TEST_ENGLISH_WORDS="${PROJECT_SOURCE_DIR}/data/english-common.txt"
    NGOSEN_TEST_KEEP_WORDS="${PROJECT_SOURCE_DIR}/data/keep-words.txt")
target_link_libraries(core_raw_word PRIVATE ngosen_core)
add_test(NAME core_raw_word COMMAND core_raw_word)
set_tests_properties(core_raw_word PROPERTIES LABELS "unit;core" TIMEOUT 120 ENVIRONMENT "DISPLAY=")

add_executable(core_vim core-vim.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_vim PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    NGOSEN_TEST_ENGLISH_WORDS="${PROJECT_SOURCE_DIR}/data/english-common.txt"
    NGOSEN_TEST_KEEP_WORDS="${PROJECT_SOURCE_DIR}/data/keep-words.txt")
target_link_libraries(core_vim PRIVATE ngosen_core)
add_test(NAME core_vim COMMAND core_vim)
set_tests_properties(core_vim PROPERTIES LABELS "unit;core" TIMEOUT 60 ENVIRONMENT "DISPLAY=")

add_executable(core_convert core-convert.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_convert PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict"
    NGOSEN_TEST_ENGLISH_WORDS="${PROJECT_SOURCE_DIR}/data/english-common.txt"
    NGOSEN_TEST_KEEP_WORDS="${PROJECT_SOURCE_DIR}/data/keep-words.txt")
target_link_libraries(core_convert PRIVATE ngosen_core)
add_test(NAME core_convert COMMAND core_convert)
set_tests_properties(core_convert PROPERTIES LABELS "unit;core" TIMEOUT 30 ENVIRONMENT "DISPLAY=")

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

add_executable(core_pty core-pty.cpp)
target_link_libraries(core_pty PRIVATE ngosen_core util)
add_test(NAME core_pty COMMAND core_pty)
set_tests_properties(core_pty PROPERTIES LABELS "unit;core" TIMEOUT 30)

# How each replacement in these tests deletes and ends, so code that moves them can show it changes none.
foreach (name core_typing core_fuzz core_retype core_retone)
    add_test(NAME ${name}_record
        COMMAND ${CMAKE_COMMAND} -DNAME=${name} -DBIN=$<TARGET_FILE:${name}> -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/replace-record/${name}.txt -P ${CMAKE_CURRENT_SOURCE_DIR}/replace-record.cmake)
    set_tests_properties(${name}_record PROPERTIES LABELS "unit;core" TIMEOUT 240)
endforeach ()
