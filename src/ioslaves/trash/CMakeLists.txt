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
kcoreaddons_add_plugin(kio_trash INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kio")

if(WIN32)
    target_sources(kio_trash PRIVATE
        kio_trash_win.cpp
        ${kio_trash_PART_DEBUG_SRCS}
    )
else()
    # Files that are shared with the KCM. Unix specific.
    add_library(trash_common_unix INTERFACE)
    target_sources(trash_common_unix INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/trashimpl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/discspaceutil.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/trashsizecache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/kinterprocesslock.cpp
        ${kio_trash_PART_DEBUG_SRCS}
    )
    target_link_libraries(kio_trash trash_common_unix)

    target_sources(kio_trash PRIVATE
        kio_trash.cpp
    )
endif()

target_link_libraries(kio_trash
  KF5::ConfigCore
  KF5::Solid
  KF5::KIOCore
  Qt${QT_MAJOR_VERSION}::DBus
  Qt${QT_MAJOR_VERSION}::Network
  KF5::I18n
  KF5::ConfigCore
)
if(APPLE)
    target_link_libraries(kio_trash "-framework DiskArbitration -framework CoreFoundation")
endif(APPLE)


add_executable(ktrash5)
ecm_mark_nongui_executable(ktrash5)

target_sources(ktrash5 PRIVATE
    ktrash.cpp
)

target_compile_definitions(ktrash5 PRIVATE -DPROJECT_VERSION="${PROJECT_VERSION}")

target_link_libraries(ktrash5
  KF5::KIOCore
  KF5::I18n
  KF5::ConfigCore
)

install(TARGETS ktrash5 ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

########### next target ###############

# currently not on win32, TODO!
if(NOT WIN32 AND NOT KIOCORE_ONLY)
    add_library(kcm_trash MODULE)
    kcoreaddons_desktop_to_json(kcm_trash kcmtrash.desktop)
    target_sources(kcm_trash PRIVATE
        kcmtrash.cpp
    )
    target_link_libraries(kcm_trash
       trash_common_unix
       Qt${QT_MAJOR_VERSION}::DBus
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

install( FILES kcmtrash.desktop DESTINATION ${KDE_INSTALL_KSERVICESDIR} )
install( FILES kcm_trash.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
