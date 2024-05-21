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
    CATEGORY_NAME kf.kio.workers.trash
    OLD_CATEGORY_NAMES kf5.kio.trash kf.kio.slaves.trash
    DESCRIPTION "kio trash (KIO)"
    EXPORT KIO
)
kcoreaddons_add_plugin(kio_trash INSTALL_NAMESPACE "kf6/kio")

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
        ${kio_trash_PART_DEBUG_SRCS}
    )
    target_link_libraries(kio_trash trash_common_unix)

    target_sources(kio_trash PRIVATE
        kio_trash.cpp
    )
endif()

target_link_libraries(kio_trash
  KF6::ConfigCore
  KF6::Solid
  KF6::KIOCore
  ${DBUS_LIB}
  Qt6::Network
  KF6::I18n
  KF6::ConfigCore
)
if(APPLE)
    target_link_libraries(kio_trash "-framework DiskArbitration -framework CoreFoundation")
endif(APPLE)


add_executable(ktrash6)
ecm_mark_nongui_executable(ktrash6)

target_sources(ktrash6 PRIVATE
    ktrash.cpp
)

target_compile_definitions(ktrash6 PRIVATE -DPROJECT_VERSION="${PROJECT_VERSION}")

target_link_libraries(ktrash6
  KF6::KIOCore
  KF6::I18n
  KF6::ConfigCore
)

install(TARGETS ktrash6 ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

