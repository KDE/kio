add_library(kshorturifilter MODULE kshorturifilter.cpp)

target_link_libraries(kshorturifilter KF5::I18n KF5::KIOWidgets Qt5::DBus)

set_target_properties(kshorturifilter PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/urifilters")
install(TARGETS kshorturifilter  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/urifilters)


########### install files ###############

install( FILES kshorturifilter.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
install( FILES kshorturifilterrc  DESTINATION  ${KDE_INSTALL_CONFDIR} )

