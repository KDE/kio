
project(kioslave-trash)



set (trashcommon_PART_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/trashimpl.cpp)

########### next target ###############

set(kio_trash_PART_SRCS kio_trash.cpp ${trashcommon_PART_SRCS})

kde4_automoc(${kio_trash_PART_SRCS})

kde4_add_plugin(kio_trash ${kio_trash_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kio_trash )

target_link_libraries(kio_trash  ${KDE4_KIO_LIBS})

install(TARGETS kio_trash  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### next target ###############

set(ktrash_SRCS ktrash.cpp )

kde4_automoc(${ktrash_SRCS})

kde4_add_executable(ktrash ${ktrash_SRCS})

target_link_libraries(ktrash  ${KDE4_KIO_LIBS} )

install(TARGETS ktrash DESTINATION bin)

########### next target ###############

if(KDE4_BUILD_TESTS)

  # Tests don't need to go into toplevel/bin, they are fine in the current dir.
  set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

  set(testtrash_SRCS testtrash.cpp ${trashcommon_PART_SRCS})

  kde4_automoc(${testtrash_SRCS})

  kde4_add_executable(testtrash ${testtrash_SRCS})

  target_link_libraries(testtrash  ${KDE4_KIO_LIBS} )

  add_test(kioslave-testtrash ${EXECUTABLE_OUTPUT_PATH}/testtrash)

endif(KDE4_BUILD_TESTS)

########### next target ###############
#Add sub dir after: generate  /kioslave/trash/trashimpl.cpp before to go to this directory
add_subdirectory( kfile-plugin )

########### install files ###############

install( FILES trash.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )




#original Makefile.am contents follow:

#INCLUDES = $(all_includes)
#METASOURCES = AUTO
#
#SUBDIRS = . kfile-plugin
#
#kde_module_LTLIBRARIES = kio_trash.la
#
#kio_trash_la_SOURCES = kio_trash.cpp
#kio_trash_la_LIBADD  = libtrashcommon.la $(LIB_KIO)
#kio_trash_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN) -no-undefined
#
#bin_PROGRAMS = ktrash
#ktrash_SOURCES = ktrash.cpp
#ktrash_LDADD = $(LIB_KIO)
#ktrash_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#
#kde_services_DATA = trash.protocol
#
#noinst_LTLIBRARIES = libtrashcommon.la
#libtrashcommon_la_SOURCES = trashimpl.cpp
#
#check_PROGRAMS = testtrash
#testtrash_SOURCES = testtrash.cpp
#testtrash_LDADD = libtrashcommon.la $(LIB_KIO)
#testtrash_LDFLAGS = $(all_libraries)
#
#TESTS = testtrash
#
#messages:
#	$(XGETTEXT) `find . -name "*.cc" -o -name "*.cpp" -o -name "*.h"` -o $(podir)/kio_trash.pot
#
