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

#ifndef TRASHCONFIGMODULE_H
#define TRASHCONFIGMODULE_H

#include <kcmodule.h>

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
        TrashConfigModule( QWidget* parent, const QVariantList& args );
        virtual ~TrashConfigModule();

        virtual void save();
        virtual void defaults();

    private Q_SLOTS:
        void percentChanged( double );
        void trashChanged( QListWidgetItem* );
        void trashChanged( int );
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

#endif
