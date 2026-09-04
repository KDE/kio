add_library(trashdirnotify MODULE
    trashdirnotify.cpp
    trashdirnotifymodule.cpp
)

ecm_qt_declare_logging_category(trashdirnotify
    HEADER debug.h
    IDENTIFIER KIO_TRASHNOTIFY_LOG
    CATEGORY_NAME kf.kio.workers.trash.notify
    DESCRIPTION "kio trash notifier (KIO)"
    EXPORT KIO
)

target_link_libraries(trashdirnotify
    trash_common_unix
    KF6::DBusAddons
    KF6::KIOCore
    KF6::ConfigCore
    KF6::I18n
    KF6::Solid
    ${DBUS_LIB}
)

install(TARGETS trashdirnotify DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kded)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
