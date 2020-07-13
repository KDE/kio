# find_package(Strigi)
# set_package_properties(Strigi PROPERTIES DESCRIPTION "Desktop indexing and search support"
#                        URL "http://strigi.sourceforge.net"
#                        TYPE ${STRIGI_TYPE}
#                       )
# if (WIN32)
#     set (STRIGI_TYPE "OPTIONAL")
# else ()
#     set (STRIGI_TYPE "REQUIRED")
# endif ()

if(BUILD_TESTING)
  if(WIN32)
    message(AUTHOR_WARNING "kio_trash unit tests are broken on Windows, disabling them")
  else()
    add_subdirectory(tests)
  endif()
endif()

########### next target ###############
ecm_qt_declare_logging_category(kio_trash_PART_DEBUG_SRCS
    HEADER kiotrashdebug.h
    IDENTIFIER KIO_TRASH
    CATEGORY_NAME kf.kio.slaves.trash
    OLD_CATEGORY_NAMES kf5.kio.trash
    DESCRIPTION "kio trash (KIO)"
    EXPORT KIO
)
if(WIN32)
  set(kio_trash_PART_SRCS kio_trash_win.cpp ${kio_trash_PART_DEBUG_SRCS})
else()
  # Files that are shared with the KCM. Unix specific.
  set (trashcommon_unix_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/trashimpl.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/discspaceutil.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/trashsizecache.cpp
                             ${CMAKE_CURRENT_SOURCE_DIR}/kinterprocesslock.cpp
    )
  set(kio_trash_PART_SRCS kio_trash.cpp ${trashcommon_unix_SRCS} ${kio_trash_PART_DEBUG_SRCS})
endif()
add_library(kio_trash MODULE ${kio_trash_PART_SRCS})

target_link_libraries(kio_trash
  KF5::Solid
  KF5::KIOCore
  Qt5::DBus
  Qt5::Network
  KF5::I18n
  KF5::ConfigCore
  KF5::ConfigGui
)
if(APPLE)
    target_link_libraries(kio_trash "-framework DiskArbitration -framework CoreFoundation")
endif(APPLE)
set_target_properties(kio_trash PROPERTIES OUTPUT_NAME "trash")
set_target_properties(kio_trash PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/kio")
install(TARGETS kio_trash  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio )

set(ktrash_SRCS ktrash.cpp )

add_executable(ktrash5 ${ktrash_SRCS})
target_compile_definitions(ktrash5 PRIVATE -DPROJECT_VERSION="${PROJECT_VERSION}")
ecm_mark_nongui_executable(ktrash5)

target_link_libraries(ktrash5
  KF5::KIOCore
  KF5::I18n
  KF5::ConfigCore
  KF5::ConfigGui
)

install(TARGETS ktrash5 ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

########### next target ###############

# currently not on win32, TODO!
if(NOT WIN32 AND NOT KIOCORE_ONLY)
    set(kcm_trash_PART_SRCS kcmtrash.cpp ${trashcommon_unix_SRCS} ${kio_trash_PART_DEBUG_SRCS})
    add_library(kcm_trash MODULE ${kcm_trash_PART_SRCS})
    target_link_libraries(kcm_trash
       Qt5::DBus
       KF5::I18n
       KF5::ConfigWidgets
       KF5::KIOCore
       KF5::Solid)
    if(APPLE)
        target_link_libraries(kcm_trash "-framework DiskArbitration -framework CoreFoundation")
    endif(APPLE)

    install(TARGETS kcm_trash DESTINATION ${KDE_INSTALL_PLUGINDIR})
endif()

########### install files ###############

install( FILES kcmtrash.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR} )
