include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckCXXSymbolExists)

check_include_files(sys/time.h    HAVE_SYS_TIME_H)
check_include_files(sys/time.h    TIME_WITH_SYS_TIME)

check_symbol_exists(strtoll         "stdlib.h"                 HAVE_STRTOLL)
