add_subdirectory(core)

add_library(ngosen MODULE ngosen.cpp ${ngosen_fcitx_sources})
set_target_properties(ngosen PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_options(ngosen PRIVATE "-Wl,-exclude-libs,ALL")
target_link_libraries(ngosen
	ngosen_core
	Fcitx5::Core
	Fcitx5::Config
  Fcitx5::Module::Emoji
	Bamboo::Core
	Pthread::Pthread
)

target_include_directories(ngosen PRIVATE
    ${PROJECT_BINARY_DIR}
)
target_compile_definitions(ngosen PRIVATE
    NGOSEN_SETTINGS_PATH=\"${CMAKE_INSTALL_FULL_BINDIR}/fcitx5-ngosen-settings\"
    NGOSEN_ICON_DIR=\"${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps\"
)
install(TARGETS ngosen DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")

fcitx5_translate_desktop_file(ngosen.conf.in ngosen.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ngosen.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod")
configure_file(ngosen-addon.conf.in.in ngosen-addon.conf.in)
fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/ngosen-addon.conf.in" ngosen-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ngosen-addon.conf" RENAME ngosen.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

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
