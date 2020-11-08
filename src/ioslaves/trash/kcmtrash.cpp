/*
    SPDX-FileCopyrightText: 2008 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kcmtrash.h"
#include "discspaceutil.h"
#include "trashimpl.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <QDialog>
#include <QIcon>
#include <KPluginFactory>
#include <KFormat>
#include <QSpinBox>

K_PLUGIN_FACTORY(KCMTrashConfigFactory, registerPlugin<TrashConfigModule>(QStringLiteral("trash"));)

TrashConfigModule::TrashConfigModule(QWidget *parent, const QVariantList &)
    : KCModule( //KCMTrashConfigFactory::componentData(),
        parent), trashInitialize(false)
{
    mTrashImpl = new TrashImpl();
    mTrashImpl->init();

    readConfig();

    setupGui();

    useTypeChanged();

    connect(mUseTimeLimit, &QAbstractButton::toggled,
            this, &TrashConfigModule::markAsChanged);
    connect(mUseTimeLimit, &QAbstractButton::toggled,
            this, &TrashConfigModule::useTypeChanged);
    connect(mDays, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TrashConfigModule::markAsChanged);
    connect(mUseSizeLimit, &QAbstractButton::toggled,
            this, &TrashConfigModule::markAsChanged);
    connect(mUseSizeLimit, &QAbstractButton::toggled,
            this, &TrashConfigModule::useTypeChanged);
    connect(mPercent, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TrashConfigModule::percentChanged);
    connect(mPercent, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TrashConfigModule::markAsChanged);
    connect(mLimitReachedAction, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrashConfigModule::markAsChanged);

    trashChanged(0);
    trashInitialize = true;
}

TrashConfigModule::~TrashConfigModule()
{
}

void TrashConfigModule::save()
{
    if (!mCurrentTrash.isEmpty()) {
        ConfigEntry entry;
        entry.useTimeLimit = mUseTimeLimit->isChecked();
        entry.days = mDays->value();
        entry.useSizeLimit = mUseSizeLimit->isChecked();
        entry.percent = mPercent->value(),
              entry.actionType = mLimitReachedAction->currentIndex();
        mConfigMap.insert(mCurrentTrash, entry);
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
    mConfigMap.insert(mCurrentTrash, entry);
    trashInitialize = false;
    trashChanged(0);
}

void TrashConfigModule::percentChanged(double percent)
{
    DiscSpaceUtil util(mCurrentTrash);

    qulonglong partitionSize = util.size();
    double size = ((double)(partitionSize / 100)) * percent;

    KFormat format;
    mSizeLabel->setText(QLatin1Char('(') + format.formatByteSize(size, 2) + QLatin1Char(')'));
}

void TrashConfigModule::trashChanged(QListWidgetItem *item)
{
    trashChanged(item->data(Qt::UserRole).toInt());
}

void TrashConfigModule::trashChanged(int value)
{
    const TrashImpl::TrashDirMap map = mTrashImpl->trashDirectories();

    if (!mCurrentTrash.isEmpty() && trashInitialize) {
        ConfigEntry entry;
        entry.useTimeLimit = mUseTimeLimit->isChecked();
        entry.days = mDays->value();
        entry.useSizeLimit = mUseSizeLimit->isChecked();
        entry.percent = mPercent->value(),
              entry.actionType = mLimitReachedAction->currentIndex();
        mConfigMap.insert(mCurrentTrash, entry);
    }

    mCurrentTrash = map[ value ];
    const auto currentTrashIt = mConfigMap.constFind(mCurrentTrash);
    if (currentTrashIt != mConfigMap.constEnd()) {
        const ConfigEntry &entry = *currentTrashIt;
        mUseTimeLimit->setChecked(entry.useTimeLimit);
        mDays->setValue(entry.days);
        mUseSizeLimit->setChecked(entry.useSizeLimit);
        mPercent->setValue(entry.percent);
        mLimitReachedAction->setCurrentIndex(entry.actionType);
    } else {
        mUseTimeLimit->setChecked(false);
        mDays->setValue(7);
        mUseSizeLimit->setChecked(true);
        mPercent->setValue(10.0);
        mLimitReachedAction->setCurrentIndex(0);
    }
    mDays->setSuffix(i18n(" days"));     // missing in Qt: plural form handling

    percentChanged(mPercent->value());

}

void TrashConfigModule::useTypeChanged()
{
    mDays->setEnabled(mUseTimeLimit->isChecked());
    mPercent->setEnabled(mUseSizeLimit->isChecked());
    mSizeLabel->setEnabled(mUseSizeLimit->isChecked());
}

void TrashConfigModule::readConfig()
{
    KConfig config(QStringLiteral("ktrashrc"));
    mConfigMap.clear();

    const QStringList groups = config.groupList();
    for (int i = 0; i < groups.count(); ++i) {
        if (groups[ i ].startsWith(QLatin1Char('/'))) {
            const KConfigGroup group = config.group(groups[ i ]);

            ConfigEntry entry;
            entry.useTimeLimit = group.readEntry("UseTimeLimit", false);
            entry.days = group.readEntry("Days", 7);
            entry.useSizeLimit = group.readEntry("UseSizeLimit", true);
            entry.percent = group.readEntry("Percent", 10.0);
            entry.actionType = group.readEntry("LimitReachedAction", 0);
            mConfigMap.insert(groups[ i ], entry);
        }
    }
}

void TrashConfigModule::writeConfig()
{
    KConfig config(QStringLiteral("ktrashrc"));

    // first delete all existing groups
    const QStringList groups = config.groupList();
    for (int i = 0; i < groups.count(); ++i)
        if (groups[ i ].startsWith(QLatin1Char('/'))) {
            config.deleteGroup(groups[ i ]);
        }

    QMapIterator<QString, ConfigEntry> it(mConfigMap);
    while (it.hasNext()) {
        it.next();
        KConfigGroup group = config.group(it.key());

        group.writeEntry("UseTimeLimit", it.value().useTimeLimit);
        group.writeEntry("Days", it.value().days);
        group.writeEntry("UseSizeLimit", it.value().useSizeLimit);
        group.writeEntry("Percent", it.value().percent);
        group.writeEntry("LimitReachedAction", it.value().actionType);
    }
    config.sync();
}

void TrashConfigModule::setupGui()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

#ifdef Q_OS_OSX
    QLabel *infoText = new QLabel( i18n( "<para>KDE's wastebin is configured to use the <b>Finder</b>'s Trash.<br></para>" ) );
    infoText->setWhatsThis( i18nc( "@info:whatsthis",
                                        "<para>Emptying KDE's wastebin will remove only KDE's trash items, while<br>"
                                        "emptying the Trash through the Finder will delete everything.</para>"
                                        "<para>KDE's trash items will show up in a folder called KDE.trash, in the Trash can.</para>"
                                        ) );
    layout->addWidget( infoText );
#endif

    TrashImpl::TrashDirMap map = mTrashImpl->trashDirectories();
    if (map.count() != 1) {
        // If we have multiple trashes, we setup a widget to choose
        // which trash to configure
        QListWidget *mountPoints = new QListWidget(this);
        layout->addWidget(mountPoints);

        QMapIterator<int, QString> it(map);
        while (it.hasNext()) {
            it.next();
            DiscSpaceUtil util(it.value());
            QListWidgetItem *item = new QListWidgetItem(QIcon(QStringLiteral("folder")), util.mountPoint());
            item->setData(Qt::UserRole, it.key());

            mountPoints->addItem(item);
        }

        mountPoints->setCurrentRow(0);

        connect(mountPoints, &QListWidget::currentItemChanged,
                this, QOverload<QListWidgetItem*>::of(&TrashConfigModule::trashChanged));
    } else {
        mCurrentTrash = map.value(0);
    }

    QFormLayout* formLayout = new QFormLayout();
    layout->addLayout(formLayout);

    QHBoxLayout *daysLayout = new QHBoxLayout();

    mUseTimeLimit = new QCheckBox(i18n("Delete files older than"), this);
    mUseTimeLimit->setWhatsThis(xi18nc("@info:whatsthis",
                                       "<para>Check this box to allow <emphasis strong='true'>automatic deletion</emphasis> of files that are older than the value specified. "
                                       "Leave this disabled to <emphasis strong='true'>not</emphasis> automatically delete any items after a certain timespan</para>"));
    daysLayout->addWidget(mUseTimeLimit);
    mDays = new QSpinBox(this);

    mDays->setRange(1, 365);
    mDays->setSingleStep(1);
    mDays->setSuffix(i18np(" day", " days", mDays->value()));
    mDays->setWhatsThis(xi18nc("@info:whatsthis",
                               "<para>Set the number of days that files can remain in the trash. "
                               "Any files older than this will be automatically deleted.</para>"));
    daysLayout->addWidget(mDays);
    daysLayout->addStretch();
    formLayout->addRow(i18n("Cleanup:"), daysLayout);


    QHBoxLayout *maximumSizeLayout = new QHBoxLayout();
    mUseSizeLimit = new QCheckBox(i18n("Limit to"), this);
    mUseSizeLimit->setWhatsThis(xi18nc("@info:whatsthis",
                                       "<para>Check this box to limit the trash to the maximum amount of disk space that you specify below. "
                                       "Otherwise, it will be unlimited.</para>"));
    maximumSizeLayout->addWidget(mUseSizeLimit);
    formLayout->addRow(i18n("Size:"), maximumSizeLayout);


    mPercent = new QDoubleSpinBox(this);
    mPercent->setRange(0.01, 100);
    mPercent->setDecimals(2);
    mPercent->setSingleStep(1);
    mPercent->setSuffix(QStringLiteral(" %"));
    mPercent->setWhatsThis(xi18nc("@info:whatsthis",
                                  "<para>This is the maximum percent of disk space that will be used for the trash.</para>"));
    maximumSizeLayout->addWidget(mPercent);

    mSizeLabel = new QLabel(this);
    mSizeLabel->setWhatsThis(xi18nc("@info:whatsthis",
                                    "<para>This is the calculated amount of disk space that will be allowed for the trash, the maximum.</para>"));
    maximumSizeLayout->addWidget(mSizeLabel);


    mLimitReachedAction = new QComboBox();
    mLimitReachedAction->addItem(i18n("Show a Warning"));
    mLimitReachedAction->addItem(i18n("Delete Oldest Files From Trash"));
    mLimitReachedAction->addItem(i18n("Delete Biggest Files From Trash"));
    mLimitReachedAction->setWhatsThis(xi18nc("@info:whatsthis",
                                      "<para>When the size limit is reached, it will prefer to delete the type of files that you specify, first. "
                                      "If this is set to warn you, it will do so instead of automatically deleting files.</para>"));
    formLayout->addRow(i18n("Full Trash:"), mLimitReachedAction);

    layout->addStretch();
}

#include "kcmtrash.moc"
