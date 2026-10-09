add_executable(ibus_shell ibus-shell.cpp)
target_link_libraries(ibus_shell PRIVATE ngosen_ibus)
add_test(NAME ibus_shell COMMAND ibus_shell)
set_tests_properties(ibus_shell PROPERTIES LABELS "unit;ibus" TIMEOUT 60)

# Needs ibus-daemon, dbus-run-session and Python's IBus bindings; skipped without them.
find_program(IBUS_DAEMON ibus-daemon)
find_program(DBUS_RUN_SESSION dbus-run-session)
find_package(Python3 COMPONENTS Interpreter)
if (IBUS_DAEMON AND DBUS_RUN_SESSION AND Python3_FOUND)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import gi; gi.require_version('IBus', '1.0'); from gi.repository import IBus"
                    RESULT_VARIABLE NGOSEN_HAVE_PYTHON_IBUS OUTPUT_QUIET ERROR_QUIET)
endif()
if (IBUS_DAEMON AND DBUS_RUN_SESSION AND NGOSEN_HAVE_PYTHON_IBUS EQUAL 0)
    set(NGOSEN_IBUS_EXEC $<TARGET_FILE:ibus-engine-ngosen>)
    set(NGOSEN_TEST_COMPONENT_DIR ${CMAKE_CURRENT_BINARY_DIR}/component)
    configure_file(${PROJECT_SOURCE_DIR}/ibus/ngosen.xml.in ${CMAKE_CURRENT_BINARY_DIR}/ngosen.xml.in @ONLY)
    file(GENERATE OUTPUT ${NGOSEN_TEST_COMPONENT_DIR}/ngosen.xml INPUT ${CMAKE_CURRENT_BINARY_DIR}/ngosen.xml.in)
    add_test(NAME ibus_daemon_typing
             COMMAND ${DBUS_RUN_SESSION} -- sh ${CMAKE_CURRENT_SOURCE_DIR}/run-with-daemon.sh ${NGOSEN_TEST_COMPONENT_DIR}
                     ${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict ${CMAKE_CURRENT_SOURCE_DIR}/ibus-daemon-typing.py)
    set_tests_properties(ibus_daemon_typing PROPERTIES LABELS "integration;ibus" TIMEOUT 120)
else()
    message(STATUS "ibus_daemon_typing skipped: needs ibus-daemon, dbus-run-session and python3-gi with IBus")
endif()
