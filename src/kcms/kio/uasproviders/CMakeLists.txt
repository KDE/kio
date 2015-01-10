FILE(GLOB useragents *.desktop)
install(FILES ${useragents} DESTINATION
    ${KDE_INSTALL_KSERVICES5DIR}/useragentstrings)
