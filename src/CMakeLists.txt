add_subdirectory(core)

add_library(lotus MODULE lotus.cpp ${fcitx_lotus_core_sources})
set_target_properties(lotus PROPERTIES OUTPUT_NAME "ngosen")
set_target_properties(lotus PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_options(lotus PRIVATE "-Wl,-exclude-libs,ALL")
target_link_libraries(lotus
	ngosen_core
	Fcitx5::Core
	Fcitx5::Config
  Fcitx5::Module::Emoji
	Bamboo::Core
	Pthread::Pthread
)

target_include_directories(lotus PRIVATE
    ${PROJECT_BINARY_DIR}
)
target_compile_definitions(lotus PRIVATE
    FCITX5_LOTUS_SETTINGS_PATH=\"${CMAKE_INSTALL_FULL_BINDIR}/fcitx5-ngosen-settings\"
    FCITX_LOTUS_ICON_DIR=\"${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps\"
)
install(TARGETS lotus DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")

fcitx5_translate_desktop_file(lotus.conf.in lotus.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lotus.conf" RENAME ngosen.conf DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod")
configure_file(lotus-addon.conf.in.in lotus-addon.conf.in)
fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/lotus-addon.conf.in" lotus-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lotus-addon.conf" RENAME ngosen.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

add_library(ngosen_migration STATIC ngosen-migration.cpp)
set_target_properties(ngosen_migration PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(ngosen_migration PUBLIC Fcitx5::Config)
target_include_directories(ngosen_migration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Not on demand, so it runs before fcitx5 reads its profile.
add_library(ngosenmigrate MODULE ngosen-migrate-addon.cpp)
target_link_options(ngosenmigrate PRIVATE "-Wl,-exclude-libs,ALL")
target_link_libraries(ngosenmigrate PRIVATE ngosen_migration Fcitx5::Core)
install(TARGETS ngosenmigrate DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
configure_file(ngosenmigrate.conf.in ngosenmigrate.conf @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ngosenmigrate.conf" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
