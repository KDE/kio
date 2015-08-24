add_executable(kioslave WIN32 kioslave.cpp)

target_link_libraries(kioslave
   KF5::KIOCore
)

install(TARGETS kioslave DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF5} )
