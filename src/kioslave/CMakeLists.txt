add_executable(kioslave5 kioslave.cpp)
ecm_mark_nongui_executable(kioslave5)

target_link_libraries(kioslave5
   KF5::KIOCore
)

install(TARGETS kioslave5 DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF5} )
