include(CheckIncludeFile)
include(CheckIncludeFiles)

check_include_files(stdio.h       HAVE_STDIO_H)
check_include_files(stdlib.h      HAVE_STDLIB_H)
check_include_files(sys/types.h   HAVE_SYS_TYPES_H)
check_include_files(sys/stat.h    HAVE_SYS_STAT_H)
