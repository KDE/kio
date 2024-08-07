/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlnavigatorbutton_p.h"

#include "../utils_p.h"
#include "kurlnavigator.h"
#include "kurlnavigatormenu_p.h"
#include <kio/listjob.h>
#include <kio/statjob.h>

#include <KLocalizedString>
#include <KStringHandler>

#include <QCollator>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QStyleOption>
#include <QTimer>

namespace KDEPrivate
{
QPointer<KUrlNavigatorMenu> KUrlNavigatorButton::m_subDirsMenu;

KUrlNavigatorButton::KUrlNavigatorButton(const QUrl &url, KUrlNavigator *parent)
    : KUrlNavigatorButtonBase(parent)
    , m_hoverOverArrow(false)
    , m_hoverOverButton(false)
    , m_pendingTextChange(false)
    , m_replaceButton(false)
    , m_showMnemonic(false)
    , m_drawSeparator(true)
    , m_wheelSteps(0)
    , m_url(url)
    , m_subDir()
    , m_openSubDirsTimer(nullptr)
    , m_subDirsJob(nullptr)
    , m_padding(5)
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

    const bool startTextResolving = m_url.isValid() && !m_url.isLocalFile() && !protocolBlacklist.contains(m_url.scheme());

    if (startTextResolving) {
        m_pendingTextChange = true;
        KIO::StatJob *job = KIO::stat(m_url, KIO::HideProgressInfo);
        connect(job, &KJob::result, this, &KUrlNavigatorButton::statFinished);
        Q_EMIT startedTextResolving();
    } else {
        setText(m_url.fileName().replace(QLatin1Char('&'), QLatin1String("&&")));
    }
    setIcon(QIcon::fromTheme(KIO::iconNameForUrl(url)));
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
    // preferred width is textWidth, iconWidth and padding combined
    // add extra padding in end to make sure the space between divider and button is consistent
    // the first padding is used between icon and text, second in the end of text
    const int width = m_padding + textWidth() + arrowWidth() + m_padding;
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

void KUrlNavigatorButton::setDrawSeparator(bool draw)
{
    if (m_drawSeparator != draw) {
        m_drawSeparator = draw;
        update();
    }
}

bool KUrlNavigatorButton::drawSeparator() const
{
    return m_drawSeparator;
}

void KUrlNavigatorButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    QFont adjustedFont(font());
    adjustedFont.setBold(m_subDir.isEmpty());
    painter.setFont(adjustedFont);

    int buttonWidth = width();
    int arrowWidth = KUrlNavigatorButton::arrowWidth();

    int preferredWidth = sizeHint().width();
    if (preferredWidth < minimumWidth()) {
        preferredWidth = minimumWidth();
    }
    if (buttonWidth > preferredWidth) {
        buttonWidth = preferredWidth;
    }
    const int buttonHeight = height();
    const QColor fgColor = foregroundColor();
    const bool leftToRight = (layoutDirection() == Qt::LeftToRight);

    // Prepare sizes for icon
    QRect textRect;
    const int textRectWidth = buttonWidth - arrowWidth - m_padding;
    if (leftToRight) {
        textRect = QRect(m_padding, 0, textRectWidth, buttonHeight);
    } else {
        // If no separator is drawn, we can start writing text from 0
        textRect = QRect(m_drawSeparator ? arrowWidth : 0, 0, textRectWidth, buttonHeight);
    }

    drawHoverBackground(&painter);

    // Draw gradient overlay if text is clipped
    painter.setPen(fgColor);
    const bool clipped = isTextClipped();
    if (clipped) {
        QColor bgColor = fgColor;
        bgColor.setAlpha(0);
        QLinearGradient gradient(textRect.topLeft(), textRect.topRight());
        if (leftToRight) {
            gradient.setFinalStop(QPoint(gradient.finalStop().x() - m_padding, gradient.finalStop().y()));
            gradient.setColorAt(0.8, fgColor);
            gradient.setColorAt(1.0, bgColor);
        } else {
            gradient.setStart(QPoint(gradient.start().x() + m_padding, gradient.start().y()));
            gradient.setColorAt(0.0, bgColor);
            gradient.setColorAt(0.2, fgColor);
        }

        QPen pen;
        pen.setBrush(QBrush(gradient));
        painter.setPen(pen);
    }

    // Draw folder name
    int textFlags = Qt::AlignVCenter;
    if (m_showMnemonic) {
        textFlags |= Qt::TextShowMnemonic;
        painter.drawText(textRect, textFlags, text());
    } else {
        painter.drawText(textRect, textFlags, plainText());
    }

    // Draw separator arrow
    if (m_drawSeparator) {
        QStyleOption option;
        option.initFrom(this);
        option.palette = palette();
        option.palette.setColor(QPalette::Text, fgColor);
        option.palette.setColor(QPalette::WindowText, fgColor);
        option.palette.setColor(QPalette::ButtonText, fgColor);

        if (leftToRight) {
            option.rect = QRect(textRect.right(), 0, arrowWidth, buttonHeight);
        } else {
            // Separator is the first item in RtL mode
            option.rect = QRect(0, 0, arrowWidth, buttonHeight);
        }

        if (!m_hoverOverArrow) {
            option.state = QStyle::State_None;
        }
        style()->drawPrimitive(leftToRight ? QStyle::PE_IndicatorArrowRight : QStyle::PE_IndicatorArrowLeft, &option, &painter, this);
    }
}

void KUrlNavigatorButton::enterEvent(QEnterEvent *event)
{
    KUrlNavigatorButtonBase::enterEvent(event);

    // if the text is clipped due to a small window width, the text should
    // be shown as tooltip
    if (isTextClipped()) {
        setToolTip(plainText());
    }
    if (!m_hoverOverButton) {
        m_hoverOverButton = true;
        update();
    }
}

void KUrlNavigatorButton::leaveEvent(QEvent *event)
{
    KUrlNavigatorButtonBase::leaveEvent(event);
    setToolTip(QString());

    if (m_hoverOverArrow) {
        m_hoverOverArrow = false;
        update();
    }
    if (m_hoverOverButton) {
        m_hoverOverButton = false;
        update();
    }
}

void KUrlNavigatorButton::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        Q_EMIT navigatorButtonActivated(m_url, Qt::LeftButton, event->modifiers());
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

    if (isAboveSeparator(rect.center().x())) {
        m_hoverOverArrow = true;
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
        if (m_subDirsMenu) {
            m_subDirsMenu->deleteLater();
            m_subDirsMenu = nullptr;
        }
        m_hoverOverArrow = false;
        update();
    }
}

void KUrlNavigatorButton::dragLeaveEvent(QDragLeaveEvent *event)
{
    KUrlNavigatorButtonBase::dragLeaveEvent(event);

    m_hoverOverArrow = false;
    setDisplayHintEnabled(DraggedHint, false);
    update();
}

void KUrlNavigatorButton::mousePressEvent(QMouseEvent *event)
{
    if (isAboveSeparator(qRound(event->position().x())) && (event->button() == Qt::LeftButton)) {
        // the mouse is pressed above the folder button
        startSubDirsJob();
    }
    KUrlNavigatorButtonBase::mousePressEvent(event);
}

void KUrlNavigatorButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (!isAboveSeparator(qRound(event->position().x())) || (event->button() != Qt::LeftButton)) {
        // the mouse has been released above the text area and not
        // above the folder button
        Q_EMIT navigatorButtonActivated(m_url, event->button(), event->modifiers());
        cancelSubDirsRequest();
    }
    KUrlNavigatorButtonBase::mouseReleaseEvent(event);
}

void KUrlNavigatorButton::mouseMoveEvent(QMouseEvent *event)
{
    KUrlNavigatorButtonBase::mouseMoveEvent(event);

    const bool hoverOverIcon = isAboveSeparator(qRound(event->position().x()));
    if (hoverOverIcon != m_hoverOverArrow) {
        m_hoverOverArrow = hoverOverIcon;
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
    const KUrlNavigator *urlNavigator = qobject_cast<KUrlNavigator *>(parent());
    Q_ASSERT(urlNavigator);
    m_subDirsJob =
        KIO::listDir(url, KIO::HideProgressInfo, urlNavigator->showHiddenFolders() ? KIO::ListJob::ListFlag::IncludeHidden : KIO::ListJob::ListFlags{});
    m_subDirs.clear(); // just to be ++safe

    connect(m_subDirsJob, &KIO::ListJob::entries, this, &KUrlNavigatorButton::addEntriesToSubDirs);

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
            if (name != QLatin1String(".") && name != QLatin1String("..")) {
                m_subDirs.push_back({name, displayName});
            }
        }
    }
}

void KUrlNavigatorButton::slotUrlsDropped(QAction *action, QDropEvent *event)
{
    const int result = action->data().toInt();
    QUrl url(m_url);
    url.setPath(Utils::concatPaths(url.path(), m_subDirs.at(result).name));
    Q_EMIT urlsDroppedOnNavButton(url, event);
}

void KUrlNavigatorButton::slotMenuActionClicked(QAction *action, Qt::MouseButton button)
{
    const int result = action->data().toInt();
    QUrl url(m_url);
    url.setPath(Utils::concatPaths(url.path(), m_subDirs.at(result).name));
    Q_EMIT navigatorButtonActivated(url, button, Qt::NoModifier);
}

void KUrlNavigatorButton::statFinished(KJob *job)
{
    const KIO::UDSEntry entry = static_cast<KIO::StatJob *>(job)->statResult();

    if (m_pendingTextChange) {
        m_pendingTextChange = false;

        QString name = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
        if (name.isEmpty()) {
            name = m_url.fileName();
        }
        setText(name);

        Q_EMIT finishedTextResolving();
    }

    const QString iconName = entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME);
    if (!iconName.isEmpty()) {
        setIcon(QIcon::fromTheme(iconName));
    }
}

/*
 * Helper struct for sorting folder names
 */
struct FolderNameNaturalLessThan {
    FolderNameNaturalLessThan(bool sortHiddenLast)
        : m_sortHiddenLast(sortHiddenLast)
    {
        m_collator.setCaseSensitivity(Qt::CaseInsensitive);
        m_collator.setNumericMode(true);
    }

    bool operator()(const KUrlNavigatorButton::SubDirInfo &a, const KUrlNavigatorButton::SubDirInfo &b)
    {
        if (m_sortHiddenLast) {
            const bool isHiddenA = a.name.startsWith(QLatin1Char('.'));
            const bool isHiddenB = b.name.startsWith(QLatin1Char('.'));
            if (isHiddenA && !isHiddenB) {
                return false;
            }
            if (!isHiddenA && isHiddenB) {
                return true;
            }
        }
        return m_collator.compare(a.name, b.name) < 0;
    }

private:
    QCollator m_collator;
    bool m_sortHiddenLast;
};

void KUrlNavigatorButton::openSubDirsMenu(KJob *job)
{
    Q_ASSERT(job == m_subDirsJob);
    m_subDirsJob = nullptr;

    if (job->error() || m_subDirs.empty()) {
        // clear listing
        return;
    }

    const KUrlNavigator *urlNavigator = qobject_cast<KUrlNavigator *>(parent());
    Q_ASSERT(urlNavigator);
    FolderNameNaturalLessThan less(urlNavigator->showHiddenFolders() && urlNavigator->sortHiddenFoldersLast());
    std::sort(m_subDirs.begin(), m_subDirs.end(), less);
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
    const int popupX = leftToRight ? width() - arrowWidth() : 0;
    const QPoint popupPos = parentWidget()->mapToGlobal(geometry().bottomLeft() + QPoint(popupX, 0));

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

    if (job->error() || m_subDirs.empty()) {
        return;
    }

    const KUrlNavigator *urlNavigator = qobject_cast<KUrlNavigator *>(parent());
    Q_ASSERT(urlNavigator);
    FolderNameNaturalLessThan less(urlNavigator->showHiddenFolders() && urlNavigator->sortHiddenFoldersLast());
    std::sort(m_subDirs.begin(), m_subDirs.end(), less);

    // Get index of the directory that is shown currently in the button
    const QString currentDir = m_url.fileName();
    int currentIndex = 0;
    const int subDirsCount = m_subDirs.size();
    while (currentIndex < subDirsCount) {
        if (m_subDirs[currentIndex].name == currentDir) {
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
    url.setPath(Utils::concatPaths(url.path(), m_subDirs[targetIndex].name));
    Q_EMIT navigatorButtonActivated(url, Qt::LeftButton, Qt::NoModifier);

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

int KUrlNavigatorButton::textWidth() const
{
    QFont adjustedFont(font());
    adjustedFont.setBold(m_subDir.isEmpty());
    return QFontMetrics(adjustedFont).size(Qt::TextSingleLine, plainText()).width();
}

bool KUrlNavigatorButton::isAboveSeparator(int x) const
{
    const bool leftToRight = (layoutDirection() == Qt::LeftToRight);
    return leftToRight ? (x >= width() - arrowWidth()) : (x < arrowWidth() + m_padding);
}

bool KUrlNavigatorButton::isTextClipped() const
{
    // Ignore padding when resizing, so text doesnt go under it
    int availableWidth = width() - arrowWidth() - m_padding;

    return textWidth() >= availableWidth;
}

void KUrlNavigatorButton::updateMinimumWidth()
{
    const int oldMinWidth = minimumWidth();

    int minWidth = sizeHint().width();
    if (minWidth < 10) {
        minWidth = 10;
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
    connect(menu, &KUrlNavigatorMenu::mouseButtonClicked, this, &KUrlNavigatorButton::slotMenuActionClicked);
    connect(menu, &KUrlNavigatorMenu::urlsDropped, this, &KUrlNavigatorButton::slotUrlsDropped);

    // So that triggering a menu item with the keyboard works
    connect(menu, &QMenu::triggered, this, [this](QAction *act) {
        slotMenuActionClicked(act, Qt::LeftButton);
    });

    menu->setLayoutDirection(Qt::LeftToRight);

    const int maxIndex = startIndex + 30; // Don't show more than 30 items in a menu
    const int subDirsSize = m_subDirs.size();
    const int lastIndex = std::min(subDirsSize - 1, maxIndex);
    for (int i = startIndex; i <= lastIndex; ++i) {
        const auto &[subDirName, subDirDisplayName] = m_subDirs[i];
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
    if (subDirsSize > maxIndex) {
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
