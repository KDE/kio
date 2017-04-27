project(KIONTLM)

set(kntlm_LIB_SRCS kntlm.cpp des.cpp )

add_library(KF5KIONTLM ${kntlm_LIB_SRCS})
generate_export_header(KF5KIONTLM BASE_NAME KNTLM)
add_library(KF5::KIONTLM ALIAS KF5KIONTLM)

target_link_libraries(KF5KIONTLM KF5::CoreAddons)

set_target_properties(KF5KIONTLM PROPERTIES VERSION ${KIO_VERSION_STRING}
                                            SOVERSION ${KIO_SOVERSION}
                                            EXPORT_NAME KIONTLM
                                            )

install(TARGETS KF5KIONTLM EXPORT KF5KIOTargets  ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

install( FILES ${CMAKE_CURRENT_BINARY_DIR}/kntlm_export.h kntlm.h  DESTINATION  ${KDE_INSTALL_INCLUDEDIR_KF5}/kio COMPONENT Devel)

# make available to ecm_add_qch in parent folder
set(KIONTLM_QCH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/kntlm.h PARENT_SCOPE)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KNTLM LIB_NAME KF5KIONTLM DEPS "KCoreAddons" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF5}/KNTLM)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
