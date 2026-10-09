# The IBus shell around the typing logic. Experimental: Telex and the Sen and Preedit modes only,
# configured through the fcitx5 version's settings file.
find_package(PkgConfig REQUIRED)
pkg_check_modules(IBus REQUIRED IMPORTED_TARGET ibus-1.0)
include(GNUInstallDirs)

set(NGOSEN_IBUS_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/ibus-ngosen")

add_library(ngosen_ibus STATIC
    ngosen-ibus-config.cpp
    ngosen-ibus-engine.cpp
    ngosen-ibus-host.cpp
    ngosen-ibus-log.cpp
    ngosen-ibus-resources.cpp
)
target_include_directories(ngosen_ibus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(ngosen_ibus PRIVATE NGOSEN_IBUS_DICTIONARY="${NGOSEN_IBUS_DATADIR}/vietnamese.cm.dict")
target_link_libraries(ngosen_ibus PUBLIC ngosen_core PkgConfig::IBus)

add_executable(ibus-engine-ngosen main.cpp)
target_compile_definitions(ibus-engine-ngosen PRIVATE NGOSEN_VERSION="${PROJECT_VERSION}")
target_link_libraries(ibus-engine-ngosen PRIVATE ngosen_ibus)

set(NGOSEN_IBUS_EXEC "${CMAKE_INSTALL_FULL_LIBEXECDIR}/ibus-engine-ngosen")
configure_file(ngosen.xml.in ${CMAKE_CURRENT_BINARY_DIR}/ngosen.xml @ONLY)

install(TARGETS ibus-engine-ngosen DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ngosen.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/ibus/component)
install(FILES ${PROJECT_SOURCE_DIR}/data/dictionaries/vietnamese.cm.dict DESTINATION ${NGOSEN_IBUS_DATADIR})
