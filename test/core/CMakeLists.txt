# Tests of the typing logic alone, run against a fake app field. They build without fcitx5.
add_executable(core_typing core-typing.cpp fake-host.cpp fake-engine.cpp)
target_compile_definitions(core_typing PRIVATE NGOSEN_TEST_DICTIONARY="${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict")
target_link_libraries(core_typing PRIVATE ngosen_core)
add_test(NAME core_typing COMMAND core_typing)
set_tests_properties(core_typing PROPERTIES LABELS "unit;core" TIMEOUT 60)

add_test(NAME core_symbol_names
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DLIB=$<TARGET_FILE:ngosen_core> -P ${CMAKE_CURRENT_SOURCE_DIR}/core-symbol-names.cmake)
set_tests_properties(core_symbol_names PROPERTIES LABELS "unit;core" TIMEOUT 60)
