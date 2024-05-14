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
# On FreeBSD extattr.h doesn't compile without manually including sys/types.h
check_include_files("sys/types.h;sys/extattr.h" HAVE_SYS_EXTATTR_H)

check_function_exists(copy_file_range HAVE_COPY_FILE_RANGE)

check_function_exists(posix_fadvise    HAVE_FADVISE)                  # KIO worker

check_struct_has_member("struct dirent" d_type dirent.h HAVE_DIRENT_D_TYPE LANGUAGE CXX)
