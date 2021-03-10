/*
    SPDX-FileCopyrightText: 2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "useragentinfo.h"

// std
#include <time.h>

// Qt
#include <QLocale>
#include <QSysInfo>

// KDE
#include <KServiceTypeTrader>

UserAgentInfo::UserAgentInfo()
{
    m_bIsDirty = true;
}

UserAgentInfo::StatusCode UserAgentInfo::createNewUAProvider(const QString &uaStr)
{
    QStringList split;
    int pos = (uaStr).indexOf(QLatin1String("::"));

    if (pos == -1) {
        pos = (uaStr).indexOf(QLatin1Char(':'));
        if (pos != -1) {
            split.append(uaStr.left(pos));
            split.append(uaStr.mid(pos + 1));
        }
    } else {
        split = uaStr.split(QStringLiteral("::"));
    }

    if (m_lstIdentity.contains(split[1])) {
        return DUPLICATE_ENTRY;
    } else {
        int count = split.count();
        m_lstIdentity.append(split[1]);
        if (count > 2) {
            m_lstAlias.append(split[2]);
        } else {
            m_lstAlias.append(split[1]);
        }
    }

    return SUCCEEDED;
}

void UserAgentInfo::loadFromDesktopFiles()
{
    m_providers.clear();
    m_providers = KServiceTypeTrader::self()->query(QStringLiteral("UserAgentStrings"));
}

void UserAgentInfo::parseDescription()
{
    auto propertyToString = [](const KService::Ptr &service, const QString &prop) {
        return service->property(prop).toString();
    };

    for (const auto &service : qAsConst(m_providers)) {
        QString tmp = propertyToString(service, QStringLiteral("X-KDE-UA-FULL"));

        if (service->property(QStringLiteral("X-KDE-UA-DYNAMIC-ENTRY")).toBool()) {
            tmp.replace(QLatin1String("appSysName"), QSysInfo::productType());
            tmp.replace(QLatin1String("appSysRelease"), QSysInfo::kernelVersion());
            tmp.replace(QLatin1String("appMachineType"), QSysInfo::currentCpuArchitecture());

            QStringList languageList = QLocale().uiLanguages();
            if (!languageList.isEmpty()) {
                int ind = languageList.indexOf(QLatin1String("C"));
                if (ind >= 0) {
                    if (languageList.contains(QLatin1String("en"))) {
                        languageList.removeAt(ind);
                    } else {
                        languageList.value(ind) = QStringLiteral("en");
                    }
                }
            }

            tmp.replace(QLatin1String("appLanguage"), languageList.join(QLatin1String(", ")));
            tmp.replace(QLatin1String("appPlatform"), QLatin1String("X11"));
        }

        // Ignore dups...
        if (m_lstIdentity.contains(tmp)) {
            continue;
        }

        m_lstIdentity << tmp;

        tmp = QLatin1String("%1 %2").arg(propertyToString(service, QStringLiteral("X-KDE-UA-SYSNAME")),
                                         propertyToString(service, QStringLiteral("X-KDE-UA-SYSRELEASE")));
        if (tmp.trimmed().isEmpty()) {
            tmp = QLatin1String("%1 %2").arg(propertyToString(service, QStringLiteral("X-KDE-UA-NAME")),
                                             propertyToString(service, QStringLiteral("X-KDE-UA-VERSION")));
        } else {
            tmp = QLatin1String("%1 %2 on %3").arg(propertyToString(service, QStringLiteral("X-KDE-UA-NAME")), (QStringLiteral("X-KDE-UA-VERSION")), tmp);
        }

        m_lstAlias << tmp;
    }

    m_bIsDirty = false;
}

QString UserAgentInfo::aliasStr(const QString &name)
{
    int id = userAgentStringList().indexOf(name);
    if (id == -1) {
        return QString();
    } else {
        return m_lstAlias.at(id);
    }
}

QString UserAgentInfo::agentStr(const QString &name)
{
    int id = userAgentAliasList().indexOf(name);
    if (id == -1) {
        return QString();
    } else {
        return m_lstIdentity.at(id);
    }
}

QStringList UserAgentInfo::userAgentStringList()
{
    if (m_bIsDirty) {
        loadFromDesktopFiles();
        if (m_providers.isEmpty()) {
            return QStringList();
        }
        parseDescription();
    }
    return m_lstIdentity;
}

QStringList UserAgentInfo::userAgentAliasList()
{
    if (m_bIsDirty) {
        loadFromDesktopFiles();
        if (m_providers.isEmpty()) {
            return QStringList();
        }
        parseDescription();
    }
    return m_lstAlias;
}
