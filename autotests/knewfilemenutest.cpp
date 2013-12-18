/* This file is part of the KDE libraries
    Copyright (c) 2012 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QtTest/QtTest>

#include <QDialog>
#include <QLineEdit>
#include <QMenu>
#include <knameandurlinputdialog.h>
#include <kactioncollection.h>
#include <knewfilemenu.h>

#include <qtemporarydir.h>

class KNewFileMenuTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        m_first = true;
    }

    void cleanupTestCase()
    {
    }

    void test_data()
    {
        QTest::addColumn<QString>("actionText");
        QTest::addColumn<QString>("typedFilename");
        QTest::addColumn<QString>("expectedFilename");

        QTest::newRow("text file") << "Text File" << "tmp_knewfilemenutest.txt" << "tmp_knewfilemenutest.txt";
        QTest::newRow("text file with jpeg extension") << "Text File" << "foo.jpg" << "foo.jpg.txt";
        QTest::newRow("url desktop file") << "Link to Location " << "tmp_link.desktop" << "tmp_link.desktop";
        QTest::newRow("url desktop file no extension") << "Link to Location " << "tmp_link" << "tmp_link";
        QTest::newRow("url desktop file .pl extension") << "Link to Location " << "tmp_link.pl" << "tmp_link.pl.desktop";
        QTest::newRow("symlink") << "Basic link" << "thelink" << "thelink";
    }

    void test()
    {
        QFETCH(QString, actionText);
        QFETCH(QString, typedFilename);
        QFETCH(QString, expectedFilename);

        QWidget parentWidget;
        KActionCollection coll(this, "foo");
        KNewFileMenu menu(&coll, "the_action", this);
        menu.setModal(false);
        menu.setParentWidget(&parentWidget);
        QList<QUrl> lst;
        lst << QUrl::fromLocalFile(m_tmpDir.path());;
        menu.setPopupFiles(lst);
        menu.checkUpToDate();
        QAction* action = coll.action("the_action");
        QVERIFY(action);
        QAction* textAct = 0;
        Q_FOREACH(QAction* act, action->menu()->actions()) {
            qDebug() << act << act->text() << act->data();
            if (act->text().contains(actionText))
                textAct = act;
        }
        if (!textAct && m_first) {
            const QString err = "action with text \"" + actionText + "\" not found. kde-baseapps not installed?";
            QSKIP(qPrintable(err));
        }
        textAct->trigger();
        QDialog* dialog = parentWidget.findChild<QDialog *>();
        QVERIFY(dialog);
        KNameAndUrlInputDialog* nauiDialog = qobject_cast<KNameAndUrlInputDialog *>(dialog);
        if (nauiDialog) {
            nauiDialog->setSuggestedName(typedFilename);
            nauiDialog->setSuggestedUrl(QUrl("file:///etc"));
        } else {
            QLineEdit* lineEdit = dialog->findChild<QLineEdit *>();
            QVERIFY(lineEdit);
            lineEdit->setText(typedFilename);
        }
        dialog->accept();
        QSignalSpy spy(&menu, SIGNAL(fileCreated(QUrl)));
        QVERIFY(spy.wait(1000));
        const QUrl url = spy.at(0).at(0).value<QUrl>();
        const QString path = m_tmpDir.path() + '/' + expectedFilename;
        QCOMPARE(url.toLocalFile(), path);
        QFile::remove(path);
        m_first = false;
    }
private:
    QTemporaryDir m_tmpDir;
    bool m_first;
};

QTEST_MAIN(KNewFileMenuTest)

#include "knewfilemenutest.moc"
