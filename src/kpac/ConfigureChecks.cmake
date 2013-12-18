include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckLibraryExists)

check_include_files(sys/param.h   HAVE_SYS_PARAM_H)
check_include_files(sys/types.h   HAVE_SYS_TYPES_H)
check_include_files(arpa/nameser_compat.h HAVE_ARPA_NAMESER_COMPAT_H)
check_include_files(arpa/nameser8_compat.h HAVE_ARPA_NAMESER8_COMPAT_H)
check_include_files("sys/types.h;netinet/in.h"  HAVE_NETINET_IN_H)
check_include_files(stdint.h      HAVE_STDINT_H) 

# Check for libresolv
# e.g. on slackware 9.1 res_init() is only a define for __res_init, so we check both, Alex
set(HAVE_RESOLV_LIBRARY FALSE)
check_library_exists(resolv res_init "" HAVE_RES_INIT_IN_RESOLV_LIBRARY)
check_library_exists(resolv __res_init "" HAVE___RES_INIT_IN_RESOLV_LIBRARY)
if (HAVE___RES_INIT_IN_RESOLV_LIBRARY OR HAVE_RES_INIT_IN_RESOLV_LIBRARY)
	set(HAVE_RESOLV_LIBRARY TRUE)
endif (HAVE___RES_INIT_IN_RESOLV_LIBRARY OR HAVE_RES_INIT_IN_RESOLV_LIBRARY)

check_library_exists(nsl gethostbyname "" HAVE_NSL_LIBRARY)
check_library_exists(socket connect "" HAVE_SOCKET_LIBRARY)
