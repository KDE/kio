/*
    fixhosturifilter.h

    This file is part of the KDE project
    Copyright (C) 2007 Lubos Lunak <llunak@suse.cz>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FIXHOSTURIFILTER_H
#define FIXHOSTURIFILTER_H

#include <KUriFilter>

class QHostInfo;
class QEventLoop;

/*
 This filter tries to automatically prepend www. to http URLs that need it.
*/
class FixHostUriFilter : public KUriFilterPlugin
{
Q_OBJECT

public:
    FixHostUriFilter(QObject* parent, const QVariantList& args);
    virtual bool filterUri(KUriFilterData &data) const;

private:
    bool exists(const QString& host) const;
    bool isResolvable(const QString& host) const;
};

#endif
