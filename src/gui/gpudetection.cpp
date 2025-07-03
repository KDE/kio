/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2023 Dave Vasilevsky <dave@vasilevsky.ca>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only
*/

#include "gpudetection_p.h"

#ifdef WITH_QTDBUS
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#endif
#include <QMap>
#include <QString>

#include <KProcess>

static void checkGpu();
// Returns true if switcheroo present
static bool checkGpuWithSwitcheroo();
static void checkGpuWithSolid();

// TODO: GPUs are hot-swappable, watch for changes using dbus PropertiesChanged
enum class GpuCheck {
    NotChecked,
    Present,
    Absent,
};
static GpuCheck s_gpuCheck = GpuCheck::NotChecked;
static QProcessEnvironment s_gpuEnv;

static void checkGpu()
{
    if (s_gpuCheck == GpuCheck::NotChecked) {
        if (!checkGpuWithSwitcheroo()) {
            checkGpuWithSolid();
        }
    }
}

static bool checkGpuWithSwitcheroo()
{
#ifdef WITH_QTDBUS
    QDBusInterface switcheroo(QStringLiteral("net.hadess.SwitcherooControl"),
                              QStringLiteral("/net/hadess/SwitcherooControl"),
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              QDBusConnection::systemBus());
    if (!switcheroo.isValid()) {
        return false;
    }

    QDBusReply<QDBusVariant> reply = switcheroo.call(QStringLiteral("Get"), QStringLiteral("net.hadess.SwitcherooControl"), QStringLiteral("GPUs"));
    if (!reply.isValid()) {
        return false;
    }

    QDBusArgument arg = qvariant_cast<QDBusArgument>(reply.value().variant());
    QList<QVariantMap> gpus;
    arg >> gpus;

    if (gpus.isEmpty()) {
        // No GPU(s) found
        return false;
    } else if (gpus.size() == 1) {
        // There is only one GPU, no need to check for others
        s_gpuCheck = GpuCheck::Absent;
        return true;
    }

    QVariantMap defaultGpu;
    QVariantMap firstDiscreteGpu;
    QVariantMap firstNonDefaultGpu;

    for (const auto &gpu : std::as_const(gpus)) {
        if (defaultGpu.isEmpty() && gpu[QStringLiteral("Default")].toBool()) {
            defaultGpu = gpu;
        } else if (firstNonDefaultGpu.isEmpty()) {
            firstNonDefaultGpu = gpu;
        }
        if (firstDiscreteGpu.isEmpty() && gpu[QStringLiteral("Discrete")].toBool()) {
            firstDiscreteGpu = gpu;
        }
    }

    if (!defaultGpu.isEmpty() && defaultGpu[QStringLiteral("Discrete")].toBool()) {
        // If the default GPU is discrete there is no need to do anything special
        s_gpuCheck = GpuCheck::Absent;
        return true;
    }

    // Otherwise prefer the discrete GPU over any other random non-default GPU (legacy behavior)
    for (const auto &gpu : {firstDiscreteGpu, firstNonDefaultGpu}) {
        if (gpu.isEmpty()) {
            continue;
        }
        s_gpuCheck = GpuCheck::Present;
        auto envList = gpu[QStringLiteral("Environment")].toStringList();
        for (int i = 0; i + 1 < envList.size(); i += 2) {
            s_gpuEnv.insert(envList[i], envList[i + 1]);
        }
        return true;
    }
#endif

    // No discrete or non-default GPU found
    s_gpuCheck = GpuCheck::Absent;
    return true;
}

static void checkGpuWithSolid()
{
#ifdef WITH_QTDBUS
    // TODO: Consider moving this check into kio, instead of using Solid
    QDBusInterface iface(QStringLiteral("org.kde.Solid.PowerManagement"),
                         QStringLiteral("/org/kde/Solid/PowerManagement"),
                         QStringLiteral("org.kde.Solid.PowerManagement"),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        QDBusReply<bool> reply = iface.call(QStringLiteral("hasDualGpu"));
        if (reply.isValid() && reply.value()) {
            s_gpuCheck = GpuCheck::Present;
            s_gpuEnv.insert(QStringLiteral("DRI_PRIME"), QStringLiteral("1"));
            return;
        }
    }

    s_gpuCheck = GpuCheck::Absent;
#endif
}

namespace KIO
{

bool hasDiscreteGpu()
{
    checkGpu();
    return s_gpuCheck == GpuCheck::Present;
}

QProcessEnvironment discreteGpuEnvironment()
{
    checkGpu();
    return s_gpuEnv;
}

}
