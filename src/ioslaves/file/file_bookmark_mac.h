#pragma once

#include "file.h"

bool createUDSEntry(const QString &filename, const QByteArray &path, KIO::UDSEntry &entry, KIO::StatDetails details, const QString &fullPath);
