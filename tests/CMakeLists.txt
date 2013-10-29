set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

#include_directories(${STRIGI_INCLUDE_DIR})

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/.. )

########### next target ###############

set(testtrash_SRCS 
    testtrash.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/../trashimpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../trashsizecache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../discspaceutil.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../kinterprocesslock.cpp
)

kde4_add_unit_test(testtrash ${testtrash_SRCS})

# This flag is needed in order to be able to support files > 2GB even on
# 32bit platforms. The default is to use the non-64bit aware syscalls on
# 32bit platforms, which makes every application to SIGXFSZ (which is
# equivalent to a crash) when they see or touch a file > 2GB.
check_cxx_source_compiles("
#include <sys/types.h>
/* Check that off_t can represent 2**63 - 1 correctly.
We can't simply define LARGE_OFF_T to be 9223372036854775807,
since some C++ compilers masquerading as C compilers
incorrectly reject 9223372036854775807.  */
#define LARGE_OFF_T (((off_t) 1 << 62) - 1 + ((off_t) 1 << 62))
int off_t_is_large[(LARGE_OFF_T % 2147483629 == 721 && LARGE_OFF_T % 2147483647 == 1) ? 1 : -1];
int main() { return 0; }
" _OFFT_IS_64BIT)

if (NOT _OFFT_IS_64BIT)
  target_compile_definitions(testtrash PUBLIC -D_FILE_OFFSET_BITS=64)
endif ()

target_link_libraries(testtrash KF5::KI18n Qt5::DBus ${KDE4_KIO_LIBS} ${KDE4_SOLID_LIBS} ${QT_QTTEST_LIBRARY} KF5::KDE4Support)

### next target ###

set(lockingtest_SRCS lockingtest.cpp ../kinterprocesslock.cpp )

kde4_add_executable(lockingtest NOGUI ${lockingtest_SRCS})

target_link_libraries(lockingtest ${QT_QTCORE_LIBRARY} ${QT_QTDBUS_LIBRARY})
