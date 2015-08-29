########### install files ###############

install(FILES
    linkProgram.desktop
    linkURL.desktop
    linkPath.desktop
    Directory.desktop
    TextFile.desktop
    HTMLFile.desktop
    DESTINATION ${KDE_INSTALL_TEMPLATEDIR}
)


install(FILES
    HTMLFile.html
    Program.desktop
    TextFile.txt
    URL.desktop
    DESTINATION ${KDE_INSTALL_TEMPLATEDIR}/.source
)
