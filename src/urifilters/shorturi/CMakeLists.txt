add_library(kshorturifilter MODULE kshorturifilter.cpp)

target_link_libraries(kshorturifilter KF5::I18n KF5::KIOWidgets Qt5::DBus)

install(TARGETS kshorturifilter  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/urifilters)


########### install files ###############

install( FILES kshorturifilter.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
install( FILES kshorturifilterrc  DESTINATION  ${KDE_INSTALL_CONFDIR} )

