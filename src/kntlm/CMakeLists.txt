# KF6 TODO: move all classes to kio-http, only left known user, and dissolve lib

add_library(KF5KIONTLM)
add_library(KF5::KIONTLM ALIAS KF5KIONTLM)

set_target_properties(KF5KIONTLM PROPERTIES
    VERSION     ${KIO_VERSION}
    SOVERSION   ${KIO_SOVERSION}
    EXPORT_NAME KIONTLM
)

target_sources(KF5KIONTLM PRIVATE
    kntlm.cpp
    des.cpp
)

ecm_generate_export_header(KF5KIONTLM
    BASE_NAME KNTLM
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 5.91
    # not using EXCLUDE_DEPRECATED_BEFORE_AND_AT as kio-http has a hard dependency,
    # so always needed to be build
)

target_link_libraries(KF5KIONTLM KF5::CoreAddons)

install(TARGETS KF5KIONTLM EXPORT KF5KIOTargets  ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

install( FILES ${CMAKE_CURRENT_BINARY_DIR}/kntlm_export.h kntlm.h  DESTINATION  ${KDE_INSTALL_INCLUDEDIR_KF}/kio COMPONENT Devel)

# make available to ecm_add_qch in parent folder
set(KIONTLM_QCH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/kntlm.h PARENT_SCOPE)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KNTLM LIB_NAME KF5KIONTLM DEPS "KCoreAddons" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF}/KNTLM)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
