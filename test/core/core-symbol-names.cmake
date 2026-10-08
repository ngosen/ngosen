# Fails when the typing logic library defines a symbol in the fcitx namespace or with a lotus name.
# Run as: cmake -DNM=<nm> -DLIB=<libngosen_core.a> -P core-symbol-names.cmake
execute_process(COMMAND ${NM} --defined-only --demangle ${LIB} OUTPUT_VARIABLE symbols RESULT_VARIABLE result)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "nm failed on ${LIB}")
endif()
string(REGEX MATCHALL "[^\n]*(fcitx::|[Ll]otus)[^\n]*" bad "${symbols}")
if (bad)
    string(REPLACE ";" "\n" bad "${bad}")
    message(FATAL_ERROR "Typing logic symbols with fcitx or lotus names:\n${bad}")
endif()
