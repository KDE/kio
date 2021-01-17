/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000, 2001 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kurlcombobox.h"

#include "../pathhelpers_p.h" // isAbsoluteLocalPath()

#include <QDir>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QApplication>

#include <QDebug>
#include <kio/global.h>
#include <KLocalizedString>
#include <KIconLoader>

#include <algorithm>
#include <memory>
#include <vector>

class KUrlComboBoxPrivate
{
public:
    KUrlComboBoxPrivate(KUrlComboBox *parent)
        : m_parent(parent),
          dirIcon(QIcon::fromTheme(QStringLiteral("folder")))
    {}

    struct KUrlComboItem {
        KUrlComboItem(const QUrl &url, const QIcon &icon, const QString &text = QString())
            : url(url), icon(icon), text(text) {}
        QUrl url;
        QIcon icon;
        QString text; // if empty, calculated from the QUrl
    };

    void init(KUrlComboBox::Mode mode);
    QString textForItem(const KUrlComboItem *item) const;
    void insertUrlItem(const KUrlComboItem *);
    QIcon getIcon(const QUrl &url) const;
    void updateItem(const KUrlComboItem *item, int index, const QIcon &icon);

    void _k_slotActivated(int);

    KUrlComboBox * const m_parent;
    QIcon dirIcon;
    bool urlAdded;
    int myMaximum;
    KUrlComboBox::Mode myMode;
    QPoint m_dragPoint;

    using KUrlComboItemList = std::vector<std::unique_ptr<const KUrlComboItem>>;
    KUrlComboItemList itemList;
    KUrlComboItemList defaultList;
    QMap<int, const KUrlComboItem *> itemMapper;

    QIcon opendirIcon;
};


QString KUrlComboBoxPrivate::textForItem(const KUrlComboItem *item) const
{
    if (!item->text.isEmpty()) {
        return item->text;
    }
    QUrl url = item->url;

    if (myMode == KUrlComboBox::Directories) {
        if (!url.path().isEmpty() && !url.path().endsWith(QLatin1Char('/'))) {
            url.setPath(url.path() + QLatin1Char('/'));
        }
    } else {
        url = url.adjusted(QUrl::StripTrailingSlash);
    }
    if (url.isLocalFile()) {
        return url.toLocalFile();
    } else {
        return url.toDisplayString();
    }
}

KUrlComboBox::KUrlComboBox(Mode mode, QWidget *parent)
    : KComboBox(parent), d(new KUrlComboBoxPrivate(this))
{
    d->init(mode);
}

KUrlComboBox::KUrlComboBox(Mode mode, bool rw, QWidget *parent)
    : KComboBox(rw, parent), d(new KUrlComboBoxPrivate(this))
{
    d->init(mode);
}

KUrlComboBox::~KUrlComboBox()
{
    delete d;
}

void KUrlComboBoxPrivate::init(KUrlComboBox::Mode mode)
{
    myMode = mode;
    urlAdded = false;
    myMaximum = 10; // default
    m_parent->setInsertPolicy(KUrlComboBox::NoInsert);
    m_parent->setTrapReturnKey(true);
    m_parent->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    m_parent->setLayoutDirection(Qt::LeftToRight);
    if (m_parent->completionObject()) {
        m_parent->completionObject()->setOrder(KCompletion::Sorted);
    }

    opendirIcon = QIcon::fromTheme(QStringLiteral("folder-open"));

    m_parent->connect(m_parent, QOverload<int>::of(&KUrlComboBox::activated),
                      m_parent, [this](int index) { _k_slotActivated(index); });
}

QStringList KUrlComboBox::urls() const
{
    // qDebug() << "::urls()";
    QStringList list;
    QString url;
    for (int i = static_cast<int>(d->defaultList.size()); i < count(); i++) {
        url = itemText(i);
        if (!url.isEmpty()) {
            if (isAbsoluteLocalPath(url))
                list.append(QUrl::fromLocalFile(url).toString());
            else
                list.append(url);
        }
    }

    return list;
}

void KUrlComboBox::addDefaultUrl(const QUrl &url, const QString &text)
{
    addDefaultUrl(url, d->getIcon(url), text);
}

void KUrlComboBox::addDefaultUrl(const QUrl &url, const QIcon &icon,
                                 const QString &text)
{
    d->defaultList.push_back(std::unique_ptr<KUrlComboBoxPrivate::KUrlComboItem>(new KUrlComboBoxPrivate::KUrlComboItem(url, icon, text)));
}

void KUrlComboBox::setDefaults()
{
    clear();
    d->itemMapper.clear();

    for (const auto& item : d->defaultList) {
        d->insertUrlItem(item.get());
    }
}

void KUrlComboBox::setUrls(const QStringList &urls)
{
    setUrls(urls, RemoveBottom);
}

void KUrlComboBox::setUrls(const QStringList &_urls, OverLoadResolving remove)
{
    setDefaults();
    d->itemList.clear();
    d->urlAdded = false;

    if (_urls.isEmpty()) {
        return;
    }

    QStringList urls;
    QStringList::ConstIterator it = _urls.constBegin();

    // kill duplicates
    while (it != _urls.constEnd()) {
        if (!urls.contains(*it)) {
            urls += *it;
        }
        ++it;
    }

    // limit to myMaximum items
    /* Note: overload is an (old) C++ keyword, some compilers (KCC) choke
       on that, so call it Overload (capital 'O').  (matz) */
    int Overload = urls.count() - d->myMaximum + static_cast<int>(d->defaultList.size());
    while (Overload > 0) {
        if (remove == RemoveBottom) {
            if (!urls.isEmpty()) {
                urls.removeLast();
            }
        } else {
            if (!urls.isEmpty()) {
                urls.removeFirst();
            }
        }
        Overload--;
    }

    it = urls.constBegin();

    while (it != urls.constEnd()) {
        if ((*it).isEmpty()) {
            ++it;
            continue;
        }
        QUrl u;
        if (isAbsoluteLocalPath(*it)) {
            u = QUrl::fromLocalFile(*it);
        } else {
            u.setUrl(*it);
        }

        // Don't restore if file doesn't exist anymore
        if (u.isLocalFile() && !QFile::exists(u.toLocalFile())) {
            ++it;
            continue;
        }

        std::unique_ptr<KUrlComboBoxPrivate::KUrlComboItem> item(new KUrlComboBoxPrivate::KUrlComboItem(u, d->getIcon(u)));
        d->insertUrlItem(item.get());
        d->itemList.push_back(std::move(item));
        ++it;
    }
}

void KUrlComboBox::setUrl(const QUrl &url)
{
    if (url.isEmpty()) {
        return;
    }

    bool blocked = blockSignals(true);

    // check for duplicates
    auto mit = d->itemMapper.constBegin();
    QString urlToInsert = url.toString(QUrl::StripTrailingSlash);
    while (mit != d->itemMapper.constEnd()) {
        Q_ASSERT(mit.value());

        if (urlToInsert == mit.value()->url.toString(QUrl::StripTrailingSlash)) {
            setCurrentIndex(mit.key());

            if (d->myMode == Directories) {
                d->updateItem(mit.value(), mit.key(), d->opendirIcon);
            }

            blockSignals(blocked);
            return;
        }
        ++mit;
    }

    // not in the combo yet -> create a new item and insert it

    // first remove the old item
    if (d->urlAdded) {
        Q_ASSERT(!d->itemList.empty());
        d->itemList.pop_back();
        d->urlAdded = false;
    }

    setDefaults();

    const int offset = qMax(0, static_cast<int>(d->itemList.size() + d->defaultList.size()) - d->myMaximum);
    for (size_t i = offset; i < d->itemList.size(); ++i) {
        d->insertUrlItem(d->itemList.at(i).get());
    }

    std::unique_ptr<KUrlComboBoxPrivate::KUrlComboItem> item(new KUrlComboBoxPrivate::KUrlComboItem(url, d->getIcon(url)));

    const int id = count();
    const QString text = d->textForItem(item.get());
    if (d->myMode == Directories) {
        KComboBox::insertItem(id, d->opendirIcon, text);
    } else {
        KComboBox::insertItem(id, item->icon, text);
    }

    d->itemMapper.insert(id, item.get());
    d->itemList.push_back(std::move(item));

    setCurrentIndex(id);
    Q_ASSERT(!d->itemList.empty());
    d->urlAdded = true;
    blockSignals(blocked);
}

void KUrlComboBoxPrivate::_k_slotActivated(int index)
{
    auto item = itemMapper.value(index);

    if (item) {
        m_parent->setUrl(item->url);
        Q_EMIT m_parent->urlActivated(item->url);
    }
}

void KUrlComboBoxPrivate::insertUrlItem(const KUrlComboItem *item)
{
    Q_ASSERT(item);

// qDebug() << "insertURLItem " << d->textForItem(item);
    int id = m_parent->count();
    m_parent->KComboBox::insertItem(id, item->icon, textForItem(item));
    itemMapper.insert(id, item);
}

void KUrlComboBox::setMaxItems(int max)
{
    d->myMaximum = max;

    if (count() > d->myMaximum) {
        int oldCurrent = currentIndex();

        setDefaults();

        const int offset = qMax(0, static_cast<int>(d->itemList.size() + d->defaultList.size()) - d->myMaximum);
        for (size_t i = offset; i < d->itemList.size(); ++i) {
            d->insertUrlItem(d->itemList.at(i).get());
        }

        if (count() > 0) {   // restore the previous currentItem
            if (oldCurrent >= count()) {
                oldCurrent = count() - 1;
            }
            setCurrentIndex(oldCurrent);
        }
    }
}

int KUrlComboBox::maxItems() const
{
    return d->myMaximum;
}

void KUrlComboBox::removeUrl(const QUrl &url, bool checkDefaultURLs)
{
    auto mit = d->itemMapper.constBegin();
    while (mit != d->itemMapper.constEnd()) {
        if (url.toString(QUrl::StripTrailingSlash) == mit.value()->url.toString(QUrl::StripTrailingSlash)) {
            auto removePredicate = [&mit](const std::unique_ptr<const KUrlComboBoxPrivate::KUrlComboItem>& item) {
                return item.get() == mit.value();
            };
            d->itemList.erase(std::remove_if(d->itemList.begin(), d->itemList.end(), removePredicate), d->itemList.end());
            if (checkDefaultURLs) {
                d->defaultList.erase(std::remove_if(d->defaultList.begin(), d->defaultList.end(), removePredicate), d->defaultList.end());
            }
        }
        ++mit;
    }

    bool blocked = blockSignals(true);
    setDefaults();
    for (const auto& item : d->itemList) {
        d->insertUrlItem(item.get());
    }
    blockSignals(blocked);
}

void KUrlComboBox::setCompletionObject(KCompletion *compObj, bool hsig)
{
    if (compObj) {
        // on a url combo box we want completion matches to be sorted. This way, if we are given
        // a suggestion, we match the "best" one. For instance, if we have "foo" and "foobar",
        // and we write "foo", the match is "foo" and never "foobar". (ereslibre)
        compObj->setOrder(KCompletion::Sorted);
    }
    KComboBox::setCompletionObject(compObj, hsig);
}

void KUrlComboBox::mousePressEvent(QMouseEvent *event)
{
    QStyleOptionComboBox comboOpt;
    comboOpt.initFrom(this);
    const int x0 = QStyle::visualRect(layoutDirection(), rect(),
                                      style()->subControlRect(QStyle::CC_ComboBox, &comboOpt, QStyle::SC_ComboBoxEditField, this)).x();
    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &comboOpt, this);

    if (event->x() < (x0 + KIconLoader::SizeSmall + frameWidth)) {
        d->m_dragPoint = event->pos();
    } else {
        d->m_dragPoint = QPoint();
    }

    KComboBox::mousePressEvent(event);
}

void KUrlComboBox::mouseMoveEvent(QMouseEvent *event)
{
    const int index = currentIndex();
    auto item = d->itemMapper.value(index);

    if (item && !d->m_dragPoint.isNull() && event->buttons() & Qt::LeftButton &&
            (event->pos() - d->m_dragPoint).manhattanLength() > QApplication::startDragDistance()) {
        QDrag *drag = new QDrag(this);
        QMimeData *mime = new QMimeData();
        mime->setUrls(QList<QUrl>() << item->url);
        mime->setText(itemText(index));
        if (!itemIcon(index).isNull()) {
            drag->setPixmap(itemIcon(index).pixmap(KIconLoader::SizeMedium));
        }
        drag->setMimeData(mime);
        drag->exec();
    }

    KComboBox::mouseMoveEvent(event);
}

QIcon KUrlComboBoxPrivate::getIcon(const QUrl &url) const
{
    if (myMode == KUrlComboBox::Directories) {
        return dirIcon;
    } else {
        return QIcon::fromTheme(KIO::iconNameForUrl(url));
    }
}

// updates "item" with icon "icon"
// kdelibs4 used to also say "and sets the URL instead of text", but this breaks const-ness,
// now that it would require clearing the text, and I don't see the point since the URL was already in the text.
void KUrlComboBoxPrivate::updateItem(const KUrlComboItem *item,
                                     int index, const QIcon &icon)
{
    m_parent->setItemIcon(index, icon);
    m_parent->setItemText(index, textForItem(item));
}

#include "moc_kurlcombobox.cpp"
