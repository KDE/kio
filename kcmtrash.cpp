/***************************************************************************
 *   Copyright (C) 2008 by Tobias Koenig <tokoe@kde.org>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "kcmtrash.h"
#include "discspaceutil.h"
#include "trashimpl.h"

#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QListWidget>
#include <QtGui/QListWidgetItem>
#include <QtGui/QSpinBox>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <kglobal.h>
#include <kicon.h>
#include <klocale.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <KDebug>

K_PLUGIN_FACTORY( KCMTrashConfigFactory, registerPlugin<TrashConfigModule>( "trash" ); )
K_EXPORT_PLUGIN( KCMTrashConfigFactory( "kcmtrash" ) )

TrashConfigModule::TrashConfigModule( QWidget* parent, const QVariantList& )
    : KCModule( KCMTrashConfigFactory::componentData(), parent ), trashInitialize( false )
{
    KGlobal::locale()->insertCatalog( "kio_trash" );

    mTrashImpl = new TrashImpl();
    mTrashImpl->init();

    readConfig();

    setupGui();

    useTypeChanged();

    connect( mUseTimeLimit, SIGNAL( toggled( bool ) ),
             this, SLOT( changed() ) );
    connect( mUseTimeLimit, SIGNAL( toggled( bool ) ),
             this, SLOT( useTypeChanged() ) );
    connect( mDays, SIGNAL( valueChanged( int ) ),
             this, SLOT( changed() ) );
    connect( mUseSizeLimit, SIGNAL( toggled( bool ) ),
             this, SLOT( changed() ) );
    connect( mUseSizeLimit, SIGNAL( toggled( bool ) ),
             this, SLOT( useTypeChanged() ) );
    connect( mPercent, SIGNAL( valueChanged( double ) ),
             this, SLOT( percentChanged( double ) ) );
    connect( mPercent, SIGNAL( valueChanged( double ) ),
             this, SLOT( changed() ) );
    connect( mLimitReachedAction, SIGNAL( activated( int ) ),
             this, SLOT( changed() ) );

    trashChanged( 0 );
    trashInitialize = true;
}

TrashConfigModule::~TrashConfigModule()
{
}

void TrashConfigModule::save()
{
    if ( !mCurrentTrash.isEmpty() ) {
        ConfigEntry entry;
        entry.useTimeLimit = mUseTimeLimit->isChecked();
        entry.days = mDays->value();
        entry.useSizeLimit = mUseSizeLimit->isChecked();
        entry.percent = mPercent->value(),
        entry.actionType = mLimitReachedAction->currentIndex();
        mConfigMap.insert( mCurrentTrash, entry );
    }

    writeConfig();
}

void TrashConfigModule::defaults()
{
    ConfigEntry entry;
    entry.useTimeLimit = false;
    entry.days = 7;
    entry.useSizeLimit = true;
    entry.percent = 10.0;
    entry.actionType = 0;
    mConfigMap.insert( mCurrentTrash, entry );
    trashInitialize = false;
    trashChanged( 0 );
}

void TrashConfigModule::percentChanged( double percent )
{
    DiscSpaceUtil util( mCurrentTrash );

    qulonglong partitionSize = util.size();
    double size = ((double)(partitionSize/100))*percent;

    QString unit = i18n( "Byte" );
    if ( size > 1024 ) {
        unit = i18n( "KByte" );
        size = size/1024.0;
    }
    if ( size > 1024 ) {
        unit = i18n( "MByte" );
        size = size/1024.0;
    }
    if ( size > 1024 ) {
        unit = i18n( "GByte" );
        size = size/1024.0;
    }
    if ( size > 1024 ) {
        unit = i18n( "TByte" );
        size = size/1024.0;
    }

    mSizeLabel->setText( i18n( "(%1 %2)", QString::number( size, 'f', 2 ), unit ) );
}

void TrashConfigModule::trashChanged( QListWidgetItem *item )
{
    trashChanged( item->data( Qt::UserRole ).toInt() );
}

void TrashConfigModule::trashChanged( int value )
{
    const TrashImpl::TrashDirMap map = mTrashImpl->trashDirectories();

    if ( !mCurrentTrash.isEmpty() && trashInitialize ) {
        ConfigEntry entry;
        entry.useTimeLimit = mUseTimeLimit->isChecked();
        entry.days = mDays->value();
        entry.useSizeLimit = mUseSizeLimit->isChecked();
        entry.percent = mPercent->value(),
        entry.actionType = mLimitReachedAction->currentIndex();
        mConfigMap.insert( mCurrentTrash, entry );
    }

    mCurrentTrash = map[ value ];
    if ( mConfigMap.contains( mCurrentTrash ) ) {
        const ConfigEntry entry = mConfigMap[ mCurrentTrash ];
        mUseTimeLimit->setChecked( entry.useTimeLimit );
        mDays->setValue( entry.days );
        mUseSizeLimit->setChecked( entry.useSizeLimit );
        mPercent->setValue( entry.percent );
        mLimitReachedAction->setCurrentIndex( entry.actionType );
    } else {
        mUseTimeLimit->setChecked( false );
        mDays->setValue( 7 );
        mUseSizeLimit->setChecked( true );
        mPercent->setValue( 10.0 );
        mLimitReachedAction->setCurrentIndex( 0 );
    }

    percentChanged( mPercent->value() );

}

void TrashConfigModule::useTypeChanged()
{
    mDays->setEnabled( mUseTimeLimit->isChecked() );
    mSizeWidget->setEnabled( mUseSizeLimit->isChecked() );
}

void TrashConfigModule::readConfig()
{
    KConfig config( "ktrashrc" );
    mConfigMap.clear();

    const QStringList groups = config.groupList();
    for ( int i = 0; i < groups.count(); ++i ) {
        if ( groups[ i ].startsWith( '/' ) ) {
            const KConfigGroup group = config.group( groups[ i ] );

            ConfigEntry entry;
            entry.useTimeLimit = group.readEntry( "UseTimeLimit", false );
            entry.days = group.readEntry( "Days", 7 );
            entry.useSizeLimit = group.readEntry( "UseSizeLimit", true );
            entry.percent = group.readEntry( "Percent", 10.0 );
            entry.actionType = group.readEntry( "LimitReachedAction", 0 );
            mConfigMap.insert( groups[ i ], entry );
        }
    }
}

void TrashConfigModule::writeConfig()
{
    KConfig config( "ktrashrc" );

    // first delete all existing groups
    const QStringList groups = config.groupList();
    for ( int i = 0; i < groups.count(); ++i )
        if ( groups[ i ].startsWith( '/' ) )
            config.deleteGroup( groups[ i ] );

    QMapIterator<QString, ConfigEntry> it( mConfigMap );
    while ( it.hasNext() ) {
        it.next();
        KConfigGroup group = config.group( it.key() );

        group.writeEntry( "UseTimeLimit", it.value().useTimeLimit );
        group.writeEntry( "Days", it.value().days );
        group.writeEntry( "UseSizeLimit", it.value().useSizeLimit );
        group.writeEntry( "Percent", it.value().percent );
        group.writeEntry( "LimitReachedAction", it.value().actionType );
    }
    config.sync();
}

void TrashConfigModule::setupGui()
{
    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setMargin( 11 );
    layout->setSpacing( 6 );

    TrashImpl::TrashDirMap map = mTrashImpl->trashDirectories();
    if ( map.count() != 1 ) {
        // If we have multiple trashes, we setup a widget to choose
        // which trash to configure
        QListWidget *mountPoints = new QListWidget( this );
        layout->addWidget( mountPoints );

        QMapIterator<int, QString> it( map );
        while ( it.hasNext() ) {
            it.next();
            DiscSpaceUtil util( it.value() );
            QListWidgetItem *item = new QListWidgetItem( KIcon( "folder" ), util.mountPoint() );
            item->setData( Qt::UserRole, it.key() );

            mountPoints->addItem( item );
        }

        mountPoints->setCurrentRow( 0 );

        connect( mountPoints, SIGNAL( currentItemChanged( QListWidgetItem*, QListWidgetItem* ) ),
                 this, SLOT( trashChanged( QListWidgetItem* ) ) );
    } else {
        mCurrentTrash = map.value( 0 );
    }

    QHBoxLayout *daysLayout = new QHBoxLayout();
    layout->addLayout( daysLayout );

    mUseTimeLimit = new QCheckBox( i18n( "Delete files older than:" ), this );
    daysLayout->addWidget( mUseTimeLimit );
    mDays = new QSpinBox( this );
    mDays->setRange( 1, 365 );
    mDays->setSingleStep( 1 );
    mDays->setSuffix( " days" );
    daysLayout->addWidget( mDays );

    QGridLayout *sizeLayout = new QGridLayout();
    layout->addLayout( sizeLayout );

    mUseSizeLimit = new QCheckBox( i18n( "Limit to maximum size" ), this );
    sizeLayout->addWidget( mUseSizeLimit, 0, 0, 1, 2 );

    mSizeWidget = new QWidget( this );
    sizeLayout->setColumnMinimumWidth( 0, 30 );
    sizeLayout->addWidget( mSizeWidget, 1, 1 );

    QGridLayout *sizeWidgetLayout = new QGridLayout( mSizeWidget );
    sizeWidgetLayout->setMargin( 11 );
    sizeWidgetLayout->setSpacing( 6 );

    QLabel *label = new QLabel( i18n( "Maximum Size:" ), mSizeWidget );
    sizeWidgetLayout->addWidget( label, 0, 0 );

    mPercent = new QDoubleSpinBox( mSizeWidget );
    mPercent->setRange( 0.001, 100 );
    mPercent->setDecimals( 3 );
    mPercent->setSingleStep( 1 );
    mPercent->setSuffix( " %" );
    sizeWidgetLayout->addWidget( mPercent, 0, 1 );

    mSizeLabel = new QLabel( mSizeWidget );
    sizeWidgetLayout->addWidget( mSizeLabel, 0, 2 );

    label = new QLabel( i18n( "When limit reached:" ), mSizeWidget );
    sizeWidgetLayout->addWidget( label, 1, 0 );

    mLimitReachedAction = new QComboBox( mSizeWidget );
    mLimitReachedAction->addItem( i18n( "Warn me" ) );
    mLimitReachedAction->addItem( i18n( "Delete oldest files from trash" ) );
    mLimitReachedAction->addItem( i18n( "Delete biggest files from trash" ) );
    sizeWidgetLayout->addWidget( mLimitReachedAction, 1, 1, 1, 2 );

    layout->addStretch();
}

#include "kcmtrash.moc"
