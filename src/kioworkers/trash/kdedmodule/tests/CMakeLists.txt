include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

ecm_qt_declare_logging_category(testtrashdirnotify_DEBUG_SRCS
    HEADER debug.h
    IDENTIFIER KIO_TRASHNOTIFY_LOG
    CATEGORY_NAME kf.kio.workers.trash.notify
)

ecm_qt_declare_logging_category(testtrashdirnotify_DEBUG_SRCS
    HEADER kiotrashdebug.h
    IDENTIFIER KIO_TRASH
    CATEGORY_NAME kf.kio.workers.trash
)

set(testtrashdirnotify_SRCS
    testtrashdirnotify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../trashdirnotify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../trashimpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../trashsizecache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../discspaceutil.cpp
    ${testtrashdirnotify_DEBUG_SRCS}
)

include(ECMAddTests)

ecm_add_test(${testtrashdirnotify_SRCS}
    TEST_NAME testtrashdirnotify
    LINK_LIBRARIES
        KF6::ConfigCore
        KF6::DBusAddons
        KF6::I18n
        KF6::KIOCore
        KF6::Solid
        Qt6::DBus
        Qt6::Test
        ${DBUS_LIB}
)

set_tests_properties(kioworkers-testtrashdirnotify PROPERTIES RUN_SERIAL TRUE)
