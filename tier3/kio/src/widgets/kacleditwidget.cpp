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

#include "kacleditwidget.h"
#include "kacleditwidget_p.h"

#if HAVE_POSIX_ACL

#include <QDialog>
#include <QDialogButtonBox>
#include <qpainter.h>
#include <qpushbutton.h>
#include <QButtonGroup>
#include <QGroupBox>
#include <qradiobutton.h>
#include <qcombobox.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <QStackedWidget>
#include <QMouseEvent>
#include <QHeaderView>

#include <klocalizedstring.h>
#include <kfileitem.h>
#include <QDebug>

#if HAVE_ACL_LIBACL_H
# include <acl/libacl.h>
#endif
extern "C" {
#include <pwd.h>
#include <grp.h>
}
#include <assert.h>

static struct {
    const char* label;
    const char* pixmapName;
    QPixmap* pixmap;
} s_itemAttributes[] = {
    { I18N_NOOP( "Owner" ), "user-grey", 0 },
    { I18N_NOOP( "Owning Group" ), "group-grey", 0 },
    { I18N_NOOP( "Others" ), "others-grey", 0 },
    { I18N_NOOP( "Mask" ), "mask", 0 },
    { I18N_NOOP( "Named User" ), "user", 0 },
    { I18N_NOOP( "Named Group" ), "group", 0 },
};

class KACLEditWidget::KACLEditWidgetPrivate
{
public:
    KACLEditWidgetPrivate()
    {
    }

    // slots
    void _k_slotUpdateButtons();

    KACLListView *m_listView;
    QPushButton *m_AddBtn;
    QPushButton *m_EditBtn;
    QPushButton *m_DelBtn;
};

KACLEditWidget::KACLEditWidget( QWidget *parent )
    : QWidget(parent), d(new KACLEditWidgetPrivate)
{
    QHBoxLayout *hbox = new QHBoxLayout( this );
    hbox->setMargin( 0 );
    d->m_listView = new KACLListView(this);
    hbox->addWidget(d->m_listView);
    connect(d->m_listView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,
            SLOT(_k_slotUpdateButtons()));
    QVBoxLayout *vbox = new QVBoxLayout();
    hbox->addLayout( vbox );
    d->m_AddBtn = new QPushButton(i18n("Add Entry..."), this);
    vbox->addWidget(d->m_AddBtn);
    d->m_AddBtn->setObjectName(QLatin1String("add_entry_button"));
    connect(d->m_AddBtn, SIGNAL(clicked()), d->m_listView, SLOT(slotAddEntry()));
    d->m_EditBtn = new QPushButton(i18n("Edit Entry..."), this);
    vbox->addWidget(d->m_EditBtn);
    d->m_EditBtn->setObjectName(QLatin1String("edit_entry_button"));
    connect(d->m_EditBtn, SIGNAL(clicked()), d->m_listView, SLOT(slotEditEntry()));
    d->m_DelBtn = new QPushButton(i18n("Delete Entry"), this);
    vbox->addWidget(d->m_DelBtn);
    d->m_DelBtn->setObjectName(QLatin1String("delete_entry_button"));
    connect(d->m_DelBtn, SIGNAL(clicked()), d->m_listView, SLOT(slotRemoveEntry()));
    vbox->addItem( new QSpacerItem( 10, 10, QSizePolicy::Fixed, QSizePolicy::Expanding ) );
    d->_k_slotUpdateButtons();
}

KACLEditWidget::~KACLEditWidget()
{
    delete d;
}

void KACLEditWidget::KACLEditWidgetPrivate::_k_slotUpdateButtons()
{
    bool atLeastOneIsNotDeletable = false;
    bool atLeastOneIsNotAllowedToChangeType = false;
    int selectedCount = 0;
    QList<QTreeWidgetItem*> selected = m_listView->selectedItems();
    QListIterator<QTreeWidgetItem*> it( selected );
    while ( it.hasNext() ) {
        KACLListViewItem *item = static_cast<KACLListViewItem*>( it.next() );
        ++selectedCount;
        if ( !item->isDeletable() )
            atLeastOneIsNotDeletable = true;
        if ( !item->isAllowedToChangeType() )
            atLeastOneIsNotAllowedToChangeType = true;
    }
    m_EditBtn->setEnabled( selectedCount && !atLeastOneIsNotAllowedToChangeType );
    m_DelBtn->setEnabled( selectedCount && !atLeastOneIsNotDeletable );
}

KACL KACLEditWidget::getACL() const
{
    return d->m_listView->getACL();
}

KACL KACLEditWidget::getDefaultACL() const
{
    return d->m_listView->getDefaultACL();
}

void KACLEditWidget::setACL( const KACL &acl )
{
    return d->m_listView->setACL(acl);
}

void KACLEditWidget::setDefaultACL( const KACL &acl )
{
    return d->m_listView->setDefaultACL(acl);
}

void KACLEditWidget::setAllowDefaults( bool value )
{
    d->m_listView->setAllowDefaults(value);
}

KACLListViewItem::KACLListViewItem( QTreeWidget* parent,
                                    KACLListView::EntryType _type,
                                    unsigned short _value, bool defaults,
                                    const QString& _qualifier )
 : QTreeWidgetItem( parent),
   type( _type ), value( _value ), isDefault( defaults ),
   qualifier( _qualifier ), isPartial( false )
{
    m_pACLListView = qobject_cast<KACLListView*>( parent );
    repaint();
}


KACLListViewItem::~ KACLListViewItem()
{

}

QString KACLListViewItem::key() const
{
    QString key;
    if ( !isDefault )
        key = 'A';
    else
        key = 'B';
    switch ( type )
    {
        case KACLListView::User:
            key += 'A';
            break;
        case KACLListView::Group:
            key += 'B';
            break;
        case KACLListView::Others:
            key += 'C';
            break;
        case KACLListView::Mask:
            key += 'D';
            break;
        case KACLListView::NamedUser:
            key += 'E' + text( 1 );
            break;
        case KACLListView::NamedGroup:
            key += 'F' + text( 1 );
            break;
        default:
            key += text( 0 );
            break;
    }
    return key;
}

bool KACLListViewItem::operator< ( const QTreeWidgetItem& other ) const
{
    return key() < static_cast<const KACLListViewItem&>(other).key();
}

#if 0
void KACLListViewItem::paintCell( QPainter* p, const QColorGroup &cg,
                                  int column, int width, int alignment )
{
    if ( isDefault ) {
        setForeground( QColor( 0, 0, 255 ) );
    }
    if ( isPartial ) {
        QFont font = p->font();
        font.setItalic( true );
        setForeground( QColor( 100, 100, 100 ) );
        p->setFont( font );
    }
    QTreeWidgetItem::paintCell( p, mycg, column, width, alignment );

    KACLListViewItem *below =0;
    if ( itemBelow() )
        below = static_cast<KACLListViewItem*>( itemBelow() );
    const bool lastUser = type == KACLListView::NamedUser && below && below->type == KACLListView::NamedGroup;
    const bool lastNonDefault = !isDefault && below && below->isDefault;
    if ( type == KACLListView::Mask || lastUser || lastNonDefault )
    {
        p->setPen( QPen( Qt::gray, 0, Qt::DotLine ) );
        if ( type == KACLListView::Mask )
            p->drawLine( 0, 0, width - 1, 0 );
        p->drawLine( 0, height() - 1, width - 1, height() - 1 );
    }
}
#endif


void KACLListViewItem::updatePermPixmaps()
{
    unsigned int partialPerms = value;

    if ( value & ACL_READ )
        setIcon( 2, m_pACLListView->getYesPixmap() );
    else if ( partialPerms & ACL_READ )
        setIcon( 2, m_pACLListView->getYesPartialPixmap() );
    else
        setIcon( 2, QIcon() );

    if ( value & ACL_WRITE )
        setIcon( 3, m_pACLListView->getYesPixmap() );
    else if ( partialPerms & ACL_WRITE )
        setIcon( 3, m_pACLListView->getYesPartialPixmap() );
    else
        setIcon( 3, QIcon() );

    if ( value & ACL_EXECUTE )
        setIcon( 4, m_pACLListView->getYesPixmap() );
    else if ( partialPerms & ACL_EXECUTE )
        setIcon( 4, m_pACLListView->getYesPartialPixmap() );
    else
        setIcon( 4, QIcon() );
}

void KACLListViewItem::repaint()
{
    int idx = 0;
    switch ( type )
    {
      case KACLListView::User:
          idx = KACLListView::OWNER_IDX;
            break;
        case KACLListView::Group:
          idx = KACLListView::GROUP_IDX;
            break;
        case KACLListView::Others:
          idx = KACLListView::OTHERS_IDX;
            break;
        case KACLListView::Mask:
          idx = KACLListView::MASK_IDX;
            break;
        case KACLListView::NamedUser:
          idx = KACLListView::NAMED_USER_IDX;
            break;
        case KACLListView::NamedGroup:
          idx = KACLListView::NAMED_GROUP_IDX;
            break;
        default:
          idx = KACLListView::OWNER_IDX;
            break;
    }
    setText( 0, i18n(s_itemAttributes[idx].label) );
    setIcon( 0, *s_itemAttributes[idx].pixmap );
    if ( isDefault )
        setText( 0, text( 0 ) + i18n( " (Default)" ) );
    setText( 1, qualifier );
    // Set the pixmaps for which of the perms are set
    updatePermPixmaps();
}

void KACLListViewItem::calcEffectiveRights()
{
    QString strEffective = QString( "---" );

    // Do we need to worry about the mask entry? It applies to named users,
    // owning group, and named groups
    if ( m_pACLListView->hasMaskEntry()
            && ( type == KACLListView::NamedUser
              || type == KACLListView::Group
              || type == KACLListView::NamedGroup )
            && !isDefault )
    {

        strEffective[0] = ( m_pACLListView->maskPermissions() & value & ACL_READ ) ? 'r' : '-';
        strEffective[1] = ( m_pACLListView->maskPermissions() & value & ACL_WRITE ) ? 'w' : '-';
        strEffective[2] = ( m_pACLListView->maskPermissions() & value & ACL_EXECUTE ) ? 'x' : '-';
/*
        // What about any partial perms?
        if ( maskPerms & partialPerms & ACL_READ || // Partial perms on entry
             maskPartialPerms & perms & ACL_READ || // Partial perms on mask
             maskPartialPerms & partialPerms & ACL_READ ) // Partial perms on mask and entry
            strEffective[0] = 'R';
        if ( maskPerms & partialPerms & ACL_WRITE || // Partial perms on entry
             maskPartialPerms & perms & ACL_WRITE || // Partial perms on mask
             maskPartialPerms & partialPerms & ACL_WRITE ) // Partial perms on mask and entry
            strEffective[1] = 'W';
        if ( maskPerms & partialPerms & ACL_EXECUTE || // Partial perms on entry
             maskPartialPerms & perms & ACL_EXECUTE || // Partial perms on mask
             maskPartialPerms & partialPerms & ACL_EXECUTE ) // Partial perms on mask and entry
            strEffective[2] = 'X';
*/
    }
    else
    {
        // No, the effective value are just the value in this entry
        strEffective[0] = ( value & ACL_READ ) ? 'r' : '-';
        strEffective[1] = ( value & ACL_WRITE ) ? 'w' : '-';
        strEffective[2] = ( value & ACL_EXECUTE ) ? 'x' : '-';

        /*
        // What about any partial perms?
        if ( partialPerms & ACL_READ )
            strEffective[0] = 'R';
        if ( partialPerms & ACL_WRITE )
            strEffective[1] = 'W';
        if ( partialPerms & ACL_EXECUTE )
            strEffective[2] = 'X';
            */
    }
    setText( 5, strEffective );
}

bool KACLListViewItem::isDeletable() const
{
    bool isMaskAndDeletable = false;
    if (type == KACLListView::Mask ) {
        if ( !isDefault &&  m_pACLListView->maskCanBeDeleted() )
            isMaskAndDeletable = true;
        else if ( isDefault &&  m_pACLListView->defaultMaskCanBeDeleted() )
            isMaskAndDeletable = true;
    }
    return type != KACLListView::User &&
           type != KACLListView::Group &&
           type != KACLListView::Others &&
           ( type != KACLListView::Mask || isMaskAndDeletable );
}

bool KACLListViewItem::isAllowedToChangeType() const
{
    return type != KACLListView::User &&
           type != KACLListView::Group &&
           type != KACLListView::Others &&
           type != KACLListView::Mask;
}

void KACLListViewItem::togglePerm( acl_perm_t perm )
{
    value ^= perm; // Toggle the perm
    if ( type == KACLListView::Mask && !isDefault ) {
        m_pACLListView->setMaskPermissions( value );
    }
    calcEffectiveRights();
    updatePermPixmaps();
/*
    // If the perm is in the partial perms then remove it. i.e. Once
    // a user changes a partial perm it then applies to all selected files.
    if ( m_pEntry->m_partialPerms & perm )
        m_pEntry->m_partialPerms ^= perm;

    m_pEntry->setPartialEntry( false );
    // Make sure that all entries have their effective rights calculated if
    // we are changing the ACL_MASK entry.
    if ( type == Mask )
    {
        m_pACLListView->setMaskPartialPermissions( m_pEntry->m_partialPerms );
        m_pACLListView->setMaskPermissions( value );
        m_pACLListView->calculateEffectiveRights();
    }
*/
}



EditACLEntryDialog::EditACLEntryDialog( KACLListView *listView, KACLListViewItem *item,
                                        const QStringList &users,
                                        const QStringList &groups,
                                        const QStringList &defaultUsers,
                                        const QStringList &defaultGroups,
                                        int allowedTypes, int allowedDefaultTypes,
                                        bool allowDefaults )
      : QDialog( listView ),
        m_listView( listView ), m_item( item ), m_users( users ), m_groups( groups ),
        m_defaultUsers( defaultUsers ), m_defaultGroups( defaultGroups ),
        m_allowedTypes( allowedTypes ), m_allowedDefaultTypes( allowedDefaultTypes ),
        m_defaultCB( 0 )
{
    setObjectName( "edit_entry_dialog" );
    setModal( true );
    setWindowTitle( i18n( "Edit ACL Entry" ) );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    QGroupBox *gb = new QGroupBox(i18n("Entry Type"), this);
    QVBoxLayout *gbLayout = new QVBoxLayout( gb );

    m_buttonGroup = new QButtonGroup(this);

    if ( allowDefaults ) {
        m_defaultCB = new QCheckBox(i18n("Default for new files in this folder"), this);
        m_defaultCB->setObjectName( QLatin1String( "defaultCB" ) );
        mainLayout->addWidget( m_defaultCB );
        connect( m_defaultCB, SIGNAL(toggled(bool)),
                 this, SLOT(slotUpdateAllowedUsersAndGroups()) );
        connect( m_defaultCB, SIGNAL(toggled(bool)),
                 this, SLOT(slotUpdateAllowedTypes()) );
    }

    QRadioButton *ownerType = new QRadioButton( i18n("Owner"), gb );
    ownerType->setObjectName( QLatin1String( "ownerType" ) );
    gbLayout->addWidget( ownerType );
    m_buttonGroup->addButton( ownerType );
    m_buttonIds.insert( ownerType, KACLListView::User );
    QRadioButton *owningGroupType = new QRadioButton( i18n("Owning Group"), gb );
    owningGroupType->setObjectName( QLatin1String( "owningGroupType" ) );
    gbLayout->addWidget( owningGroupType );
    m_buttonGroup->addButton( owningGroupType );
    m_buttonIds.insert( owningGroupType, KACLListView::Group );
    QRadioButton *othersType = new QRadioButton( i18n("Others"), gb );
    othersType->setObjectName( QLatin1String( "othersType" ) );
    gbLayout->addWidget( othersType );
    m_buttonGroup->addButton( othersType );
    m_buttonIds.insert( othersType, KACLListView::Others );
    QRadioButton *maskType = new QRadioButton( i18n("Mask"), gb );
    maskType->setObjectName( QLatin1String( "maskType" ) );
    gbLayout->addWidget( maskType );
    m_buttonGroup->addButton( maskType );
    m_buttonIds.insert( maskType, KACLListView::Mask );
    QRadioButton *namedUserType = new QRadioButton( i18n("Named user"), gb );
    namedUserType->setObjectName( QLatin1String( "namesUserType" ) );
    gbLayout->addWidget( namedUserType );
    m_buttonGroup->addButton( namedUserType );
    m_buttonIds.insert( namedUserType, KACLListView::NamedUser );
    QRadioButton *namedGroupType = new QRadioButton( i18n("Named group"), gb );
    namedGroupType->setObjectName( QLatin1String( "namedGroupType" ) );
    gbLayout->addWidget( namedGroupType );
    m_buttonGroup->addButton( namedGroupType );
    m_buttonIds.insert( namedGroupType, KACLListView::NamedGroup );

    mainLayout->addWidget( gb );

    connect( m_buttonGroup, SIGNAL(buttonClicked(QAbstractButton*)),
             this, SLOT(slotSelectionChanged(QAbstractButton*)) );

    m_widgetStack = new QStackedWidget(this);
    mainLayout->addWidget( m_widgetStack );

    // users box
    QWidget *usersBox = new QWidget( m_widgetStack );
    QHBoxLayout *usersLayout = new QHBoxLayout( usersBox );
    usersBox->setLayout( usersLayout );
    m_widgetStack->addWidget( usersBox );

    QLabel *usersLabel = new QLabel( i18n( "User: " ), usersBox );
    m_usersCombo = new KComboBox( usersBox );
    m_usersCombo->setEditable( false );
    m_usersCombo->setObjectName( QLatin1String( "users" ) );
    usersLabel->setBuddy( m_usersCombo );

    usersLayout->addWidget( usersLabel );
    usersLayout->addWidget( m_usersCombo );

    // groups box
    QWidget *groupsBox = new QWidget( m_widgetStack );
    QHBoxLayout *groupsLayout = new QHBoxLayout( usersBox );
    groupsBox->setLayout( groupsLayout );
    m_widgetStack->addWidget( groupsBox );

    QLabel *groupsLabel = new QLabel( i18n( "Group: " ), groupsBox );
    m_groupsCombo = new KComboBox( groupsBox );
    m_groupsCombo->setEditable( false );
    m_groupsCombo->setObjectName( QLatin1String( "groups" ) );
    groupsLabel->setBuddy( m_groupsCombo );

    groupsLayout->addWidget( groupsLabel );
    groupsLayout->addWidget( m_groupsCombo );

    if ( m_item ) {
        m_buttonIds.key( m_item->type )->setChecked( true );
        if ( m_defaultCB )
            m_defaultCB->setChecked( m_item->isDefault );
        slotUpdateAllowedTypes();
        slotSelectionChanged( m_buttonIds.key( m_item->type ) );
        slotUpdateAllowedUsersAndGroups();
        if ( m_item->type == KACLListView::NamedUser ) {
            m_usersCombo->setItemText( m_usersCombo->currentIndex(), m_item->qualifier );
        } else if ( m_item->type == KACLListView::NamedGroup ) {
            m_groupsCombo->setItemText( m_groupsCombo->currentIndex(), m_item->qualifier );
        }
    } else {
        // new entry, preselect "named user", arguably the most common one
        m_buttonIds.key( KACLListView::NamedUser )->setChecked( true );
        slotUpdateAllowedTypes();
        slotSelectionChanged( m_buttonIds.key( KACLListView::NamedUser ) );
        slotUpdateAllowedUsersAndGroups();
    }

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotOk()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    mainLayout->addWidget(buttonBox);

    adjustSize();
}

void EditACLEntryDialog::slotUpdateAllowedTypes()
{
    int allowedTypes = m_allowedTypes;
    if ( m_defaultCB && m_defaultCB->isChecked() ) {
        allowedTypes = m_allowedDefaultTypes;
    }
    for ( int i=1; i < KACLListView::AllTypes; i=i*2 ) {
        if ( allowedTypes & i )
            m_buttonIds.key( i )->show();
        else
            m_buttonIds.key( i )->hide();
    }
}

void EditACLEntryDialog::slotUpdateAllowedUsersAndGroups()
{
    const QString oldUser = m_usersCombo->currentText();
    const QString oldGroup = m_groupsCombo->currentText();
    m_usersCombo->clear();
    m_groupsCombo->clear();
    if ( m_defaultCB && m_defaultCB->isChecked() ) {
        m_usersCombo->addItems( m_defaultUsers );
        if ( m_defaultUsers.contains( oldUser ) )
            m_usersCombo->setItemText( m_usersCombo->currentIndex(), oldUser );
        m_groupsCombo->addItems( m_defaultGroups );
        if ( m_defaultGroups.contains( oldGroup ) )
            m_groupsCombo->setItemText( m_groupsCombo->currentIndex(), oldGroup );
    } else {
        m_usersCombo->addItems( m_users );
        if ( m_users.contains( oldUser ) )
            m_usersCombo->setItemText( m_usersCombo->currentIndex(), oldUser );
        m_groupsCombo->addItems( m_groups );
        if ( m_groups.contains( oldGroup ) )
            m_groupsCombo->setItemText( m_groupsCombo->currentIndex(), oldGroup );
    }
}
void EditACLEntryDialog::slotOk()
{
    KACLListView::EntryType type = static_cast<KACLListView::EntryType>( m_buttonIds[m_buttonGroup->checkedButton()] );

    qWarning() << "Type 2: " << type;

    QString qualifier;
    if ( type == KACLListView::NamedUser )
      qualifier = m_usersCombo->currentText();
    if ( type == KACLListView::NamedGroup )
      qualifier = m_groupsCombo->currentText();

    if ( !m_item ) {
        m_item = new KACLListViewItem( m_listView, type, ACL_READ | ACL_WRITE | ACL_EXECUTE, false, qualifier );
    } else {
        m_item->type = type;
        m_item->qualifier = qualifier;
    }
    if ( m_defaultCB )
        m_item->isDefault = m_defaultCB->isChecked();
    m_item->repaint();

    QDialog::accept();
}

void EditACLEntryDialog::slotSelectionChanged( QAbstractButton *button )
{
    switch ( m_buttonIds[ button ] ) {
        case KACLListView::User:
        case KACLListView::Group:
        case KACLListView::Others:
        case KACLListView::Mask:
            m_widgetStack->setEnabled( false );
            break;
        case KACLListView::NamedUser:
            m_widgetStack->setEnabled( true );
            m_widgetStack->setCurrentIndex( 0 /* User */ );
            break;
        case KACLListView::NamedGroup:
            m_widgetStack->setEnabled( true );
            m_widgetStack->setCurrentIndex( 1 /* Group */ );
            break;
        default:
            break;
    }
}


KACLListView::KACLListView( QWidget* parent )
 : QTreeWidget( parent ),
   m_hasMask( false ), m_allowDefaults( false )
{
    // Add the columns
    setColumnCount( 6 );
    QStringList headers;
    headers <<  i18n( "Type" );
    headers <<  i18n( "Name" );
    headers <<  i18nc( "read permission", "r" );
    headers <<  i18nc( "write permission", "w" );
    headers <<  i18nc( "execute permission", "x" );
    headers <<  i18n( "Effective" );
    setHeaderLabels( headers );

    setSortingEnabled( false );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    setRootIsDecorated( false );

    // Load the avatars
    for ( int i=0; i < LAST_IDX; ++i ) {
        s_itemAttributes[i].pixmap = new QPixmap( QString::fromLatin1(":/images/%1").arg(s_itemAttributes[i].pixmapName) );
    }
    m_yesPixmap = new QPixmap( ":/images/yes.png" );
    m_yesPartialPixmap = new QPixmap( ":/images/yespartial.png" );


    // fill the lists of all legal users and groups
    struct passwd *user = 0;
    setpwent();
    while ( ( user = getpwent() ) != 0 ) {
       m_allUsers << QString::fromLatin1( user->pw_name );
    }
    endpwent();

    struct group *gr = 0;
    setgrent();
    while ( ( gr = getgrent() ) != 0 ) {
       m_allGroups << QString::fromLatin1( gr->gr_name );
    }
    endgrent();
    m_allUsers.sort();
    m_allGroups.sort();

    connect( this, SIGNAL(itemClicked(QTreeWidgetItem*,int)),
             this, SLOT(slotItemClicked(QTreeWidgetItem*,int)) );
}


KACLListView::~KACLListView()
{
    for ( int i=0; i < LAST_IDX; ++i ) {
       delete s_itemAttributes[i].pixmap;
    }
    delete m_yesPixmap;
    delete m_yesPartialPixmap;
}

QStringList KACLListView::allowedUsers( bool defaults, KACLListViewItem *allowedItem )
{
    QStringList allowedUsers = m_allUsers;
    QTreeWidgetItemIterator it( this );
    while ( *it ) {
        const KACLListViewItem *item = static_cast<const KACLListViewItem*>( *it );
        ++it;
        if ( item->type != NamedUser || item->isDefault != defaults ) continue;
        if ( allowedItem && item == allowedItem && allowedItem->isDefault == defaults ) continue;
        allowedUsers.removeAll( item->qualifier );
    }
    return allowedUsers;
}

QStringList KACLListView::allowedGroups( bool defaults, KACLListViewItem *allowedItem )
{
    QStringList allowedGroups = m_allGroups;
    QTreeWidgetItemIterator it( this );
    while ( *it ) {
        const KACLListViewItem *item = static_cast<const KACLListViewItem*>( *it );
        ++it;
        if ( item->type != NamedGroup || item->isDefault != defaults ) continue;
        if ( allowedItem && item == allowedItem && allowedItem->isDefault == defaults ) continue;
        allowedGroups.removeAll( item->qualifier );
    }
    return allowedGroups;
}

void KACLListView::fillItemsFromACL( const KACL &pACL, bool defaults )
{
    // clear out old entries of that ilk
    QTreeWidgetItemIterator it( this );
    while ( KACLListViewItem *item = static_cast<KACLListViewItem*>( *it ) ) {
        ++it;
        if ( item->isDefault == defaults )
            delete item;
    }

    new KACLListViewItem( this, User, pACL.ownerPermissions(), defaults );

    new KACLListViewItem( this, Group, pACL.owningGroupPermissions(), defaults );

    new KACLListViewItem( this, Others, pACL.othersPermissions(), defaults );

    bool hasMask = false;
    unsigned short mask = pACL.maskPermissions( hasMask );
    if ( hasMask ) {
        new KACLListViewItem( this, Mask, mask, defaults );
    }

    // read all named user entries
    const ACLUserPermissionsList &userList =  pACL.allUserPermissions();
    ACLUserPermissionsConstIterator itu = userList.begin();
    while ( itu != userList.end() ) {
        new KACLListViewItem( this, NamedUser, (*itu).second, defaults, (*itu).first );
        ++itu;
    }

    // and now all named groups
    const ACLUserPermissionsList &groupList =  pACL.allGroupPermissions();
    ACLUserPermissionsConstIterator itg = groupList.begin();
    while ( itg != groupList.end() ) {
        new KACLListViewItem( this, NamedGroup, (*itg).second, defaults, (*itg).first );
        ++itg;
    }
}

void KACLListView::setACL( const KACL &acl )
{
    if ( !acl.isValid() ) return;
    // Remove any entries left over from displaying a previous ACL
    m_ACL = acl;
    fillItemsFromACL( m_ACL );

    m_mask = acl.maskPermissions( m_hasMask );
    calculateEffectiveRights();
}

void KACLListView::setDefaultACL( const KACL &acl )
{
    if ( !acl.isValid() ) return;
    m_defaultACL = acl;
    fillItemsFromACL( m_defaultACL, true );
    calculateEffectiveRights();
}

KACL KACLListView::itemsToACL( bool defaults ) const
{
    KACL newACL( 0 );
    bool atLeastOneEntry = false;
    ACLUserPermissionsList users;
    ACLGroupPermissionsList groups;
    QTreeWidgetItemIterator it( const_cast<KACLListView*>( this ) );
    while ( QTreeWidgetItem* qlvi = *it ) {
        ++it;
        const KACLListViewItem* item = static_cast<KACLListViewItem*>( qlvi );
        if ( item->isDefault != defaults ) continue;
        atLeastOneEntry = true;
        switch ( item->type ) {
            case User:
                newACL.setOwnerPermissions( item->value );
                break;
            case Group:
                newACL.setOwningGroupPermissions( item->value );
                break;
            case Others:
                newACL.setOthersPermissions( item->value );
                break;
            case Mask:
                newACL.setMaskPermissions( item->value );
                break;
            case NamedUser:
                users.append( qMakePair( item->text( 1 ), item->value ) );
                break;
            case NamedGroup:
                groups.append( qMakePair( item->text( 1 ), item->value ) );
                break;
            default:
                break;
        }
    }
    if ( atLeastOneEntry ) {
        newACL.setAllUserPermissions( users );
        newACL.setAllGroupPermissions( groups );
        if ( newACL.isValid() )
            return newACL;
    }
    return KACL();
}

KACL KACLListView::getACL()
{
    return itemsToACL( false );
}


KACL KACLListView::getDefaultACL()
{
    return itemsToACL( true );
}

void KACLListView::contentsMousePressEvent( QMouseEvent * /*e*/ )
{
    /*
    QTreeWidgetItem *clickedItem = itemAt( e->pos() );
    if ( !clickedItem ) return;
    // if the click is on an as yet unselected item, select it first
    if ( !clickedItem->isSelected() )
        QAbstractItemView::contentsMousePressEvent( e );

    if ( !currentItem() ) return;
    int column = header()->sectionAt( e->x() );
    acl_perm_t perm;
    switch ( column )
    {
        case 2:
            perm = ACL_READ;
            break;
        case 3:
            perm = ACL_WRITE;
            break;
        case 4:
            perm = ACL_EXECUTE;
            break;
        default:
            return QTreeWidget::contentsMousePressEvent( e );
    }
    KACLListViewItem* referenceItem = static_cast<KACLListViewItem*>( clickedItem );
    unsigned short referenceHadItSet = referenceItem->value & perm;
    QTreeWidgetItemIterator it( this );
    while ( KACLListViewItem* item = static_cast<KACLListViewItem*>( *it ) ) {
        ++it;
        if ( !item->isSelected() ) continue;
        // toggle those with the same value as the clicked item, leave the others
        if ( referenceHadItSet == ( item->value & perm ) )
            item->togglePerm( perm );
    }
     */
}

void KACLListView::slotItemClicked( QTreeWidgetItem* pItem,  int col )
{
    if ( !pItem ) return;

    QTreeWidgetItemIterator it( this );
    while ( KACLListViewItem* item = static_cast<KACLListViewItem*>( *it ) ) {
        ++it;
        if ( !item->isSelected() ) continue;
        switch ( col )
        {
            case 2:
                item->togglePerm( ACL_READ );
                break;
            case 3:
                item->togglePerm( ACL_WRITE );
                break;
            case 4:
                item->togglePerm( ACL_EXECUTE );
                break;

            default:
                ; // Do nothing
        }
    }
    /*
    // Has the user changed one of the required entries in a default ACL?
    if ( m_pACL->aclType() == ACL_TYPE_DEFAULT &&
    ( col == 2 || col == 3 || col == 4 ) &&
    ( pACLItem->entryType() == ACL_USER_OBJ ||
    pACLItem->entryType() == ACL_GROUP_OBJ ||
    pACLItem->entryType() == ACL_OTHER ) )
    {
    // Mark the required entries as no longer being partial entries.
    // That is, they will get applied to all selected directories.
    KACLListViewItem* pUserObj = findACLEntryByType( this, ACL_USER_OBJ );
    pUserObj->entry()->setPartialEntry( false );

    KACLListViewItem* pGroupObj = findACLEntryByType( this, ACL_GROUP_OBJ );
    pGroupObj->entry()->setPartialEntry( false );

    KACLListViewItem* pOther = findACLEntryByType( this, ACL_OTHER );
    pOther->entry()->setPartialEntry( false );

    update();
    }
     */
}


void KACLListView::calculateEffectiveRights()
{
    QTreeWidgetItemIterator it( this );
    KACLListViewItem* pItem;
    while ( ( pItem = dynamic_cast<KACLListViewItem*>( *it ) ) != 0 )
    {
        ++it;
        pItem->calcEffectiveRights();
    }
}


unsigned short KACLListView::maskPermissions() const
{
  return m_mask;
}


void KACLListView::setMaskPermissions( unsigned short maskPerms )
{
    m_mask = maskPerms;
    calculateEffectiveRights();
}


acl_perm_t KACLListView::maskPartialPermissions() const
{
  //  return m_pMaskEntry->m_partialPerms;
  return 0;
}


void KACLListView::setMaskPartialPermissions( acl_perm_t /*maskPartialPerms*/ )
{
    //m_pMaskEntry->m_partialPerms = maskPartialPerms;
    calculateEffectiveRights();
}

bool KACLListView::hasDefaultEntries() const
{
    QTreeWidgetItemIterator it( const_cast<KACLListView*>( this ) );
    while ( *it ) {
        const KACLListViewItem *item = static_cast<const KACLListViewItem*>( *it );
        ++it;
        if ( item->isDefault ) return true;
    }
    return false;
}

const KACLListViewItem* KACLListView::findDefaultItemByType( EntryType type ) const
{
    return findItemByType( type, true );
}

const KACLListViewItem* KACLListView::findItemByType( EntryType type, bool defaults ) const
{
    QTreeWidgetItemIterator it( const_cast<KACLListView*>( this ) );
    while ( *it ) {
        const KACLListViewItem *item = static_cast<const KACLListViewItem*>( *it );
        ++it;
        if ( item->isDefault == defaults && item->type == type ) {
            return item;
        }
    }
    return 0;
}


unsigned short KACLListView::calculateMaskValue( bool defaults ) const
{
    // KACL auto-adds the relevant maks entries, so we can simply query
    bool dummy;
    return itemsToACL( defaults ).maskPermissions( dummy );
}

void KACLListView::slotAddEntry()
{
    int allowedTypes = NamedUser | NamedGroup;
    if ( !m_hasMask )
        allowedTypes |= Mask;
    int allowedDefaultTypes = NamedUser | NamedGroup;
    if ( !findDefaultItemByType( Mask ) )
        allowedDefaultTypes |=  Mask;
    if ( !hasDefaultEntries() )
        allowedDefaultTypes |= User | Group;
    EditACLEntryDialog dlg( this, 0,
                            allowedUsers( false ), allowedGroups( false ),
                            allowedUsers( true ), allowedGroups( true ),
                            allowedTypes, allowedDefaultTypes, m_allowDefaults );
    dlg.exec();
    KACLListViewItem *item = dlg.item();
    if ( !item ) return; // canceled
    if ( item->type == Mask && !item->isDefault ) {
        m_hasMask = true;
        m_mask = item->value;
    }
    if ( item->isDefault && !hasDefaultEntries() ) {
        // first default entry, fill in what is needed
        if ( item->type != User ) {
            unsigned short v = findDefaultItemByType( User )->value;
            new KACLListViewItem( this, User, v, true );
        }
        if ( item->type != Group ) {
            unsigned short v = findDefaultItemByType( Group )->value;
            new KACLListViewItem( this, Group, v, true );
        }
        if ( item->type != Others ) {
            unsigned short v = findDefaultItemByType( Others )->value;
            new KACLListViewItem( this, Others, v, true );
        }
    }
    const KACLListViewItem *defaultMaskItem = findDefaultItemByType( Mask );
    if ( item->isDefault && !defaultMaskItem ) {
        unsigned short v = calculateMaskValue( true );
        new KACLListViewItem( this, Mask, v, true );
    }
    if ( !item->isDefault && !m_hasMask &&
            ( item->type == Group
              || item->type == NamedUser
              || item->type == NamedGroup ) ) {
        // auto-add a mask entry
        unsigned short v = calculateMaskValue( false );
        new KACLListViewItem( this, Mask, v, false );
        m_hasMask = true;
        m_mask = v;
    }
    calculateEffectiveRights();
    sortItems( sortColumn(), Qt::AscendingOrder );
    setCurrentItem( item );
    // QTreeWidget doesn't seem to emit, in this case, and we need to update
    // the buttons...
    if ( topLevelItemCount() == 1 )
        emit currentItemChanged( item, item );
}

void KACLListView::slotEditEntry()
{
    QTreeWidgetItem * current = currentItem();
    if ( !current ) return;
    KACLListViewItem *item = static_cast<KACLListViewItem*>( current );
    int allowedTypes = item->type | NamedUser | NamedGroup;
    bool itemWasMask = item->type == Mask;
    if ( !m_hasMask || itemWasMask )
        allowedTypes |= Mask;
    int allowedDefaultTypes = item->type | NamedUser | NamedGroup;
    if ( !findDefaultItemByType( Mask ) )
        allowedDefaultTypes |=  Mask;
    if ( !hasDefaultEntries() )
        allowedDefaultTypes |= User | Group;

    EditACLEntryDialog dlg( this, item,
                            allowedUsers( false, item ), allowedGroups( false, item ),
                            allowedUsers( true, item ), allowedGroups( true, item ),
                            allowedTypes, allowedDefaultTypes, m_allowDefaults );
    dlg.exec();
    if ( itemWasMask && item->type != Mask ) {
        m_hasMask = false;
        m_mask = 0;
    }
    if ( !itemWasMask && item->type == Mask ) {
        m_mask = item->value;
        m_hasMask = true;
    }
    calculateEffectiveRights();
    sortItems( sortColumn(), Qt::AscendingOrder );
}

void KACLListView::slotRemoveEntry()
{
    QTreeWidgetItemIterator it( this, QTreeWidgetItemIterator::Selected );
    while ( *it ) {
        KACLListViewItem *item = static_cast<KACLListViewItem*>( *it );
        ++it;
        /* First check if it's a mask entry and if so, make sure that there is
         * either no name user or group entry, which means the mask can be
         * removed, or don't remove it, but reset it. That is allowed. */
        if ( item->type == Mask ) {
            bool itemWasDefault = item->isDefault;
            if ( !itemWasDefault && maskCanBeDeleted() ) {
                m_hasMask= false;
                m_mask = 0;
                delete item;
            } else if ( itemWasDefault && defaultMaskCanBeDeleted() ) {
                delete item;
            } else {
                item->value = 0;
                item->repaint();
            }
            if ( !itemWasDefault )
                calculateEffectiveRights();
        } else {
            // for the base permissions, disable them, which is what libacl does
            if ( !item->isDefault &&
                    ( item->type == User
                      || item->type == Group
                      || item->type == Others ) ) {
                item->value = 0;
                item->repaint();
            } else {
                delete item;
            }
        }
    }
}

bool KACLListView::maskCanBeDeleted() const
{
   return !findItemByType( NamedUser ) && !findItemByType( NamedGroup );
}

bool KACLListView::defaultMaskCanBeDeleted() const
{
    return !findDefaultItemByType( NamedUser ) && !findDefaultItemByType( NamedGroup );
}

#include "moc_kacleditwidget.cpp"
#include "moc_kacleditwidget_p.cpp"
#endif
// vim:set ts=8 sw=4:
