set(localdomainurifilter_SRCS localdomainurifilter.cpp )

kcoreaddons_add_plugin(localdomainurifilter INSTALL_NAMESPACE "kf5/urifilters" JSON localdomainurifilter.json SOURCES ${localdomainurifilter_SRCS})
ecm_generate_qloggingcategories_source(localdomainurifilter
    HEADER localdomainurifilter_debug.h
    GROUP LOCALDOMAINURIFILTER
)

target_link_libraries(localdomainurifilter KF5::KIOWidgets)

