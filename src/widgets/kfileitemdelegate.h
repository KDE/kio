/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2006-2007, 2008 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEITEMDELEGATE_H
#define KFILEITEMDELEGATE_H

#include "kiowidgets_export.h"
#include <KFileItem>
#include <QAbstractItemDelegate>
#include <QTextOption>

#include <memory>

class QAbstractItemModel;
class QAbstractItemView;
class QHelpEvent;
class QModelIndex;
class QPainter;

/*!
 * \class KFileItemDelegate
 * \inmodule KIOWidgets
 *
 * \brief KFileItemDelegate is intended to be used to provide a KDE file system
 * view, when using one of the standard item views in Qt with KDirModel.
 *
 * While primarily intended to be used with KDirModel, it uses
 * Qt::DecorationRole and Qt::DisplayRole for the icons and text labels,
 * just like QItemDelegate, and can thus be used with any standard model.
 *
 * When used with KDirModel however, KFileItemDelegate can change the way
 * the display and/or decoration roles are drawn, based on properties
 * of the file items. For example, if the file item is a symbolic link,
 * it will use an italic font to draw the file name.
 *
 * KFileItemDelegate also supports showing additional information about
 * the file items below the icon labels.
 *
 * Which information should be shown, if any, is controlled by the
 * information property, which is a list that can be set by calling
 * setShowInformation(), and read by calling showInformation().
 * By default this list is empty.
 *
 * To use KFileItemDelegate, instantiate an object from the delegate,
 * and call setItemDelegate() in one of the standard item views in Qt:
 *
 * \code
 * QListView *listview = new QListView(this);
 * KFileItemDelegate *delegate = new KFileItemDelegate(this);
 * listview->setItemDelegate(delegate);
 * \endcode
 */
class KIOWIDGETS_EXPORT KFileItemDelegate : public QAbstractItemDelegate
{
    Q_OBJECT

    /*!
     * \property KFileItemDelegate::information
     *
     * This property holds which additional information (if any) should be shown below
     * items in icon views.
     */
    Q_PROPERTY(InformationList information READ showInformation WRITE setShowInformation)

    /*!
     * \property KFileItemDelegate::shadowColor
     *
     * This property holds the color used for the text shadow.
     *
     * The alpha value in the color determines the opacity of the shadow.
     * Shadows are only rendered when the alpha value is non-zero.
     * The default value for this property is Qt::transparent.
     */
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor)

    /*!
     * \property KFileItemDelegate::shadowOffset
     *
     * This property holds the horizontal and vertical offset for the text shadow.
     * The default value for this property is (1, 1).
     */
    Q_PROPERTY(QPointF shadowOffset READ shadowOffset WRITE setShadowOffset)

    /*!
     * \property KFileItemDelegate::shadowBlur
     *
     * This property holds the blur radius for the text shadow.
     * The default value for this property is 2.
     */
    Q_PROPERTY(qreal shadowBlur READ shadowBlur WRITE setShadowBlur)

    /*!
     * \property KFileItemDelegate::maximumSize
     *
     * This property holds the maximum size that can be returned
     * by KFileItemDelegate::sizeHint(). If the maximum size is empty,
     * it will be ignored.
     */
    Q_PROPERTY(QSize maximumSize READ maximumSize WRITE setMaximumSize)

    /*!
     * \property KFileItemDelegate::showToolTipWhenElided
     *
     * This property determines whether a tooltip will be shown by the delegate
     * if the display role is elided. This tooltip will contain the full display
     * role information. The tooltip will only be shown if the Qt::ToolTipRole differs
     * from Qt::DisplayRole, or if they match, showToolTipWhenElided flag is set and
     * the display role information is elided.
     */
    Q_PROPERTY(bool showToolTipWhenElided READ showToolTipWhenElided WRITE setShowToolTipWhenElided)

    /*!
     * \property KFileItemDelegate::jobTransfersVisible
     *
     * This property determines if there are KIO jobs on a destination URL visible, then
     * they will have a small animation overlay displayed on them.
     */
    Q_PROPERTY(bool jobTransfersVisible READ jobTransfersVisible WRITE setJobTransfersVisible)

public:
    /*!
     * This enum defines the additional information that can be displayed below item
     * labels in icon views.
     *
     * The information will only be shown for indexes for which the model provides
     * a valid value for KDirModel::FileItemRole, and only when there's sufficient vertical
     * space to display at least one line of the information, along with the display label.
     *
     * For the number of items to be shown for folders, the model must provide a valid
     * value for KDirMode::ChildCountRole, in addition to KDirModel::FileItemRole.
     *
     * Note that KFileItemDelegate will not call KFileItem::determineMimeType() if
     * KFileItem::isMimeTypeKnown() returns false, so if you want to display MIME types
     * you should use a KMimeTypeResolver with the model and the view, to ensure that MIME
     * types are resolved. If the MIME type isn't known, "Unknown" will be displayed until
     * the MIME type has been successfully resolved.
     *
     * \value NoInformation No additional information will be shown for items.
     * \value Size The file size for files, and the number of items for folders.
     * \value Permissions A UNIX permissions string, e.g. -rwxr-xr-x.
     * \value OctalPermissions The permissions as an octal value, e.g. 0644.
     * \value Owner  The user name of the file owner, e.g. root
     * \value OwnerAndGroup The user and group that owns the file, e.g. root:root
     * \value CreationTime The date and time the file/folder was created.
     * \value ModificationTime The date and time the file/folder was last modified.
     * \value AccessTime The date and time the file/folder was last accessed.
     * \value MimeType The MIME type for the item, e.g. text/html.
     * \value FriendlyMimeType The descriptive name for the MIME type, e.g. HTML Document.
     * \value[since 4.5] LinkDest The destination of a symbolic link.
     * \value[since 4.5] LocalPathOrUrl The local path to the file or the URL in case it is not a local file.
     * \value[since 4.6] Comment A simple comment that can be displayed to the user as is.
     *
     * \sa setShowInformation()
     * \sa showInformation()
     * \sa information
     */
    enum Information {
        NoInformation,
        Size,
        Permissions,
        OctalPermissions,
        Owner,
        OwnerAndGroup,
        CreationTime,
        ModificationTime,
        AccessTime,
        MimeType,
        FriendlyMimeType,
        LinkDest,
        LocalPathOrUrl,
        Comment,
    };
    Q_ENUM(Information)

    /*!
     * \typedef KFileItemDelegate::InformationList
     */
    typedef QList<Information> InformationList;

    /*!
     * Constructs a new KFileItemDelegate.
     *
     * \a parent The parent object for the delegate.
     */
    explicit KFileItemDelegate(QObject *parent = nullptr);

    /*!
     * Destroys the item delegate.
     */
    ~KFileItemDelegate() override;

    /*!
     * Returns the nominal size for the item referred to by \a index, given the
     * provided options.
     *
     * If the model provides a valid Qt::FontRole and/or Qt::TextAlignmentRole for the item,
     * those will be used instead of the ones specified in the style options.
     *
     * This function is reimplemented from QAbstractItemDelegate.
     *
     * \a option  The style options that should be used when painting the item.
     *
     * \a index   The index to the item for which to return the size hint.
     */
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /*!
     * Paints the item indicated by \a index, using \a painter.
     *
     * The item will be drawn in the rectangle specified by option.rect.
     * The correct size for that rectangle can be obtained by calling
     * sizeHint().
     *
     * This function will use the following data values if the model provides
     * them for the item, in place of the values in \a option:
     *
     * \list
     * \li Qt::FontRole           The font that should be used for the display role.
     * \li Qt::TextAlignmentRole  The alignment of the display role.
     * \li Qt::ForegroundRole     The text color for the display role.
     * \li Qt::BackgroundRole     The background color for the item.
     * \endlist
     *
     * This function is reimplemented from QAbstractItemDelegate.
     *
     * \a painter The painter with which to draw the item.
     *
     * \a option  The style options that should be used when painting the item.
     *
     * \a index   The index to the item that should be painted.
     */
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /*!
     * Sets the list of information lines that are shown below the icon label in list views.
     *
     * You will typically construct the list like this:
     * \code
     * KFileItemDelegate::InformationList list;
     * list << KFileItemDelegate::FriendlyMimeType << KFileItemDelegate::Size;
     * delegate->setShowInformation(list);
     * \endcode
     *
     * The information lines will be displayed in the list order.
     * The delegate will first draw the item label, and then as many information
     * lines as will fit in the available space.
     *
     * \a list A list of information items that should be shown
     */
    void setShowInformation(const InformationList &list);

    /*!
     * Sets a single information line that is shown below the icon label in list views.
     *
     * This is a convenience function for when you only want to show a single line
     * of information.
     *
     * \a information The information that should be shown
     */
    void setShowInformation(Information information);

    /*!
     * Returns the file item information that should be shown below item labels in list views.
     */
    InformationList showInformation() const;

    /*!
     * Sets the color used for drawing the text shadow.
     *
     * To enable text shadows, set the shadow color to a non-transparent color.
     * To disable text shadows, set the color to Qt::transparent.
     *
     * \sa shadowColor()
     */
    void setShadowColor(const QColor &color);

    /*!
     * Returns the color used for the text shadow.
     *
     * \sa setShadowColor()
     */
    QColor shadowColor() const;

    /*!
     * Sets the horizontal and vertical offset for the text shadow.
     *
     * \sa shadowOffset()
     */
    void setShadowOffset(const QPointF &offset);

    /*!
     * Returns the offset used for the text shadow.
     *
     * \sa setShadowOffset()
     */
    QPointF shadowOffset() const;

    /*!
     * Sets the blur radius for the text shadow.
     *
     * \sa shadowBlur()
     */
    void setShadowBlur(qreal radius);

    /*!
     * Returns the blur radius for the text shadow.
     *
     * \sa setShadowBlur()
     */
    qreal shadowBlur() const;

    /*!
     * Sets the maximum size for KFileItemDelegate::sizeHint().
     *
     * \sa maximumSize()
     */
    void setMaximumSize(const QSize &size);

    /*!
     * Returns the maximum size for KFileItemDelegate::sizeHint().
     *
     * \sa setMaximumSize()
     */
    QSize maximumSize() const;

    /*!
     * Sets whether a tooltip should be shown if the display role is
     * elided containing the full display role information.
     *
     * \note The tooltip will only be shown if the Qt::ToolTipRole differs
     *       from Qt::DisplayRole, or if they match, showToolTipWhenElided
     *       flag is set and the display role information is elided.
     * \sa showToolTipWhenElided()
     */
    void setShowToolTipWhenElided(bool showToolTip);

    /*!
     * Returns whether a tooltip should be shown if the display role
     * is elided containing the full display role information.
     *
     * \note The tooltip will only be shown if the Qt::ToolTipRole differs
     *       from Qt::DisplayRole, or if they match, showToolTipWhenElided
     *       flag is set and the display role information is elided.
     * \sa setShowToolTipWhenElided()
     */
    bool showToolTipWhenElided() const;

    /*!
     * Returns the rectangle of the icon that is aligned inside the decoration
     * rectangle.
     */
    QRect iconRect(const QStyleOptionViewItem &option, const QModelIndex &index) const;

    /*!
     * When the contents text needs to be wrapped, \a wrapMode strategy
     * will be followed.
     *
     */
    void setWrapMode(QTextOption::WrapMode wrapMode);

    /*!
     * Returns the wrapping strategy followed to show text when it needs
     * wrapping.
     *
     */
    QTextOption::WrapMode wrapMode() const;

    /*!
     * Enable/Disable the displaying of an animated overlay that is shown for any destination
     * urls (in the view). When enabled, the animations (if any) will be drawn automatically.
     *
     * Only the files/folders that are visible and have jobs associated with them
     * will display the animation.
     * You would likely not want this enabled if you perform some kind of custom painting
     * that takes up a whole item, and will just make this(and what you paint) look funky.
     *
     * Default is disabled.
     *
     * Note: The model (KDirModel) needs to have it's method called with the same
     * value, when you make the call to this method.
     */
    void setJobTransfersVisible(bool jobTransfersVisible);

    /*!
     * Returns whether or not the displaying of job transfers is enabled.
     * \sa setJobTransfersVisible()
     */
    bool jobTransfersVisible() const;

    bool eventFilter(QObject *object, QEvent *event) override;

    /*!
     * Returns the rectangle where selectionEmblem is being drawn
     *
     * \since 6.14
     */
    QRect selectionEmblemRect() const;

    /*!
     * Set the rectangle where selectionEmblem should be drawn in.
     *
     * \since 6.14
     */
    void setSelectionEmblemRect(QRect rect, int iconSize);

    /*!
     * \since 6.14
     */
    KFileItem fileItem(const QModelIndex &index) const;

public Q_SLOTS:
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override;

    /*!
     * Returns the shape of the item as a region.
     *
     * The returned region can be used for precise hit testing of the item.
     */
    QRegion shape(const QStyleOptionViewItem &option, const QModelIndex &index);

private:
    class Private;
    std::unique_ptr<Private> const d; /// \internal
    Q_DISABLE_COPY(KFileItemDelegate)

    void drawSelectionEmblem(QStyleOptionViewItem option, QPainter *painter, const QModelIndex &index) const;
};

#endif // KFILEITEMDELEGATE_H
