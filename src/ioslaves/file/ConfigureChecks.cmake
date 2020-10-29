include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckStructHasMember)
include(CheckCXXSourceCompiles)

check_include_files(sys/time.h    HAVE_SYS_TIME_H)
check_include_files(string.h      HAVE_STRING_H)
check_include_files(limits.h      HAVE_LIMITS_H)
check_include_files(sys/xattr.h   HAVE_SYS_XATTR_H)
check_include_files("sys/types.h;sys/extattr.h" HAVE_SYS_EXTATTR_H)

check_function_exists(sendfile    HAVE_SENDFILE)

check_function_exists(posix_fadvise    HAVE_FADVISE)                  # kioslave

check_library_exists(volmgt volmgt_running "" HAVE_VOLMGT)

check_struct_has_member("struct dirent" d_type dirent.h HAVE_DIRENT_D_TYPE LANGUAGE CXX)

check_symbol_exists("__GLIBC__" "stdlib.h" LIBC_IS_GLIBC)
if (LIBC_IS_GLIBC)
    check_cxx_source_compiles("
        #include <fcntl.h>
        #include <sys/stat.h>

        int main() {
            struct statx buf;
            statx(AT_FDCWD, \"/foo\", AT_EMPTY_PATH, STATX_BASIC_STATS, &buf);
            return 0;
        }
    " HAVE_STATX)
else()
    set(HAVE_STATX 0)
endif()
