// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

#include <QSignalSpy>
#include <QTest>

#include <KIO/BuildSycocaInterface>
#include <KIO/BuildSycocaJob>
#include <KJobUiDelegate>

#include "kiotesthelper.h"

class Iface : public KIO::BuildSycocaInterface
{
public:
    using KIO::BuildSycocaInterface::BuildSycocaInterface;
    void showProgress() override
    {
        m_showProgressCalled = true;
    }
    void hideProgress() override
    {
        m_hideProgressCalled = true;
    }

    bool m_showProgressCalled = false;
    bool m_hideProgressCalled = false;
};

class BuildSycocaJobTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCore()
    {
        auto job = KIO::buildSycoca(KIO::JobFlag::HideProgressInfo);
        QCOMPARE(job->uiDelegate(), nullptr);
        job->exec();
    }

    void testUI()
    {
        auto job = KIO::buildSycoca();
        auto delegate = new KJobUiDelegate;
        auto iface = new Iface;
        iface->setParent(delegate);
        job->setUiDelegate(delegate);
        job->exec();
        QVERIFY(iface->m_showProgressCalled);
        QVERIFY(iface->m_hideProgressCalled);
    }
};

QTEST_MAIN(BuildSycocaJobTest)

#include "buildsycocajobtest.moc"
