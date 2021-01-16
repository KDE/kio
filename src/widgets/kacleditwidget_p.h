/*
    SPDX-FileCopyrightText: 2005 Sean Harmer <sh@rama.homelinux.org>
    SPDX-FileCopyrightText: 2005-2007 Till Adam <adam@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KACLEDITWIDGET_P_H
#define KACLEDITWIDGET_P_H

#include <config-kiowidgets.h>

#if HAVE_POSIX_ACL || defined(Q_MOC_RUN)
#include <sys/acl.h>
#include <kacl.h>

#include <QDialog>
#include <QTreeWidget>
#include <QHash>

#include <QComboBox>
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
    enum Types {
        OWNER_IDX = 0,
        GROUP_IDX,
        OTHERS_IDX,
        MASK_IDX,
        NAMED_USER_IDX,
        NAMED_GROUP_IDX,
        LAST_IDX,
    };
    enum EntryType { User = 1,
                     Group = 2,
                     Others = 4,
                     Mask = 8,
                     NamedUser = 16,
                     NamedGroup = 32,
                     AllTypes = 63,
                   };

    explicit KACLListView(QWidget *parent = nullptr);
    ~KACLListView();

    bool hasMaskEntry() const
    {
        return m_hasMask;
    }
    bool hasDefaultEntries() const;
    bool allowDefaults() const
    {
        return m_allowDefaults;
    }
    void setAllowDefaults(bool v)
    {
        m_allowDefaults = v;
    }
    unsigned short maskPermissions() const;
    void setMaskPermissions(unsigned short maskPerms);
    acl_perm_t maskPartialPermissions() const;
    void setMaskPartialPermissions(acl_perm_t maskPerms);

    bool maskCanBeDeleted() const;
    bool defaultMaskCanBeDeleted() const;

    const KACLListViewItem *findDefaultItemByType(EntryType type) const;
    const KACLListViewItem *findItemByType(EntryType type,
                                           bool defaults = false) const;
    unsigned short calculateMaskValue(bool defaults) const;
    void calculateEffectiveRights();

    QStringList allowedUsers(bool defaults, KACLListViewItem *allowedItem = nullptr);
    QStringList allowedGroups(bool defaults, KACLListViewItem *allowedItem = nullptr);

    KACL getACL();
    KACL getDefaultACL();

public Q_SLOTS:
    void slotAddEntry();
    void slotEditEntry();
    void slotRemoveEntry();
    void setACL(const KACL &anACL);
    void setDefaultACL(const KACL &anACL);

protected Q_SLOTS:
    void slotItemClicked(QTreeWidgetItem *pItem, int col);
    void slotItemDoubleClicked(QTreeWidgetItem *item, int col);
protected:
    void contentsMousePressEvent(QMouseEvent *e);

private:
    void fillItemsFromACL(const KACL &pACL, bool defaults = false);
    KACL itemsToACL(bool defaults) const;

    KACL m_ACL;
    KACL m_defaultACL;
    unsigned short m_mask;
    bool m_hasMask;
    bool m_allowDefaults;
    QStringList m_allUsers;
    QStringList m_allGroups;
};

class EditACLEntryDialog : public QDialog
{
    Q_OBJECT
public:
    EditACLEntryDialog(KACLListView *listView, KACLListViewItem *item,
                       const QStringList &users,
                       const QStringList &groups,
                       const QStringList &defaultUsers,
                       const QStringList &defaultGroups,
                       int allowedTypes = KACLListView::AllTypes,
                       int allowedDefaultTypes = KACLListView::AllTypes,
                       bool allowDefault = false);
    KACLListViewItem *item() const
    {
        return m_item;
    }
public Q_SLOTS:
    void slotOk();
    void slotSelectionChanged(QAbstractButton *);
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
    QComboBox *m_usersCombo;
    QComboBox *m_groupsCombo;
    QStackedWidget *m_widgetStack;
    QCheckBox *m_defaultCB;
    QHash<QAbstractButton *, int> m_buttonIds;
};

class KACLListViewItem : public QTreeWidgetItem
{
public:
    KACLListViewItem(QTreeWidget *parent, KACLListView::EntryType type,
                     unsigned short value,
                     bool defaultEntry,
                     const QString &qualifier = QString());
    virtual ~KACLListViewItem();
    QString key() const;
    bool operator< (const QTreeWidgetItem &other) const override;

    void calcEffectiveRights();

    bool isDeletable() const;
    bool isAllowedToChangeType() const;

    void togglePerm(acl_perm_t perm);

    void updatePermissionIcons();
    void repaint();

    KACLListView::EntryType type;
    unsigned short value;
    bool isDefault;
    QString qualifier;
    bool isPartial;

private:
    KACLListView *m_pACLListView;
};

#endif
#endif
