


########### install files ###############
file(GLOB providersfile *.desktop)
install( FILES ${providersfile} DESTINATION
${KDE_INSTALL_KSERVICESDIR}/searchproviders )

