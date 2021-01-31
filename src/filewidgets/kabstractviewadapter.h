/*
    SPDX-FileCopyrightText: 2008 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KABSTRACTVIEWADAPTER_H
#define KABSTRACTVIEWADAPTER_H

#include <QObject>
#include "kiofilewidgets_export.h"

class QAbstractItemModel;
class QModelIndex;
class QPalette;
class QRect;
class QSize;

/*
 * TODO KF6 Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
 * TODO KF6 virtual void setIconSize(const QSize &size);
 * TODO KF6 iconSizeChanged();
 *
 * TODO KF6:
 * KAbstractViewAdapter exists to allow KFilePreviewGenerator to be
 * reused with new kinds of views. Unfortunately it doesn't cover
 * all use cases that would be useful right now, in particular there
 * are no change notifications for the properties it has getters for.
 * This requires view implementations to e.g. call updateIcons() on
 * the generator when the icon size changes, which means updating two
 * entities (the generator and the adapter) instead of only one.
 * In KF6 we should make iconSize a Q_PROPERTY with a virtual setter
 * and a change notification signal, and make KFilePreviewGenerator
 * listen to that signal.
 * A related problem is that while the adapter is supposed to inter-
 * face a view to the generator, it is sometimes the generator that
 * is responsible for instantiating the adapter: KDirOperator in this
 * framework uses the KFilePreviewGenerator constructor that doesn't
 * take an adapter instance, which makes the generator instantiate a
 * KIO::DefaultViewAdapter internally, which it doesn't expose to the
 * outside. That means even when a setIconSize() is added,
 * KDirOperator won't be able to call it on the adapter. This mis-
 * design needs to be addressed as well so all change notifications
 * can run through the adapter, also for the DefaultViewAdapter
 * implementation (though for this specific example, perhaps Qt will
 * one day give us a NOTIFY for QAbstractItemView::iconSize that the
 * DefaultViewAdapter can use, obviating the need for KDirOperator
 * to do anything except call setIconSize on its QAbstractItemView).
 */

/**
 * @class KAbstractViewAdapter kabstractviewadapter.h <KAbstractViewAdapter>
 *
 * Interface used by KFilePreviewGenerator to generate previews
 * for files. The interface allows KFilePreviewGenerator to be
 * independent from the view implementation.
 */
class KIOFILEWIDGETS_EXPORT KAbstractViewAdapter : public QObject
{
public:
    enum Signal { ScrollBarValueChanged, IconSizeChanged };

    KAbstractViewAdapter(QObject *parent) : QObject(parent) {}
    virtual ~KAbstractViewAdapter() {}
    virtual QAbstractItemModel *model() const = 0;
    virtual QSize iconSize() const = 0;
    virtual QPalette palette() const = 0;
    virtual QRect visibleArea() const = 0;
    virtual QRect visualRect(const QModelIndex &index) const = 0;

    // TODO KF6 make this connect work with a PointerToMemberFunction/Functor
    virtual void connect(Signal signal, QObject *receiver, const char *slot) = 0;
};

#endif

