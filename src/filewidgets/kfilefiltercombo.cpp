/* This file is part of the KDE libraries
    Copyright (C) Stephan Kulow <coolo@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kfilefiltercombo.h"

#include <QDebug>
#include <klocalizedstring.h>
#include <qmimedatabase.h>
#include <config-kiofilewidgets.h>
#include <QtCore/QEvent>
#include <QLineEdit>

class KFileFilterCombo::Private
{
public:
    Private( KFileFilterCombo *_parent )
        : parent(_parent),
          hasAllSupportedFiles(false),
          isMimeFilter(false),
          defaultFilter(i18n("*|All Files"))
    {
    }

    void _k_slotFilterChanged();

    KFileFilterCombo *parent;
    // when we have more than 3 mimefilters and no default-filter,
    // we don't show the comments of all mimefilters in one line,
    // instead we show "All supported files". We have to translate
    // that back to the list of mimefilters in currentFilter() tho.
    bool hasAllSupportedFiles;
    // true when setMimeFilter was called
    bool isMimeFilter;
    QString lastFilter;
    QString defaultFilter;

    QStringList m_filters;
    bool m_allTypes;
};

KFileFilterCombo::KFileFilterCombo( QWidget *parent)
    : KComboBox(true, parent), d( new Private(this) )
{
    setTrapReturnKey( true );
    setInsertPolicy(QComboBox::NoInsert);
    connect( this, SIGNAL(activated(int)), this, SIGNAL(filterChanged()));
    connect( this, SIGNAL(returnPressed()), this, SIGNAL(filterChanged()));
    connect( this, SIGNAL(filterChanged()), SLOT(_k_slotFilterChanged()));
    d->m_allTypes = false;
}

KFileFilterCombo::~KFileFilterCombo()
{
    delete d;
}

void KFileFilterCombo::setFilter(const QString& filter)
{
    clear();
    d->m_filters.clear();
    d->hasAllSupportedFiles = false;

    if (!filter.isEmpty()) {
	QString tmp = filter;
	int index = tmp.indexOf('\n');
	while (index > 0) {
	    d->m_filters.append(tmp.left(index));
	    tmp = tmp.mid(index + 1);
	    index = tmp.indexOf('\n');
	}
	d->m_filters.append(tmp);
    }
    else
	d->m_filters.append( d->defaultFilter );

    QStringList::ConstIterator it;
    QStringList::ConstIterator end(d->m_filters.constEnd());
    for (it = d->m_filters.constBegin(); it != end; ++it) {
	int tab = (*it).indexOf('|');
	addItem((tab < 0) ? *it :
		   (*it).mid(tab + 1));
    }

    d->lastFilter = currentText();
    d->isMimeFilter = false;
}

QString KFileFilterCombo::currentFilter() const
{
    QString f = currentText();
    if (f == itemText(currentIndex())) { // user didn't edit the text
	f = d->m_filters.value(currentIndex());
        if ( d->isMimeFilter || (currentIndex() == 0 && d->hasAllSupportedFiles) ) {
            return f; // we have a mimetype as filter
        }
    }

    int tab = f.indexOf('|');
    if (tab < 0)
	return f;
    else
	return f.left(tab);
}

bool KFileFilterCombo::showsAllTypes() const
{
    return d->m_allTypes;
}

QStringList KFileFilterCombo::filters() const
{
    return d->m_filters;
}

void KFileFilterCombo::setCurrentFilter( const QString& filter )
{
    setCurrentIndex(d->m_filters.indexOf(filter));
    filterChanged();
}

void KFileFilterCombo::setMimeFilter( const QStringList& types,
                                      const QString& defaultType )
{
    clear();
    d->m_filters.clear();
    QString delim = QLatin1String(", ");
    d->hasAllSupportedFiles = false;
    bool hasAllFilesFilter = false;
    QMimeDatabase db;

    d->m_allTypes = defaultType.isEmpty() && (types.count() > 1);

    QString allComments, allTypes;
    for(QStringList::ConstIterator it = types.begin(); it != types.end(); ++it)
    {
        // qDebug() << *it;
        QMimeType type = db.mimeTypeForName(*it);

        if (type.name().startsWith(QLatin1String("all/"))) {
            hasAllFilesFilter = true;
            continue;
        }

        if ( d->m_allTypes && it != types.begin() ) {
            allComments += delim;
            allTypes += ' ';
        }

        d->m_filters.append(type.name());
        if ( d->m_allTypes )
        {
            allTypes += type.name();
            allComments += type.comment();
        }
        addItem( type.comment() );
        if (type.name() == defaultType)
            setCurrentIndex( count() - 1 );
    }

    if ( d->m_allTypes )
    {
        if ( count() <= 3 ) // show the mime-comments of at max 3 types
            insertItem(0, allComments);
        else {
            insertItem(0, i18n("All Supported Files"));
            d->hasAllSupportedFiles = true;
        }
        setCurrentIndex( 0 );

        d->m_filters.prepend( allTypes );
    }

    if ( hasAllFilesFilter ) {
        addItem(i18n("All Files"));
        d->m_filters.append( QLatin1String("all/allfiles") );
    }

    d->lastFilter = currentText();
    d->isMimeFilter = true;
}

void KFileFilterCombo::Private::_k_slotFilterChanged()
{
    lastFilter = parent->currentText();
}

bool KFileFilterCombo::eventFilter( QObject *o, QEvent *e )
{
    if ( o == lineEdit() && e->type() == QEvent::FocusOut ) {
        if ( currentText() != d->lastFilter )
            emit filterChanged();
    }

    return KComboBox::eventFilter( o, e );
}

void KFileFilterCombo::setDefaultFilter( const QString& filter )
{
    d->defaultFilter = filter;
}

QString KFileFilterCombo::defaultFilter() const
{
    return d->defaultFilter;
}

bool KFileFilterCombo::isMimeFilter() const
{
    return d->isMimeFilter;
}

#include "moc_kfilefiltercombo.cpp"
