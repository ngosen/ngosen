add_executable(ibus_shell ibus-shell.cpp)
target_link_libraries(ibus_shell PRIVATE ngosen_ibus)
add_test(NAME ibus_shell COMMAND ibus_shell)
set_tests_properties(ibus_shell PROPERTIES LABELS "unit;ibus" TIMEOUT 60)
