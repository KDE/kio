include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/.. )

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

include(ECMAddTests)

ecm_add_test(${testtrash_SRCS}
    TEST_NAME testtrash
    LINK_LIBRARIES
        KF6::ConfigCore
        KF6::I18n
        ${DBUS_LIB}
        KF6::KIOCore
        KF6::Solid
        Qt6::Test
        Qt6::Network
)

if(APPLE)
    target_link_libraries(testtrash "-framework DiskArbitration -framework CoreFoundation")
endif(APPLE)

# other tests like dropjob and fileundomanager use the trash in ~/.qttest, which this test cleans up
set_tests_properties(testtrash PROPERTIES RUN_SERIAL TRUE)
