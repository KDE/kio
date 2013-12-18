
project(kioslave-file)

include(ConfigureChecks.cmake)
configure_file(config-kioslave-file.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioslave-file.h )


########### next target ###############

if(WIN32)
  set(kio_file_PART_SRCS file.cpp file_win.cpp )
else()
  set(kio_file_PART_SRCS file.cpp file_unix.cpp )
endif()

find_package(ACL)
set(HAVE_LIBACL ${ACL_FOUND})
set(HAVE_POSIX_ACL ${ACL_FOUND})

set_package_properties(ACL PROPERTIES DESCRIPTION "LibACL" URL "ftp://oss.sgi.com/projects/xfs/cmd_tars"
                       TYPE RECOMMENDED PURPOSE "Support for manipulating access control lists")

add_library(kio_file MODULE ${kio_file_PART_SRCS})
set_target_properties(kio_file PROPERTIES PREFIX "")
target_link_libraries(kio_file KF5::KIOCore KF5::I18n)

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
  target_compile_definitions(kio_file PUBLIC -D_FILE_OFFSET_BITS=64)
endif ()

if (HAVE_VOLMGT AND CMAKE_SYSTEM_NAME MATCHES SunOS)
    target_link_libraries(kio_file -lvolmgt)
endif ()

if(ACL_FOUND)
   target_link_libraries(kio_file ${ACL_LIBS})
endif()

install(TARGETS kio_file  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES file.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )




