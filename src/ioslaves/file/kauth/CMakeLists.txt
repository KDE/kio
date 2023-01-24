add_executable(file_helper filehelper.cpp fdsender.cpp)
target_link_libraries(file_helper Qt6::Network KF6::AuthCore KF6::I18n KF6::KIOCore)

#install(TARGETS file_helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
#kauth_install_helper_files(file_helper org.kde.kio.file root)
#kauth_install_actions(org.kde.kio.file file.actions)
