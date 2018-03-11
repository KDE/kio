#ifndef _MEIN_XSLT_HELP_H_
#define _MEIN_XSLT_HELP_H_

#include <QString>

QString lookForCache(const QString &filename);

/**
 * Compares two files and returns true if @param newer exists and is newer than
 * @param older
 **/
bool compareTimeStamps(const QString &older, const QString &newer);

#endif
