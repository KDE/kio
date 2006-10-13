
########### kfile_trash ###############

set(kfile_trash_PART_SRCS kfile_trash.cpp ${trashcommon_PART_SRCS})

kde4_automoc(kfile_trash ${kfile_trash_PART_SRCS})

kde4_add_plugin(kfile_trash ${kfile_trash_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kfile_trash )

target_link_libraries(kfile_trash  ${KDE4_KIO_LIBS} ${QT_QT3SUPPORT_LIBRARY} )

install(TARGETS kfile_trash  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES kfile_trash.desktop kfile_trash_system.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
