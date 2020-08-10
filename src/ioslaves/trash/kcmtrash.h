/*
    SPDX-FileCopyrightText: 2008 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KCMTRASH_H
#define KCMTRASH_H

#include <KCModule>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QLabel;
class QListWidgetItem;
class QSpinBox;
class TrashImpl;

/**
 * @brief Allow to configure the trash.
 */
class TrashConfigModule : public KCModule
{
    Q_OBJECT

public:
    TrashConfigModule(QWidget *parent, const QVariantList &args);
    virtual ~TrashConfigModule();

    void save() override;
    void defaults() override;

private Q_SLOTS:
    void percentChanged(double);
    void trashChanged(QListWidgetItem *);
    void trashChanged(int);
    void useTypeChanged();

private:
    void readConfig();
    void writeConfig();
    void setupGui();

    QCheckBox *mUseTimeLimit;
    QSpinBox *mDays;
    QCheckBox *mUseSizeLimit;
    QWidget *mSizeWidget;
    QDoubleSpinBox *mPercent;
    QLabel *mSizeLabel;
    QComboBox *mLimitReachedAction;

    TrashImpl *mTrashImpl;
    QString mCurrentTrash;
    bool trashInitialize;
    typedef struct {
        bool useTimeLimit;
        int days;
        bool useSizeLimit;
        double percent;
        int actionType;
    } ConfigEntry;

    typedef QMap<QString, ConfigEntry> ConfigMap;
    ConfigMap mConfigMap;
};

#endif // KCMTRASH_H
