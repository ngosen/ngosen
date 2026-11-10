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

# Writes OUT: SOURCE drawn SCALE times larger around the centre of its viewBox.
function(ngosen_scaled_icon SOURCE SCALE OUT)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SOURCE})
    file(READ ${SOURCE} SVG)
    if(NOT SVG MATCHES "viewBox=\"(-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\"")
        message(FATAL_ERROR "${SOURCE}: expected a whole-number viewBox")
    endif()
    math(EXPR CX "(2 * ${CMAKE_MATCH_1} + ${CMAKE_MATCH_3}) / 2")
    math(EXPR CY "(2 * ${CMAKE_MATCH_2} + ${CMAKE_MATCH_4}) / 2")
    string(REGEX REPLACE "(<svg[^>]*>)" "\\1<g transform=\"translate(${CX} ${CY}) scale(${SCALE}) translate(-${CX} -${CY})\">" SVG "${SVG}")
    string(REPLACE "</svg>" "</g></svg>" SVG "${SVG}")
    file(WRITE ${OUT} "${SVG}")
endfunction()

# GNOME's top bar fits icons to a 16 px box that its own icons nearly fill, so the margin that
# matches other trays leaves Ngó Sen's tile smaller there.
foreach(ICON ${NGOSEN_STATUS_ICONS})
    set(OUT "${CMAKE_CURRENT_BINARY_DIR}/icons/gnome/${ICON}-gnome.svg")
    ngosen_scaled_icon("${CMAKE_CURRENT_SOURCE_DIR}/icons/hicolor/scalable/apps/${ICON}.svg" 1.3 ${OUT})
    install(FILES ${OUT} DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/status")
endforeach()

# The tiles for TrayIconSize=Large. Plasma takes a tray icon from the Breeze theme by name, so the
# Breeze folders get them too.
set(NGOSEN_LARGE_PNG_OUTPUTS)
foreach(ICON fcitx-ngosen fcitx-ngosen-off fcitx-ngosen-emoji)
    set(OUT "${CMAKE_CURRENT_BINARY_DIR}/icons/large/${ICON}-large.svg")
    ngosen_scaled_icon("${CMAKE_CURRENT_SOURCE_DIR}/icons/hicolor/scalable/apps/${ICON}.svg" 1.25 ${OUT})
    install(FILES ${OUT} DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/status")
    install(FILES ${OUT} DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
    foreach(THEME breeze breeze-dark)
        foreach(SIZE 22 24)
            install(FILES ${OUT} DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/${THEME}/status/${SIZE}")
        endforeach()
    endforeach()
    foreach(SIZE 22 24)
        set(PNG "${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${SIZE}x${SIZE}/status/${ICON}-large.png")
        add_custom_command(OUTPUT ${PNG}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${SIZE}x${SIZE}/status"
            COMMAND ${RSVG_CONVERT} -w ${SIZE} -h ${SIZE} ${OUT} -o ${PNG}
            DEPENDS ${OUT}
            COMMENT "Generating ${SIZE}x${SIZE} status icon for ${ICON}-large")
        install(FILES ${PNG} DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/status")
        list(APPEND NGOSEN_LARGE_PNG_OUTPUTS ${PNG})
    endforeach()
endforeach()
add_custom_target(ngosen_large_icons ALL DEPENDS ${NGOSEN_LARGE_PNG_OUTPUTS})

install(FILES dictionaries/vietnamese.cm.dict DESTINATION "${FCITX_INSTALL_PKGDATADIR}/ngosen")
# fcitx5 adds this input method to the first profile it builds on a Vietnamese desktop.
install(FILES default/vi_VN DESTINATION "${FCITX_INSTALL_PKGDATADIR}/default")
