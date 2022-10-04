<!--
SPDX-FileCopyrightText: 2022 Friedrich W. H. Kossebau <kossebau@kde.org>
SPDX-License-Identifier: CC0-1.0
-->

# KIO worker to test messageboxes manually

Enable the installation in CMakeLists.txt, then build and install.

Open the URL messagebox:/ in Dolphin. That will list directories named by the enums.
Click a directory to enter it (list its content), that will instead trigger the respective warning and a redirect to the root dir.
