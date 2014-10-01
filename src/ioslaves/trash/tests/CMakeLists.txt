include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/.. )

########### next target ###############

set(testtrash_SRCS 
    testtrash.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../trashimpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../trashsizecache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../discspaceutil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../kinterprocesslock.cpp
)

add_executable(testtrash ${testtrash_SRCS})
ecm_mark_as_test(testtrash)
add_test(testtrash testtrash)

target_link_libraries(testtrash KF5::I18n Qt5::DBus KF5::KIOCore KF5::Solid Qt5::Test KF5::KDE4Support)

### next target ###

add_executable(lockingtest lockingtest.cpp ../kinterprocesslock.cpp)
ecm_mark_nongui_executable(lockingtest)

target_link_libraries(lockingtest Qt5::Core Qt5::DBus)
