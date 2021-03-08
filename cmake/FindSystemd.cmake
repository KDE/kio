# SPDX-FileCopyrightText: 2021 Henri Chain <henri.chain@enioka.com>
#
# SPDX-License-Identifier: BSD-3-Clause

# Try to find systemd on a linux system
# This will define the following variables:
#
# ``Systemd_FOUND``
#     True if systemd is available
# ``Systemd_VERSION``
#     The version of systemd

find_package(PkgConfig)
pkg_check_modules(PKG_systemd QUIET systemd)

set(Systemd_FOUND ${PKG_systemd_FOUND})
set(Systemd_VERSION ${PKG_systemd_VERSION})