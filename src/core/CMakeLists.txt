# The typing logic, shared by every input method framework shell. It must not use fcitx5: the
# library gets no fcitx5 include path, so an fcitx5 include here fails the build.
add_library(ngosen_core STATIC
    ngosen-state.cpp
    ngosen-app-quirks.cpp
    ngosen-globals.cpp
    ngosen-strings.cpp
    ngosen-utf8.cpp
    ngosen-xtest.cpp
)
set_target_properties(ngosen_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ngosen_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# The shell provides ngosen::writeLog and ngosen::logEnabled (ngosen-log.h).
target_link_libraries(ngosen_core PUBLIC
    Bamboo::Core
    Pthread::Pthread
    ${CMAKE_DL_LIBS}
)
