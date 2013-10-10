include_directories(${KDE4_INCLUDE_DIR}/kio ${KDE4_INCLUDE_DIR}/KDE)

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0)

#add_subdirectory(tests)

set (trashcommon_PART_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/trashimpl.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/discspaceutil.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/trashsizecache.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/kinterprocesslock.cpp
    )

########### next target ###############
if(WIN32)
  set(kio_trash_PART_SRCS kio_trash_win.cpp kio_trash_win.h)
else(WIN32)
  set(kio_trash_PART_SRCS kio_trash.cpp ${trashcommon_PART_SRCS})
endif(WIN32)
kde4_add_plugin(kio_trash ${kio_trash_PART_SRCS})

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
  target_compile_definitions(kio_trash PUBLIC -D_FILE_OFFSET_BITS=64)
endif ()

target_link_libraries(kio_trash
	KF5::Solid
        ${KDE4Support_LIBRARIES}
	KF5::KDeclarative
	KF5::KIOCore
	KF5::KIOWidgets
        Qt5::DBus
        KF5::KI18n KF5::KConfigCore KF5::KConfigGui 
        )
install(TARGETS kio_trash  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### next target ###############
# currently not on win32, TODO!
if(NOT WIN32 AND STRIGI_FOUND)
    include_directories(${STRIGI_INCLUDE_DIR} ${KDE4_INCLUDE_DIR}/kio)

    kde4_add_library(trash MODULE trashthroughanalyzer.cpp ${trashcommon_PART_SRCS})
    target_link_libraries(trash
       ${KDE4Support_LIBRARIES}
       KF5::KDeclarative
       KF5::KIOCore
       KF5::KIOWidgets
       ${STRIGI_STREAMANALYZER_LIBRARY}
       KF5::Solid
       KF5::KI18n KF5::KConfigCore KF5::KConfigGui
    )
    set_target_properties(trash PROPERTIES
        PREFIX strigita_)
    #install(TARGETS trash LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strigi)
endif(NOT WIN32 AND STRIGI_FOUND)
########### next target ###############

set(ktrash_SRCS ktrash.cpp )

kde4_add_executable(ktrash NOGUI ${ktrash_SRCS})

target_link_libraries(ktrash    
        ${KDE4Support_LIBRARIES}
	KF5::KDeclarative
	KF5::KIOCore
	KF5::KIOWidgets
        KF5::KI18n KF5::KConfigCore KF5::KConfigGui
        )

install(TARGETS ktrash ${INSTALL_TARGETS_DEFAULT_ARGS})

########### next target ###############

# currently not on win32, TODO!
if(NOT WIN32)
    set(kcm_trash_PART_SRCS kcmtrash.cpp ${trashcommon_PART_SRCS})
    kde4_add_plugin(kcm_trash ${kcm_trash_PART_SRCS})
    target_link_libraries(kcm_trash  Qt5::DBus  ${KDE4Support_LIBRARIES}
	    ${KDECLARATIVE_LIBRARIES} KF5::KI18n KF5::KDE4Attic KConfigWidgets KF5::KIOCore KF5::KIOWidgets KF5::Solid)

    install(TARGETS kcm_trash DESTINATION ${PLUGIN_INSTALL_DIR})
endif(NOT WIN32)

########### install files ###############

install( FILES trash.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES kcmtrash.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
