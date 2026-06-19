include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/.. )

include(ECMAddTests)

if(WIN32)
    # testtrash exercises the Unix trash layout (info/ and files/) and does not build
    # on Windows. Only the recycle-bin worker test runs here; it drives the worker
    # against the real Windows trash, so it stops itself when the trash is not empty.
    ecm_add_test(kiotrashwintest.cpp
        TEST_NAME kiotrashwintest
        LINK_LIBRARIES KF6::KIOCore KF6::I18n Qt6::Test shell32
    )
    return()
endif()

########### next target ###############

ecm_qt_declare_logging_category(kio_trash_PART_test_DEBUG_SRCS
    HEADER kiotrashdebug.h
    IDENTIFIER KIO_TRASH
    CATEGORY_NAME kf.kio.workers.trash
)

set(testtrash_SRCS
    testtrash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../trashimpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../trashsizecache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../discspaceutil.cpp
    ${kio_trash_PART_test_DEBUG_SRCS}
)

ecm_add_test(${testtrash_SRCS}
    TEST_NAME testtrash
    LINK_LIBRARIES
        KF6::ConfigCore
        KF6::I18n
        ${DBUS_LIB}
        KF6::KIOCore
        Qt6::Test
        Qt6::Network
)

if(APPLE)
    target_link_libraries(testtrash "-framework DiskArbitration -framework CoreFoundation")
endif(APPLE)

# other tests like dropjob and fileundomanager use the trash in ~/.qttest, which this test cleans up
set_tests_properties(kioworkers-testtrash PROPERTIES RUN_SERIAL TRUE)
