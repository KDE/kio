/***************************************************************************
 *   Copyright (C) 2005 by Sean Harmer <sh@rama.homelinux.org>             *
 *                 2005 - 2007 Till Adam <adam@kde.org>                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by  the Free Software Foundation; either version 2 of the   *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.             *
 ***************************************************************************/
#ifndef KACLEDITWIDGET_P_H
#define KACLEDITWIDGET_P_H

#include <config-kiowidgets.h>

#if HAVE_POSIX_ACL || defined(Q_MOC_RUN)
#include <sys/acl.h>
#include <kacl.h>

#include <QDialog>
#include <QPixmap>
#include <QTreeWidget>
#include <QtCore/QHash>

#include <kcombobox.h>
#include <kfileitem.h>

class KACLListViewItem;
class QButtonGroup;
class KACLListView;
class QStackedWidget;
class QCheckBox;
class QAbstractButton;
class QColorGroup;

/**
@author Sean Harmer
*/
class KACLListView : public QTreeWidget
{
Q_OBJECT
    friend class KACLListViewItem;
public:
    enum Types
    {
        OWNER_IDX = 0,
        GROUP_IDX,
        OTHERS_IDX,
        MASK_IDX,
        NAMED_USER_IDX,
        NAMED_GROUP_IDX,
        LAST_IDX
    };
    enum EntryType { User = 1,
                     Group = 2,
                     Others = 4,
                     Mask = 8,
                     NamedUser = 16,
                     NamedGroup = 32,
                     AllTypes = 63 };

    KACLListView( QWidget* parent = 0 );
    ~KACLListView();

    bool hasMaskEntry() const { return m_hasMask; }
    bool hasDefaultEntries() const;
    bool allowDefaults() const { return m_allowDefaults; }
    void setAllowDefaults( bool v ) { m_allowDefaults = v; }
    unsigned short maskPermissions() const;
    void setMaskPermissions( unsigned short maskPerms );
    acl_perm_t maskPartialPermissions() const;
    void setMaskPartialPermissions( acl_perm_t maskPerms );

    bool maskCanBeDeleted() const;
    bool defaultMaskCanBeDeleted() const;

    const KACLListViewItem* findDefaultItemByType( EntryType type ) const;
    const KACLListViewItem* findItemByType( EntryType type,
                                            bool defaults = false ) const;
    unsigned short calculateMaskValue( bool defaults ) const;
    void calculateEffectiveRights();

    QStringList allowedUsers( bool defaults, KACLListViewItem *allowedItem = 0 );
    QStringList allowedGroups( bool defaults, KACLListViewItem *allowedItem = 0 );

    const KACL getACL() const { return getACL(); }
    KACL getACL();

    const KACL getDefaultACL() const { return getDefaultACL(); }
    KACL getDefaultACL();

    QPixmap getYesPixmap() const { return *m_yesPixmap; }
    QPixmap getYesPartialPixmap() const { return *m_yesPartialPixmap; }

public Q_SLOTS:
    void slotAddEntry();
    void slotEditEntry();
    void slotRemoveEntry();
    void setACL( const KACL &anACL );
    void setDefaultACL( const KACL &anACL );

protected Q_SLOTS:
    void slotItemClicked( QTreeWidgetItem* pItem, int col );
protected:
    void contentsMousePressEvent( QMouseEvent * e );

private:
    void fillItemsFromACL( const KACL &pACL, bool defaults = false );
    KACL itemsToACL( bool defaults ) const;

    KACL m_ACL;
    KACL m_defaultACL;
    unsigned short m_mask;
    bool m_hasMask;
    bool m_allowDefaults;
    QStringList m_allUsers;
    QStringList m_allGroups;
    QPixmap* m_yesPixmap;
    QPixmap* m_yesPartialPixmap;
};

class EditACLEntryDialog : public QDialog
{
    Q_OBJECT
public:
    EditACLEntryDialog( KACLListView *listView, KACLListViewItem *item,
                        const QStringList &users,
                        const QStringList &groups,
                        const QStringList &defaultUsers,
                        const QStringList &defaultGroups,
                        int allowedTypes = KACLListView::AllTypes,
                        int allowedDefaultTypes = KACLListView::AllTypes,
                        bool allowDefault = false );
    KACLListViewItem* item() const { return m_item; }
public Q_SLOTS:
     void slotOk();
     void slotSelectionChanged( QAbstractButton* );
private Q_SLOTS:
     void slotUpdateAllowedUsersAndGroups();
     void slotUpdateAllowedTypes();
private:
     KACLListView *m_listView;
     KACLListViewItem *m_item;
     QStringList m_users;
     QStringList m_groups;
     QStringList m_defaultUsers;
     QStringList m_defaultGroups;
     int m_allowedTypes;
     int m_allowedDefaultTypes;
     QButtonGroup *m_buttonGroup;
     KComboBox *m_usersCombo;
     KComboBox *m_groupsCombo;
     QStackedWidget *m_widgetStack;
     QCheckBox *m_defaultCB;
     QHash<QAbstractButton*, int> m_buttonIds;
};


class KACLListViewItem : public QTreeWidgetItem
{
public:
    KACLListViewItem( QTreeWidget* parent, KACLListView::EntryType type,
                      unsigned short value,
                      bool defaultEntry,
                      const QString& qualifier = QString() );
    virtual ~KACLListViewItem();
    QString key() const;
    bool operator< ( const QTreeWidgetItem & other ) const;

    void calcEffectiveRights();

    bool isDeletable() const;
    bool isAllowedToChangeType() const;

    void togglePerm( acl_perm_t perm );

#if 0
    virtual void paintCell( QPainter *p, const QColorGroup &cg,
                            int column, int width, int alignment );
#endif

    void updatePermPixmaps();
    void repaint();

    KACLListView::EntryType type;
    unsigned short value;
    bool isDefault;
    QString qualifier;
    bool isPartial;

private:
    KACLListView* m_pACLListView;
};


#endif
#endif
