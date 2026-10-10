# Tests of the typing logic alone, run against a fake app field. They build without fcitx5.
add_executable(core_typing core-typing.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_typing PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict")
target_link_libraries(core_typing PRIVATE ngosen_core)
add_test(NAME core_typing COMMAND core_typing)
set_tests_properties(core_typing PROPERTIES LABELS "unit;core" TIMEOUT 60)

add_executable(core_fuzz core-fuzz.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_fuzz PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict")
target_link_libraries(core_fuzz PRIVATE ngosen_core)
add_test(NAME core_fuzz COMMAND core_fuzz)
set_tests_properties(core_fuzz PROPERTIES LABELS "unit;core" TIMEOUT 120)

add_executable(core_replay core-replay.cpp fake-app.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_replay PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict")
target_link_libraries(core_replay PRIVATE ngosen_core)
add_test(NAME core_replay COMMAND core_replay ${CMAKE_CURRENT_SOURCE_DIR}/replay-sample.txt)
set_tests_properties(core_replay PROPERTIES LABELS "unit;core" TIMEOUT 60)
# A key rolled over in a slow app: our commit lands while the app still reports the old text.
add_test(NAME core_replay_slow_app COMMAND core_replay ${CMAKE_CURRENT_SOURCE_DIR}/replay-slow-app.txt)
set_tests_properties(core_replay_slow_app PROPERTIES LABELS "unit;core" TIMEOUT 60 ENVIRONMENT "NGOSEN_REPLAY_LAG_MS=30")

add_test(NAME core_symbol_names
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DLIB=$<TARGET_FILE:ngosen_core> -P ${CMAKE_CURRENT_SOURCE_DIR}/core-symbol-names.cmake)
set_tests_properties(core_symbol_names PROPERTIES LABELS "unit;core" TIMEOUT 60)
