


########### install files ###############
file(GLOB providersfile *.desktop)
install( FILES ${providersfile} DESTINATION
${SERVICES_INSTALL_DIR}/searchproviders )

