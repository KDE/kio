# SPDX-FileCopyrightText: 2022 Friedrich W. H. Kossebau <kossebau@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

add_library(kiomessagebox MODULE)
set_target_properties(kiomessagebox PROPERTIES OUTPUT_NAME "messagebox")

target_sources(kiomessagebox PRIVATE
    messageboxworker.cpp
)

target_link_libraries(kiomessagebox
    KF5::KIOCore
)

# Uncomment to deploy
# install(TARGETS kiomessagebox DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf${QT_MAJOR_VERSION}/kio)
