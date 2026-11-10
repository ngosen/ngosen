install(DIRECTORY icons/ DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/"
    PATTERN .* EXCLUDE
    PATTERN *~ EXCLUDE)

# ── Status/tray icon coverage: hicolor status/ ────────────────────────────
# Engine resolves icons to absolute paths (see ngosen-engine.cpp
# subModeIconImpl) to bypass XDG icon theme lookup on non-Breeze themes.
# Install copies of every variant into hicolor status dirs (SVG + PNG).
set(NGOSEN_TILE_ICONS fcitx-ngosen fcitx-ngosen-off fcitx-ngosen-emoji)
set(NGOSEN_STATUS_ICONS
    ${NGOSEN_TILE_ICONS}
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

function(ngosen_status_pngs NAME SVG)
    foreach(SIZE 22 24)
        set(DIR "${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${SIZE}x${SIZE}/status")
        add_custom_command(OUTPUT ${DIR}/${NAME}.png
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DIR}
            COMMAND ${RSVG_CONVERT} -w ${SIZE} -h ${SIZE} ${SVG} -o ${DIR}/${NAME}.png
            DEPENDS ${SVG}
            COMMENT "Generating ${SIZE}x${SIZE} status icon for ${NAME}")
        install(FILES ${DIR}/${NAME}.png
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/status")
        list(APPEND NGOSEN_PNG_OUTPUTS ${DIR}/${NAME}.png)
    endforeach()
    set(NGOSEN_PNG_OUTPUTS ${NGOSEN_PNG_OUTPUTS} PARENT_SCOPE)
endfunction()

foreach(ICON ${NGOSEN_STATUS_ICONS})
    ngosen_status_pngs(${ICON} "${CMAKE_CURRENT_SOURCE_DIR}/icons/hicolor/scalable/apps/${ICON}.svg")
endforeach()

# CMake has no floating-point maths, so the centre must come out whole.
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

# Plasma ignores the icon path the engine sends and loads the Breeze icon of the same name.
foreach(ICON ${NGOSEN_TILE_ICONS})
    set(OUT "${CMAKE_CURRENT_BINARY_DIR}/icons/large/${ICON}-large.svg")
    ngosen_scaled_icon("${CMAKE_CURRENT_SOURCE_DIR}/icons/hicolor/scalable/apps/${ICON}.svg" 1.25 ${OUT})
    install(FILES ${OUT} DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/status")
    install(FILES ${OUT} DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
    foreach(THEME breeze breeze-dark)
        foreach(SIZE 22 24)
            install(FILES ${OUT} DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/${THEME}/status/${SIZE}")
        endforeach()
    endforeach()
    ngosen_status_pngs(${ICON}-large ${OUT})
endforeach()

add_custom_target(ngosen_icons ALL DEPENDS ${NGOSEN_PNG_OUTPUTS})

install(FILES dictionaries/vietnamese.cm.dict DESTINATION "${FCITX_INSTALL_PKGDATADIR}/ngosen")
# fcitx5 adds this input method to the first profile it builds on a Vietnamese desktop.
install(FILES default/vi_VN DESTINATION "${FCITX_INSTALL_PKGDATADIR}/default")
