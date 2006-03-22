kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/kioslave/trash/ )


########### next target ###############

set(kfile_trash_PART_SRCS kfile_trash.cpp ${trashcommon_PART_SRCS})

kde4_automoc(${kfile_trash_PART_SRCS})

kde4_add_plugin(kfile_trash ${kfile_trash_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kfile_trash )

target_link_libraries(kfile_trash  ${KDE4_KDECORE_LIBS} kio ${QT_QT3SUPPORT_LIBRARY} DCOP )

install_targets(${LIB_INSTALL_DIR}/kde4 kfile_trash )


########### install files ###############

install_files( ${SERVICES_INSTALL_DIR} FILES kfile_trash.desktop kfile_trash_system.desktop )

kde4_footer()



#original Makefile.am contents follow:

### Makefile.am for trash file meta info plugin
#
#AM_CPPFLAGS      = $(all_includes)
#
#kde_module_LTLIBRARIES = kfile_trash.la
#
#kfile_trash_la_SOURCES = kfile_trash.cpp
#kfile_trash_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN)
#kfile_trash_la_LIBADD = ../libtrashcommon.la $(LIB_KIO)
#
#METASOURCES = AUTO
#
#services_DATA = kfile_trash.desktop kfile_trash_system.desktop
#servicesdir = $(kde_servicesdir)
