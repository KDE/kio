/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlnavigatorbutton_p.h"

#include "kurlnavigator.h"
#include "kurlnavigatormenu_p.h"
#include "kdirsortfilterproxymodel.h"
#include "../pathhelpers_p.h"

#include <kio/job.h>
#include <KLocalizedString>
#include <KStringHandler>

#include <QTimer>
#include <QPainter>
#include <QKeyEvent>
#include <QStyleOption>
#include <QMimeData>
#include <QCollator>

namespace KDEPrivate
{

QPointer<KUrlNavigatorMenu> KUrlNavigatorButton::m_subDirsMenu;

KUrlNavigatorButton::KUrlNavigatorButton(const QUrl &url, KUrlNavigator *parent) :
    KUrlNavigatorButtonBase(parent),
    m_hoverArrow(false),
    m_pendingTextChange(false),
    m_replaceButton(false),
    m_showMnemonic(false),
    m_wheelSteps(0),
    m_url(url),
    m_subDir(),
    m_openSubDirsTimer(nullptr),
    m_subDirsJob(nullptr)
{
    setAcceptDrops(true);
    setUrl(url);
    setMouseTracking(true);

    m_openSubDirsTimer = new QTimer(this);
    m_openSubDirsTimer->setSingleShot(true);
    m_openSubDirsTimer->setInterval(300);
    connect(m_openSubDirsTimer, &QTimer::timeout, this, &KUrlNavigatorButton::startSubDirsJob);

    connect(this, &QAbstractButton::pressed, this, &KUrlNavigatorButton::requestSubDirs);
}

KUrlNavigatorButton::~KUrlNavigatorButton()
{
}

void KUrlNavigatorButton::setUrl(const QUrl &url)
{
    m_url = url;

    // Doing a text-resolving with KIO::stat() for all non-local
    // URLs leads to problems for protocols where a limit is given for
    // the number of parallel connections. A black-list
    // is given where KIO::stat() should not be used:
    static const QSet<QString> protocolBlacklist = QSet<QString>{
        QStringLiteral("nfs"),
        QStringLiteral("fish"),
        QStringLiteral("ftp"),
        QStringLiteral("sftp"),
        QStringLiteral("smb"),
        QStringLiteral("webdav"),
        QStringLiteral("mtp"),
    };

    const bool startTextResolving = m_url.isValid() && !m_url.isLocalFile()
                                && !protocolBlacklist.contains(m_url.scheme());

    if (startTextResolving) {
        m_pendingTextChange = true;
        KIO::StatJob *job = KIO::stat(m_url, KIO::HideProgressInfo);
        connect(job, &KJob::result,
                this, &KUrlNavigatorButton::statFinished);
        Q_EMIT startedTextResolving();
    } else {
        setText(m_url.fileName().replace(QLatin1Char('&'), QLatin1String("&&")));
    }
}

QUrl KUrlNavigatorButton::url() const
{
    return m_url;
}

void KUrlNavigatorButton::setText(const QString &text)
{
    QString adjustedText = text;
    if (adjustedText.isEmpty()) {
        adjustedText = m_url.scheme();
    }
    // Assure that the button always consists of one line
    adjustedText.remove(QLatin1Char('\n'));

    KUrlNavigatorButtonBase::setText(adjustedText);
    updateMinimumWidth();

    // Assure that statFinished() does not overwrite a text that has been
    // set by a client of the URL navigator button
    m_pendingTextChange = false;
}

void KUrlNavigatorButton::setActiveSubDirectory(const QString &subDir)
{
    m_subDir = subDir;

    // We use a different (bold) font on active, so the size hint changes
    updateGeometry();
    update();
}

QString KUrlNavigatorButton::activeSubDirectory() const
{
    return m_subDir;
}

QSize KUrlNavigatorButton::sizeHint() const
{
    QFont adjustedFont(font());
    adjustedFont.setBold(m_subDir.isEmpty());
    // the minimum size is textWidth + arrowWidth() + 2 * BorderWidth; for the
    // preferred size we add the BorderWidth 2 times again for having an uncluttered look
    const int width = QFontMetrics(adjustedFont).size(Qt::TextSingleLine, plainText()).width() + arrowWidth() + 4 * BorderWidth;
    return QSize(width, KUrlNavigatorButtonBase::sizeHint().height());
}

void KUrlNavigatorButton::setShowMnemonic(bool show)
{
    if (m_showMnemonic != show) {
        m_showMnemonic = show;
        update();
    }
}

bool KUrlNavigatorButton::showMnemonic() const
{
    return m_showMnemonic;
}

void KUrlNavigatorButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    QFont adjustedFont(font());
    adjustedFont.setBold(m_subDir.isEmpty());
    painter.setFont(adjustedFont);

    int buttonWidth  = width();
    int preferredWidth = sizeHint().width();
    if (preferredWidth < minimumWidth()) {
        preferredWidth = minimumWidth();
    }
    if (buttonWidth > preferredWidth) {
        buttonWidth = preferredWidth;
    }
    const int buttonHeight = height();

    const QColor fgColor = foregroundColor();
    drawHoverBackground(&painter);

    int textLeft = 0;
    int textWidth = buttonWidth;

    const bool leftToRight = (layoutDirection() == Qt::LeftToRight);

    if (!m_subDir.isEmpty()) {
        // draw arrow
        const int arrowSize = arrowWidth();
        const int arrowX = leftToRight ? (buttonWidth - arrowSize) - BorderWidth : BorderWidth;
        const int arrowY = (buttonHeight - arrowSize) / 2;

        QStyleOption option;
        option.initFrom(this);
        option.rect = QRect(arrowX, arrowY, arrowSize, arrowSize);
        option.palette = palette();
        option.palette.setColor(QPalette::Text, fgColor);
        option.palette.setColor(QPalette::WindowText, fgColor);
        option.palette.setColor(QPalette::ButtonText, fgColor);

        if (m_hoverArrow) {
            // highlight the background of the arrow to indicate that the directories
            // popup can be opened by a mouse click
            QColor hoverColor = palette().color(QPalette::HighlightedText);
            hoverColor.setAlpha(96);
            painter.setPen(Qt::NoPen);
            painter.setBrush(hoverColor);

            int hoverX = arrowX;
            if (!leftToRight) {
                hoverX -= BorderWidth;
            }
            painter.drawRect(QRect(hoverX, 0, arrowSize + BorderWidth, buttonHeight));
        }

        if (leftToRight) {
            style()->drawPrimitive(QStyle::PE_IndicatorArrowRight, &option, &painter, this);
        } else {
            style()->drawPrimitive(QStyle::PE_IndicatorArrowLeft, &option, &painter, this);
            textLeft += arrowSize + 2 * BorderWidth;
        }

        textWidth -= arrowSize + 2 * BorderWidth;
    }

    painter.setPen(fgColor);
    const bool clipped = isTextClipped();
    const QRect textRect(textLeft, 0, textWidth, buttonHeight);
    if (clipped) {
        QColor bgColor = fgColor;
        bgColor.setAlpha(0);
        QLinearGradient gradient(textRect.topLeft(), textRect.topRight());
        if (leftToRight) {
            gradient.setColorAt(0.8, fgColor);
            gradient.setColorAt(1.0, bgColor);
        } else {
            gradient.setColorAt(0.0, bgColor);
            gradient.setColorAt(0.2, fgColor);
        }

        QPen pen;
        pen.setBrush(QBrush(gradient));
        painter.setPen(pen);
    }

    int textFlags = clipped ? Qt::AlignVCenter : Qt::AlignCenter;
    if (m_showMnemonic) {
        textFlags |= Qt::TextShowMnemonic;
        painter.drawText(textRect, textFlags, text());
    } else {
        painter.drawText(textRect, textFlags, plainText());
    }
}

void KUrlNavigatorButton::enterEvent(QEvent *event)
{
    KUrlNavigatorButtonBase::enterEvent(event);

    // if the text is clipped due to a small window width, the text should
    // be shown as tooltip
    if (isTextClipped()) {
        setToolTip(plainText());
    }
}

void KUrlNavigatorButton::leaveEvent(QEvent *event)
{
    KUrlNavigatorButtonBase::leaveEvent(event);
    setToolTip(QString());

    if (m_hoverArrow) {
        m_hoverArrow = false;
        update();
    }
}

void KUrlNavigatorButton::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        Q_EMIT clicked(m_url, Qt::LeftButton, event->modifiers());
        break;
    case Qt::Key_Down:
    case Qt::Key_Space:
        startSubDirsJob();
        break;
    default:
        KUrlNavigatorButtonBase::keyPressEvent(event);
    }
}

void KUrlNavigatorButton::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        setDisplayHintEnabled(DraggedHint, true);

        Q_EMIT urlsDroppedOnNavButton(m_url, event);

        setDisplayHintEnabled(DraggedHint, false);
        update();
    }
}

void KUrlNavigatorButton::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        setDisplayHintEnabled(DraggedHint, true);
        event->acceptProposedAction();

        update();
    }
}

void KUrlNavigatorButton::dragMoveEvent(QDragMoveEvent *event)
{
    QRect rect = event->answerRect();
    if (isAboveArrow(rect.center().x())) {
        m_hoverArrow = true;
        update();

        if (m_subDirsMenu == nullptr) {
            requestSubDirs();
        } else if (m_subDirsMenu->parent() != this) {
            m_subDirsMenu->close();
            m_subDirsMenu->deleteLater();
            m_subDirsMenu = nullptr;

            requestSubDirs();
        }
    } else {
        if (m_openSubDirsTimer->isActive()) {
            cancelSubDirsRequest();
        }
        m_subDirsMenu->deleteLater();
        m_subDirsMenu = nullptr;
        m_hoverArrow = false;
        update();
    }
}

void KUrlNavigatorButton::dragLeaveEvent(QDragLeaveEvent *event)
{
    KUrlNavigatorButtonBase::dragLeaveEvent(event);

    m_hoverArrow = false;
    setDisplayHintEnabled(DraggedHint, false);
    update();
}

void KUrlNavigatorButton::mousePressEvent(QMouseEvent *event)
{
    if (isAboveArrow(event->x()) && (event->button() == Qt::LeftButton)) {
        // the mouse is pressed above the [>] button
        startSubDirsJob();
    }
    KUrlNavigatorButtonBase::mousePressEvent(event);
}

void KUrlNavigatorButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (!isAboveArrow(event->x()) || (event->button() != Qt::LeftButton)) {
        // the mouse has been released above the text area and not
        // above the [>] button
        Q_EMIT clicked(m_url, event->button(), event->modifiers());
        cancelSubDirsRequest();
    }
    KUrlNavigatorButtonBase::mouseReleaseEvent(event);
}

void KUrlNavigatorButton::mouseMoveEvent(QMouseEvent *event)
{
    KUrlNavigatorButtonBase::mouseMoveEvent(event);

    const bool hoverArrow = isAboveArrow(event->x());
    if (hoverArrow != m_hoverArrow) {
        m_hoverArrow = hoverArrow;
        update();
    }
}

void KUrlNavigatorButton::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() != 0) {
        m_wheelSteps = event->angleDelta().y() / 120;
        m_replaceButton = true;
        startSubDirsJob();
    }

    KUrlNavigatorButtonBase::wheelEvent(event);
}

void KUrlNavigatorButton::requestSubDirs()
{
    if (!m_openSubDirsTimer->isActive() && (m_subDirsJob == nullptr)) {
        m_openSubDirsTimer->start();
    }
}

void KUrlNavigatorButton::startSubDirsJob()
{
    if (m_subDirsJob != nullptr) {
        return;
    }

    const QUrl url = m_replaceButton ? KIO::upUrl(m_url) : m_url;
    m_subDirsJob = KIO::listDir(url, KIO::HideProgressInfo, false /*no hidden files*/);
    m_subDirs.clear(); // just to be ++safe

    connect(m_subDirsJob, &KIO::ListJob::entries,
            this, &KUrlNavigatorButton::addEntriesToSubDirs);

    if (m_replaceButton) {
        connect(m_subDirsJob, &KJob::result, this, &KUrlNavigatorButton::replaceButton);
    } else {
        connect(m_subDirsJob, &KJob::result, this, &KUrlNavigatorButton::openSubDirsMenu);
    }
}

void KUrlNavigatorButton::addEntriesToSubDirs(KIO::Job *job, const KIO::UDSEntryList &entries)
{
    Q_ASSERT(job == m_subDirsJob);
    Q_UNUSED(job);

    for (const KIO::UDSEntry &entry : entries) {
        if (entry.isDir()) {
            const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
            QString displayName = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
            if (displayName.isEmpty()) {
                displayName = name;
            }
            if ((name != QLatin1String(".")) && (name != QLatin1String(".."))) {
                m_subDirs.append(qMakePair(name, displayName));
            }
        }
    }
}

void KUrlNavigatorButton::slotUrlsDropped(QAction *action, QDropEvent *event)
{
    const int result = action->data().toInt();
    QUrl url(m_url);
    url.setPath(concatPaths(url.path(), m_subDirs.at(result).first));
    Q_EMIT urlsDroppedOnNavButton(url, event);
}

void KUrlNavigatorButton::slotMenuActionClicked(QAction *action, Qt::MouseButton button)
{
    const int result = action->data().toInt();
    QUrl url(m_url);
    url.setPath(concatPaths(url.path(), m_subDirs.at(result).first));
    Q_EMIT clicked(url, button, Qt::NoModifier);
}

void KUrlNavigatorButton::statFinished(KJob *job)
{
    if (m_pendingTextChange) {
        m_pendingTextChange = false;

        const KIO::UDSEntry entry = static_cast<KIO::StatJob *>(job)->statResult();
        QString name = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
        if (name.isEmpty()) {
            name = m_url.fileName();
        }
        setText(name);

        Q_EMIT finishedTextResolving();
    }
}

/**
 * Helper class for openSubDirsMenu
 */
class NaturalLessThan
{
public:
    NaturalLessThan()
    {
        m_collator.setCaseSensitivity(Qt::CaseInsensitive);
        m_collator.setNumericMode(true);
    }

    bool operator()(const QPair<QString, QString> &s1, const QPair<QString, QString> &s2)
    {
        return m_collator.compare(s1.first, s2.first) < 0;
    }

private:
    QCollator m_collator;
};

void KUrlNavigatorButton::openSubDirsMenu(KJob *job)
{
    Q_ASSERT(job == m_subDirsJob);
    m_subDirsJob = nullptr;

    if (job->error() || m_subDirs.isEmpty()) {
        // clear listing
        return;
    }

    NaturalLessThan nlt;
    std::sort(m_subDirs.begin(), m_subDirs.end(), nlt);
    setDisplayHintEnabled(PopupActiveHint, true);
    update(); // ensure the button is drawn highlighted

    if (m_subDirsMenu != nullptr) {
        m_subDirsMenu->close();
        m_subDirsMenu->deleteLater();
        m_subDirsMenu = nullptr;
    }

    m_subDirsMenu = new KUrlNavigatorMenu(this);
    initMenu(m_subDirsMenu, 0);

    const bool leftToRight = (layoutDirection() == Qt::LeftToRight);
    const int popupX = leftToRight ? width() - arrowWidth() - BorderWidth : 0;
    const QPoint popupPos  = parentWidget()->mapToGlobal(geometry().bottomLeft() + QPoint(popupX, 0));

    QPointer<QObject> guard(this);

    m_subDirsMenu->exec(popupPos);

    // If 'this' has been deleted in the menu's nested event loop, we have to return
    // immediately because any access to a member variable might cause a crash.
    if (!guard) {
        return;
    }

    m_subDirs.clear();
    delete m_subDirsMenu;
    m_subDirsMenu = nullptr;

    setDisplayHintEnabled(PopupActiveHint, false);
}

void KUrlNavigatorButton::replaceButton(KJob *job)
{
    Q_ASSERT(job == m_subDirsJob);
    m_subDirsJob = nullptr;
    m_replaceButton = false;

    if (job->error() || m_subDirs.isEmpty()) {
        return;
    }

    NaturalLessThan nlt;
    std::sort(m_subDirs.begin(), m_subDirs.end(), nlt);

    // Get index of the directory that is shown currently in the button
    const QString currentDir = m_url.fileName();
    int currentIndex = 0;
    const int subDirsCount = m_subDirs.count();
    while (currentIndex < subDirsCount) {
        if (m_subDirs[currentIndex].first == currentDir) {
            break;
        }
        ++currentIndex;
    }

    // Adjust the index by respecting the wheel steps and
    // trigger a replacing of the button content
    int targetIndex = currentIndex - m_wheelSteps;
    if (targetIndex < 0) {
        targetIndex = 0;
    } else if (targetIndex >= subDirsCount) {
        targetIndex = subDirsCount - 1;
    }

    QUrl url(KIO::upUrl(m_url));
    url.setPath(concatPaths(url.path(), m_subDirs[targetIndex].first));
    Q_EMIT clicked(url, Qt::LeftButton, Qt::NoModifier);

    m_subDirs.clear();
}

void KUrlNavigatorButton::cancelSubDirsRequest()
{
    m_openSubDirsTimer->stop();
    if (m_subDirsJob != nullptr) {
        m_subDirsJob->kill();
        m_subDirsJob = nullptr;
    }
}

QString KUrlNavigatorButton::plainText() const
{
    // Replace all "&&" by '&' and remove all single
    // '&' characters
    const QString source = text();
    const int sourceLength = source.length();

    QString dest;
    dest.resize(sourceLength);

    int sourceIndex = 0;
    int destIndex = 0;
    while (sourceIndex < sourceLength) {
        if (source.at(sourceIndex) == QLatin1Char('&')) {
            ++sourceIndex;
            if (sourceIndex >= sourceLength) {
                break;
            }
        }
        dest[destIndex] = source.at(sourceIndex);
        ++sourceIndex;
        ++destIndex;
    }

    dest.resize(destIndex);

    return dest;
}

int KUrlNavigatorButton::arrowWidth() const
{
    // if there isn't arrow then return 0
    int width = 0;
    if (!m_subDir.isEmpty()) {
        width = height() / 2;
        if (width < 4) {
            width = 4;
        }
    }

    return width;
}

bool KUrlNavigatorButton::isAboveArrow(int x) const
{
    const bool leftToRight = (layoutDirection() == Qt::LeftToRight);
    return leftToRight ? (x >= width() - arrowWidth()) : (x < arrowWidth());
}

bool KUrlNavigatorButton::isTextClipped() const
{
    int availableWidth = width() - 2 * BorderWidth;
    if (!m_subDir.isEmpty()) {
        availableWidth -= arrowWidth() - BorderWidth;
    }

    QFont adjustedFont(font());
    adjustedFont.setBold(m_subDir.isEmpty());
    return QFontMetrics(adjustedFont).size(Qt::TextSingleLine, plainText()).width() >= availableWidth;
}

void KUrlNavigatorButton::updateMinimumWidth()
{
    const int oldMinWidth = minimumWidth();

    int minWidth = sizeHint().width();
    if (minWidth < 40) {
        minWidth = 40;
    } else if (minWidth > 150) {
        // don't let an overlong path name waste all the URL navigator space
        minWidth = 150;
    }
    if (oldMinWidth != minWidth) {
        setMinimumWidth(minWidth);
    }
}

void KUrlNavigatorButton::initMenu(KUrlNavigatorMenu *menu, int startIndex)
{
    connect(menu, &KUrlNavigatorMenu::mouseButtonClicked,
            this, &KUrlNavigatorButton::slotMenuActionClicked);
    connect(menu, &KUrlNavigatorMenu::urlsDropped, this, &KUrlNavigatorButton::slotUrlsDropped);

    // So that triggering a menu item with the keyboard works
    connect(menu, &QMenu::triggered, this, [this](QAction *act) {
        slotMenuActionClicked(act, Qt::LeftButton);
    });

    menu->setLayoutDirection(Qt::LeftToRight);

    const int maxIndex = startIndex + 30;  // Don't show more than 30 items in a menu
    const int lastIndex = qMin(m_subDirs.count() - 1, maxIndex);
    for (int i = startIndex; i <= lastIndex; ++i) {
        const QString subDirName = m_subDirs[i].first;
        const QString subDirDisplayName = m_subDirs[i].second;
        QString text = KStringHandler::csqueeze(subDirDisplayName, 60);
        text.replace(QLatin1Char('&'), QLatin1String("&&"));
        QAction *action = new QAction(text, this);
        if (m_subDir == subDirName) {
            QFont font(action->font());
            font.setBold(true);
            action->setFont(font);
        }
        action->setData(i);
        menu->addAction(action);
    }
    if (m_subDirs.count() > maxIndex) {
        // If too much items are shown, move them into a sub menu
        menu->addSeparator();
        KUrlNavigatorMenu *subDirsMenu = new KUrlNavigatorMenu(menu);
        subDirsMenu->setTitle(i18nc("@action:inmenu", "More"));
        initMenu(subDirsMenu, maxIndex);
        menu->addMenu(subDirsMenu);
    }
}

} // namespace KDEPrivate

#include "moc_kurlnavigatorbutton_p.cpp"
