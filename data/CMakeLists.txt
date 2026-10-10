install(DIRECTORY icons/ DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/"
    PATTERN .* EXCLUDE
    PATTERN *~ EXCLUDE)

# ── Status/tray icon coverage: hicolor status/ ────────────────────────────
# Engine resolves icons to absolute paths (see ngosen-engine.cpp
# subModeIconImpl) to bypass XDG icon theme lookup on non-Breeze themes.
# Install copies of every variant into hicolor status dirs (SVG + PNG).
set(NGOSEN_STATUS_ICONS
    fcitx-ngosen fcitx-ngosen-off fcitx-ngosen-emoji
    fcitx-ngosen-default fcitx-ngosen-default-black
    fcitx-ngosen-off-default fcitx-ngosen-off-default-black
    fcitx-ngosen-emoji-default fcitx-ngosen-emoji-default-black)

foreach(ICON ${NGOSEN_STATUS_ICONS})
    # status/ — system tray (SVG, renderable by most compositors)
    install(FILES icons/hicolor/scalable/apps/${ICON}.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/status"
        RENAME "${ICON}.svg")
endforeach()

# PNG tray icons: 22/24px status PNGs per icon (build-time, rsvg-convert).
# PNG is only a raster fallback; the resolver prefers SVG.
find_program(RSVG_CONVERT rsvg-convert REQUIRED)
set(NGOSEN_PNG_OUTPUTS)

foreach(ICON ${NGOSEN_STATUS_ICONS})
    set(ICON_SVG "${CMAKE_CURRENT_SOURCE_DIR}/icons/hicolor/scalable/apps/${ICON}.svg")

    foreach(SIZE 22 24)
        set(OUT "${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${SIZE}x${SIZE}/status/${ICON}.png")
        add_custom_command(OUTPUT ${OUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${SIZE}x${SIZE}/status"
            COMMAND ${RSVG_CONVERT} -w ${SIZE} -h ${SIZE} ${ICON_SVG} -o ${OUT}
            DEPENDS ${ICON_SVG}
            COMMENT "Generating ${SIZE}x${SIZE} status icon for ${ICON}")
        install(FILES ${OUT}
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/status")
        list(APPEND NGOSEN_PNG_OUTPUTS ${OUT})
    endforeach()
endforeach()

add_custom_target(ngosen_icons ALL DEPENDS ${NGOSEN_PNG_OUTPUTS})

install(FILES dictionaries/vietnamese.cm.dict DESTINATION "${FCITX_INSTALL_PKGDATADIR}/ngosen")
