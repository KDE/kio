add_executable(file_helper filehelper.cpp fdsender.cpp)
target_link_libraries(file_helper Qt5::Network KF5::AuthCore KF5::I18n KF5::KIOCore)

#install(TARGETS file_helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
#kauth_install_helper_files(file_helper org.kde.kio.file root)
#kauth_install_actions(org.kde.kio.file file.actions)
