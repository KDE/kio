include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/.. )

########### next target ###############

set(testtrash_SRCS 
    testtrash.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../trashimpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../trashsizecache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../discspaceutil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../kinterprocesslock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../kiotrashdebug.cpp
)

if(APPLE)
    set(testtrash_APPLE_LIBS "-framework DiskArbitration -framework CoreFoundation")
endif(APPLE)

include(ECMAddTests)

ecm_add_test(${testtrash_SRCS}
    TEST_NAME testtrash
    LINK_LIBRARIES KF5::I18n Qt5::DBus KF5::KIOCore KF5::Solid Qt5::Test ${testtrash_APPLE_LIBS}
)

### next target ###

add_executable(lockingtest lockingtest.cpp ../kinterprocesslock.cpp)
ecm_mark_nongui_executable(lockingtest)

target_link_libraries(lockingtest Qt5::Core Qt5::DBus)
